#include "seamless_looper.h"

#include "CLI11.hpp"

#include "subcommands/fader/fader.h"

CLI::App *SeamlessLooper::CreateSubcommandCLI(CLI::App &app) {
    auto looper = app.add_subcommand(
        "seamless-loop",
        "Turns the files(s) into seamless loops by crossfading a given percentage of audio from the start of the file to the end of the file. Due to this overlap, the resulting file is shorter.");
    looper
        ->add_option("crossfade-percent", m_crossfade_percent,
                     "The size of the crossfade region as a percent of the whole file.")
        ->check(CLI::Range(0, 100));
    return looper;
}

void SeamlessLooper::ProcessFiles(AudioFiles &files) {
    for (auto &f : files) {
        const auto num_frames = f.GetAudio().NumFrames();
        const auto num_xfade_frames = usize(num_frames * (m_crossfade_percent / 100.0));
        if (num_frames < num_xfade_frames || num_xfade_frames == 0) {
            WarningWithNewLine(
                GetName(),
                "Cannot make the file a seamless loop because the file or crossfade-region are too small. Number of frames in the file: {}, number of frames in the crossfade-region: {}. File: {}",
                num_frames, num_xfade_frames, f.GetPath());
            continue;
        }
        auto &audio = f.GetWritableAudio();
        Fader::PerformFade(audio, 0, num_xfade_frames, Fader::Shape::Sine);
        Fader::PerformFade(audio, num_frames - 1, (num_frames - 1) - num_xfade_frames, Fader::Shape::Sine);
        for (usize i = 0; i < num_xfade_frames; ++i) {
            auto write_index = (num_frames - 1) - num_xfade_frames + i;
            for (unsigned chan = 0; chan < audio.num_channels; ++chan) {
                audio.GetSample(chan, write_index) += audio.GetSample(chan, i);
            }
        }
        auto &samples = audio.interleaved_samples;
        samples.erase(samples.begin(), samples.begin() + num_xfade_frames * audio.num_channels);
        audio.FramesWereRemovedFromStart(num_xfade_frames);
    }
}
