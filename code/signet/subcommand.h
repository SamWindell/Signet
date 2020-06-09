#pragma once
#include <optional>

#include "CLI11.hpp"
#include "filesystem.hpp"

struct AudioFile;

class Subcommand;

class SubcommandProcessor {
  public:
    virtual ~SubcommandProcessor() {}
    virtual void ProcessAllFiles(Subcommand &subcommand) = 0;
    virtual bool IsProcessingMultipleFiles() const = 0;
};

class Subcommand {
  public:
    virtual ~Subcommand() {}
    virtual CLI::App *CreateSubcommandCLI(CLI::App &app) = 0;
    virtual void Run(SubcommandProcessor &) = 0;
    virtual bool ProcessAudio(AudioFile &input, const std::string_view filename) { return false; };
    virtual bool ProcessFilename(fs::path &path, const AudioFile &input) { return false; };
};
