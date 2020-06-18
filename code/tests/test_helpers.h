#pragma once
#include <cmath>

#include "CLI11.hpp"
#include "audio_file.h"
#include "edit/subcommand.h"
#include "string_utils.h"

namespace TestHelpers {

AudioFile CreateSingleOscillationSineWave(const unsigned num_channels,
                                          const unsigned sample_rate,
                                          const size_t num_frames);
AudioFile CreateSineWaveAtFrequency(const unsigned num_channels,
                                    const unsigned sample_rate,
                                    const double length_seconds,
                                    const double frequency_hz);
AudioFile CreateSquareWaveAtFrequency(const unsigned num_channels,
                                      const unsigned sample_rate,
                                      const double length_seconds,
                                      const double frequency_hz);

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

class TestSubcommandProcessor : public SubcommandHost {
  public:
    template <typename SubcommandType>
    static TestSubcommandProcessor
    Run(const std::string_view subcommand_and_args_string, const AudioFile &buf, const fs::path path) {
        std::string whole_args = "signet-edit " + std::string(subcommand_and_args_string);
        CAPTURE(whole_args);
        const auto args = TestHelpers::StringToArgs {whole_args};

        SubcommandType subcommand {};
        CLI::App app;
        subcommand.CreateSubcommandCLI(app);
        app.parse(args.Size(), args.Args());

        TestSubcommandProcessor processor(subcommand, buf, path);
        return processor;
    }

    void ProcessAllFiles(Subcommand &subcommand) override {
        const auto filename = GetJustFilenameWithNoExtension(m_path);
        m_processed = subcommand.ProcessAudio(m_buf, filename);
        m_processed_path = subcommand.ProcessFilename(m_path, m_buf);
    }
    bool IsProcessingMultipleFiles() const override { return false; }

    std::optional<AudioFile> GetBuf() const {
        if (m_processed) return m_buf;
        return {};
    }

    std::optional<std::string> GetFilename() const {
        if (m_processed_path) {
            return GetJustFilenameWithNoExtension(m_path);
        }
        return {};
    }

    std::optional<std::string> GetPath() const {
        if (m_processed_path) return m_path.generic_string();
        return {};
    }

  private:
    TestSubcommandProcessor(Subcommand &subcommand, const AudioFile &buf, const fs::path &path)
        : m_buf(buf), m_path(path) {
        subcommand.Run(*this);
    }

    bool m_processed {false};
    bool m_processed_path {false};
    AudioFile m_buf {};
    fs::path m_path;
};

template <typename SubcommandType>
std::optional<AudioFile> ProcessBufferWithSubcommand(const std::string_view subcommand_and_args_string,
                                                     const AudioFile &buf) {
    return TestSubcommandProcessor::Run<SubcommandType>(subcommand_and_args_string, buf, "test.wav").GetBuf();
}

template <typename SubcommandType>
std::optional<std::string> ProcessFilenameWithSubcommand(const std::string_view subcommand_and_args_string,
                                                         const AudioFile &buf,
                                                         const fs::path path) {
    return TestSubcommandProcessor::Run<SubcommandType>(subcommand_and_args_string, buf, path).GetFilename();
}

template <typename SubcommandType>
std::optional<std::string> ProcessPathWithSubcommand(const std::string_view subcommand_and_args_string,
                                                     const AudioFile &buf,
                                                     const fs::path path) {
    return TestSubcommandProcessor::Run<SubcommandType>(subcommand_and_args_string, buf, path).GetPath();
}

} // namespace TestHelpers
