#pragma once

#include "audio_file.h"
#include "r8brain-resampler/CDSPResampler.h"
#include "signet_interface.h"

class Converter final : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    std::optional<AudioFile> Process(const AudioFile &input, ghc::filesystem::path &output_filename) override;
    void Run(SignetInterface &signet) override { signet.ProcessAllFiles(*this); }

  private:
    unsigned m_sample_rate {};
    unsigned m_bit_depth {};
};
