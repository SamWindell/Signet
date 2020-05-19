#pragma once

#include <optional>

#include "CLI11.hpp"
#include "common.h"
#include "filesystem.hpp"

struct AudioFile;
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

    int Main(const int argc, const char *argv[]);
    void ProcessAllFiles();
    bool IsProcessingMultipleFiles() const { return ghc::filesystem::is_directory(m_input_filepath); }

  private:
    void ProcessFile(const ghc::filesystem::path input_filepath, ghc::filesystem::path output_filepath);

    Processor &m_processor;
    bool m_delete_input_files = false;
    ghc::filesystem::path m_input_filepath;
    ghc::filesystem::path m_output_filepath;
    bool m_recursive_directory_search = false;
};
