#include "PreviewPlayer.h"
#include <iostream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

PreviewPlayer::PreviewPlayer(const std::string &ffplayPath)
    : ffplayPath_(ffplayPath), processHandle_(nullptr) {}

PreviewPlayer::~PreviewPlayer() {
    stop();
}

bool PreviewPlayer::play(const std::string &file, bool loop) {
#ifdef _WIN32
    // If already running, stop first
    if (processHandle_) stop();

    std::string cmd = "\"" + ffplayPath_ + "\" -autoexit -nodisp ";
    if (loop) cmd += "-loop 0 ";
    cmd += "\"" + file + "\"";

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // CreateProcessA requires the command line as mutable char*
    std::vector<char> cmdbuf(cmd.begin(), cmd.end());
    cmdbuf.push_back('\0');

    BOOL ok = CreateProcessA(
        nullptr,
        cmdbuf.data(),
        nullptr, nullptr, FALSE, CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &pi);

    if (!ok) {
        std::cerr << "Failed to start ffplay. CreateProcess error: " << GetLastError() << std::endl;
        return false;
    }

    // Close thread handle and keep process handle for termination
    CloseHandle(pi.hThread);
    processHandle_ = pi.hProcess;
    return true;
#else
    // Non-Windows: simply spawn system command (not used for target Windows 8.1)
    std::string cmd = "\"" + ffplayPath_ + "\" -autoexit -nodisp ";
    if (loop) cmd += "-loop 0 ";
    cmd += "\"" + file + "\"";
    int r = std::system(cmd.c_str());
    return (r == 0);
#endif
}

bool PreviewPlayer::stop() {
#ifdef _WIN32
    if (!processHandle_) return false;
    // Terminate process
    BOOL ok = TerminateProcess((HANDLE)processHandle_, 0);
    if (!ok) {
        std::cerr << "Failed to terminate ffplay process: " << GetLastError() << std::endl;
        CloseHandle((HANDLE)processHandle_);
        processHandle_ = nullptr;
        return false;
    }
    CloseHandle((HANDLE)processHandle_);
    processHandle_ = nullptr;
    return true;
#else
    // Not implemented for non-Windows in this simple wrapper.
    return false;
#endif
}

bool PreviewPlayer::isRunning() const {
#ifdef _WIN32
    if (!processHandle_) return false;
    DWORD exitCode = 0;
    if (GetExitCodeProcess((HANDLE)processHandle_, &exitCode)) {
        return (exitCode == STILL_ACTIVE);
    }
    return false;
#else
    return false;
#endif
}