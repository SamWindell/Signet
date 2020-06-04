#pragma once

#include "subcommand.h"

class Tuner final : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    bool Process(AudioFile &input) override;
    void Run(SubcommandProcessor &processor) override { processor.ProcessAllFiles(*this); }

  private:
    double m_tune_cents {};
};
