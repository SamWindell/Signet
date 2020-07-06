#include "pitch_detector.h"

#include "CLI11.hpp"
#include "dywapitchtrack/dywapitchtrack.h"

#include "audio_file.h"
#include "common.h"
#include "input_files.h"
#include "midi_pitches.h"
#include "subcommands/normaliser/gain_calculators.h"

CLI::App *PitchDetector::CreateSubcommandCLI(CLI::App &app) {
    auto pitch_detector =
        app.add_subcommand("detect-pitch", "Pitch-detector: prints out the detected pitch of the file(s).");
    return pitch_detector;
}

std::optional<double> PitchDetector::DetectPitch(const AudioData &audio) {
    std::vector<double> mono_signal;
    mono_signal.reserve(audio.NumFrames());
    for (usize frame = 0; frame < audio.NumFrames(); ++frame) {
        double v = 0;
        for (unsigned chan = 0; chan < audio.num_channels; ++chan) {
            v += audio.GetSample(chan, frame);
        }
        mono_signal.push_back(v);
    }
    NormaliseToTarget(mono_signal, 1);

    std::vector<double> pitches;
    constexpr auto chunk_seconds = 0.1;
    const auto chunk_frames = (usize)(chunk_seconds * audio.sample_rate);
    for (usize frame = 0; frame < audio.NumFrames(); frame += chunk_frames) {
        const auto chunk_size = (int)std::min(chunk_frames, audio.NumFrames() - frame);

        dywapitchtracker pitch_tracker;
        dywapitch_inittracking(&pitch_tracker);
        auto detected_pitch = dywapitch_computepitch(&pitch_tracker, const_cast<double *>(mono_signal.data()),
                                                     (int)frame, chunk_size);
        if (audio.sample_rate != 44100) {
            detected_pitch *= static_cast<double>(audio.sample_rate) / 44100.0;
        }
        pitches.push_back(detected_pitch);
    }

    constexpr double epsilon = 0.8;
    usize max_num_matches = 0;
    usize index_with_max_num_matches = 0;
    for (usize i = 0; i < pitches.size(); ++i) {
        const auto p1 = pitches[i];

        usize num_matches = 0;
        for (const auto &p2 : pitches) {
            if (p2 == 0) continue;
            if (p1 > (p2 - epsilon) && p1 < (p2 + epsilon)) {
                num_matches++;
            }
        }

        if (num_matches > max_num_matches) {
            max_num_matches = num_matches;
            index_with_max_num_matches = i;
        }
    }

    if (pitches[index_with_max_num_matches] != 0.0) {
        return pitches[index_with_max_num_matches];
    }
    return {};
}

void PitchDetector::ProcessFiles(const tcb::span<EditTrackedAudioFile> files) {
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
