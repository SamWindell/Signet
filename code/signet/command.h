#pragma once
#include <string>

#include "CLI11_Fwd.hpp"

#include "audio_files.h"

class SignetBackup;

class Command {
  public:
    virtual ~Command() {}
    virtual CLI::App *CreateCommandCLI(CLI::App &app) = 0;
    virtual std::string GetName() const = 0;

    virtual bool AllowsOutputFolder() const { return true; }
    virtual bool AllowsSingleOutputFile() const { return true; }

    virtual void GenerateFiles(AudioFiles &, SignetBackup &) {}
    virtual void ProcessFiles(AudioFiles &) {}
};
