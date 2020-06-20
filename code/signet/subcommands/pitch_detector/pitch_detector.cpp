#include "pitch_detector.h"

#include "CLI11.hpp"
#include "dywapitchtrack/dywapitchtrack.h"

#include "audio_file.h"
#include "common.h"
#include "input_files.h"
#include "midi_pitches.h"

CLI::App *PitchDetector::CreateSubcommandCLI(CLI::App &app) {
    auto pitch_detector =
        app.add_subcommand("detect-pitch", "Pitch-detector: prints out the detected pitch of the file(s).");
    return pitch_detector;
}

std::optional<double> PitchDetector::DetectPitch(const AudioData &audio) {
    std::vector<double> channel_pitches;
    ForEachDeinterleavedChannel(
        audio.interleaved_samples, audio.num_channels, [&](const auto &channel_buffer, auto channel) {
            dywapitchtracker pitch_tracker;
            dywapitch_inittracking(&pitch_tracker);

            auto detected_pitch = dywapitch_computepitch(
                &pitch_tracker, const_cast<double *>(channel_buffer.data()), 0, (int)channel_buffer.size());
            if (audio.sample_rate != 44100) {
                detected_pitch *= static_cast<double>(audio.sample_rate) / 44100.0;
            }
            channel_pitches.push_back(detected_pitch);
        });

    double average_pitch = 0;
    for (const auto detected_pitch : channel_pitches) {
        if (detected_pitch == 0) {
            average_pitch = 0;
            break;
        }
        average_pitch += detected_pitch;
    }
    average_pitch /= channel_pitches.size();

    if (average_pitch) {
        return average_pitch;
    } else {
        return {};
    }
}

void PitchDetector::ProcessFiles(const tcb::span<InputAudioFile> files) {
    for (auto &f : files) {
        const auto pitch = DetectPitch(f.GetAudio());
        if (pitch) {
            const auto closest_musical_note = FindClosestMidiPitch(*pitch);
            MessageWithNewLine("Pitch-Dectector", f.filename, " detected a pitch of ", *pitch,
                               "Hz, this has a difference of ",
                               GetCentsDifference(*pitch, closest_musical_note.pitch),
                               " cents off of the closest note ", closest_musical_note.ToString());
        } else {
            MessageWithNewLine("Pitch-Dectector", "No pitch could be found");
        }
    }
}
