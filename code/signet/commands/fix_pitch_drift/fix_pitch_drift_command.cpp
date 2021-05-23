#include "fix_pitch_drift_command.h"

#include <algorithm>
#include <optional>

#include "CLI11.hpp"
#include "doctest.hpp"

#include "pitch_drift_corrector.h"
#include "test_helpers.h"
#include "tests_config.h"

CLI::App *FixPitchDriftCommand::CreateCommandCLI(CLI::App &app) {
    auto auto_tune = app.add_subcommand(
        "fix-pitch-drift",
        "Automatically corrects regions of drifting pitch in the file(s). This tool is ideal for samples of single-note instruments that subtly drift out of pitch, such as a human voice or a wind instrument. It analyses the audio for regions of consistent pitch (avoiding noise or silence), and for each of these regions, it smoothly speeds up or slows down the audio to counteract any drift pitch. The result is a file that stays in-tune throughout its duration. Only the drifting pitch is corrected by this tool; it does not tune the audio to be a standard musical pitch. See Signet's other auto-tune command for that. As well as this, fix-pitch-drift is a bit more specialised and does not always work as ubiquitously as Signet's other auto-tune command.");

    auto_tune
        ->add_option(
            "--chunk-ms", m_chunk_length_milliseconds,
            "fix-pitch-drift evaluates the audio in small chunks. The pitch of each chunk is determined in order to get a picture of the audio's pitch over time. You can set the chunk size with this option. The default is 60 milliseconds. If you are finding this tool is incorrectly changing the pitch, you might try increasing the chunk size by 10 ms or so.")
        ->check(CLI::Range(20, 200));
    ;

    auto_tune->add_flag(
        "--print-csv", m_print_csv,
        "Print a block of CSV data that can be loaded into a spreadsheet in order to determine what fix-pitch-drift is doing to the audio over time.");

    return auto_tune;
}

void FixPitchDriftCommand::ProcessFiles(AudioFiles &files) {
    for (auto &f : files) {
        PitchDriftCorrector pitch_drift_corrector(f.GetAudio(), GetName(), m_chunk_length_milliseconds,
                                                  m_print_csv);
        if (!pitch_drift_corrector.CanFileBePitchCorrected()) {
            WarningWithNewLine(GetName(), "{}: cannot be pitch-drift corrected", f.OriginalFilename());
        } else {
            MessageWithNewLine(GetName(), "{}: correcting pitch-drift", f.OriginalFilename());
            if (pitch_drift_corrector.ProcessFile(f.GetWritableAudio())) {
                MessageWithNewLine(GetName(), "{}: successfully pitch-drift corrected.",
                                   f.OriginalFilename());
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
