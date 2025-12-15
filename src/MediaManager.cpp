#include "MediaManager.h"
#include "Utils.h"
#include <filesystem>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <fstream>

namespace fs = std::filesystem;

MediaManager::MediaManager(const std::string &ffmpegPath, const std::string &workdir)
    : ffmpegPath_(ffmpegPath), workdir_(workdir) {
    fs::path p(ffmpegPath_);
    ffprobePath_ = (p.parent_path() / "ffprobe.exe").string();
    if (!fs::exists(ffprobePath_)) {
        ffprobePath_ = "ffprobe";
    }
    util::ensureDir(workdir_);
    util::ensureDir((fs::path(workdir_) / "normalized").string());
    loadPersistedIndex();
}

void MediaManager::loadPersistedIndex() {
    fs::path idxp = fs::path(workdir_) / "media_index.json";
    if (!fs::exists(idxp)) return;
    try {
        std::ifstream ifs(idxp);
        json j;
        ifs >> j;
        persistedIndex_ = j;
    } catch (...) {}
}

int MediaManager::scanAssets(const std::string &assetsDir) {
    entries_.clear();
    if (!fs::exists(assetsDir)) {
        std::cerr << "Assets directory not found: " << assetsDir << std::endl;
        return 0;
    }
    for (auto &it : fs::recursive_directory_iterator(assetsDir)) {
        if (!it.is_regular_file()) continue;
        std::string path = it.path().string();
        json probe;
        probeFile(path, probe); // may fail but we'll still add entry
        MediaEntry e;
        e.path = path;
        e.rawProbe = probe;
        e.type = inferType(probe, path);
        try {
            if (probe.contains("format") && probe["format"].contains("duration")) {
                e.duration = std::stod(probe["format"]["duration"].get<std::string>());
            }
        } catch (...) {}
        try {
            for (auto &s : probe["streams"]) {
                if (s.contains("codec_type") && s["codec_type"] == "video") {
                    if (s.contains("width")) e.width = s["width"].get<int>();
                    if (s.contains("height")) e.height = s["height"].get<int>();
                    if (s.contains("r_frame_rate")) {
                        std::string r = s["r_frame_rate"].get<std::string>();
                        size_t pos = r.find('/');
                        if (pos != std::string::npos) {
                            double num = std::stod(r.substr(0,pos));
                            double den = std::stod(r.substr(pos+1));
                            if (den != 0) e.fps = num/den;
                        } else {
                            e.fps = std::stod(r);
                        }
                    }
                    break;
                }
            }
        } catch (...) {}
        e.fingerprint = util::fileFingerprint(path);

        // attempt to recover normalized path from persistedIndex_ if fingerprint matches
        if (!persistedIndex_.is_null() && persistedIndex_.contains("entries")) {
            for (auto &pe : persistedIndex_["entries"]) {
                try {
                    if (pe.contains("path") && pe["path"] == e.path && pe.contains("fingerprint") && pe["fingerprint"] == e.fingerprint && pe.contains("normalized_path")) {
                        e.normalized_path = pe["normalized_path"].get<std::string>();
                        break;
                    }
                } catch (...) {}
            }
        }

        entries_.push_back(e);
    }
    saveIndex();
    return (int)entries_.size();
}

bool MediaManager::probeFile(const std::string &path, json &out) {
    std::ostringstream cmd;
    cmd << util::quote(ffprobePath_) << " -v quiet -print_format json -show_format -show_streams " << util::quote(path);
    auto pr = util::runCapture(cmd.str());
    if (pr.first != 0 && pr.second.empty()) {
        return false;
    }
    try {
        out = json::parse(pr.second);
        return true;
    } catch (...) {
        return false;
    }
}

std::string MediaManager::inferType(const json &probeJson, const std::string &path) {
    std::string ext = fs::path(path).extension().string();
    for (auto &c : ext) c = (char)tolower(c);
    if (ext == ".mp4" || ext == ".mov" || ext == ".mkv" || ext == ".webm" || ext == ".avi") return "video";
    if (ext == ".mp3" || ext == ".wav" || ext == ".aac" || ext == ".flac" || ext == ".m4a") return "audio";
    if (ext == ".gif") return "gif";
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp") return "image";
    try {
        if (probeJson.contains("streams")) {
            for (auto &s : probeJson["streams"]) {
                if (s.contains("codec_type") && s["codec_type"] == "video") return "video";
                if (s.contains("codec_type") && s["codec_type"] == "audio") return "audio";
            }
        }
    } catch (...) {}
    return "unknown";
}

