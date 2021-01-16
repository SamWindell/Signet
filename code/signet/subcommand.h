#pragma once
#include <string>

#include "CLI11_Fwd.hpp"

#include "audio_files.h"

class SignetBackup;

class Subcommand {
  public:
    virtual ~Subcommand() {}
    virtual CLI::App *CreateSubcommandCLI(CLI::App &app) = 0;
    virtual std::string GetName() const = 0;

    virtual void GenerateFiles(AudioFiles &files, SignetBackup &backup) {}
    virtual void ProcessFiles(AudioFiles &files) {}
};
