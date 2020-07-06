#include "gain_calculators.h"

#include "doctest.hpp"
#include "test_helpers.h"
#include <type_traits>

void NormaliseToTarget(AudioData &audio, const double target_amp) {
    PeakGainCalculator calc;
    calc.RegisterBufferMagnitudes(audio);
    const auto gain = calc.GetGain(target_amp);
    audio.MultiplyByScalar(gain);
}

void NormaliseToTarget(std::vector<double> &samples, const double target_amp) {
    AudioData audio;
    audio.interleaved_samples = std::move(samples);
    audio.num_channels = 1;
    NormaliseToTarget(audio, target_amp);
    samples = std::move(audio.interleaved_samples);
}

double GetRMS(const tcb::span<double> samples) {
    double result = 0;
    for (const auto s : samples) {
        result += s * s;
    }
    result /= samples.size();
    return std::sqrt(result);
}

TEST_CASE_TEMPLATE("[Normaliser] gain calcs", T, RMSGainCalculator, PeakGainCalculator) {
    T calc;
    INFO(calc.GetName());
    auto buf = TestHelpers::CreateSingleOscillationSineWave(1, 44100, 100);

    SUBCASE("full volume sample with full volume target") {
        calc.RegisterBufferMagnitudes(buf);
        const auto magnitude = calc.GetLargestRegisteredMagnitude();
        REQUIRE(calc.GetGain(magnitude) == doctest::Approx(1.0));

        SUBCASE("mulitple buffers") {
            for (int i = 0; i < 10; ++i) {
                calc.RegisterBufferMagnitudes(buf);
            }
            REQUIRE(calc.GetGain(magnitude) == doctest::Approx(1.0));
        }
    }

    SUBCASE("half volume sample with full volume target") {
        for (auto &s : buf.interleaved_samples) {
            s *= 0.5;
        }
        calc.RegisterBufferMagnitudes(buf);
        const auto magnitude = calc.GetLargestRegisteredMagnitude();
        REQUIRE(calc.GetGain(magnitude * 2) == doctest::Approx(2.0));
    }

    SUBCASE("full volume sample with half volume target") {
        for (int i = 0; i < 10; ++i) {
            calc.RegisterBufferMagnitudes(buf);
            const auto magnitude = calc.GetLargestRegisteredMagnitude();
            REQUIRE(calc.GetGain(magnitude / 2) == doctest::Approx(0.5));
        }
    }
}
