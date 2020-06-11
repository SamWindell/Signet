#include "auto_tuner.h"

#include "audio_file.h"
#include "common.h"
#include "doctest.hpp"
#include "midi_pitches.h"
#include "subcommands/pitch_detector/pitch_detector.h"
#include "subcommands/tuner/tuner.h"

CLI::App *AutoTuner::CreateSubcommandCLI(CLI::App &app) {
    auto auto_tuner = app.add_subcommand(
        "auto-tune", "Auto-tune: tunes the file(s) to their nearest detected musical pitch. For example, a "
                     "file with a detected pitch of 450Hz will be tuned to 440Hz (A4).");
    return auto_tuner;
}

bool AutoTuner::ProcessAudio(AudioFile &input, const std::string_view filename) {
    if (!input.interleaved_samples.size()) return false;

    if (const auto pitch = PitchDetector::DetectPitch(input)) {
        const auto closest_musical_note = FindClosestMidiPitch(*pitch);
        const double cents = GetCentsDifference(*pitch, closest_musical_note.pitch);
        if (std::abs(cents) < 1) {
            MessageWithNewLine("AutoTuner", "Sample is already in tune: ", closest_musical_note.ToString());
            return false;
        }
        MessageWithNewLine("AutoTuner", "Changing pitch from ", *pitch, " to ",
                           closest_musical_note.ToString());
        Tuner::ChangePitch(input, cents);
        return true;
    } else {
        MessageWithNewLine("AutoTuner", "No pitch could be found");
        return false;
    }
}
