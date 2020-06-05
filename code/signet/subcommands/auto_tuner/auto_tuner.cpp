#include "auto_tuner.h"

#include "audio_file.h"
#include "common.h"
#include "doctest.hpp"
#include "midi_pitches.h"
#include "subcommands/pitch_detector/pitch_detector.h"
#include "subcommands/tuner/tuner.h"

CLI::App *AutoTuner::CreateSubcommandCLI(CLI::App &app) {
    auto auto_tuner = app.add_subcommand("auto-tune", "Tune the sample to near nearest detected pitch");
    return auto_tuner;
}

bool AutoTuner::Process(AudioFile &input) {
    if (!input.interleaved_samples.size()) return false;

    if (const auto pitch = PitchDetector::DetectPitch(input)) {
        const auto closest_musical_note = FindClosestMidiPitch(*pitch);

        constexpr double cents_in_octave = 100 * 12;
        double cents;
        if (*pitch <= closest_musical_note.pitch) {
            const auto ratio = closest_musical_note.pitch / *pitch;
            cents = std::sqrt(ratio) * cents_in_octave;
        } else {
            const auto ratio = *pitch / closest_musical_note.pitch;
            cents = -(std::sqrt(ratio) * cents_in_octave);
        }

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
