#pragma once
#include <optional>

#include "CLI11.hpp"
#include "filesystem.hpp"

struct AudioFile;
class SignetInterface;

class Processor {
  public:
    virtual std::optional<AudioFile> Process(const AudioFile &input,
                                             ghc::filesystem::path &output_filename) = 0;
    virtual void AddCLI(CLI::App &app) = 0;
    virtual void Run(SignetInterface &) = 0;
};
