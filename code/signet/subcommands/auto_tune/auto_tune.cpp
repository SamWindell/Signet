#include "auto_tune.h"

#include "CLI11.hpp"
#include "doctest.hpp"

#include "audio_file_io.h"
#include "audio_files.h"
#include "common.h"
#include "midi_pitches.h"
#include "subcommands/detect_pitch/detect_pitch.h"
#include "subcommands/tune/tune.h"

CLI::App *AutoTuneSubcommand::CreateSubcommandCLI(CLI::App &app) {
    auto auto_tune = app.add_subcommand(
        "auto-tune", "Auto-tune: tunes the file(s) to their nearest detected musical pitch. For example, a "
                     "file with a detected pitch of 450Hz will be tuned to 440Hz (A4).");
    return auto_tune;
}

void AutoTuneSubcommand::ProcessFiles(AudioFiles &files) {
    for (auto &f : files) {
        if (const auto pitch = PitchDetector::DetectPitch(f.GetAudio())) {
            const auto closest_musical_note = FindClosestMidiPitch(*pitch);
            const double cents = GetCentsDifference(*pitch, closest_musical_note.pitch);
            if (std::abs(cents) < 1) {
                MessageWithNewLine(GetName(), "Sample is already in tune: {}",
                                   closest_musical_note.ToString());
                continue;
            }
            MessageWithNewLine(GetName(), "Changing pitch from {} to {}", *pitch,
                               closest_musical_note.ToString());
            Tuner::ChangePitch(f.GetWritableAudio(), cents);
        } else {
            MessageWithNewLine(GetName(), "No pitch could be found");
        }
    }
}
