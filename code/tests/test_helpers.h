#pragma once
#include <cmath>

#include "CLI11.hpp"
#include "doctest.hpp"

#include "audio_file_io.h"
#include "audio_files.h"
#include "backup.h"
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

struct DataAndPath {
    AudioData data;
    fs::path path;
};

class TestSubcommandProcessor {
  public:
    template <typename SubcommandType>
    static TestSubcommandProcessor Run(const std::string_view subcommand_and_args_string,
                                       const tcb::span<DataAndPath> files) {
        std::string whole_args = "signet-edit " + std::string(subcommand_and_args_string);
        CAPTURE(whole_args);
        const auto args = TestHelpers::StringToArgs {whole_args};

        SubcommandType subcommand {};
        CLI::App app;
        subcommand.CreateSubcommandCLI(app);
        app.parse(args.Size(), args.Args());

        TestSubcommandProcessor processor(subcommand, files);
        return processor;
    }

    std::vector<std::optional<AudioData>> GetBufs() {
        std::vector<std::optional<AudioData>> result;
        for (auto &f : *m_files) {
            if (f.AudioChanged())
                result.push_back(f.GetAudio());
            else
                result.push_back({});
        }
        return result;
    }

    std::vector<std::optional<std::string>> GetFilenames() {
        std::vector<std::optional<std::string>> result;
        for (auto &f : *m_files) {
            if (f.PathChanged())
                result.push_back(GetJustFilenameWithNoExtension(f.GetPath()));
            else
                result.push_back({});
        }
        return result;
    }

    std::vector<std::optional<std::string>> GetPaths() {
        std::vector<std::optional<std::string>> result;
        for (auto &f : *m_files) {
            if (f.PathChanged())
                result.push_back(f.GetPath().generic_string());
            else
                result.push_back({});
        }
        return result;
    }

    std::optional<AudioData> GetBuf() {
        REQUIRE(m_files->Size() == 1);
        return GetBufs()[0];
    }
    std::optional<std::string> GetFilename() {
        REQUIRE(m_files->Size() == 1);
        return GetFilenames()[0];
    }
    std::optional<std::string> GetPath() {
        REQUIRE(m_files->Size() == 1);
        return GetPaths()[0];
    }

  private:
    TestSubcommandProcessor(Subcommand &subcommand, const tcb::span<DataAndPath> files_and_data) {
        std::vector<EditTrackedAudioFile> files;
        for (auto &fd : files_and_data) {
            files.push_back(fd.path);
            files.back().SetAudioData(fd.data);
        }
        m_files.emplace(files);

        subcommand.ProcessFiles(*m_files);
        SignetBackup backup;
        subcommand.GenerateFiles(*m_files, backup);
    }

    std::optional<AudioFiles> m_files;
};

template <typename SubcommandType>
std::optional<AudioData> ProcessBufferWithSubcommand(const std::string_view subcommand_and_args_string,
                                                     const AudioData &buf,
                                                     fs::path fake_file_name = "test.wav") {
    DataAndPath d {buf, fake_file_name};
    tcb::span<DataAndPath> bufs {&d, 1};
    return TestSubcommandProcessor::Run<SubcommandType>(subcommand_and_args_string, bufs).GetBuf();
}

template <typename SubcommandType>
std::optional<std::string> ProcessFilenameWithSubcommand(const std::string_view subcommand_and_args_string,
                                                         const AudioData &buf,
                                                         const fs::path path) {
    DataAndPath d {buf, path};
    tcb::span<DataAndPath> bufs {&d, 1};
    return TestSubcommandProcessor::Run<SubcommandType>(subcommand_and_args_string, bufs).GetFilename();
}

template <typename SubcommandType>
std::optional<std::string> ProcessPathWithSubcommand(const std::string_view subcommand_and_args_string,
                                                     const AudioData &buf,
                                                     const fs::path path) {
    DataAndPath d {buf, path};
    tcb::span<DataAndPath> bufs {&d, 1};
    return TestSubcommandProcessor::Run<SubcommandType>(subcommand_and_args_string, bufs).GetPath();
}

//

template <typename SubcommandType>
auto ProcessBuffersWithSubcommand(const std::string_view subcommand_and_args_string,
                                  std::vector<AudioData> bufs,
                                  std::vector<fs::path> paths = {}) {
    std::vector<DataAndPath> files;
    if (paths.size() == 0) {
        for (const auto &b : bufs) {
            files.push_back({b, "test.wav"});
        }
    } else {
        assert(bufs.size() == paths.size());
        for (usize i = 0; i < bufs.size(); ++i) {
            files.push_back({bufs[i], paths[i]});
        }
    }
    return TestSubcommandProcessor::Run<SubcommandType>(subcommand_and_args_string, files).GetBufs();
}
template <typename SubcommandType>
auto ProcessFilenamesWithSubcommand(const std::string_view subcommand_and_args_string,
                                    std::vector<DataAndPath> files) {
    return TestSubcommandProcessor::Run<SubcommandType>(subcommand_and_args_string, files).GetFilenames();
}

template <typename SubcommandType>
auto ProcessPathsWithSubcommand(const std::string_view subcommand_and_args_string,
                                std::vector<DataAndPath> files) {
    return TestSubcommandProcessor::Run<SubcommandType>(subcommand_and_args_string, files).GetPaths();
}

} // namespace TestHelpers
