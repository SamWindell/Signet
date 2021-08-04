#include "expected_midi_pitch.h"

#include <regex>

#include "common.h"
#include "edit_tracked_audio_file.h"

#include "CLI11.hpp"

// clang-format off
static const char *expected_midi_pitch_description =
R"aa(Only correct the audio if the detected target pitch matches the one given{}. To do this, specify a regex pattern that has a single capture group. This will be compared against each filename (excluding folder or file extension). The bit that you capture should be the MIDI note number of the audio file. You can also optionally specify an additional argument: the octave number for MIDI note zero (the default is that MIDI note 0 is C-1).

Example: fix-pitch-drift --expected-note ".*-note-(\d+)-.*" 0
This would find the digits after the text '-note-' in the filename and interpret them as the expected pitch of the track using 0 as the octave number for MIDI note 0.)aa";
// clang-format on

void ExpectedMidiPitch::AddCli(CLI::App &command, bool accept_any_octave) {
    command
        .add_option_function<std::vector<std::string>>(
            "--expected-note",
            [this](const std::vector<std::string> &args) {
                m_expected_note_capture = args[0];
                if (args.size() == 2) {
                    m_expected_note_capture_midi_zero_octave = std::atoi(args[1].data());
                }
            },
            fmt::format(expected_midi_pitch_description,
                        accept_any_octave ? " (or any octave of that note)" : ""))
        ->expected(1, 2);
}

std::optional<MIDIPitch> ExpectedMidiPitch::GetExpectedMidiPitch(const std::string &command_name,
                                                                 EditTrackedAudioFile &f) {
    std::optional<MIDIPitch> expected_midi_pitch {};
    if (m_expected_note_capture) {
        const auto filename = GetJustFilenameWithNoExtension(f.GetPath());
        std::smatch match;
        std::regex re {*m_expected_note_capture};
        if (std::regex_match(filename, match, re)) {
            if (match.size() != 2) {
                ErrorWithNewLine(command_name, f,
                                 "Regex pattern {} contains {} capture group when it should only contain one",
                                 *m_expected_note_capture, match.size() - 1);
            }
            const auto midi_note = std::atoi(match[1].str().data());
            if (midi_note < 0 || midi_note > 127) {
                ErrorWithNewLine(
                    command_name, f,
                    "The captured midi note is outside the valid range - {} is not >=0 and <=127", midi_note);
            }

            const auto index = midi_note + (m_expected_note_capture_midi_zero_octave + 1) * 12;
            if (index < 0 || index > 127) {
                ErrorWithNewLine(
                    command_name, f,
                    "The captured midi note index is outside of Signet's midi pitch range - check if the MIDI note 0 octave is set correctly.",
                    index);
            }
            expected_midi_pitch = g_midi_pitches[index];
        } else {
            ErrorWithNewLine(command_name, f, "Failed to match regex pattern {} to filename {}",
                             *m_expected_note_capture, filename);
        }
    }
    return expected_midi_pitch;
}
