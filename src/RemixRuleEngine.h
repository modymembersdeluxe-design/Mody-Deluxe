#pragma once
#include <string>
#include <vector>

class RemixRuleEngine {
public:
    RemixRuleEngine(const std::string &ffmpegPath, const std::string &workdir);
    ~RemixRuleEngine();

    int runFromJson(const std::string &jsonPath, bool dryRun = false);

private:
    std::string ffmpegPath_;
    std::string workdir_;

    int runCommand(const std::string &cmd, bool dryRun);

    int processStutter(const std::string &input, double start, double duration, int repeats, const std::string &output, bool dryRun);
    int processOverlay(const std::string &input, const std::string &overlay, double start, double end, double scale, const std::string &position, const std::string &output, bool dryRun);
    int processPitch(const std::string &input, double semitones, const std::string &output, bool dryRun);
    int processRandomChop(const std::string &input, int count, double min_len, double max_len, bool shuffle, const std::string &output, bool dryRun);
    int processConcat(const std::vector<std::string> &inputs, const std::string &output, bool dryRun);

    // New: bleep censor using explicit timestamp ranges
    int processBleep(const std::string &input, const std::vector<std::pair<double,double>> &ranges, const std::string &output, bool dryRun);

    // New: preview operation - launch ffplay (uses ffplay sibling of ffmpeg)
    int processPreview(const std::string &file, bool loop, bool dryRun);
};