#include "gain_calculators.h"

#include "doctest.hpp"
#include "test_helpers.h"

void NormaliseToTarget(AudioData &audio, const double target_amp) {
    PeakGainCalculator calc {};
    calc.RegisterBufferMagnitudes(audio, {});
    const auto gain = calc.GetGain(target_amp);
    audio.MultiplyByScalar(gain);
}

void NormaliseToTarget(std::vector<double> &samples, const double target_amp) {
    AudioData audio {};
    audio.interleaved_samples = std::move(samples);
    audio.num_channels = 1;
    NormaliseToTarget(audio, target_amp);
    samples = std::move(audio.interleaved_samples);
}

double GetRMS(const tcb::span<const double> samples) {
    if (!samples.size()) return 0;
    double result = 0;
    for (const auto s : samples) {
        result += s * s;
    }
    result /= samples.size();
    REQUIRE(result >= 0);
    return std::sqrt(result);
}

Peak GetPeak(const tcb::span<const double> samples) {
    if (!samples.size()) return {0, 0};
    Peak result {0, 0};
    for (size_t i = 0; i < samples.size(); ++i) {
        auto const s = std::abs(samples[i]);
        if (s > result.value) {
            result.value = s;
            result.index = i;
        }
    }
    return result;
}

TEST_CASE_TEMPLATE("[NormaliseCommand] gain calcs", T, RMSGainCalculator, PeakGainCalculator) {
    T calc;
    INFO(calc.GetName());
    auto buf = TestHelpers::CreateSingleOscillationSineWave(1, 44100, 100);

    SUBCASE("full volume sample with full volume target") {
        calc.RegisterBufferMagnitudes(buf, {});
        const auto magnitude = calc.GetLargestRegisteredMagnitude();
        REQUIRE(calc.GetGain(magnitude) == doctest::Approx(1.0));

        SUBCASE("mulitple buffers") {
            for (int i = 0; i < 10; ++i) {
                calc.RegisterBufferMagnitudes(buf, {});
            }
            REQUIRE(calc.GetGain(magnitude) == doctest::Approx(1.0));
        }
    }

    SUBCASE("half volume sample with full volume target") {
        for (auto &s : buf.interleaved_samples) {
            s *= 0.5;
        }
        calc.RegisterBufferMagnitudes(buf, {});
        const auto magnitude = calc.GetLargestRegisteredMagnitude();
        REQUIRE(calc.GetGain(magnitude * 2) == doctest::Approx(2.0));
    }

    SUBCASE("full volume sample with half volume target") {
        for (int i = 0; i < 10; ++i) {
            calc.RegisterBufferMagnitudes(buf, {});
            const auto magnitude = calc.GetLargestRegisteredMagnitude();
            REQUIRE(calc.GetGain(magnitude / 2) == doctest::Approx(0.5));
        }
    }
}
