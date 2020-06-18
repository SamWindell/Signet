#pragma once
#include <optional>

#include "CLI11.hpp"
#include "filesystem.hpp"

struct AudioFile;
struct InputAudioFile;
class SignetBackup;

class Subcommand;

class SubcommandHost {
  public:
    virtual ~SubcommandHost() {}
    virtual void ProcessAllFiles(Subcommand &subcommand) = 0;
    virtual bool IsProcessingMultipleFiles() const = 0;
};

class Subcommand {
  public:
    virtual ~Subcommand() {}
    virtual CLI::App *CreateSubcommandCLI(CLI::App &app) = 0;
    virtual void Run(SubcommandHost &) {}
    virtual void GenerateFiles(const std::vector<InputAudioFile> &files, SignetBackup &backup) {}

    virtual bool ProcessAudio(AudioFile &input, const std::string_view filename) { return false; };
    virtual bool ProcessFilename(fs::path &path, const AudioFile &input) { return false; };
};
