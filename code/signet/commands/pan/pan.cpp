#include "pan.h"

#include "common.h"
#include "test_helpers.h"

PanUnit::PanUnit(std::string str) {
    Lowercase(str);

    value = (double)std::atoi(str.data());
    if (value > 100 || value < 0) {
        throw CLI::ConversionError("Pan value must be from 0 to 100");
    }

    if (EndsWith(str, "l")) {
        value *= -1;
    } else if (EndsWith(str, "r")) {
    } else {
        throw CLI::ConversionError("Pan value must be end with either R or L. For example 75R.");
    }

    value /= 100;
    assert(value >= -1 && value <= 1);
}

CLI::App *PanCommand::CreateCommandCLI(CLI::App &app) {
    auto pan =
        app.add_subcommand("pan", "Changes the pan of stereo file(s). Does not work on non-stereo files.");

    pan->add_option(
           "pan-amount", m_pan,
           "The pan amount. This is a number from 0 to 100 followed by either L or R (representing left or right). For example: 100R (full right pan), 100L (full left pan), 10R (pan right with 10% intensity).")
        ->required();

    return pan;
}

inline void SetEqualPan(double pan_pos, double &out_left, double &out_right) {
    assert(pan_pos >= -1 && pan_pos <= 1);
    const auto angle = (pan_pos * (pi * 0.5)) * 0.5;
    const auto cosx = std::cos(angle);
    const auto sinx = std::sin(angle);

    auto left = (sqrt_two / 2) * (cosx - sinx);
    auto right = (sqrt_two / 2) * (cosx + sinx);
    assert(left >= 0 && right >= 0);

    out_left *= left;
    out_right *= right;
}

void PanCommand::ProcessFiles(AudioFiles &files) {
    for (auto &f : files) {
        auto &audio = f.GetWritableAudio();
        if (audio.IsEmpty()) continue;
        if (audio.num_channels != 2) {
            MessageWithNewLine(GetName(), f, "Skipping non-stereo file");
            continue;
        }

        for (size_t frame = 0; frame < audio.NumFrames(); ++frame) {
            SetEqualPan(m_pan, audio.GetSample(0, frame), audio.GetSample(1, frame));
        }
    }
}

TEST_CASE("PanCommand") {
    const auto buf = TestHelpers::CreateSquareWaveAtFrequency(2, 44100, 0.2, 440);

    SUBCASE("requires pan arg") {
        REQUIRE_THROWS(TestHelpers::ProcessBufferWithCommand<PanCommand>("pan", buf));
    }

    SUBCASE("pans fully left") {
        const auto out = TestHelpers::ProcessBufferWithCommand<PanCommand>("pan 100L", buf);
        REQUIRE(out);
        REQUIRE(out->interleaved_samples[0] == doctest::Approx(1));
        REQUIRE(out->interleaved_samples[1] == doctest::Approx(0));
    }

    SUBCASE("pans fully right") {
        const auto out = TestHelpers::ProcessBufferWithCommand<PanCommand>("pan 100R", buf);
        REQUIRE(out);
        REQUIRE(out->interleaved_samples[0] == doctest::Approx(0));
        REQUIRE(out->interleaved_samples[1] == doctest::Approx(1));
    }
}
