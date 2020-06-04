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

    // TODO: do this for all channels and use the average pitch
    std::vector<double> channel_buffer;
    channel_buffer.reserve(input.NumFrames());
    for (size_t frame = 0; frame < input.NumFrames(); ++frame) {
        channel_buffer.push_back(input.interleaved_samples[frame * input.num_channels + 0]);
    }

    dywapitchtracker pitch_tracker;
    dywapitch_inittracking(&pitch_tracker);

    auto detected_pitch =
        dywapitch_computepitch(&pitch_tracker, channel_buffer.data(), 0, (int)channel_buffer.size());
    if (input.sample_rate != 44100) {
        detected_pitch *= static_cast<double>(input.sample_rate) / 44100.0;
    }

    if (detected_pitch != 0) {
        const auto closest_musical_note = FindClosestMidiPitch(detected_pitch);
        MessageWithNewLine("Pitch-Dectector", "Detected a pitch of ", detected_pitch,
                           "Hz, the closest note is ", closest_musical_note.name, " (MIDI number ",
                           closest_musical_note.midi_note, "), which has a pitch of ",
                           closest_musical_note.pitch);
    } else {
        MessageWithNewLine("Pitch-Dectector", "No pitch could be found");
    }
    return false;
}
