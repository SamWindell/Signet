#pragma once

#include <optional>
#include <string>
#include <vector>

#include "dr_wav.h"
#include <cereal/cereal.hpp>

static const char *flac_custom_signet_application_id = "SGNT";
static const char *signet_root_json_object_name = "metadata";

namespace MetadataItems {

struct SamplerMapping {
    int fine_tune_cents {0}; // -50 to +50
    int gain_db {0}; // -64 to +64
    int low_note {0}; //  0 - 127
    int high_note {127}; // 0 - 127
    int low_velocity {1}; //  1 - 127
    int high_velocity {127}; // 1 - 127

    template <class Archive>
    void serialize(Archive &archive) {
        archive(CEREAL_NVP(fine_tune_cents), CEREAL_NVP(gain_db), CEREAL_NVP(low_note), CEREAL_NVP(high_note),
                CEREAL_NVP(low_velocity), CEREAL_NVP(high_velocity));
    }

    bool operator==(const SamplerMapping &other) const {
        return std::memcmp(this, &other, sizeof(*this)) == 0;
    }
};

enum class LoopType { Forward, Backward, PingPong };

struct Loop {
    std::optional<std::string> name;
    LoopType type;
    size_t start_frame;
    size_t num_frames;
    unsigned num_times_to_loop; // 0 for infinite

    template <class Archive>
    void serialize(Archive &archive) {
        archive(CEREAL_NVP(name), CEREAL_NVP(type), CEREAL_NVP(start_frame), CEREAL_NVP(num_frames),
                CEREAL_NVP(num_times_to_loop));
    }

    bool operator==(const Loop &b) const {
        return name == b.name && type == b.type && start_frame == b.start_frame &&
               num_frames == b.num_frames && num_times_to_loop == b.num_times_to_loop;
    }
};

struct Region {
    std::optional<std::string> initial_marker_name;
    std::optional<std::string> name;
    size_t start_frame;
    size_t num_frames;

    template <class Archive>
    void serialize(Archive &archive) {
        archive(CEREAL_NVP(initial_marker_name), CEREAL_NVP(name), CEREAL_NVP(start_frame),
                CEREAL_NVP(num_frames));
    }

    bool operator==(const Region &b) const {
        return initial_marker_name == b.initial_marker_name && name == b.name &&
               start_frame == b.start_frame && num_frames == b.num_frames;
    }
};

struct Marker {
    std::optional<std::string> name;
    size_t start_frame;

    template <class Archive>
    void serialize(Archive &archive) {
        archive(CEREAL_NVP(name), CEREAL_NVP(start_frame));
    }

    bool operator==(const Marker &other) const {
        return name == other.name && start_frame == other.start_frame;
    }
};

enum class PlaybackType { OneShot, Loop };

struct TimingInfo {
    PlaybackType playback_type {PlaybackType::OneShot};
    unsigned num_beats = 4;
    unsigned time_signature_denominator = 4;
    unsigned time_signature_numerator = 4;
    float tempo = 0;

    template <class Archive>
    void serialize(Archive &archive) {
        archive(CEREAL_NVP(playback_type), CEREAL_NVP(num_beats), CEREAL_NVP(time_signature_denominator),
                CEREAL_NVP(time_signature_numerator), CEREAL_NVP(tempo));
    }

    bool operator==(const TimingInfo &other) const {
        return playback_type == other.playback_type && num_beats == other.num_beats &&
               time_signature_denominator == other.time_signature_denominator &&
               time_signature_numerator == other.time_signature_numerator && tempo == other.tempo;
    }
};

struct MidiMapping {
    int root_midi_note {60};
    std::optional<SamplerMapping> sampler_mapping {};

    template <class Archive>
    void serialize(Archive &archive) {
        archive(CEREAL_NVP(root_midi_note), CEREAL_NVP(sampler_mapping));
    }
};

} // namespace MetadataItems

struct Metadata {
    std::optional<MetadataItems::MidiMapping> midi_mapping {};
    std::optional<MetadataItems::TimingInfo> timing_info {};
    std::vector<MetadataItems::Loop> loops {};
    std::vector<MetadataItems::Marker> markers {};
    std::vector<MetadataItems::Region> regions {};

    template <class Archive>
    void serialize(Archive &archive) {
        archive(CEREAL_NVP(midi_mapping), CEREAL_NVP(timing_info), CEREAL_NVP(loops), CEREAL_NVP(markers),
                CEREAL_NVP(regions));
    }

    template <typename Type>
    bool HandleStartFramesRemovedForType(std::vector<Type> &vector, size_t num_frames_removed) {
        const auto initial_size = vector.size();
        for (auto it = vector.begin(); it != vector.end();) {
            if (it->start_frame < num_frames_removed) {
                it = vector.erase(it);
            } else {
                it->start_frame -= num_frames_removed;
                ++it;
            }
        }
        return initial_size != vector.size();
    }

    template <typename Type>
    bool HandleEndFramesRemovedForType(std::vector<Type> &vector, size_t new_file_size_in_frames) {
        const auto initial_size = vector.size();
        for (auto it = vector.begin(); it != vector.end();) {
            if (it->start_frame + it->num_frames > new_file_size_in_frames) {
                it = vector.erase(it);
            } else {
                ++it;
            }
        }
        return initial_size != vector.size();
    }
};

struct WaveMetadata {
    void Assign(const drwav_metadata *owned_metadata, unsigned _num_wave_metadata) {
        num_metadata = _num_wave_metadata;
        metadata = {owned_metadata, [](const drwav_metadata *m) { drwav_free((void *)m, NULL); }};
    }

    std::optional<drwav_acid> GetAcid() const {
        const auto result = GetType(drwav_metadata_type_acid);
        if (result) return result->data.acid;
        return {};
    }

    std::optional<drwav_smpl> GetSmpl() const {
        const auto result = GetType(drwav_metadata_type_smpl);
        if (result) return result->data.smpl;
        return {};
    }

    tcb::span<const drwav_metadata> Items() const {
        if (num_metadata) return {metadata.get(), num_metadata};
        return {};
    }
    unsigned NumItems() const { return num_metadata; }

    const drwav_metadata *begin() const { return (NumItems() != 0) ? &Items().front() : nullptr; }
    const drwav_metadata *end() const { return (NumItems() != 0) ? (&Items().back() + 1) : nullptr; }

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
