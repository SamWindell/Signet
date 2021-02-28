#include "fix_pitch_drift_command.h"

#include <algorithm>
#include <optional>

#include "CLI11.hpp"
#include "doctest.hpp"

#include "pitch_drift_corrector.h"
#include "test_helpers.h"
#include "tests_config.h"

CLI::App *FixPitchDriftCommand::CreateCommandCLI(CLI::App &app) {
    auto auto_tune = app.add_subcommand("fix-pitch-drift", "");
    return auto_tune;
}

void FixPitchDriftCommand::ProcessFiles(AudioFiles &files) {
    for (auto &f : files) {
        PitchDriftCorrector pitch_drift_corrector(f.GetAudio(), GetName());
        if (!pitch_drift_corrector.CanFileBePitchCorrected()) {
            MessageWithNewLine(GetName(), "File cannot be pitch-drift corrected");
        } else {
            if (pitch_drift_corrector.ProcessFile(f.GetWritableAudio())) {
                MessageWithNewLine(GetName(), "File successfully auto-tuned.");
            }
        }
    }
}

AudioData CreateSineWaveDriftingPitch(double multiplier) {
    const unsigned sample_rate = 44410;
    const unsigned num_frames = sample_rate * 2;
    const double frequency_hz = 440;

    const auto oscillations_per_sec = frequency_hz;
    const auto oscillations_in_whole = oscillations_per_sec * 2;
    const auto taus_in_whole = oscillations_in_whole * 2 * pi;
    const auto taus_per_sample = taus_in_whole / num_frames;

    AudioData buf;
    buf.num_channels = 1;
    buf.sample_rate = sample_rate;
    buf.interleaved_samples.resize(num_frames * 1);
    double phase = -pi * 2;
    for (size_t frame = 0; frame < num_frames; ++frame) {
        const double value = (double)std::sin(phase);
        phase += taus_per_sample;
        phase *= multiplier;
        buf.interleaved_samples[frame] = value;
    }
    return buf;
}

TEST_CASE("RealTimeAutoTune") {
    {
        const auto buf = CreateSineWaveDriftingPitch(1.0000008);
        WriteAudioFile(fs::path(BUILD_DIRECTORY) / "subtle-drifting-pitch-sine.wav", buf, {});

        const auto out = TestHelpers::ProcessBufferWithCommand<FixPitchDriftCommand>("fix-pitch-drift", buf);
        REQUIRE(out);
        WriteAudioFile(fs::path(BUILD_DIRECTORY) / "subtle-drifting-pitch-sine-processed.wav", *out, {});
    }
    {
        const auto buf = CreateSineWaveDriftingPitch(1.000002);
        WriteAudioFile(fs::path(BUILD_DIRECTORY) / "obvious-drifting-pitch-sine.wav", buf, {});

        const auto out = TestHelpers::ProcessBufferWithCommand<FixPitchDriftCommand>("fix-pitch-drift", buf);
        REQUIRE(out);
        WriteAudioFile(fs::path(BUILD_DIRECTORY) / "obvious-drifting-pitch-sine-processed.wav", *out, {});
    }
}
