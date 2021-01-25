#include "detect_pitch.h"

#include "CLI11.hpp"
#include "doctest.hpp"

#include "audio_files.h"
#include "common.h"
#include "midi_pitches.h"

CLI::App *DetectPitchCommand::CreateCommandCLI(CLI::App &app) {
    auto detect_pitch = app.add_subcommand("detect-pitch", "Prints out the detected pitch of the file(s).");
    return detect_pitch;
}

void DetectPitchCommand::ProcessFiles(AudioFiles &files) {
    for (auto &f : files) {
        const auto pitch = f.GetAudio().DetectPitch();
        if (pitch) {
            const auto closest_musical_note = FindClosestMidiPitch(*pitch);

            std::array<char, 16> cents_diff;
            snprintf(cents_diff.data(), cents_diff.size(), "%.1f",
                     GetCentsDifference(closest_musical_note.pitch, *pitch));

            MessageWithNewLine(GetName(), "{} detected pitch {} Hz ({} cents from {}, MIDI {})",
                               f.OriginalFilename(), *pitch, cents_diff.data(), closest_musical_note.name,
                               closest_musical_note.midi_note);
        } else {
            MessageWithNewLine(GetName(), "No pitch could be found");
        }
    }
}
