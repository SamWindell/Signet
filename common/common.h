#pragma once
#include <functional>
#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

#include "filesystem.hpp"

template <typename Arg, typename... Args>
void FatalErrorWithNewLine(Arg &&arg, Args &&... args) {
    std::cout << "ERROR: ";
    std::cout << std::forward<Arg>(arg);
    ((std::cout << std::forward<Args>(args)), ...);
    std::cout << "\n";
    exit(1);
}

template <typename Arg, typename... Args>
void WarningWithNewLine(Arg &&arg, Args &&... args) {
    std::cout << "WARNING: ";
    std::cout << std::forward<Arg>(arg);
    ((std::cout << std::forward<Args>(args)), ...);
    std::cout << "\n";
}

void ForEachAudioFileInDirectory(const std::string &directory,
                                 const bool recursive,
                                 std::function<void(const ghc::filesystem::path &)> callback);

struct AudioFile {
    size_t NumFrames() const { return interleaved_samples.size() / num_channels; }

    std::vector<float> interleaved_samples {};
    unsigned num_channels {};
    unsigned sample_rate {};
};

std::optional<AudioFile> ReadAudioFile(const ghc::filesystem::path &filename);
bool WriteWaveFile(const ghc::filesystem::path &filename, const AudioFile &audio_file);

float DBToAmp(float d);
