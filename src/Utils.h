#pragma once
#include <string>
#include <vector>
#include <functional>

namespace util {

// Quote a path for Windows cmd (simple)
std::string quote(const std::string &s);

// Run a command and return exit code
int runCommand(const std::string &cmd);

// Run a command and capture stdout (returns pair<exitcode,stdout>)
std::pair<int, std::string> runCapture(const std::string &cmd);

// Create directories recursively, return true on success
bool ensureDir(const std::string &path);

// Get absolute path (basic)
std::string absolutePath(const std::string &path);

// Random selection: pick up to N indices from 0..(n-1)
std::vector<int> pickRandomIndices(int n, int want);

// Produce a fast fingerprint string for a file (size + last_write_time)
std::string fileFingerprint(const std::string &path);

// Simple thread pool
class ThreadPool {
public:
    explicit ThreadPool(size_t workerCount = 1);
    ~ThreadPool();

    // Enqueue a job (callable<void()>)
    void enqueue(std::function<void()> job);

    // Wait until all jobs finished (simple barrier)
    void waitAll();

private:
    struct Impl;
    Impl* impl_;
};

} // namespace util