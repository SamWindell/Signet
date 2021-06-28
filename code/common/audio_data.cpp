#include "audio_data.h"

#include "dywapitchtrack/dywapitchtrack.h"
#include "r8brain-resampler/CDSPResampler.h"

#include "common.h"
#include "gain_calculators.h"

size_t AudioData::NumFrames() const {
    assert(num_channels != 0);
    return interleaved_samples.size() / num_channels;
}

double &AudioData::GetSample(unsigned channel, size_t frame) {
    return interleaved_samples[frame * num_channels + channel];
}

const double &AudioData::GetSample(unsigned channel, size_t frame) const {
    return interleaved_samples[frame * num_channels + channel];
}

//

void AudioData::ChangePitch(double cents) {
    constexpr auto cents_in_octave = 100.0 * 12.0;
    const auto multiplier = std::pow(2, -cents / cents_in_octave);
    const auto new_sample_rate = (double)sample_rate * multiplier;
    const auto original_sample_rate = sample_rate;
    Resample(new_sample_rate);
    sample_rate = original_sample_rate; // we don't want to change the sample rate
}

void AudioData::Resample(double new_sample_rate) {
    if (sample_rate == new_sample_rate) return;

    const auto result_num_frames = (usize)(NumFrames() * (new_sample_rate / (double)sample_rate));
    std::vector<double> result_interleaved_samples(num_channels * result_num_frames);

    r8b::CDSPResampler24 resampler(sample_rate, new_sample_rate, (int)NumFrames());

    ForEachDeinterleavedChannel(
        interleaved_samples, num_channels, [&](const auto &channel_buffer, auto channel) {
            std::vector<double> output_buffer(result_num_frames);
            resampler.oneshot(channel_buffer.data(), (int)channel_buffer.size(), output_buffer.data(),
                              (int)result_num_frames);
            for (size_t frame = 0; frame < result_num_frames; ++frame) {
                result_interleaved_samples[frame * num_channels + channel] = output_buffer[frame];
            }

            resampler.clear();
        });

    interleaved_samples = std::move(result_interleaved_samples);

    const auto stretch_factor = new_sample_rate / (double)sample_rate;
    AudioDataWasStretched(stretch_factor);
    sample_rate = (unsigned int)new_sample_rate;
}

void AudioData::MultiplyByScalar(const double amount) {
    for (auto &s : interleaved_samples) {
        s *= amount;
    }
}

void AudioData::AddOther(const AudioData &other) {
    if (other.interleaved_samples.size() > interleaved_samples.size()) {
        interleaved_samples.resize(other.interleaved_samples.size());
    }
    for (usize i = 0; i < other.interleaved_samples.size(); ++i) {
        interleaved_samples[i] += other.interleaved_samples[i];
    }
}

std::vector<double> AudioData::MixDownToMono() const {
    std::vector<double> mono_signal;
    mono_signal.reserve(NumFrames());
    for (usize frame = 0; frame < NumFrames(); ++frame) {
        double v = 0;
        for (unsigned chan = 0; chan < num_channels; ++chan) {
            v += GetSample(chan, frame);
        }
        mono_signal.push_back(v);
    }
    return mono_signal;
}

static std::optional<double> DetectSinglePitch(const AudioData &audio) {
    auto mono_signal = audio.MixDownToMono();
    NormaliseToTarget(mono_signal, 1);

    struct ChunkData {
        double detected_pitch {};
        double rms {};
        double suitability {};
    };
    constexpr auto chunk_seconds = 0.1;

    std::vector<ChunkData> chunks;
    const auto chunk_frames = (usize)(chunk_seconds * audio.sample_rate);
    for (usize frame = 0; frame < audio.NumFrames(); frame += chunk_frames) {
        const auto chunk_size = (int)std::min(chunk_frames, audio.NumFrames() - frame);

        dywapitchtracker pitch_tracker;
        dywapitch_inittracking(&pitch_tracker);
        auto detected_pitch = dywapitch_computepitch(&pitch_tracker, const_cast<double *>(mono_signal.data()),
                                                     (int)frame, chunk_size);
        if (audio.sample_rate != 44100) {
            detected_pitch *= static_cast<double>(audio.sample_rate) / 44100.0;
        }
        chunks.push_back({detected_pitch, GetRMS({mono_signal.data() + frame, (usize)chunk_size}), 0});
    }

    for (auto &chunk : chunks) {
        const auto p1 = chunk.detected_pitch;

        for (const auto &other_c : chunks) {
            const auto p2 = other_c.detected_pitch;
            if (p2 == 0) continue;

            const auto GaussianFunction = [](const auto x) {
                constexpr auto height = 10;
                constexpr auto peak_centre = 0;
                constexpr auto width = 0.9;
                return height * std::exp(-(std::pow(x - peak_centre, 2) / (2 * std::pow(width, 2))));
            };

            const auto pitch_delta = p2 - p1;
            chunk.suitability += GaussianFunction(pitch_delta);
        }
    }

    // Make chunks that contain louder audio a little bit more important
    {
        double max_rms = 0;
        double min_rms = DBL_MAX;
        for (auto &chunk : chunks) {
            REQUIRE(chunk.rms >= 0);
            if (chunk.rms < min_rms) min_rms = chunk.rms;
            if (chunk.rms > max_rms) max_rms = chunk.rms;
        }

        for (auto &chunk : chunks) {
            if ((max_rms - min_rms) == 0) continue;
            const auto rms_relative = (chunk.rms - min_rms) / (max_rms - min_rms);
            REQUIRE(rms_relative >= 0);
            REQUIRE(rms_relative <= 1);
            constexpr auto multiplier_for_loudest_chunk = 1.5;
            chunk.suitability *=
                1 + (std::cos(half_pi - (rms_relative * half_pi)) * multiplier_for_loudest_chunk);
        }
    }

    const ChunkData *most_suitable_chunk = &chunks[0];
    for (const auto &c : chunks) {
        if (c.suitability > most_suitable_chunk->suitability) {
            most_suitable_chunk = &c;
        }
    }

    if (most_suitable_chunk->detected_pitch != 0.0) {
        return most_suitable_chunk->detected_pitch;
    }
    return std::nullopt;
}

