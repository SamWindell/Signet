#pragma once
#include <cmath>

#include "CLI11.hpp"

#include "audio_file.h"
#include "backup.h"
#include "input_files.h"
#include "string_utils.h"
#include "subcommand.h"

namespace TestHelpers {

AudioData CreateSingleOscillationSineWave(const unsigned num_channels,
                                          const unsigned sample_rate,
                                          const size_t num_frames);
AudioData CreateSineWaveAtFrequency(const unsigned num_channels,
                                    const unsigned sample_rate,
                                    const double length_seconds,
                                    const double frequency_hz);
AudioData CreateSquareWaveAtFrequency(const unsigned num_channels,
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

class TestSubcommandProcessor {
  public:
    template <typename SubcommandType>
    static TestSubcommandProcessor
    Run(const std::string_view subcommand_and_args_string, const AudioData &buf, const fs::path path) {
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

    std::optional<AudioData> GetBuf() {
        if (m_file.AudioChanged()) return m_file.GetAudio();
        return {};
    }

    std::optional<std::string> GetFilename() {
        if (m_file.FilepathChanged()) {
            return GetJustFilenameWithNoExtension(m_file.GetPath());
        }
        return {};
    }

    std::optional<std::string> GetPath() {
        if (m_file.FilepathChanged()) return m_file.GetPath().generic_string();
        return {};
    }

  private:
    TestSubcommandProcessor(Subcommand &subcommand, const AudioData &buf, const fs::path &path)
        : m_file(path) {
        m_file.LoadAudioData(buf);

        tcb::span<EditTrackedAudioFile> vec {&m_file, 1};
        subcommand.ProcessFiles(vec);
        SignetBackup backup;
        subcommand.GenerateFiles(vec, backup);
    }

    EditTrackedAudioFile m_file;
};

template <typename SubcommandType>
std::optional<AudioData> ProcessBufferWithSubcommand(const std::string_view subcommand_and_args_string,
                                                     const AudioData &buf) {
    return TestSubcommandProcessor::Run<SubcommandType>(subcommand_and_args_string, buf, "test.wav").GetBuf();
}

template <typename SubcommandType>
std::optional<std::string> ProcessFilenameWithSubcommand(const std::string_view subcommand_and_args_string,
                                                         const AudioData &buf,
                                                         const fs::path path) {
    return TestSubcommandProcessor::Run<SubcommandType>(subcommand_and_args_string, buf, path).GetFilename();
}

template <typename SubcommandType>
std::optional<std::string> ProcessPathWithSubcommand(const std::string_view subcommand_and_args_string,
                                                     const AudioData &buf,
                                                     const fs::path path) {
    return TestSubcommandProcessor::Run<SubcommandType>(subcommand_and_args_string, buf, path).GetPath();
}

} // namespace TestHelpers
