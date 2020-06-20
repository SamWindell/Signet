#pragma once
#include <optional>
#include <vector>

#include "CLI11_Fwd.hpp"
#include "filesystem.hpp"
#include "span.hpp"

#include "input_files.h"

struct AudioData;
class SignetBackup;

class Subcommand {
  public:
    virtual ~Subcommand() {}
    virtual CLI::App *CreateSubcommandCLI(CLI::App &app) = 0;

    virtual void GenerateFiles(const tcb::span<const InputAudioFile> files, SignetBackup &backup) {}
    virtual void ProcessFiles(const tcb::span<InputAudioFile> files) {}
};
