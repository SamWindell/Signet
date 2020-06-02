#pragma once
#include <optional>
#include <vector>

#include "doctest.hpp"
#include "filesystem.hpp"

struct AudioFile {
    size_t NumFrames() const {
        REQUIRE(num_channels != 0);
        return interleaved_samples.size() / num_channels;
    }
    double &GetSample(unsigned channel, size_t frame) {
        return interleaved_samples[frame * num_channels + channel];
    }
    const double &GetSample(unsigned channel, size_t frame) const {
        return interleaved_samples[frame * num_channels + channel];
    }

    std::vector<double> interleaved_samples {};
    unsigned num_channels {};
    unsigned sample_rate {};
    unsigned bits_per_sample = 24;
};

std::optional<AudioFile> ReadAudioFile(const ghc::filesystem::path &filename);
bool WriteAudioFile(const ghc::filesystem::path &filename,
                    const AudioFile &audio_file,
                    const std::optional<unsigned> new_bits_per_sample = {});

static constexpr unsigned valid_wave_bit_depths[] = {8, 16, 24, 32, 64};
static constexpr unsigned valid_flac_bit_depths[] = {8, 16, 20, 24};
