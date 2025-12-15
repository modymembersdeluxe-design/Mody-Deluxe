#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct MediaEntry {
    std::string path;
    std::string type; // "video","audio","image","gif"
    double duration = 0.0;
    int width = 0;
    int height = 0;
    double fps = 0.0;
    std::string fingerprint; // file fingerprint
    std::string normalized_path; // optional
    json rawProbe;
};

class MediaManager {
public:
    MediaManager(const std::string &ffmpegPath, const std::string &workdir = "output");

    // Scan an assets directory and probe all files; returns number of entries
    int scanAssets(const std::string &assetsDir);

    // Normalize all matching media in parallel (workerCount); returns true on success
    bool normalizeAll(int workerCount = 1, int targetWidth = 1280, int targetHeight = 720, double targetFps = 30.0);

    // Normalize a single media file (skips if fingerprint matches existing normalized output).
    std::string normalizeMedia(const std::string &inputPath, int targetWidth, int targetHeight, double targetFps);

    // Trim a clip: start & duration -> output path
    std::string trimClip(const std::string &inputPath, double start, double duration, const std::string &outName);

    // Save index JSON to workdir/media_index.json
    bool saveIndex();

    const std::vector<MediaEntry>& entries() const { return entries_; }

    // Randomly pick up to 'count' entries of a given type
    std::vector<MediaEntry> pickRandom(const std::string &type, int count);

private:
    std::string ffmpegPath_;
    std::string ffprobePath_;
    std::string workdir_;
    std::vector<MediaEntry> entries_;
    json persistedIndex_; // load/save for fingerprint data

    bool probeFile(const std::string &path, json &out);
    std::string inferType(const json &probeJson, const std::string &path);

    // load previously saved index if present
    void loadPersistedIndex();
};