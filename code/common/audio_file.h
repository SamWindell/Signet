#pragma once
#include <optional>
#include <vector>

#include "FLAC/metadata.h"
#include "doctest.hpp"
#include "filesystem.hpp"

#include "types.h"

enum class AudioFileFormat {
    Wav,
    Flac,
};

struct InstrumentData {
    s8 midi_note; //  0 - 127
    s8 fine_tune_db; //  -50 - +50
    s8 gain; // -64 - +64
    s8 low_note; //  0 - 127
    s8 high_note; // 0 - 127
    s8 low_velocity; //  1 - 127
    s8 high_velocity; // 1 - 127
};

enum class LoopType { Forward, Backward, PingPong };

struct SampleLoop {
    std::string name;
    LoopType type;
    size_t start;
    size_t end;
    unsigned num_times_to_loop; // 0 for infinite
};

struct SampleLoops {
    std::vector<SampleLoop> loops;
};

struct AudioData {
    bool IsEmpty() const { return interleaved_samples.empty(); }
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
    void MultiplyByScalar(const double amount) {
        for (auto &s : interleaved_samples) {
            s *= amount;
        }
    }
    void AddOther(const AudioData &other) {
        if (other.interleaved_samples.size() > interleaved_samples.size()) {
            interleaved_samples.resize(other.interleaved_samples.size());
        }
        for (usize i = 0; i < other.interleaved_samples.size(); ++i) {
            interleaved_samples[i] += other.interleaved_samples[i];
        }
    }

    std::vector<double> interleaved_samples {};
    unsigned num_channels {};
    unsigned sample_rate {};
    unsigned bits_per_sample = 24;
    AudioFileFormat format {AudioFileFormat::Wav};

    std::optional<InstrumentData> instrument_data {};
    std::optional<SampleLoops> loops {};

    std::vector<std::vector<u8>> wave_metadata {};
    std::vector<std::shared_ptr<FLAC__StreamMetadata>> flac_metadata {};
};

std::optional<AudioData> ReadAudioFile(const fs::path &filename);
bool WriteAudioFile(const fs::path &filename,
                    const AudioData &audio_data,
                    const std::optional<unsigned> new_bits_per_sample = {});

bool CanFileBeConvertedToBitDepth(AudioFileFormat file, unsigned bit_depth);
bool IsAudioFileReadable(const fs::path &path);
std::string GetLowercaseExtension(AudioFileFormat file);