static bool ApproxEqual(double a, double b, double epsilon) {
    return a > (b - epsilon / 2) && a < (b + epsilon / 2);
}

std::optional<double> AudioData::DetectPitch() const {
    // The pitch detection algorithm that we are using can get it wrong sometimes when the audio is very
    // high pitch or very low pitch. To help with this, we detect the pitch at different octaves and then
    // work out which one is giving us the best results.

    struct PitchedData {
        std::optional<double> detected_pitch {};
        double cents {};
        double suitability {};
    };

    std::vector<PitchedData> pitches;
    for (double cents = -2400; cents < 2400; cents += 1200) {
        AudioData pitched_audio = *this;
        pitched_audio.ChangePitch(cents);
        pitches.push_back({DetectSinglePitch(pitched_audio), cents});
    }

    for (auto &p : pitches) {
        if (!p.detected_pitch) continue;
        for (const auto &p2 : pitches) {
            if (!p2.detected_pitch) continue;
            const auto delta_cents = p2.cents - p.cents;
            const auto expected_hz = GetFreqWithCentDifference(*p.detected_pitch, delta_cents);
            if (ApproxEqual(expected_hz, *p2.detected_pitch, 3)) {
                p.suitability += 1;
            }
        }
    }

    const PitchedData *most_suitable = &pitches[0];
    for (const auto &p : pitches) {
        if (p.suitability > most_suitable->suitability) {
            most_suitable = &p;
        }
    }

    if (!most_suitable->detected_pitch) {
        return {};
    }

    return GetFreqWithCentDifference(*most_suitable->detected_pitch, -most_suitable->cents);
}

bool AudioData::IsSilent() const {
    for (const auto v : interleaved_samples) {
        if (v != 0.0) return false;
    }
    return true;
}

//

void AudioData::FramesWereRemovedFromStart(size_t num_frames) {
    if (metadata.HandleStartFramesRemovedForType<MetadataItems::Region>(metadata.regions, num_frames)) {
        PrintMetadataRemovalWarning("regions");
    }
    if (metadata.HandleStartFramesRemovedForType<MetadataItems::Marker>(metadata.markers, num_frames)) {
        PrintMetadataRemovalWarning("markers");
    }
    if (metadata.HandleStartFramesRemovedForType<MetadataItems::Loop>(metadata.loops, num_frames)) {
        PrintMetadataRemovalWarning("loops");
    }
}

void AudioData::FramesWereRemovedFromEnd() {
    if (metadata.HandleEndFramesRemovedForType<MetadataItems::Region>(metadata.regions, NumFrames())) {
        PrintMetadataRemovalWarning("regions");
    }
    if (metadata.HandleEndFramesRemovedForType<MetadataItems::Loop>(metadata.loops, NumFrames())) {
        PrintMetadataRemovalWarning("loops");
    }
    bool markers_removed = false;
    for (auto it = metadata.markers.begin(); it != metadata.markers.end();) {
        if (it->start_frame >= NumFrames()) {
            it = metadata.markers.erase(it);
            markers_removed = true;
        } else {
            ++it;
        }
    }
    if (markers_removed) {
        PrintMetadataRemovalWarning("markers");
    }
}

void AudioData::AudioDataWasStretched(double stretch_factor) {
    for (auto &r : metadata.regions) {
        r.start_frame = (size_t)(r.start_frame * stretch_factor);
        r.num_frames = (size_t)(r.num_frames * stretch_factor);
        assert(r.start_frame < NumFrames());
        assert(r.start_frame + r.num_frames <= NumFrames());
    }
    for (auto &r : metadata.markers) {
        r.start_frame = (size_t)(r.start_frame * stretch_factor);
        assert(r.start_frame < NumFrames());
    }
    for (auto &r : metadata.loops) {
        r.start_frame = (size_t)(r.start_frame * stretch_factor);
        r.num_frames = (size_t)(r.num_frames * stretch_factor);
        assert(r.start_frame < NumFrames());
        assert(r.start_frame + r.num_frames <= NumFrames());
    }
}

void AudioData::PrintMetadataRemovalWarning(std::string_view metadata_name) {
    WarningWithNewLine("Signet", {},
                       "One or more metadata {} were removed from the file because the file changed size",
                       metadata_name);
}