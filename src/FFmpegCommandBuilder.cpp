#include "FFmpegCommandBuilder.h"
#include <cmath>
#include <sstream>
#include <iomanip>

static std::string doubleToStr(double v) {
    std::ostringstream ss;
    ss.setf(std::ios::fixed);
    ss << std::setprecision(6) << v;
    return ss.str();
}

std::string FFmpegCommandBuilder::quote(const std::string &s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += '\\';
        out += c;
    }
    out += "\"";
    return out;
}

std::string FFmpegCommandBuilder::extractFragmentCmd(const std::string &ffmpegPath,
                                                     const std::string &input,
                                                     double start,
                                                     double duration,
                                                     const std::string &output,
                                                     bool stripAudio,
                                                     const std::string &vcodec) {
    std::ostringstream cmd;
    cmd << quote(ffmpegPath) << " -y -ss " << doubleToStr(start)
        << " -i " << quote(input)
        << " -t " << doubleToStr(duration)
        << " -c:v " << vcodec << " -crf 18 -preset veryfast ";
    if (stripAudio) cmd << " -an ";
    cmd << quote(output);
    return cmd.str();
}

std::string FFmpegCommandBuilder::concatFromListCmd(const std::string &ffmpegPath,
                                                    const std::string &listFile,
                                                    const std::string &output) {
    std::ostringstream cmd;
    cmd << quote(ffmpegPath) << " -y -f concat -safe 0 -i " << quote(listFile)
        << " -c copy " << quote(output);
    return cmd.str();
}

std::string FFmpegCommandBuilder::makeRepeatConcatCmd(const std::string &ffmpegPath,
                                                      const std::string &fragmentPath,
                                                      int repeats,
                                                      const std::string &outPath,
                                                      const std::string &tmpListPath) {
    return concatFromListCmd(ffmpegPath, tmpListPath, outPath);
}

std::string FFmpegCommandBuilder::overlayCmd(const std::string &ffmpegPath,
                                             const std::string &mainInput,
                                             const std::string &overlayInput,
                                             double enableStart,
                                             double enableEnd,
                                             double overlayScale,
                                             const std::string &position,
                                             const std::string &outPath) {
    std::string posExpr;
    if (position == "topright") posExpr = "x=main_w-overlay_w-10:y=10";
    else if (position == "topleft") posExpr = "x=10:y=10";
    else if (position == "bottomright") posExpr = "x=main_w-overlay_w-10:y=main_h-overlay_h-10";
    else if (position == "bottomleft") posExpr = "x=10:y=main_h-overlay_h-10";
    else posExpr = "x=(main_w-overlay_w)/2:y=(main_h-overlay_h)/2";

    std::ostringstream fc;
    fc << quote(ffmpegPath) << " -y -i " << quote(mainInput)
       << " -ignore_loop 0 -i " << quote(overlayInput)
       << " -filter_complex \"[1:v] scale=iw*" << overlayScale << ":-1 [ovr];"
       << "[0:v][ovr] overlay=" << posExpr
       << ":enable='between(t," << doubleToStr(enableStart) << "," << doubleToStr(enableEnd) << ")'\""
       << " -map 0:a? -map 0:v -c:v libx264 -crf 18 -preset veryfast "
       << quote(outPath);
    return fc.str();
}

std::string FFmpegCommandBuilder::pitchShiftCmd(const std::string &ffmpegPath,
                                                const std::string &input,
                                                double semitones,
                                                const std::string &output) {
    double factor = std::pow(2.0, semitones / 12.0);
    std::ostringstream af;
    af << "asetrate=48000*" << std::fixed << std::setprecision(6) << factor
       << ",aresample=48000,atempo=" << (1.0 / factor);

    std::ostringstream cmd;
    cmd << quote(ffmpegPath) << " -y -i " << quote(input)
        << " -af \"" << af.str() << "\""
        << " -c:v copy -c:a aac -b:a 192k " << quote(output);
    return cmd.str();
}

std::string FFmpegCommandBuilder::randomChopExtractCmd(const std::string &ffmpegPath,
                                                       const std::string &input,
                                                       int idx,
                                                       double start,
                                                       double duration,
                                                       const std::string &outFragment) {
    std::ostringstream cmd;
    cmd << quote(ffmpegPath) << " -y -ss " << doubleToStr(start)
        << " -i " << quote(input)
        << " -t " << doubleToStr(duration)
        << " -c:v libx264 -crf 24 -preset veryfast -c:a aac -b:a 128k "
        << quote(outFragment);
    return cmd.str();
}

std::string FFmpegCommandBuilder::concatFilesCmd(const std::string &ffmpegPath,
                                                 const std::vector<std::string> &files,
                                                 const std::string &outPath,
                                                 const std::string &tmpListPath) {
    return concatFromListCmd(ffmpegPath, tmpListPath, outPath);
}

std::string FFmpegCommandBuilder::bleepCensorCmd(const std::string &ffmpegPath,
                                                 const std::string &input,
                                                 const std::vector<std::pair<double,double>> &ranges,
                                                 const std::string &output) {
    // Build expression: if(any range,0,1)
    // Using FFmpeg expression: if(gt(sum(between(t,a,b),between(t,c,d),...),0),0,1)
    std::ostringstream expr;
    expr << "if(gt(";
    bool first = true;
    for (auto &r : ranges) {
        if (!first) expr << "+";
        expr << "between(t," << doubleToStr(r.first) << "," << doubleToStr(r.second) << ")";
        first = false;
    }
    if (first) {
        // no ranges -> no-op
        expr << "0";
    }
    expr << ",0),0,1)"; // if sum>0 then 0 else 1 -> volume multiplier

    std::ostringstream cmd;
    cmd << quote(ffmpegPath) << " -y -i " << quote(input)
        << " -af \"volume='" << expr.str() << "'\""
        << " -c:v copy -c:a aac -b:a 192k " << quote(output);
    return cmd.str();
}

std::string FFmpegCommandBuilder::previewCmd(const std::string &ffplayPath,
                                             const std::string &file,
                                             bool loop) {
    std::ostringstream cmd;
    cmd << quote(ffplayPath) << " -autoexit -nodisp ";
    if (loop) cmd << " -loop 0 ";
    cmd << quote(file);
    // For Windows: start it detached (user may close it or engine can attempt to terminate)
    // The orchestration layer will use CreateProcess for more control; this string is a fallback.
    return cmd.str();
}