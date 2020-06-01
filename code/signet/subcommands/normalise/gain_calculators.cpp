#include "gain_calculators.h"

#include "doctest.hpp"
#include "test_helpers.h"
#include <type_traits>

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
