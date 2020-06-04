#include "pitch_detector.h"

#include "dywapitchtrack/dywapitchtrack.h"

#include "audio_file.h"
#include "common.h"
#include "midi_pitches.h"

CLI::App *PitchDetector::CreateSubcommandCLI(CLI::App &app) {
    auto pitch_detector = app.add_subcommand("pitch-detect", "Prints out the detected pitch of the file");
    return pitch_detector;
}

bool PitchDetector::Process(AudioFile &input) {
    if (!input.interleaved_samples.size()) return false;

    dywapitchtracker pitch_tracker;
    dywapitch_inittracking(&pitch_tracker);

    auto detected_pitch = dywapitch_computepitch(&pitch_tracker, input.interleaved_samples.data(), 0,
                                                 (int)input.interleaved_samples.size());
    if (input.sample_rate != 44100) {
        detected_pitch *= static_cast<double>(input.sample_rate) / 44100.0;
    }

    if (detected_pitch != 0) {
        const auto closest_musical_note = FindClosestMidiPitch(detected_pitch);
        MessageWithNewLine("Pitch-Dectector", "Detected a pitch of ", detected_pitch,
                           "Hz, the closest note is ", closest_musical_note.name, " (MIDI number ",
                           closest_musical_note.midi_note, ")");
    } else {
        MessageWithNewLine("Pitch-Dectector", "No pitch could be found");
    }
    return false;
}
