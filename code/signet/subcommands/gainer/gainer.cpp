#include "gainer.h"

#include "common.h"
#include "test_helpers.h"

GainAmount::GainAmount(std::string str) {
    Lowercase(str);
    if (EndsWith(str, "db")) {
        m_unit = Unit::Decibels;
        m_value = std::stof(str);
    } else if (EndsWith(str, "%")) {
        m_unit = Unit::Percent;
        const auto value = std::stof(str);
        if (value < 0) {
            throw CLI::ValidationError("GainAmount", "A percentage value cannot be negative.");
        }
        m_value = value;
    } else {
        throw CLI::ValidationError("GainAmount", "This value must be a number followed by a '%%' unit or "
                                                 "a 'db' unit. For example 10% or -3db.");
    }
}

double GainAmount::GetMultiplier() const {
    switch (m_unit) {
        case Unit::Decibels: {
            return DBToAmp(m_value);
        }
        case Unit::Percent: {
            if (m_value == 0) {
                return 0;
            }
            const auto db = AmpToDB(m_value / 100);
            return DBToAmp(db);
        }
        default: REQUIRE(0);
    }
    return 1;
}

CLI::App *Gainer::CreateSubcommandCLI(CLI::App &app) {
    auto gainer = app.add_subcommand("gain", "Gainer: changes the volume of the file(s).");

    gainer
        ->add_option("gain", m_gain,
                     "The gain amount. This is a number followed by a unit. The unit can be % or db. For "
                     "example 10% or -3.5db. A gain of 50% makes the signal half as loud. A gain of 200% "
                     "makes it twice as loud.")
        ->required();

    return gainer;
}

void Gainer::ProcessFiles(const tcb::span<EditTrackedAudioFile> files) {
    for (auto &f : files) {
        auto &audio = f.GetWritableAudio();
        if (audio.IsEmpty()) continue;

        const auto amp = m_gain.GetMultiplier();
        MessageWithNewLine("Gainer", "Applying a gain of ", amp);
        for (auto &s : audio.interleaved_samples) {
            s *= amp;
        }
    }
}

TEST_CASE("Gainer") {
    const auto buf = TestHelpers::CreateSquareWaveAtFrequency(1, 44100, 0.2, 440);

    SUBCASE("requires db arg") {
        REQUIRE_THROWS(TestHelpers::ProcessBufferWithSubcommand<Gainer>("gain", buf));
    }

    SUBCASE("-6db roughly halves") {
        const auto out = TestHelpers::ProcessBufferWithSubcommand<Gainer>("gain -6db", buf);
        REQUIRE(out);
        REQUIRE(buf.interleaved_samples[0] == -1);
        REQUIRE(out->interleaved_samples[0] == doctest::Approx(-0.5).epsilon(0.01));
    }

    SUBCASE("+6db roughly doubles") {
        const auto out = TestHelpers::ProcessBufferWithSubcommand<Gainer>("gain 6db", buf);
        REQUIRE(out);
        REQUIRE(buf.interleaved_samples[0] == -1);
        REQUIRE(out->interleaved_samples[0] == doctest::Approx(-2).epsilon(0.01));
    }

    SUBCASE("50% roughly halves") {
        const auto out = TestHelpers::ProcessBufferWithSubcommand<Gainer>("gain 50%", buf);
        REQUIRE(out);
        REQUIRE(buf.interleaved_samples[0] == -1);
        REQUIRE(out->interleaved_samples[0] == doctest::Approx(-0.5).epsilon(0.01));
    }

    SUBCASE("200% roughly doubles") {
        const auto out = TestHelpers::ProcessBufferWithSubcommand<Gainer>("gain 200%", buf);
        REQUIRE(out);
        REQUIRE(buf.interleaved_samples[0] == -1);
        REQUIRE(out->interleaved_samples[0] == doctest::Approx(-2).epsilon(0.01));
    }
}
