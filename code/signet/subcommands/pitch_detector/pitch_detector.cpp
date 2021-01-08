#include "pitch_detector.h"

#include "CLI11.hpp"
#include "doctest.hpp"
#include "dywapitchtrack/dywapitchtrack.h"

#include "audio_file.h"
#include "common.h"
#include "input_files.h"
#include "midi_pitches.h"
#include "subcommands/normaliser/gain_calculators.h"
#include "subcommands/tuner/tuner.h"

CLI::App *PitchDetector::CreateSubcommandCLI(CLI::App &app) {
    auto pitch_detector =
        app.add_subcommand("detect-pitch", "Pitch-detector: prints out the detected pitch of the file(s).");
    return pitch_detector;
}

std::vector<double> MixDownToMono(const AudioData &audio) {
    std::vector<double> mono_signal;
    mono_signal.reserve(audio.NumFrames());
    for (usize frame = 0; frame < audio.NumFrames(); ++frame) {
        double v = 0;
        for (unsigned chan = 0; chan < audio.num_channels; ++chan) {
            v += audio.GetSample(chan, frame);
        }
        mono_signal.push_back(v);
    }
    return mono_signal;
}

std::optional<double> DetectSinglePitch(const AudioData &audio) {
    auto mono_signal = MixDownToMono(audio);
    NormaliseToTarget(mono_signal, 1);

    struct ChunkData {
        double detected_pitch {};
        double rms {};
        double suitability {};
    };
    constexpr auto chunk_seconds = 0.1;

    std::vector<ChunkData> chunks;
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
        chunks.push_back({detected_pitch, GetRMS({mono_signal.data() + frame, (usize)chunk_size}), 0});
    }

    for (auto &chunk : chunks) {
        const auto p1 = chunk.detected_pitch;

        for (const auto &other_c : chunks) {
            const auto p2 = other_c.detected_pitch;
            if (p2 == 0) continue;

            const auto GaussianFunction = [](const auto x) {
                constexpr auto height = 10;
                constexpr auto peak_centre = 0;
                constexpr auto width = 0.9;
                return height * std::exp(-(std::pow(x - peak_centre, 2) / (2 * std::pow(width, 2))));
            };

            const auto pitch_delta = p2 - p1;
            chunk.suitability += GaussianFunction(pitch_delta);
        }
    }

    // Make chunks that contain louder audio a little bit more important
    {
        double max_rms = 0;
        double min_rms = DBL_MAX;
        for (auto &chunk : chunks) {
            REQUIRE(chunk.rms >= 0);
            if (chunk.rms < min_rms) min_rms = chunk.rms;
            if (chunk.rms > max_rms) max_rms = chunk.rms;
        }

        for (auto &chunk : chunks) {
            if ((max_rms - min_rms) == 0) continue;
            const auto rms_relative = (chunk.rms - min_rms) / (max_rms - min_rms);
            REQUIRE(rms_relative >= 0);
            REQUIRE(rms_relative <= 1);
            constexpr auto multiplier_for_loudest_chunk = 1.5;
            chunk.suitability *=
                1 + (std::cos(half_pi - (rms_relative * half_pi)) * multiplier_for_loudest_chunk);
        }
    }

    const ChunkData *most_suitable_chunk = &chunks[0];
    for (const auto &c : chunks) {
        if (c.suitability > most_suitable_chunk->suitability) {
            most_suitable_chunk = &c;
        }
    }

    if (most_suitable_chunk->detected_pitch != 0.0) {
        return most_suitable_chunk->detected_pitch;
    }
    return {};
}

bool ApproxEqual(double a, double b, double epsilon) {
    return a > (b - epsilon / 2) && a < (b + epsilon / 2);
}

std::optional<double> PitchDetector::DetectPitch(const AudioData &audio) {
    // The pitch detection algorithm that we are using can get it wrong sometimes when the audio is very
    // high pitch or very low pitch. To help with this, we detect the pitch at different octaves and then
    // work out which one is giving us the best results.

    struct PitchedData {
        std::optional<double> detected_pitch {};
        double cents {};
        double suitability {};
    };

    std::vector<PitchedData> pitches;
    for (double cents = -2400; cents < 2400; cents += 1200) {
        AudioData pitched_audio = audio;
        Tuner::ChangePitch(pitched_audio, cents);
        pitches.push_back({DetectSinglePitch(pitched_audio), cents});
    }

    for (auto &p : pitches) {
        if (!p.detected_pitch) continue;
        for (const auto &p2 : pitches) {
            if (!p2.detected_pitch) continue;
            const auto delta_cents = p2.cents - p.cents;
            const auto expected_hz = GetFreqWithCentDifference(*p.detected_pitch, delta_cents);
            if (ApproxEqual(expected_hz, *p2.detected_pitch, 3)) {
                p.suitability += 1;
            }
        }
    }

    const PitchedData *most_suitable = &pitches[0];
    for (const auto &p : pitches) {
        if (p.suitability > most_suitable->suitability) {
            most_suitable = &p;
        }
    }

    if (!most_suitable->detected_pitch) {
        return {};
    }

    return GetFreqWithCentDifference(*most_suitable->detected_pitch, -most_suitable->cents);
}

void PitchDetector::ProcessFiles(const tcb::span<EditTrackedAudioFile> files) {
    for (auto &f : files) {
        const auto pitch = DetectPitch(f.GetAudio());
        if (pitch) {
            const auto closest_musical_note = FindClosestMidiPitch(*pitch);

            std::array<char, 16> cents_diff;
            snprintf(cents_diff.data(), cents_diff.size(), "%.1f",
                     GetCentsDifference(closest_musical_note.pitch, *pitch));

            MessageWithNewLine(GetName(), "{} detected pitch {} Hz ({} cents from {}, MIDI {})", f.filename,
                               *pitch, cents_diff.data(), closest_musical_note.name,
                               closest_musical_note.midi_note);
        } else {
            MessageWithNewLine(GetName(), "No pitch could be found");
        }
    }
}
