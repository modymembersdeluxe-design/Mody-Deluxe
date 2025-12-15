#pragma once
#include <string>
#include <vector>

class FFmpegCommandBuilder {
public:
    static std::string quote(const std::string &s);

    static std::string extractFragmentCmd(const std::string &ffmpegPath,
                                          const std::string &input,
                                          double start,
                                          double duration,
                                          const std::string &output,
                                          bool stripAudio = false,
                                          const std::string &vcodec = "libx264");

    static std::string concatFromListCmd(const std::string &ffmpegPath,
                                         const std::string &listFile,
                                         const std::string &output);

    static std::string makeRepeatConcatCmd(const std::string &ffmpegPath,
                                           const std::string &fragmentPath,
                                           int repeats,
                                           const std::string &outPath,
                                           const std::string &tmpListPath);

    static std::string overlayCmd(const std::string &ffmpegPath,
                                  const std::string &mainInput,
                                  const std::string &overlayInput,
                                  double enableStart,
                                  double enableEnd,
                                  double overlayScale,
                                  const std::string &position,
                                  const std::string &outPath);

    static std::string pitchShiftCmd(const std::string &ffmpegPath,
                                     const std::string &input,
                                     double semitones,
                                     const std::string &output);

    static std::string randomChopExtractCmd(const std::string &ffmpegPath,
                                            const std::string &input,
                                            int idx,
                                            double start,
                                            double duration,
                                            const std::string &outFragment);

    static std::string concatFilesCmd(const std::string &ffmpegPath,
                                      const std::vector<std::string> &files,
                                      const std::string &outPath,
                                      const std::string &tmpListPath);

    // New: bleep censor: provide an array of (start,end) timestamp pairs to mute
    // The builder emits an -af expression that mutes the audio when any range is active.
    static std::string bleepCensorCmd(const std::string &ffmpegPath,
                                      const std::string &input,
                                      const std::vector<std::pair<double,double>> &ranges,
                                      const std::string &output);

    // New: preview command using ffplay to play a file (detached invocation)
    static std::string previewCmd(const std::string &ffplayPath,
                                  const std::string &file,
                                  bool loop = false);
};