#pragma once
#include <cmath>

#include "CLI11.hpp"
#include "audio_file.h"

namespace TestHelpers {

AudioFile CreateSingleOscillationSineWave(const unsigned num_channels,
                                          const unsigned sample_rate,
                                          const size_t num_frames);
AudioFile CreateSineWaveAtFrequency(const unsigned num_channels,
                                    const unsigned sample_rate,
                                    const float length_seconds,
                                    const float frequency_hz);
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

    Subcommand subcommand {};
    CLI::App app;
    subcommand.CreateSubcommandCLI(app);
    REQUIRE_NOTHROW(app.parse(args.Size(), args.Args()));

    ghc::filesystem::path filename {};
    return subcommand.Process(buf, filename);
}

} // namespace TestHelpers
