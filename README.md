# Mody+ Deluxe — Automatic Remix Orchestrator (v1.0)

Mody+ Deluxe is a C++ orchestration tool that uses FFmpeg to create YTP / YTPMV‑style remixes, stutters, overlays, pitch shifts, randomized edits, and other chaotic effects for the "Mody+" concept project. This README is a step‑by‑step tutorial describing how to build, run, and produce videos with Mody+ Deluxe on Windows 8.1 using Visual Studio Code + CMake.

This project is an orchestrator: it builds FFmpeg command lines and runs them. It does not embed large media content — put your assets into `assets/` (use royalty‑free content for public releases).

Table of contents
- Features
- Prerequisites
- Repository layout
- Build (VS Code + CMake) — quick steps
- Run the tool — examples
- JSON rules: operations reference
- Source material handling & preprocessing
- Previewing & iteration workflow
- Packaging a GitHub release
- Troubleshooting
- Next steps & integrations
- License & contribution

---

Features (high level)
- Scan and index media in `assets/` with `ffprobe`.
- Normalize videos/GIFs to a target resolution / framerate.
- Parallel normalization with fingerprint caching to skip unchanged files.
- JSON rule engine that runs ordered operations:
  - stutter (extract + repeat fragment)
  - overlay (image/GIF/video overlay)
  - pitch (audio pitch-shift via asetrate/atempo technique)
  - random_chop (extract many short fragments + concat)
  - concat (concatenate files)
  - bleep (mute specified timestamp ranges)
  - preview (launch ffplay for playback)
- Preview playback via ffplay integration (play / stop).
- Outputs to `output/` (normalized assets in `output/normalized/`).

Prerequisites (Windows 8.1)
- Visual Studio Build Tools or Visual Studio with "Desktop development with C++".
  - Use Visual Studio 2017/2019 if available on Windows 8.1.
- Visual Studio Code (recommended) with extensions:
  - CMake Tools (ms-vscode.cmake-tools)
  - C/C++ (ms-vscode.cpptools)
- CMake >= 3.10 (add to PATH)
- Git
- FFmpeg static build (contains `ffmpeg.exe`, `ffprobe.exe`, `ffplay.exe`)
  - Download and unpack; either add the `bin/` folder to PATH or pass the full path to the CLI.
- Optional:
  - gh (GitHub CLI) for automated release uploads
  - 7‑Zip or Windows compress for packaging

Repository layout (important files)
- CMakeLists.txt — build config
- src/
  - main.cpp — CLI entry
  - FFmpegCommandBuilder.* — builds ffmpeg commands
  - RemixRuleEngine.* — interprets JSON rules and runs commands
  - MediaManager.* — scans assets, probes, normalizes, saves media_index.json
  - PreviewPlayer.* — launches ffplay for previews
  - Utils.* — helpers (fingerprinting, thread pool, runCapture)
- config/sample_rules.json — example operations flow
- tools/package_release.bat — helper to assemble a release folder
- assets/ — place your source media here
- output/ — outputs and normalized media are written here

Build (Visual Studio Code + CMake) — quick steps
1. Clone the repository:
   git clone https://github.com/<your>/<repo>.git
   cd <repo>

2. Open repository folder in Visual Studio Code.

3. Select CMake Kit (bottom status bar; choose MSVC kit that matches your installed Visual Studio).

4. Configure & Build:
   - Use the CMake Tools commands from the Command Palette:
     - CMake: Configure
     - CMake: Build
   - Or from Developer Command Prompt:
     mkdir build
     cd build
     cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
     cmake --build . --config Release

5. After build, the binary is available in the build output folder (or `build/Release/`).

Run the tool — basic example
- Minimal usage:
  modyplus_deluxe "C:\path\to\ffmpeg.exe" config\sample_rules.json

- Dry-run (print FFmpeg commands without executing):
  modyplus_deluxe "C:\path\to\ffmpeg.exe" config\sample_rules.json --dry-run

What the tool does when invoked:
1. MediaManager scans `assets/` and writes `output/media_index.json`.
2. If preprocessing is enabled in the JSON, it will normalize video/GIF assets to `output/normalized/` (parallelized, cached).
3. RemixRuleEngine executes each operation in the JSON `operations` array sequentially and writes outputs to `output/`.

JSON rules: operations reference
The rule file is a JSON object with optionally `global`, `preprocessing`, and `operations` array.

Example top-level:
{
  "global": { "workdir": "output", "assets_dir": "assets" },
  "preprocessing": {
    "target_width": 1280,
    "target_height": 720,
    "target_fps": 30,
    "normalize_all": true,
    "normalize_workers": 2
  },
  "operations": [ ... ]
}

Supported operation types (fields described briefly)

- stutter
  - input: source path
  - start: seconds
  - duration: seconds (fragment length)
  - repeats: integer
  - output: path
  - Effect: extracts the fragment and concatenates it `repeats` times.

- overlay
  - input: base video
  - overlay: image/GIF/video path
  - start, end: enable time range for overlay
  - overlay_scale: fraction of main width (0.0..1.0)
  - position: "topright"/"topleft"/"center"/"bottomright"/"bottomleft"
  - output: path
  - Effect: overlays scaled overlay input onto main video.

