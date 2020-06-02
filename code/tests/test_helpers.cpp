#include "test_helpers.h"

#include "common.h"

namespace TestHelpers {

AudioFile CreateSingleOscillationSineWave(const unsigned num_channels,
                                          const unsigned sample_rate,
                                          const size_t num_frames) {
    AudioFile buf;
    buf.num_channels = num_channels;
    buf.sample_rate = sample_rate;
    buf.interleaved_samples.resize(num_frames * num_channels);
    for (size_t frame = 0; frame < num_frames; ++frame) {
        for (unsigned channel = 0; channel < num_channels; ++channel) {
            constexpr float two_pi = 6.28318530718f;
            buf.GetSample(channel, frame) = std::sin(frame * (two_pi / num_frames));
        }
    }
    return buf;
}

AudioFile CreateSineWaveAtFrequency(const unsigned num_channels,
                                    const unsigned sample_rate,
                                    const float length_seconds,
                                    const float frequency_hz) {
    const auto num_frames = (size_t)(length_seconds * sample_rate);
    const auto oscillations_per_sec = frequency_hz;
    const auto oscillations_in_whole = oscillations_per_sec * length_seconds;
    const auto taus_in_whole = oscillations_in_whole * 2 * pi;
    const auto taus_per_sample = taus_in_whole / num_frames;

    AudioFile buf;
    buf.num_channels = num_channels;
    buf.sample_rate = sample_rate;
    buf.interleaved_samples.resize(num_frames * num_channels);
    float phase = -pi * 2;
    for (size_t frame = 0; frame < num_frames; ++frame) {
        const float value = (float)std::sin(phase);
        phase += taus_per_sample;
        for (unsigned channel = 0; channel < num_channels; ++channel) {
            buf.GetSample(channel, frame) = value;
        }
    }
    return buf;
}

} // namespace TestHelpers
