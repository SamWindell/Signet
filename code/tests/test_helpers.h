#pragma once
#include <cmath>

#include "CLI11.hpp"

#include "audio_file.h"
#include "backup.h"
#include "input_files.h"
#include "string_utils.h"
#include "subcommand.h"

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
        m_file.file_edited = subcommand.ProcessAudio(m_file.file, m_file.filename);
        m_file.renamed = subcommand.ProcessFilename(m_file.path, m_file.file);
    }
    bool IsProcessingMultipleFiles() const override { return false; }

    std::optional<AudioFile> GetBuf() const {
        if (m_file.file_edited) return m_file.file;
        return {};
    }

    std::optional<std::string> GetFilename() const {
        if (m_file.renamed) {
            return GetJustFilenameWithNoExtension(m_file.path);
        }
        return {};
    }

    std::optional<std::string> GetPath() const {
        if (m_file.renamed) return m_file.path.generic_string();
        return {};
    }

  private:
    TestSubcommandProcessor(Subcommand &subcommand, const AudioFile &buf, const fs::path &path)
        : m_file(buf, path) {
        subcommand.Run(*this);

        SignetBackup backup;
        std::vector<InputAudioFile> vec {m_file};
        subcommand.GenerateFiles(vec, backup);
    }

    InputAudioFile m_file;
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
