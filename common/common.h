#pragma once
#include <iostream>
#include <string_view>
#include <vector>

template <typename Arg, typename... Args>
void FatalErrorWithNewLine(Arg &&arg, Args &&... args) {
    std::cout << "ERROR: ";
    std::cout << std::forward<Arg>(arg);
    ((std::cout << std::forward<Args>(args)), ...);
    std::cout << "\n";
    exit(1);
}

std::string_view GetExtension(const std::string_view path);

struct AudioFile {
    size_t NumFrames() const { return interleaved_samples.size() / num_channels; }

    std::vector<float> interleaved_samples {};
    unsigned num_channels {};
    unsigned sample_rate {};
};

AudioFile ReadAudioFile(const std::string &filename);
bool WriteWaveFile(const std::string &filename, const AudioFile &audio_file);
