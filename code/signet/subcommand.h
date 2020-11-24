#pragma once
#include <optional>
#include <vector>

#include "CLI11_Fwd.hpp"
#include "filesystem.hpp"
#include "span.hpp"

#include "edit_tracked_audio_file.h"

class SignetBackup;

class Subcommand {
  public:
    virtual ~Subcommand() {}
    virtual CLI::App *CreateSubcommandCLI(CLI::App &app) = 0;
    virtual std::string GetName() = 0;

    virtual void GenerateFiles(const tcb::span<EditTrackedAudioFile> files, SignetBackup &backup) {}
    virtual void ProcessFiles(const tcb::span<EditTrackedAudioFile> files) {}
};