std::string MediaManager::normalizeMedia(const std::string &inputPath, int targetWidth, int targetHeight, double targetFps) {
    fs::path in(inputPath);
    std::string fingerprint = util::fileFingerprint(inputPath);
    std::string base = in.stem().string();
    fs::path out = fs::path(workdir_) / "normalized" / (base + "_norm.mp4");

    // If we already have a normalized path recorded for this fingerprint, skip
    for (auto &e : entries_) {
        if (e.path == inputPath && !e.normalized_path.empty() && e.fingerprint == fingerprint && fs::exists(e.normalized_path)) {
            // Skip re-normalization
            return e.normalized_path;
        }
    }

    // Build vf: scale with pad/preserve aspect and fps filter
    std::ostringstream vf;
    vf << "scale=w=" << targetWidth << ":h=" << targetHeight << ":force_original_aspect_ratio=decrease";
    vf << ",pad=" << targetWidth << ":" << targetHeight << ":(ow-iw)/2:(oh-ih)/2";
    if (targetFps > 0.0) vf << ",fps=" << std::fixed << std::setprecision(2) << targetFps;

    std::ostringstream cmd;
    cmd << util::quote(ffmpegPath_) << " -y -i " << util::quote(inputPath)
        << " -vf \"" << vf.str() << "\" -c:v libx264 -crf 20 -preset veryfast -c:a aac -b:a 128k "
        << util::quote(out.string());
    int rc = util::runCommand(cmd.str());
    if (rc != 0) return "";

    // Update entries_ metadata
    for (auto &e : entries_) {
        if (e.path == inputPath) {
            e.normalized_path = out.string();
            e.fingerprint = fingerprint;
            break;
        }
    }
    saveIndex();
    return out.string();
}

bool MediaManager::normalizeAll(int workerCount, int targetWidth, int targetHeight, double targetFps) {
    if (workerCount <= 0) workerCount = 1;
    util::ThreadPool pool((size_t)workerCount);
    std::atomic<int> tasksSubmitted{0};

    for (auto &e : entries_) {
        if (e.type == "video" || e.type == "gif") {
            // If normalized exists and fingerprint matches, skip scheduling
            if (!e.normalized_path.empty() && fs::exists(e.normalized_path)) {
                // Already normalized
                continue;
            }
            std::string inputPath = e.path;
            pool.enqueue([this, inputPath, targetWidth, targetHeight, targetFps, &tasksSubmitted](){
                std::string out = this->normalizeMedia(inputPath, targetWidth, targetHeight, targetFps);
                if (out.empty()) {
                    std::cerr << "Normalization failed for: " << inputPath << std::endl;
                } else {
                    std::cout << "Normalized: " << inputPath << " -> " << out << std::endl;
                }
                tasksSubmitted++;
            });
        }
    }

    pool.waitAll();
    // final save to ensure persisted info
    saveIndex();
    return true;
}

std::string MediaManager::trimClip(const std::string &inputPath, double start, double duration, const std::string &outName) {
    fs::path out = fs::path(workdir_) / outName;
    std::ostringstream cmd;
    cmd << util::quote(ffmpegPath_) << " -y -ss " << std::fixed << std::setprecision(3) << start
        << " -i " << util::quote(inputPath)
        << " -t " << std::fixed << std::setprecision(3) << duration
        << " -c:v libx264 -crf 20 -preset veryfast -c:a aac -b:a 128k "
        << util::quote(out.string());
    int rc = util::runCommand(cmd.str());
    if (rc != 0) return "";
    return out.string();
}

bool MediaManager::saveIndex() {
    json j;
    j["entries"] = json::array();
    for (auto &e : entries_) {
        json je;
        je["path"] = e.path;
        je["type"] = e.type;
        je["duration"] = e.duration;
        je["width"] = e.width;
        je["height"] = e.height;
        je["fps"] = e.fps;
        je["fingerprint"] = e.fingerprint;
        je["normalized_path"] = e.normalized_path;
        je["probe"] = e.rawProbe;
        j["entries"].push_back(je);
    }
    fs::path out = fs::path(workdir_) / "media_index.json";
    try {
        std::ofstream ofs(out);
        ofs << std::setw(2) << j;
        ofs.close();
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<MediaEntry> MediaManager::pickRandom(const std::string &type, int count) {
    std::vector<int> idx;
    for (int i = 0; i < (int)entries_.size(); ++i) {
        if (type.empty() || entries_[i].type == type) idx.push_back(i);
    }
    std::vector<int> picked = util::pickRandomIndices((int)idx.size(), count);
    std::vector<MediaEntry> out;
    for (int k : picked) {
        out.push_back(entries_[idx[k]]);
    }
    return out;
}