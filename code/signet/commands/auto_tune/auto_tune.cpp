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

    m_identical_processing_set.AddCli(*auto_tune);
    return auto_tune;
}

void AutoTuneCommand::ProcessFiles(AudioFiles &files) {
    if (!m_identical_processing_set.ShouldProcessInSets()) {
        for (auto &f : files) {
            if (const auto pitch = f.GetAudio().DetectPitch()) {
                const auto closest_musical_note = FindClosestMidiPitch(*pitch);
                const double cents = GetCentsDifference(*pitch, closest_musical_note.pitch);
                if (std::abs(cents) < 1) {
                    MessageWithNewLine(GetName(), f, "Sample is already in tune: {}",
                                       closest_musical_note.ToString());
                    continue;
                }
                MessageWithNewLine(GetName(), f, "Changing pitch from {} to {}", *pitch,
                                   closest_musical_note.ToString());
                f.GetWritableAudio().ChangePitch(cents);
            } else {
                MessageWithNewLine(GetName(), f, "No pitch could be found");
            }
        }
    } else {
        m_identical_processing_set.ProcessSets(
            files, GetName(),
            [this](EditTrackedAudioFile *authority_file, const std::vector<EditTrackedAudioFile *> &set) {
                if (const auto pitch = authority_file->GetAudio().DetectPitch()) {
                    const auto closest_musical_note = FindClosestMidiPitch(*pitch);
                    const double cents = GetCentsDifference(*pitch, closest_musical_note.pitch);
                    if (std::abs(cents) < 1) {
                        MessageWithNewLine(GetName(), {}, "Sample set is already in tune: {}",
                                           closest_musical_note.ToString());
                        return;
                    }
                    MessageWithNewLine(GetName(), *authority_file,
                                       "Sample set {}: changing pitch from {} to {}", *pitch,
                                       closest_musical_note.ToString());

                    for (auto &f : set) {
                        f->GetWritableAudio().ChangePitch(cents);
                    }
                } else {
                    MessageWithNewLine(GetName(), {}, "No pitch could be found for sample set: {}",
                                       authority_file->OriginalFilename());
                }
            });
    }
}
