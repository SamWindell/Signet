#include "tune.h"

#include "common.h"
#include "gain_calculators.h"
#include "test_helpers.h"

CLI::App *TuneCommand::CreateCommandCLI(CLI::App &app) {
    auto tune =
        app.add_subcommand("tune", "TuneCommand: changes the tune the file(s) by stretching or shrinking "
                                   "them. Uses a high quality resampling algorithm.");
    tune->add_option("tune cents", m_tune_cents, "The cents to change the pitch by.")->required();
    return tune;
}

void TuneCommand::ProcessFiles(AudioFiles &files) {
    for (auto &f : files) {
        MessageWithNewLine(GetName(), "Tuning sample by {} cents", m_tune_cents);
        f.GetWritableAudio().ChangePitch(m_tune_cents);
    }
}

TEST_CASE("TuneCommand") {
    auto sine = TestHelpers::CreateSineWaveAtFrequency(1, 44100, 1, 220);
    const auto starting_rms = GetRMS(sine.interleaved_samples);

    SUBCASE("220 is detected") {
        const auto detected = sine.DetectPitch();
        REQUIRE(detected);
        REQUIRE(*detected == doctest::Approx(220).epsilon(1.0));
        REQUIRE(starting_rms == doctest::Approx(GetRMS(sine.interleaved_samples)).epsilon(0.1));
    }

    SUBCASE("tuned down 1 octave") {
        sine.ChangePitch(-1200);
        const auto detected = sine.DetectPitch();
        REQUIRE(detected);
        REQUIRE(*detected == doctest::Approx(110).epsilon(1.0));
        REQUIRE(starting_rms == doctest::Approx(GetRMS(sine.interleaved_samples)).epsilon(0.1));
    }

    SUBCASE("tuned down 2 octaves") {
        sine.ChangePitch(-2400);
        const auto detected = sine.DetectPitch();
        REQUIRE(detected);
        REQUIRE(*detected == doctest::Approx(55).epsilon(1.0));
        REQUIRE(starting_rms == doctest::Approx(GetRMS(sine.interleaved_samples)).epsilon(0.1));
    }

    SUBCASE("tuned up 1 octave") {
        sine.ChangePitch(1200);
        const auto detected = sine.DetectPitch();
        REQUIRE(detected);
        REQUIRE(*detected == doctest::Approx(440).epsilon(1.0));
        REQUIRE(starting_rms == doctest::Approx(GetRMS(sine.interleaved_samples)).epsilon(0.1));
    }

    SUBCASE("tuned down 1 semitone") {
        sine.ChangePitch(-100);
        const auto detected = sine.DetectPitch();
        REQUIRE(detected);
        REQUIRE(*detected == doctest::Approx(207.65).epsilon(1.0));
        REQUIRE(starting_rms == doctest::Approx(GetRMS(sine.interleaved_samples)).epsilon(0.1));
    }

    SUBCASE("tuned up 1 semitone") {
        sine.ChangePitch(100);
        const auto detected = sine.DetectPitch();
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
            const auto out = TestHelpers::ProcessBufferWithCommand<TuneCommand>("tune -1200", data);
            REQUIRE(out);
            REQUIRE(out->metadata.markers.size() == 1);
            REQUIRE(out->metadata.markers[0].start_frame == 10);
        }

        SUBCASE("loops") {
            data.metadata.loops.push_back({"loop", MetadataItems::LoopType::Forward, 2, 6, 0});
            const auto out = TestHelpers::ProcessBufferWithCommand<TuneCommand>("tune -1200", data);
            REQUIRE(out);
            REQUIRE(out->metadata.loops.size() == 1);
            REQUIRE(out->metadata.loops[0].start_frame == 4);
            REQUIRE(out->metadata.loops[0].num_frames == 12);
        }

        SUBCASE("regions") {
            data.metadata.regions.push_back({"marker", "region", 2, 6});
            const auto out = TestHelpers::ProcessBufferWithCommand<TuneCommand>("tune -1200", data);
            REQUIRE(out);
            REQUIRE(out->metadata.regions.size() == 1);
            REQUIRE(out->metadata.regions[0].start_frame == 4);
            REQUIRE(out->metadata.regions[0].num_frames == 12);
        }
    }
}
