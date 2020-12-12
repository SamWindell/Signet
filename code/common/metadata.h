#pragma once

#include <optional>
#include <string>
#include <vector>

#include "dr_wav.h"

namespace MetadataItems {

struct SamplerMapping {
    int fine_tune_cents {0}; // -50 to +50
    int gain_db {0}; // -64 to +64
    int low_note {0}; //  0 - 127
    int high_note {127}; // 0 - 127
    int low_velocity {1}; //  1 - 127
    int high_velocity {127}; // 1 - 127
};

enum class LoopType { Forward, Backward, PingPong };

struct Loop {
    std::optional<std::string> name;
    LoopType type;
    size_t start_frame;
    size_t num_frames;
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

enum class PlaybackType { OneShot, Loop };

struct TimingInfo {
    PlaybackType playback_type {PlaybackType::OneShot};
    unsigned num_beats = 4;
    unsigned time_signature_denominator = 4;
    unsigned time_signature_numerator = 4;
    float tempo = 0;
};

struct MidiMapping {
    int root_midi_note {};
    std::optional<SamplerMapping> sampler_mapping {};
};

} // namespace MetadataItems

struct Metadata {
    std::optional<MetadataItems::MidiMapping> midi_mapping {};
    std::optional<MetadataItems::TimingInfo> timing_info {};
    std::optional<MetadataItems::Loops> loops {};
    std::optional<MetadataItems::Markers> markers {};
    std::optional<MetadataItems::Regions> regions {};

    template <typename Type>
    void HandleStartFramesRemovedForType(std::vector<Type> &vector, size_t num_frames_removed) {
        for (auto it = vector.begin(); it != vector.end();) {
            if (it->start_frame < num_frames_removed) {
                it = vector.erase(it);
            } else {
                it->start_frame -= num_frames_removed;
                ++it;
            }
        }
    }

    template <typename Type>
    void HandleEndFramesRemovedForType(std::vector<Type> &vector, size_t new_file_size_in_frames) {
        for (auto it = vector.begin(); it != vector.end();) {
            if (it->start_frame + it->num_frames > new_file_size_in_frames) {
                it = vector.erase(it);
            } else {
                ++it;
            }
        }
    }
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

    const drwav_metadata *begin() const { return (NumItems() != 0) ? &Items().front() : nullptr; }
    const drwav_metadata *end() const { return (NumItems() != 0) ? &Items().back() : nullptr; }

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
