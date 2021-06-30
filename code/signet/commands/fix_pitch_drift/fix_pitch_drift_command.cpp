#include "fix_pitch_drift_command.h"

#include <algorithm>
#include <optional>
#include <regex>

#include "CLI11.hpp"
#include "doctest.hpp"

#include "pitch_drift_corrector.h"
#include "test_helpers.h"
#include "tests_config.h"

CLI::App *FixPitchDriftCommand::CreateCommandCLI(CLI::App &app) {
    auto fix_pitch_drift = app.add_subcommand(
        "fix-pitch-drift",
        "Automatically corrects regions of drifting pitch in the file(s). This tool is ideal for samples of single-note instruments that subtly drift out of pitch, such as a human voice or a wind instrument. It analyses the audio for regions of consistent pitch (avoiding noise or silence), and for each of these regions, it smoothly speeds up or slows down the audio to counteract any drift pitch. The result is a file that stays in-tune throughout its duration. Only the drifting pitch is corrected by this tool; it does not tune the audio to be a standard musical pitch. See Signet's other auto-tune command for that. As well as this, fix-pitch-drift is a bit more specialised and does not always work as ubiquitously as Signet's other auto-tune command.");

    m_identical_processing_set.AddCli(*fix_pitch_drift);

    fix_pitch_drift
        ->add_option(
            "--chunk-ms", m_chunk_length_milliseconds,
            "fix-pitch-drift evaluates the audio in small chunks. The pitch of each chunk is determined in order to get a picture of the audio's pitch over time. You can set the chunk size with this option. The default is 60 milliseconds. If you are finding this tool is incorrectly changing the pitch, you might try increasing the chunk size by 10 ms or so.")
        ->check(CLI::Range(20, 200));
    ;

    fix_pitch_drift
        ->add_option_function<std::vector<std::string>>(
            "--expected-note",
            [this](const std::vector<std::string> &args) {
                m_expected_note_capture = args[0];
                if (args.size() == 2) {
                    m_expected_note_capture_midi_zero_octave = std::atoi(args[1].data());
                }
            },
            R"aa(Only correct the audio if the detected target pitch matches the one given. To do this, specify a regex pattern that has a single capture group. This will be compared against each filename (excluding folder or file extension). The bit that you capture should be the MIDI note number of the audio file. You can also optionally specify an additional argument: the octave number for MIDI note zero (the default is that MIDI note 0 is C-1).

Example: fix-pitch-drift --expected-note ".*-note-(\d+)-.*" 0
This would find the digits after the text '-note-' in the filename and interpret them as the expected pitch of the track using 0 as the octave number for MIDI note 0.)aa")
        ->expected(1, 2);

    fix_pitch_drift->add_flag(
        "--print-csv", m_print_csv,
        "Print a block of CSV data that can be loaded into a spreadsheet in order to determine what fix-pitch-drift is doing to the audio over time.");

    return fix_pitch_drift;
}

void FixPitchDriftCommand::ProcessFiles(AudioFiles &files) {
    const auto GetExpectedMidiPitch = [&](EditTrackedAudioFile &f) {
        std::optional<MIDIPitch> expected_midi_pitch {};
        if (m_expected_note_capture) {
            const auto filename = GetJustFilenameWithNoExtension(f.GetPath());
            std::smatch match;
            std::regex re {*m_expected_note_capture};
            if (std::regex_match(filename, match, re)) {
                if (match.size() != 2) {
                    ErrorWithNewLine(
                        GetName(), f,
                        "Regex pattern {} contains {} capture group when it should only contain one",
                        *m_expected_note_capture, match.size() - 1);
                }
                const auto midi_note = std::atoi(match[1].str().data());
                if (midi_note < 0 || midi_note > 127) {
                    ErrorWithNewLine(
                        GetName(), f,
                        "The captured midi note is outside the valid range - {} is not >=0 and <=127",
                        midi_note);
                }

                const auto index = midi_note + (m_expected_note_capture_midi_zero_octave + 1) * 12;
                if (index < 0 || index > 127) {
                    ErrorWithNewLine(
                        GetName(), f,
                        "The captured midi note index is outside of Signet's midi pitch range - check if the MIDI note 0 octave is set correctly.",
                        index);
                }
                expected_midi_pitch = g_midi_pitches[index];
            } else {
                ErrorWithNewLine(GetName(), f, "Failed to match regex pattern {} to filename {}",
                                 *m_expected_note_capture, filename);
            }
        }
        return expected_midi_pitch;
    };

    if (!m_identical_processing_set.ShouldProcessInSets()) {
        for (auto &f : files) {
            PitchDriftCorrector pitch_drift_corrector(f.GetAudio(), GetName(), f.OriginalPath(),
                                                      m_chunk_length_milliseconds, m_print_csv);
            if (!pitch_drift_corrector.CanFileBePitchCorrected()) {
                ErrorWithNewLine(GetName(), f, "cannot be pitch-drift corrected");
            } else {
                MessageWithNewLine(GetName(), f, "correcting pitch-drift");

                if (pitch_drift_corrector.ProcessFile(f.GetWritableAudio(), GetExpectedMidiPitch(f))) {
                    MessageWithNewLine(GetName(), f, "successfully pitch-drift corrected.");
                }
            }
        }
    } else {
        m_identical_processing_set.ProcessSets(
            files, GetName(),
            [&](EditTrackedAudioFile *authority_file, const std::vector<EditTrackedAudioFile *> &set) {
                if (!IdenticalProcessingSet::AllHaveSameNumFrames(set)) {
                    ErrorWithNewLine(
                        GetName(), *authority_file,
                        "the files in the set do not all have the same number of frames and therefore cannot be processed with fix-pitch-drift.");
                    return;
                }

                PitchDriftCorrector pitch_drift_corrector(authority_file->GetAudio(), GetName(),
                                                          authority_file->OriginalPath(),
                                                          m_chunk_length_milliseconds, m_print_csv);
                if (!pitch_drift_corrector.CanFileBePitchCorrected()) {
                    ErrorWithNewLine(
                        GetName(), *authority_file,
                        "authority file for set cannot be pitch-drift corrected, therefore the set cannot be processed");
                } else {
                    for (auto f : set) {
                        MessageWithNewLine(GetName(), *f, "correcting pitch-drift");
                        if (pitch_drift_corrector.ProcessFile(f->GetWritableAudio(),
                                                              GetExpectedMidiPitch(*f))) {
                            MessageWithNewLine(GetName(), *f, "successfully pitch-drift corrected.");
                        }
                    }
                }
            });
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
