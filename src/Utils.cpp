#include "Utils.h"
#include <cstdlib>
#include <filesystem>
#include <random>
#include <sstream>
#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

#ifdef _WIN32
#include <windows.h>
#endif

namespace util {

std::string quote(const std::string &s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += '\\';
        out += c;
    }
    out += "\"";
    return out;
}

int runCommand(const std::string &cmd) {
    std::cout << "[cmd] " << cmd << std::endl;
    return std::system(cmd.c_str());
}

std::pair<int, std::string> runCapture(const std::string &cmd) {
#ifdef _WIN32
    FILE *pipe = _popen(cmd.c_str(), "r");
    if (!pipe) return { -1, "" };
    std::string result;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result += buffer;
    }
    int rc = _pclose(pipe);
    return { rc, result };
#else
    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe) return { -1, "" };
    std::string result;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result += buffer;
    }
    int rc = pclose(pipe);
    return { rc, result };
#endif
}

bool ensureDir(const std::string &path) {
    try {
        std::filesystem::create_directories(path);
        return true;
    } catch (...) {
        return false;
    }
}

std::string absolutePath(const std::string &path) {
    try {
        return std::filesystem::absolute(path).string();
    } catch (...) {
        return path;
    }
}

std::vector<int> pickRandomIndices(int n, int want) {
    std::vector<int> all(n);
    for (int i = 0; i < n; ++i) all[i] = i;
    std::random_device rd;
    std::mt19937 g(rd());
    if (want >= n) return all;
    std::shuffle(all.begin(), all.end(), g);
    std::vector<int> out(all.begin(), all.begin() + want);
    return out;
}

std::string fileFingerprint(const std::string &path) {
    try {
        std::filesystem::path p(path);
        if (!std::filesystem::exists(p)) return "";
        auto fsize = std::filesystem::file_size(p);
        auto ftime = std::filesystem::last_write_time(p);
        // Convert file_time_type to time_t safely via system_clock
        using namespace std::chrono;
        auto sctp = time_point_cast<system_clock::duration>(ftime - decltype(ftime)::clock::now()
                     + system_clock::now());
        std::time_t tt = system_clock::to_time_t(sctp);
        std::ostringstream ss;
        ss << fsize << "-" << tt;
        return ss.str();
    } catch (...) {
        return "";
    }
}

// ThreadPool implementation (very small)
struct util::ThreadPool::Impl {
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex mtx;
    std::condition_variable cv;
    bool stop = false;
    std::atomic<int> active{0};
    Impl(size_t n) {
        for (size_t i = 0; i < n; ++i) {
            workers.emplace_back([this](){
                while (true) {
                    std::function<void()> job;
                    {
                        std::unique_lock<std::mutex> lk(mtx);
                        cv.wait(lk, [this](){ return stop || !tasks.empty(); });
                        if (stop && tasks.empty()) return;
                        job = std::move(tasks.front());
                        tasks.pop();
                        active++;
                    }
                    try { job(); } catch (...) {}
                    active--;
                    cv.notify_all();
                }
            });
        }
    }
    ~Impl() {
        {
            std::unique_lock<std::mutex> lk(mtx);
            stop = true;
        }
        cv.notify_all();
        for (auto &t : workers) if (t.joinable()) t.join();
    }
};

util::ThreadPool::ThreadPool(size_t workerCount) {
    if (workerCount == 0) workerCount = 1;
    impl_ = new Impl(workerCount);
}

util::ThreadPool::~ThreadPool() {
    delete impl_;
}

void util::ThreadPool::enqueue(std::function<void()> job) {
    {
        std::unique_lock<std::mutex> lk(impl_->mtx);
        impl_->tasks.push(std::move(job));
    }
    impl_->cv.notify_one();
}

void util::ThreadPool::waitAll() {
    std::unique_lock<std::mutex> lk(impl_->mtx);
    impl_->cv.wait(lk, [this](){ return impl_->tasks.empty() && impl_->active.load() == 0; });
}

} // namespace util