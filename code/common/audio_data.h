#pragma once

#include <cassert>
#include <vector>

#include "FLAC/metadata.h"
#include "span.hpp"

#include "metadata.h"
#include "types.h"

enum class AudioFileFormat {
    Wav,
    Flac,
};

struct AudioData {
    bool IsEmpty() const { return interleaved_samples.empty(); }
    size_t NumFrames() const {
        assert(num_channels != 0);
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
        if (metadata.regions) {
            metadata.HandleStartFramesRemovedForType<MetadataItems::Region>(metadata.regions->regions,
                                                                            num_frames);
        }
        if (metadata.markers) {
            metadata.HandleStartFramesRemovedForType<MetadataItems::Marker>(metadata.markers->markers,
                                                                            num_frames);
        }
        if (metadata.loops) {
            metadata.HandleStartFramesRemovedForType<MetadataItems::Loop>(metadata.loops->loops, num_frames);
        }
    }

    void FramesWereRemovedFromEnd() {
        if (metadata.regions) {
            metadata.HandleEndFramesRemovedForType<MetadataItems::Region>(metadata.regions->regions,
                                                                          NumFrames());
        }
        if (metadata.markers) {
            for (auto it = metadata.markers->markers.begin(); it != metadata.markers->markers.end();) {
                if (it->start_frame >= NumFrames()) {
                    it = metadata.markers->markers.erase(it);
                } else {
                    ++it;
                }
            }
        }
        if (metadata.loops) {
            metadata.HandleEndFramesRemovedForType<MetadataItems::Loop>(metadata.loops->loops, NumFrames());
        }
    }

    void AudioDataWasStretched(double stretch_factor) {
        if (metadata.regions) {
            for (auto &r : metadata.regions->regions) {
                r.start_frame = (size_t)(r.start_frame * stretch_factor);
                assert(r.start_frame < NumFrames());
            }
        }
        if (metadata.markers) {
            for (auto &r : metadata.markers->markers) {
                r.start_frame = (size_t)(r.start_frame * stretch_factor);
                assert(r.start_frame < NumFrames());
            }
        }
        if (metadata.loops) {
            for (auto &r : metadata.loops->loops) {
                r.start_frame = (size_t)(r.start_frame * stretch_factor);
                assert(r.start_frame < NumFrames());
            }
        }
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
