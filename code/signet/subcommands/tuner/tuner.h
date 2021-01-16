#pragma once

#include "audio_file_io.h"
#include "subcommand.h"

class Tuner final : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
    std::string GetName() const override { return "Tuner"; }

    static void ChangePitch(AudioData &audio, double cents);

  private:
    double m_tune_cents {};
};
