#include "auto_tune.h"

#include <regex>
#include <unordered_map>

#include "CLI11.hpp"
#include "doctest.hpp"

#include "audio_files.h"
#include "common.h"
#include "midi_pitches.h"

CLI::App *AutoTuneCommand::CreateCommandCLI(CLI::App &app) {
    auto auto_tune = app.add_subcommand(
        "auto-tune",
        "Tunes the file(s) to their nearest detected musical pitch. For example, a file with a detected pitch of 450Hz will be tuned to 440Hz (A4). The whole audio is analysed, and the most frequent and prominent pitch is determined. The whole audio is then retuned as if by using Signet's tune command (i.e. sped up or slowed down). This command works surprising well for almost any type of sample - transparently shifting it by the smallest amount possible to be more musically in-tune.");

    auto_tune->footer(R"aa(Examples:
  signet file.wav auto-tune
  signet sample-* auto-tune --sample-sets ".*(close|room|ambient).*" "close"
  signet sample-*.wav auto-tune --authority-file "sample-close"
  signet piano-root-*-*.wav auto-tune --expected-note "piano-root-(\d+)-.*")aa");

    m_identical_processing_set.AddCli(*auto_tune);
    m_expected_midi_pitch.AddCli(*auto_tune, true);

    return auto_tune;
}

void AutoTuneCommand::ProcessFiles(AudioFiles &files) {
    auto ExpectedNoteIsValid = [this](MIDIPitch target_midi_pitch, EditTrackedAudioFile &f) {
        if (const auto expected_midi_note = m_expected_midi_pitch.GetExpectedMidiPitch(GetName(), f)) {
            const auto target_note = target_midi_pitch.midi_note % 12;
            const auto expected_note = expected_midi_note->midi_note % 12;
            if (target_note != expected_note) {
                WarningWithNewLine(
                    GetName(), f,
                    "Failed to auto-tune the file because the detected target pitch is {}, while the --expected-note is {}",
                    target_midi_pitch.ToString(), expected_midi_note->ToString(),
                    g_note_names[expected_note]);
                return false;
            }
        }
        return true;
    };

    if (!m_identical_processing_set.ShouldProcessInSets()) {
        for (auto &f : files) {
            if (const auto pitch = f.GetAudio().DetectPitch()) {
                const auto closest_musical_note = FindClosestMidiPitch(*pitch);
                if (ExpectedNoteIsValid(closest_musical_note, f)) {
                    const double cents = GetCentsDifference(*pitch, closest_musical_note.pitch);
                    if (std::abs(cents) < 1) {
                        MessageWithNewLine(GetName(), f, "Sample is already in tune: {}",
                                           closest_musical_note.ToString());
                        continue;
                    }
                    MessageWithNewLine(GetName(), f, "Changing pitch by {:.2f} cents", cents);
                    f.GetWritableAudio().ChangePitch(cents);
                }
            } else {
                WarningWithNewLine(GetName(), f, "No pitch could be found");
            }
        }
    } else {
        m_identical_processing_set.ProcessSets(
            files, GetName(),
            [&](EditTrackedAudioFile *authority_file, const std::vector<EditTrackedAudioFile *> &set) {
                if (const auto pitch = authority_file->GetAudio().DetectPitch()) {
                    const auto closest_musical_note = FindClosestMidiPitch(*pitch);
                    if (ExpectedNoteIsValid(closest_musical_note, *authority_file)) {
                        const double cents = GetCentsDifference(*pitch, closest_musical_note.pitch);
                        if (std::abs(cents) < 1) {
                            MessageWithNewLine(GetName(), *authority_file,
                                               "Sample set is already in tune - {}",
                                               closest_musical_note.ToString());
                            return;
                        }
                        MessageWithNewLine(GetName(), *authority_file,
                                           "Sample set changing pitch by {:.2f} cents", cents);

                        for (auto &f : set) {
                            f->GetWritableAudio().ChangePitch(cents);
                        }
                    }
                } else {
                    WarningWithNewLine(GetName(), *authority_file, "No pitch could be found for sample set");
                }
            });
    }
}
