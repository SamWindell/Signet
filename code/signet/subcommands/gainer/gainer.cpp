#include "gainer.h"

#include "common.h"
#include "test_helpers.h"

CLI::App *Gainer::CreateSubcommandCLI(CLI::App &app) {
    auto gainer = app.add_subcommand("gain", "Gainer: changes the volume of the file(s).");

    gainer->add_option("gain-db", m_gain, "The gain amount in decibels.")
        ->check(CLI::Range(-400, 400))
        ->required();

    return gainer;
}

void Gainer::ProcessFiles(const tcb::span<EditTrackedAudioFile> files) {
    for (auto &f : files) {
        auto &audio = f.GetWritableAudio();
        if (audio.IsEmpty()) continue;

        const auto amp = DBToAmp(m_gain);
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
        const auto out = TestHelpers::ProcessBufferWithSubcommand<Gainer>("gain -6", buf);
        REQUIRE(out);
        REQUIRE(buf.interleaved_samples[0] == -1);
        REQUIRE(out->interleaved_samples[0] == doctest::Approx(-0.5).epsilon(0.01));
    }

    SUBCASE("+6db roughly doubles") {
        const auto out = TestHelpers::ProcessBufferWithSubcommand<Gainer>("gain 6", buf);
        REQUIRE(out);
        REQUIRE(buf.interleaved_samples[0] == -1);
        REQUIRE(out->interleaved_samples[0] == doctest::Approx(-2).epsilon(0.01));
    }
}
