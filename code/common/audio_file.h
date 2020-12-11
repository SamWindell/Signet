#pragma once
#include <optional>
#include <vector>

#include "FLAC/metadata.h"
#include "doctest.hpp"
#include "dr_wav.h"
#include "filesystem.hpp"
#include "span.hpp"

#include "types.h"

enum class AudioFileFormat {
    Wav,
    Flac,
};

namespace MetadataItems {

struct SamplerMappingData {
    s8 fine_tune_cents {}; // -50 to +50
    s8 gain_db {}; // -64 to +64
    s8 low_note {0}; //  0 - 127
    s8 high_note {127}; // 0 - 127
    s8 low_velocity {1}; //  1 - 127
    s8 high_velocity {127}; // 1 - 127
};

enum class LoopType { Forward, Backward, PingPong };

struct Loop {
    std::optional<std::string> name;
    LoopType type;
    size_t start_frame;
    size_t end_frame;
    unsigned num_times_to_loop; // 0 for infinite
};

struct Loops {
    std::vector<Loop> loops;
};

struct Region {
    std::optional<std::string> initial_marker_name;
    std::optional<std::string> name;
    size_t start_frame;
    size_t num_frames;
};

struct Regions {
    std::vector<Region> regions;
};

struct Marker {
    std::optional<std::string> name;
    size_t start_frame;
};

struct Markers {
    std::vector<Marker> markers;
};

struct TimingInfo {
    unsigned num_beats = 4;
    unsigned time_signature_denominator = 4;
    unsigned time_signature_numerator = 4;
    float tempo = 120;
};

} // namespace MetadataItems

struct Metadata {
    std::optional<int> root_midi_note {};
    std::optional<MetadataItems::TimingInfo> timing_info {};
    std::optional<MetadataItems::SamplerMappingData> sampler_mapping_data {};
    std::optional<MetadataItems::Loops> loops {};
    std::optional<MetadataItems::Markers> markers {};
    std::optional<MetadataItems::Regions> regions {};
};

struct WaveMetadata {
    void Assign(const drwav_metadata *owned_metadata, unsigned _num_wave_metadata) {
        num_metadata = _num_wave_metadata;
        metadata = {owned_metadata, [](const drwav_metadata *m) { drwav_free((void *)m, NULL); }};
    }

    std::optional<drwav_acid> GetAcid() const {
        const auto result = GetType(drwav_metadata_type_acid);
        if (result) return result->acid;
        return {};
    }

    std::optional<drwav_smpl> GetSmpl() const {
        const auto result = GetType(drwav_metadata_type_smpl);
        if (result) return result->smpl;
        return {};
    }

    tcb::span<const drwav_metadata> Items() const {
        if (num_metadata) return {metadata.get(), num_metadata};
        return {};
    }
    unsigned NumItems() const { return num_metadata; }

  private:
    std::optional<drwav_metadata> GetType(drwav_metadata_type type) const {
        for (const auto &m : Items()) {
            if (m.type == type) return m;
        }
        return {};
    }

    std::shared_ptr<const drwav_metadata> metadata {};
    unsigned num_metadata {};
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

    void FramesWereRemovedFromStart(size_t num_frames) {
        // go through all the metadata and handle if these removed frames effects any markers
    }

    void AudioDataWasStretched(float stretch_factor) {
        // go through all the metadata and scale any frame markers
    }

    std::vector<double> interleaved_samples {};
    unsigned num_channels {};
    unsigned sample_rate {};
    unsigned bits_per_sample = 24;
    AudioFileFormat format {AudioFileFormat::Wav};

    Metadata metadata {};

    // Useful to keep these around in case there are particulars that are not set in the editable Metadata
    // block
    WaveMetadata wave_metadata {};
    std::vector<std::shared_ptr<FLAC__StreamMetadata>> flac_metadata {};
};

std::optional<AudioData> ReadAudioFile(const fs::path &filename);
bool WriteAudioFile(const fs::path &filename,
                    const AudioData &audio_data,
                    const std::optional<unsigned> new_bits_per_sample = {});

bool CanFileBeConvertedToBitDepth(AudioFileFormat file, unsigned bit_depth);
bool IsAudioFileReadable(const fs::path &path);
std::string GetLowercaseExtension(AudioFileFormat file);
