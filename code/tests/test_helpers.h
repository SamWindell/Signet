#pragma once
#include <cmath>

#include "audio_file.h"

struct TestHelpers {
    static AudioFile GetSine() {
        AudioFile buf;
        buf.num_channels = 1;
        buf.sample_rate = 44100;
        buf.interleaved_samples.resize(100);
        for (size_t i = 0; i < buf.interleaved_samples.size(); ++i) {
            constexpr float two_pi = 6.28318530718f;
            buf.interleaved_samples[i] = std::sin(i * (two_pi / buf.interleaved_samples.size()));
        }
        return buf;
    }
};
