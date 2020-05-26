#pragma once
#include <optional>

#include "CLI11.hpp"
#include "filesystem.hpp"

struct AudioFile;
class AudioUtilInterface;

class Processor {
  public:
    virtual std::optional<AudioFile> Process(const AudioFile &input,
                                             ghc::filesystem::path &output_filename) = 0;
    virtual void AddCLI(CLI::App &app) = 0;
    virtual void Run(AudioUtilInterface &) = 0;
};
