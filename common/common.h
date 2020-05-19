#pragma once
#include <assert.h>
#include <functional>
#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

#include "CLI11.hpp"
#include "filesystem.hpp"

template <typename Arg, typename... Args>
void FatalErrorWithNewLine(Arg &&arg, Args &&... args) {
    std::cout << "ERROR: ";
    std::cout << std::forward<Arg>(arg);
    ((std::cout << std::forward<Args>(args)), ...);
    std::cout << "\n";
    exit(1);
}

template <typename Arg, typename... Args>
void WarningWithNewLine(Arg &&arg, Args &&... args) {
    std::cout << "WARNING: ";
    std::cout << std::forward<Arg>(arg);
    ((std::cout << std::forward<Args>(args)), ...);
    std::cout << "\n";
}

void ForEachAudioFileInDirectory(const std::string &directory,
                                 const bool recursive,
                                 std::function<void(const ghc::filesystem::path &)> callback);

struct AudioFile {
    size_t NumFrames() const { return interleaved_samples.size() / num_channels; }

    std::vector<float> interleaved_samples {};
    unsigned num_channels {};
    unsigned sample_rate {};
};

std::optional<AudioFile> ReadAudioFile(const ghc::filesystem::path &filename);
bool WriteWaveFile(const ghc::filesystem::path &filename, const AudioFile &audio_file);

float DBToAmp(float d);

//
//

class AudioUtilInterface;

class Processor {
  public:
    virtual std::optional<AudioFile> Process(const AudioFile &input,
                                             ghc::filesystem::path &output_filename) = 0;
    virtual void AddCLI(CLI::App &app) = 0;
    virtual void Run(AudioUtilInterface &) = 0;
    virtual std::string GetDescription() = 0;
};

class AudioUtilInterface {
  public:
    AudioUtilInterface(Processor &processor) : m_processor(processor) {}

    int Main(const int argc, const char *argv[]) {
        CLI::App app {m_processor.GetDescription()};

        app.add_option("input-file-or-directory", m_input_filepath, "The file or directory to read from")
            ->required()
            ->check(CLI::ExistingPath);
        app.add_option("output-wave-filename", m_output_filepath,
                       "The filename to write to - only relevant if the input file is not a directory");
        app.add_flag("-r,--recursive-directory-search", m_recursive_directory_search,
                     "Search for files recursively in the given directory");
        app.add_flag("-d,--delete-input-files", m_delete_input_files,
                     "Delete the input files if the new file was successfully written");
        m_processor.AddCLI(app);

        CLI11_PARSE(app, argc, argv);

        if (ghc::filesystem::is_directory(m_input_filepath)) {
            if (!m_output_filepath.empty()) {
                FatalErrorWithNewLine(
                    "the input path is a directory, there must be no output filepath - output "
                    "files will be placed adjacent to originals");
            }
        } else {
            if (m_recursive_directory_search) {
                WarningWithNewLine("input path is a file, ignoring the recursive flag");
            }
            if (ghc::filesystem::is_directory(m_output_filepath)) {
                FatalErrorWithNewLine(
                    "output filepath cannot be a directory if the input filepath is a file");
            }
        }

        m_processor.Run(*this);
        return 0;
    }

    void ProcessAllFiles() {
        if (IsProcessingMultipleFiles()) {
            ForEachAudioFileInDirectory(m_input_filepath, m_recursive_directory_search,
                                        [this](const ghc::filesystem::path &path) { ProcessFile(path, {}); });
        } else {
            ProcessFile(m_input_filepath, m_output_filepath);
        }
    }

    bool IsProcessingMultipleFiles() const { return ghc::filesystem::is_directory(m_input_filepath); }

  private:
    void ProcessFile(const ghc::filesystem::path input_filepath, ghc::filesystem::path output_filepath) {
        if (output_filepath.empty()) {
            output_filepath = input_filepath;
            if (!m_delete_input_files) {
                output_filepath.replace_extension();
                output_filepath += "(edited)";
            }
            output_filepath.replace_extension(".wav");
        }
        assert(!input_filepath.empty());

        if (const auto audio_file = ReadAudioFile(input_filepath)) {
            if (const auto new_audio_file = m_processor.Process(*audio_file, output_filepath)) {
                if (!WriteWaveFile(output_filepath, *new_audio_file)) {
                    FatalErrorWithNewLine("could not write the wave file ", output_filepath);
                }
                std::cout << "Successfully wrote file " << output_filepath << "\n";

                if (m_delete_input_files && input_filepath != output_filepath) {
                    std::cout << "Deleting file " << input_filepath << "\n";
                    ghc::filesystem::remove(input_filepath);
                }
            }
        }

        std::cout << "\n";
    }

    Processor &m_processor;
    bool m_delete_input_files = false;
    ghc::filesystem::path m_input_filepath;
    ghc::filesystem::path m_output_filepath;
    bool m_recursive_directory_search = false;
};
