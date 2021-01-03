#include "tuner.h"

#include "common.h"
#include "subcommands/converter/converter.h"
#include "subcommands/normaliser/gain_calculators.h"
#include "subcommands/pitch_detector/pitch_detector.h"
#include "test_helpers.h"

CLI::App *Tuner::CreateSubcommandCLI(CLI::App &app) {
    auto tuner = app.add_subcommand("tune", "Tuner: changes the tune the file(s) by stretching or shrinking "
                                            "them. Uses a high quality resampling algorithm.");
    tuner->add_option("tune cents", m_tune_cents, "The cents to change the pitch by.")->required();
    return tuner;
}

void Tuner::ChangePitch(AudioData &audio, const double cents) {
    constexpr auto cents_in_octave = 100.0 * 12.0;
    const auto multiplier = std::pow(2, -cents / cents_in_octave);
    const auto new_sample_rate = (double)audio.sample_rate * multiplier;
    Converter::ConvertSampleRate(audio.interleaved_samples, audio.num_channels, (double)audio.sample_rate,
                                 new_sample_rate);
    audio.AudioDataWasStretched(multiplier);
}

void Tuner::ProcessFiles(const tcb::span<EditTrackedAudioFile> files) {
    for (auto &f : files) {
        MessageWithNewLine(GetName(), "Tuning sample by {} cents", m_tune_cents);
        ChangePitch(f.GetWritableAudio(), m_tune_cents);
    }
}

TEST_CASE("Tuner") {
    auto sine = TestHelpers::CreateSineWaveAtFrequency(1, 44100, 1, 220);
    const auto starting_rms = GetRMS(sine.interleaved_samples);

    SUBCASE("220 is detected") {
        const auto detected = PitchDetector::DetectPitch(sine);
        REQUIRE(detected);
        REQUIRE(*detected == doctest::Approx(220).epsilon(1.0));
        REQUIRE(starting_rms == doctest::Approx(GetRMS(sine.interleaved_samples)).epsilon(0.1));
    }

    SUBCASE("tuned down 1 octave") {
        Tuner::ChangePitch(sine, -1200);
        const auto detected = PitchDetector::DetectPitch(sine);
        REQUIRE(detected);
        REQUIRE(*detected == doctest::Approx(110).epsilon(1.0));
        REQUIRE(starting_rms == doctest::Approx(GetRMS(sine.interleaved_samples)).epsilon(0.1));
    }

    SUBCASE("tuned down 2 octaves") {
        Tuner::ChangePitch(sine, -2400);
        const auto detected = PitchDetector::DetectPitch(sine);
        REQUIRE(detected);
        REQUIRE(*detected == doctest::Approx(55).epsilon(1.0));
        REQUIRE(starting_rms == doctest::Approx(GetRMS(sine.interleaved_samples)).epsilon(0.1));
    }

    SUBCASE("tuned up 1 octave") {
        Tuner::ChangePitch(sine, 1200);
        const auto detected = PitchDetector::DetectPitch(sine);
        REQUIRE(detected);
        REQUIRE(*detected == doctest::Approx(440).epsilon(1.0));
        REQUIRE(starting_rms == doctest::Approx(GetRMS(sine.interleaved_samples)).epsilon(0.1));
    }

    SUBCASE("tuned down 1 semitone") {
        Tuner::ChangePitch(sine, -100);
        const auto detected = PitchDetector::DetectPitch(sine);
        REQUIRE(detected);
        REQUIRE(*detected == doctest::Approx(207.65).epsilon(1.0));
        REQUIRE(starting_rms == doctest::Approx(GetRMS(sine.interleaved_samples)).epsilon(0.1));
    }

    SUBCASE("tuned up 1 semitone") {
        Tuner::ChangePitch(sine, 100);
        const auto detected = PitchDetector::DetectPitch(sine);
        REQUIRE(detected);
        REQUIRE(*detected == doctest::Approx(233.08).epsilon(1.0));
        REQUIRE(starting_rms == doctest::Approx(GetRMS(sine.interleaved_samples)).epsilon(0.1));
    }

    SUBCASE("metadata stretches") {
        AudioData data {};
        data.num_channels = 1;
        data.sample_rate = 44100;
        data.interleaved_samples.resize(10);

        SUBCASE("markers") {
            data.metadata.markers.push_back({"marker", 5});
            const auto out = TestHelpers::ProcessBufferWithSubcommand<Tuner>("tune -1200", data);
            REQUIRE(out);
            REQUIRE(out->metadata.markers.size() == 1);
            REQUIRE(out->metadata.markers[0].start_frame == 10);
        }

        SUBCASE("loops") {
            data.metadata.loops.push_back({"loop", MetadataItems::LoopType::Forward, 2, 6, 0});
            const auto out = TestHelpers::ProcessBufferWithSubcommand<Tuner>("tune -1200", data);
            REQUIRE(out);
            REQUIRE(out->metadata.loops.size() == 1);
            REQUIRE(out->metadata.loops[0].start_frame == 4);
            REQUIRE(out->metadata.loops[0].num_frames == 12);
        }

        SUBCASE("regions") {
            data.metadata.regions.push_back({"marker", "region", 2, 6});
            const auto out = TestHelpers::ProcessBufferWithSubcommand<Tuner>("tune -1200", data);
            REQUIRE(out);
            REQUIRE(out->metadata.regions.size() == 1);
            REQUIRE(out->metadata.regions[0].start_frame == 4);
            REQUIRE(out->metadata.regions[0].num_frames == 12);
        }
    }
}
