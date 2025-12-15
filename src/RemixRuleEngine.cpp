#include "RemixRuleEngine.h"
#include "FFmpegCommandBuilder.h"
#include "PreviewPlayer.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

RemixRuleEngine::RemixRuleEngine(const std::string &ffmpegPath, const std::string &workdir)
    : ffmpegPath_(ffmpegPath), workdir_(workdir) {
    if (!fs::exists(workdir_)) {
        fs::create_directories(workdir_);
    }
}

RemixRuleEngine::~RemixRuleEngine() = default;

int RemixRuleEngine::runCommand(const std::string &cmd, bool dryRun) {
    std::cout << "[exec] " << cmd << std::endl;
    if (dryRun) return 0;
    int r = std::system(cmd.c_str());
    if (r != 0) {
        std::cerr << "Command failed with code: " << r << std::endl;
    }
    return r;
}

int RemixRuleEngine::processStutter(const std::string &input, double start, double duration, int repeats, const std::string &output, bool dryRun) {
    fs::path frag = fs::path(workdir_) / "stutter_fragment.mp4";
    auto cmd1 = FFmpegCommandBuilder::extractFragmentCmd(ffmpegPath_, input, start, duration, frag.string(), false, "libx264");
    if (runCommand(cmd1, dryRun) != 0) return 1;

    fs::path listFile = fs::path(workdir_) / "stutter_list.txt";
    std::ofstream ofs(listFile);
    for (int i = 0; i < repeats; ++i) {
        ofs << "file '" << fs::absolute(frag).string() << "'\n";
    }
    ofs.close();

    auto cmd2 = FFmpegCommandBuilder::makeRepeatConcatCmd(ffmpegPath_, frag.string(), repeats, output, listFile.string());
    return runCommand(cmd2, dryRun);
}

int RemixRuleEngine::processOverlay(const std::string &input, const std::string &overlay, double start, double end, double scale, const std::string &position, const std::string &output, bool dryRun) {
    auto cmd = FFmpegCommandBuilder::overlayCmd(ffmpegPath_, input, overlay, start, end, scale, position, output);
    return runCommand(cmd, dryRun);
}

int RemixRuleEngine::processPitch(const std::string &input, double semitones, const std::string &output, bool dryRun) {
    auto cmd = FFmpegCommandBuilder::pitchShiftCmd(ffmpegPath_, input, semitones, output);
    return runCommand(cmd, dryRun);
}

