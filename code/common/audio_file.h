#pragma once
#include <optional>
#include <vector>

#include "filesystem.hpp"

struct AudioFile {
    size_t NumFrames() const { return interleaved_samples.size() / num_channels; }
    float &GetSample(unsigned channel, size_t frame) {
        return interleaved_samples[frame * num_channels + channel];
    }

    std::vector<float> interleaved_samples {};
    unsigned num_channels {};
    unsigned sample_rate {};
};

std::optional<AudioFile> ReadAudioFile(const ghc::filesystem::path &filename);
bool WriteWaveFile(const ghc::filesystem::path &filename, const AudioFile &audio_file);
