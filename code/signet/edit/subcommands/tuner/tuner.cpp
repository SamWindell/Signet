#include "tuner.h"

#include "common.h"
#include "edit/subcommands/converter/converter.h"
#include "edit/subcommands/pitch_detector/pitch_detector.h"
#include "test_helpers.h"

CLI::App *Tuner::CreateSubcommandCLI(CLI::App &app) {
    auto tuner = app.add_subcommand("tune", "Tuner: changes the tune the file(s) by stretching or shrinking "
                                            "them. Uses a high quality resampling algorithm.");
    tuner->add_option("tune cents", m_tune_cents, "The cents to change the pitch by.")->required();
    return tuner;
}

void Tuner::ChangePitch(AudioFile &input, const double cents) {
    constexpr auto cents_in_octave = 100.0 * 12.0;
    const auto multiplier = std::pow(2, -cents / cents_in_octave);
    const auto new_sample_rate = (double)input.sample_rate * multiplier;
    Converter::ConvertSampleRate(input.interleaved_samples, input.num_channels, (double)input.sample_rate,
                                 new_sample_rate);
}

bool Tuner::ProcessAudio(AudioFile &input, const std::string_view filename) {
    if (!input.interleaved_samples.size()) return false;
    MessageWithNewLine("Tuner", "Tuning sample by ", m_tune_cents, " cents");
    ChangePitch(input, m_tune_cents);
    return true;
}

TEST_CASE("Tuner") {
    auto sine = TestHelpers::CreateSineWaveAtFrequency(1, 44100, 1, 220);

    SUBCASE("220 is detected") {
        const auto detected = PitchDetector::DetectPitch(sine);
        REQUIRE(detected);
        REQUIRE(*detected == doctest::Approx(220).epsilon(1.0));
    }

    SUBCASE("tuned down 1 octave") {
        Tuner::ChangePitch(sine, -1200);
        const auto detected = PitchDetector::DetectPitch(sine);
        REQUIRE(detected);
        REQUIRE(*detected == doctest::Approx(110).epsilon(1.0));
    }

    SUBCASE("tuned down 2 octaves") {
        Tuner::ChangePitch(sine, -2400);
        const auto detected = PitchDetector::DetectPitch(sine);
        REQUIRE(detected);
        REQUIRE(*detected == doctest::Approx(55).epsilon(1.0));
    }

    SUBCASE("tuned up 1 octave") {
        Tuner::ChangePitch(sine, 1200);
        const auto detected = PitchDetector::DetectPitch(sine);
        REQUIRE(detected);
        REQUIRE(*detected == doctest::Approx(440).epsilon(1.0));
    }

    SUBCASE("tuned down 1 semitone") {
        Tuner::ChangePitch(sine, -100);
        const auto detected = PitchDetector::DetectPitch(sine);
        REQUIRE(detected);
        REQUIRE(*detected == doctest::Approx(207.65).epsilon(1.0));
    }

    SUBCASE("tuned up 1 semitone") {
        Tuner::ChangePitch(sine, 100);
        const auto detected = PitchDetector::DetectPitch(sine);
        REQUIRE(detected);
        REQUIRE(*detected == doctest::Approx(233.08).epsilon(1.0));
    }
}