int RemixRuleEngine::processRandomChop(const std::string &input, int count, double min_len, double max_len, bool shuffle, const std::string &output, bool dryRun) {
    double duration = 0.0;
    {
        fs::path probe = fs::path(ffmpegPath_).parent_path() / "ffprobe.exe";
        if (fs::exists(probe)) {
            std::ostringstream pcmd;
            pcmd << '"' << probe.string() << "\" -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 " << FFmpegCommandBuilder::quote(input);
            FILE *pipe = _popen(pcmd.str().c_str(), "r");
            if (pipe) {
                char buf[128];
                if (fgets(buf, sizeof(buf), pipe)) {
                    try { duration = std::stod(buf); } catch (...) { duration = 0.0; }
                }
                _pclose(pipe);
            }
        }
    }

    if (duration <= 0.0) {
        duration = 60.0;
        std::cout << "Warning: unable to probe duration; assuming " << duration << "s\n";
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> startDist(0.0, std::max(0.0, duration - min_len));
    std::uniform_real_distribution<> lenDist(min_len, max_len);

    std::vector<std::pair<double,double>> segs;
    for (int i = 0; i < count; ++i) {
        double len = lenDist(gen);
        double start = startDist(gen);
        if (start + len > duration) start = std::max(0.0, duration - len);
        segs.emplace_back(start, len);
    }

    if (shuffle) std::shuffle(segs.begin(), segs.end(), gen);

    std::vector<std::string> fragFiles;
    for (int i = 0; i < (int)segs.size(); ++i) {
        fs::path frag = fs::path(workdir_) / ("rand_frag_" + std::to_string(i) + ".mp4");
        auto cmd = FFmpegCommandBuilder::randomChopExtractCmd(ffmpegPath_, input, i, segs[i].first, segs[i].second, frag.string());
        if (runCommand(cmd, dryRun) != 0) return 1;
        fragFiles.push_back(fs::absolute(frag).string());
    }

    fs::path listFile = fs::path(workdir_) / "rand_list.txt";
    std::ofstream ofs(listFile);
    for (auto &f : fragFiles) {
        ofs << "file '" << f << "'\n";
    }
    ofs.close();

    auto concatCmd = FFmpegCommandBuilder::concatFromListCmd(ffmpegPath_, listFile.string(), output);
    return runCommand(concatCmd, dryRun);
}

int RemixRuleEngine::processConcat(const std::vector<std::string> &inputs, const std::string &output, bool dryRun) {
    fs::path listFile = fs::path(workdir_) / "concat_list.txt";
    std::ofstream ofs(listFile);
    for (auto &p : inputs) {
        ofs << "file '" << fs::absolute(p).string() << "'\n";
    }
    ofs.close();
    auto cmd = FFmpegCommandBuilder::concatFromListCmd(ffmpegPath_, listFile.string(), output);
    return runCommand(cmd, dryRun);
}

int RemixRuleEngine::processBleep(const std::string &input, const std::vector<std::pair<double,double>> &ranges, const std::string &output, bool dryRun) {
    if (ranges.empty()) {
        std::cerr << "bleep: no timestamp ranges provided\n";
        return 1;
    }
    auto cmd = FFmpegCommandBuilder::bleepCensorCmd(ffmpegPath_, input, ranges, output);
    return runCommand(cmd, dryRun);
}

int RemixRuleEngine::processPreview(const std::string &file, bool loop, bool dryRun) {
    // Locate ffplay next to ffmpeg
    fs::path ffplay = fs::path(ffmpegPath_).parent_path() / "ffplay.exe";
    if (!fs::exists(ffplay)) {
        std::cerr << "ffplay not found next to ffmpeg: " << ffplay << "\n";
        return 2;
    }
    if (dryRun) {
        std::cout << "[preview] would play: " << file << " (loop=" << loop << ")\n";
        return 0;
    }

    // Use PreviewPlayer to run/stop ffplay
    PreviewPlayer player(ffplay.string());
    bool started = player.play(file, loop);
    if (!started) return 3;

    std::cout << "Preview started. Close the ffplay window to continue, or press Enter to stop early...\n";
    // Wait for user input to stop early
    std::string s;
    std::getline(std::cin, s);
    player.stop();
    return 0;
}

int RemixRuleEngine::runFromJson(const std::string &jsonPath, bool dryRun) {
    std::ifstream ifs(jsonPath);
    if (!ifs) {
        std::cerr << "Unable to open JSON rules file: " << jsonPath << std::endl;
        return 2;
    }
    json j;
    try { ifs >> j; } catch (const std::exception &ex) { std::cerr << "JSON parse error: " << ex.what() << std::endl; return 3; }

    std::string workdir = workdir_;
    if (j.contains("global") && j["global"].contains("workdir")) {
        workdir = j["global"]["workdir"].get<std::string>();
        if (!fs::exists(workdir)) fs::create_directories(workdir);
    }

    if (!j.contains("operations") || !j["operations"].is_array()) {
        std::cerr << "No operations array in JSON.\n";
        return 4;
    }

    for (auto &op : j["operations"]) {
        if (!op.contains("type")) {
            std::cerr << "Operation missing type; skipping.\n";
            continue;
        }
        std::string type = op["type"].get<std::string>();
        std::cout << "Processing operation type: " << type << std::endl;

        if (type == "stutter") {
            std::string input = op["input"].get<std::string>();
            double start = op.value("start", 0.0);
            double duration = op.value("duration", 0.25);
            int repeats = op.value("repeats", 8);
            std::string output = op.value("output", (fs::path(workdir) / "stutter_out.mp4").string());
            int r = processStutter(input, start, duration, repeats, output, dryRun);
            if (r != 0) return r;
        } else if (type == "overlay") {
            std::string input = op["input"].get<std::string>();
            std::string overlay = op["overlay"].get<std::string>();
            double start = op.value("start", 0.0);
            double end = op.value("end", 9999.0);
            double scale = op.value("overlay_scale", 0.2);
            std::string pos = op.value("position", "topright");
            std::string output = op.value("output", (fs::path(workdir) / "overlay_out.mp4").string());
            int r = processOverlay(input, overlay, start, end, scale, pos, output, dryRun);
            if (r != 0) return r;
        } else if (type == "pitch") {
            std::string input = op["input"].get<std::string>();
            double semi = op.value("semitones", 0.0);
            std::string output = op.value("output", (fs::path(workdir) / "pitch_out.mp4").string());
            int r = processPitch(input, semi, output, dryRun);
            if (r != 0) return r;
        } else if (type == "random_chop") {
            std::string input = op["input"].get<std::string>();
            int count = op.value("count", 8);
            double min_len = op.value("min_len", 0.05);
            double max_len = op.value("max_len", 0.5);
            bool shuffle = op.value("shuffle", true);
            std::string output = op.value("output", (fs::path(workdir) / "rand_out.mp4").string());
            int r = processRandomChop(input, count, min_len, max_len, shuffle, output, dryRun);
            if (r != 0) return r;
        } else if (type == "concat") {
            if (!op.contains("inputs") || !op["inputs"].is_array()) {
                std::cerr << "concat requires inputs array\n";
                continue;
            }
            std::vector<std::string> inputs;
            for (auto &it : op["inputs"]) inputs.push_back(it.get<std::string>());
            std::string output = op.value("output", (fs::path(workdir) / "concat_out.mp4").string());
            int r = processConcat(inputs, output, dryRun);
            if (r != 0) return r;
        } else if (type == "bleep") {
            std::string input = op["input"].get<std::string>();
            std::string output = op.value("output", (fs::path(workdir) / "bleep_out.mp4").string());
            std::vector<std::pair<double,double>> ranges;
            if (op.contains("ranges") && op["ranges"].is_array()) {
                for (auto &r : op["ranges"]) {
                    double s = r.value("start", 0.0);
                    double e = r.value("end", s + 0.5);
                    ranges.emplace_back(s, e);
                }
            } else {
                std::cerr << "bleep operation missing ranges array\n";
                return 6;
            }
            int rc = processBleep(input, ranges, output, dryRun);
            if (rc != 0) return rc;
        } else if (type == "preview") {
            std::string file = op["file"].get<std::string>();
            bool loop = op.value("loop", false);
            int rc = processPreview(file, loop, dryRun);
            if (rc != 0) return rc;
        } else {
            std::cerr << "Unknown operation type: " << type << " (skipping)\n";
        }
    }

    return 0;
}