- pitch
  - input: video file
  - semitones: positive/up or negative/down
  - output: path
  - Effect: approximate pitch-shift via asetrate + atempo (fast, imperfect). For higher quality use Rubber Band or SoX.

- random_chop
  - input: source video
  - count: number of fragments
  - min_len, max_len: seconds
  - shuffle: true/false
  - output: path
  - Effect: extracts many short random segments and concatenates them.

- concat
  - inputs: array of file paths
  - output: path
  - Effect: concatenates files using ffmpeg concat demuxer.

- bleep
  - input: source file
  - ranges: array of { "start": seconds, "end": seconds }
  - output: path
  - Effect: mutes audio for specified ranges using an ffmpeg volume expression.

- preview
  - file: path to play
  - loop: true/false
  - Effect: launches `ffplay.exe` (must be next to `ffmpeg.exe`) in a new window; press Enter in the orchestrator console to stop early.

Source material handling & preprocessing
- Place all source media in `assets/` (organized as you like).
- Preprocessing can normalize all video/GIF assets to a uniform resolution/framerate into `output/normalized/`.
- The MediaManager uses a fast fingerprint (file size + last_write_time) to skip re-normalizing unchanged files.
- Configure `preprocessing.normalize_workers` to control parallelism (0 -> auto heuristic = max(1, cores/2)).
- Normalized files are recorded in `output/media_index.json`.

Previewing & iteration workflow
1. Work on short sample clips (5–15s) to iterate quickly.
2. Use `--dry-run` to validate FFmpeg command lines without executing.
3. Run the rules JSON and watch the `preview` step to evaluate the result.
4. Tweak JSON parameters (start/duration/repeats/overlay_scale/semitones etc.) and re-run. The normalization step will be skipped for unchanged assets.

Packaging a GitHub release (suggested)
- Build a Release variant of the binary.
- Create a `release/` folder with:
  - `modyplus_deluxe.exe`
  - README.md, LICENSE, config sample JSON files
  - (optionally) `output/` demo MP4s — ensure you have rights to include them.
- Zip the folder:
  Compress-Archive -Path release\* -DestinationPath modyplus_deluxe_v1.0.zip
- Create a GitHub release and upload the ZIP (via the web UI or `gh` CLI):
  gh release create v1.0 modyplus_deluxe_v1.0.zip --title "Mody+ Deluxe v1.0" --notes "Initial release"

Troubleshooting (common issues)
- ffmpeg not found:
  - Provide full path to `ffmpeg.exe` on the command line or add its folder to PATH.
- ffplay not found for preview:
  - Place `ffplay.exe` in the same folder as `ffmpeg.exe` (pack ffplay with ffmpeg static build).
- Long path or permission errors:
  - Use shorter project paths (e.g., `C:\projects\modyplus`) and check file permissions.
- Normalization slow:
  - Reduce `normalize_workers` or use NVENC (I can add a GPU preset).
- Pitch artifacting:
  - The built-in pitch method is a fast workaround; integrate Rubber Band / SoX for better quality.

Safety & legal notes
- Do NOT include copyrighted Mody+ channel media or music in public releases without permission.
- For demo assets, use royalty‑free or CC0/CC‑BY material.
- The tool gives the user responsibility for uploaded content.

Next steps / optional integrations I can provide
- SHA256 fingerprint mode (optional; slower but more robust).
- NVENC / GPU encoding presets for faster normalization (requires NVIDIA + drivers).
- STT + forced aligner pipeline script (Python) to auto-generate bleep ranges (uses Whisper + aligner or heuristics).
- Simple GUI timeline (ImGui prototype) for drag & drop editing and rule building.
- GitHub Actions workflow to build on Windows and upload artifacts automatically.

Contributing
- Contributions are welcome. Please open issues for feature requests or submit PRs.
- Follow the repository style: simple C++17, header-only nlohmann/json via CMake FetchContent, Windows-compatible code paths.

License
- The project itself is provided under the MIT License (or specify your chosen license).
- FFmpeg and bundled binaries are not licensed under MIT — follow FFmpeg licensing terms when redistributing.

Contact / support
- For help with building, running, or customizing presets, open an issue with:
  - OS / Visual Studio version
  - FFmpeg path used
  - A short snippet of the rules JSON you're running
- For feature requests (e.g., Mody+ export spec or GUI), open a feature request issue.

---

Example quick "make a video" flow (commands)
1. Build release binary via CMake (see Build section).
2. Put sample files:
   assets/videos/sample1.mp4
   assets/videos/sample2.mp4
   assets/gifs/logo.gif
3. Edit `config/make_video_rules.json` (example provided in repo).
4. Run:
   build\Release\modyplus_deluxe.exe "C:\tools\ffmpeg\bin\ffmpeg.exe" config\make_video_rules.json
5. Preview will launch; outputs are in `output/`.

If you'd like, I can now:
- Add a ready-to-run sample folder with small royalty-free assets and a pre-filled rules file so you can run the pipeline immediately,
- or generate a GitHub Actions workflow that builds the project on Windows and creates release artifacts automatically.

Which of these would you like next?
```
```
