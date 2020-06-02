#pragma once

#include "audio_file.h"
#include "signet_interface.h"

class Converter final : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    bool Process(AudioFile &input) override;
    void Run(SignetInterface &signet) override { signet.ProcessAllFiles(*this); }

  private:
    unsigned m_sample_rate {};
    unsigned m_bit_depth {};
};
