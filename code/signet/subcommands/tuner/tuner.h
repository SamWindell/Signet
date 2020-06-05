#pragma once

#include "audio_file.h"
#include "subcommand.h"

class Tuner final : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    bool Process(AudioFile &input, const std::string_view filename) override;
    void Run(SubcommandProcessor &processor) override { processor.ProcessAllFiles(*this); }
    static void ChangePitch(AudioFile &input, double cents);

  private:
    double m_tune_cents {};
};
