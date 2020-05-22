#include "gain_calculators.h"

#include "doctest.hpp"
#include <type_traits>

TEST_CASE_TEMPLATE("[Normaliser] gain calcs", T, RMSGainCalculator, PeakGainCalculator) {
    T calc;

    AudioFile buf;
    buf.num_channels = 1;
    buf.sample_rate = 44100;
    buf.interleaved_samples.resize(100);
    for (size_t i = 0; i < buf.interleaved_samples.size(); ++i) {
        constexpr float two_pi = 6.28318530718f;
        buf.interleaved_samples[i] = std::sin(i * (two_pi / buf.interleaved_samples.size()));
    }

    float max_value = 1;
    if (std::is_same<T, RMSGainCalculator>()) {
        float sum_of_squares = 0;
        for (auto s : buf.interleaved_samples) {
            sum_of_squares += std::pow(s, 2.0f);
        }
        const auto rms = std::sqrt((1.0f / (float)buf.interleaved_samples.size()) * sum_of_squares);
        max_value = rms;
    }

    SUBCASE("full volume sample with full volume target") {
        calc.AddBuffer(buf);
        REQUIRE(calc.GetGain(max_value) == doctest::Approx(1.0f));

        SUBCASE("mulitple buffers") {
            for (int i = 0; i < 10; ++i) {
                calc.AddBuffer(buf);
            }
            REQUIRE(calc.GetGain(max_value) == doctest::Approx(1.0f));
        }
    }

    SUBCASE("silent sample with full volume target") {
        buf.interleaved_samples.resize(100, 0);
        calc.AddBuffer(buf);
        REQUIRE(calc.GetGain(max_value) == doctest::Approx(1.0f));
    }

    SUBCASE("half volume sample with full volume target") {
        for (auto &s : buf.interleaved_samples) {
            s *= 0.5f;
        }
        calc.AddBuffer(buf);
        REQUIRE(calc.GetGain(max_value) == doctest::Approx(2.0f));
    }

    SUBCASE("full volume sample with half volume target") {
        for (int i = 0; i < 10; ++i) {
            calc.AddBuffer(buf);
            REQUIRE(calc.GetGain(max_value / 2) == doctest::Approx(0.5f));
        }
    }
}
