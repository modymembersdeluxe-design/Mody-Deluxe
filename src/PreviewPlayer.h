#pragma once
#include <string>

class PreviewPlayer {
public:
    // ffplayPath: path to ffplay.exe (usually next to ffmpeg.exe)
    PreviewPlayer(const std::string &ffplayPath);
    ~PreviewPlayer();

    // Start playback of 'file'. If loop == true, loop playback until stopped.
    // Returns true if the player was launched.
    bool play(const std::string &file, bool loop = false);

    // Stop playback if running. Returns true if a running process was terminated.
    bool stop();

    // Is player running?
    bool isRunning() const;

private:
    std::string ffplayPath_;
    void *processHandle_; // opaque handle (Windows HANDLE)
};