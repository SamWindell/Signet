#pragma once
#include <cmath>

#include "audio_file.h"

namespace TestHelpers {

static AudioFile CreateSingleOscillationSineWave(const unsigned num_channels,
                                                 const unsigned sample_rate,
                                                 const size_t num_frames) {
    AudioFile buf;
    buf.num_channels = num_channels;
    buf.sample_rate = sample_rate;
    buf.interleaved_samples.resize(num_frames * num_channels);
    for (size_t frame = 0; frame < num_frames; ++frame) {
        for (unsigned channel = 0; channel < num_channels; ++channel) {
            constexpr float two_pi = 6.28318530718f;
            buf.GetSample(channel, frame) = std::sin(frame * (two_pi / num_frames));
        }
    }
    return buf;
}

static AudioFile CreateSineWaveAtFrequency(const unsigned num_channels,
                                           const unsigned sample_rate,
                                           const float length_seconds,
                                           const float frequency_hz) {
    const auto num_frames = (size_t)(length_seconds * sample_rate);
    const auto oscillations_per_sec = frequency_hz;
    const auto oscillations_in_whole = oscillations_per_sec * length_seconds;
    const auto taus_in_whole = oscillations_in_whole * 2 * pi;
    const auto taus_per_sample = taus_in_whole / num_frames;

    AudioFile buf;
    buf.num_channels = num_channels;
    buf.sample_rate = sample_rate;
    buf.interleaved_samples.resize(num_frames * num_channels);
    float phase = -pi * 2;
    for (size_t frame = 0; frame < num_frames; ++frame) {
        const float value = (float)std::sin(phase);
        phase += taus_per_sample;
        for (unsigned channel = 0; channel < num_channels; ++channel) {
            buf.GetSample(channel, frame) = value;
        }
    }
    return buf;
}

class StringToArgs {
  public:
    StringToArgs(std::string_view s) {
        size_t pos = 0;
        while ((pos = s.find(' ')) != std::string_view::npos) {
            args.push_back(std::string(s.substr(0, pos)));
            s.remove_prefix(pos + 1);
        }
        args.push_back(std::string(s));

        for (const auto &arg : args) {
            arg_ptrs.push_back(arg.data());
        }
    }

    const char *const *const Args() const { return arg_ptrs.data(); }
    int Size() const { return (int)args.size(); }

  private:
    std::vector<const char *> arg_ptrs;
    std::vector<std::string> args;
};

template <typename Subcommand>
std::optional<AudioFile> ProcessBufferWithSubcommand(const std::string_view subcommand_and_args_string,
                                                     const AudioFile &buf) {
    std::string whole_args = "signet-test " + std::string(subcommand_and_args_string);
    const auto args = TestHelpers::StringToArgs {whole_args};

    Subcommand subcommand;
    CLI::App app;
    subcommand.CreateSubcommandCLI(app);
    REQUIRE_NOTHROW(app.parse(args.Size(), args.Args()));

    ghc::filesystem::path filename {};
    return subcommand.Process(buf, filename);
}

} // namespace TestHelpers
