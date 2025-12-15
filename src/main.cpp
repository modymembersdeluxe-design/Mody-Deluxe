#include "RemixRuleEngine.h"
#include "MediaManager.h"
#include <iostream>
#include <nlohmann/json.hpp>
#include <fstream>
#include <thread>

using json = nlohmann::json;

int main(int argc, char **argv) {
    std::cout << "Mody+ Deluxe Orchestrator v1.0 (with Source Material Handling + parallel normalization)\n";
    std::cout << "Usage: modyplus_deluxe <path-to-ffmpeg.exe> <path-to-rules.json> [--dry-run]\n\n";

    if (argc < 3) {
        std::cerr << "Not enough arguments.\n";
        return 1;
    }

    std::string ffmpegPath = argv[1];
    std::string rulesPath = argv[2];
    bool dryRun = false;
    if (argc >= 4) {
        std::string opt = argv[3];
        if (opt == "--dry-run") dryRun = true;
    }

    // Load rules to inspect preprocessing settings
    json rules;
    {
        std::ifstream ifs(rulesPath);
        if (!ifs) {
            std::cerr << "Failed to open rules file: " << rulesPath << std::endl;
            return 2;
        }
        try { ifs >> rules; } catch (const std::exception &ex) { std::cerr << "JSON parse error: " << ex.what() << std::endl; return 3; }
    }

    // Create media manager and scan assets
    std::string assetsDir = "assets";
    if (rules.contains("global") && rules["global"].contains("assets_dir")) {
        assetsDir = rules["global"]["assets_dir"].get<std::string>();
    }

    MediaManager mm(ffmpegPath, "output");
    int found = mm.scanAssets(assetsDir);
    std::cout << "MediaManager: scanned " << found << " assets.\n";

    // Preprocessing: normalize_all with parallel workers
    if (rules.contains("preprocessing") && rules["preprocessing"].is_object()) {
        auto pre = rules["preprocessing"];
        int targetW = pre.value("target_width", 1280);
        int targetH = pre.value("target_height", 720);
        double targetFps = pre.value("target_fps", 30.0);
        bool normalizeAll = pre.value("normalize_all", true);
        int workers = pre.value("normalize_workers", 0); // 0 -> auto

        if (normalizeAll) {
            // default worker count heuristic
            if (workers <= 0) {
                unsigned int cores = std::thread::hardware_concurrency();
                if (cores == 0) cores = 2;
                // be conservative on Windows 8.1: half the cores, at least 1
                workers = std::max(1u, cores / 2);
            }
            std::cout << "Normalizing media to " << targetW << "x" << targetH << " @" << targetFps << "fps using " << workers << " workers\n";
            mm.normalizeAll(workers, targetW, targetH, targetFps);
        }
    }

    // Now run the RemixRuleEngine as before
    RemixRuleEngine engine(ffmpegPath, "output");
    int r = engine.runFromJson(rulesPath, dryRun);
    if (r != 0) {
        std::cerr << "Processing failed with error: " << r << std::endl;
        return r;
    }

    std::cout << "Processing complete. Check the output/ folder.\n";
    return 0;
}