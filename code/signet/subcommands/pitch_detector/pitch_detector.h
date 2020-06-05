#pragma once

#include "subcommand.h"

class PitchDetector final : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    bool Process(AudioFile &input) override;
    void Run(SubcommandProcessor &processor) override { processor.ProcessAllFiles(*this); }

    static std::optional<double> DetectPitch(const AudioFile &input);
};
