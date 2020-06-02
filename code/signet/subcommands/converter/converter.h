#pragma once

#include "audio_file.h"
#include "signet_interface.h"

class Converter final : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    void Run(SubcommandProcessor &processor) override;
    bool Process(AudioFile &input) override;

  private:
    enum class Mode {
        ValidatingCorrectFormat,
        Converting,
    };

    bool m_files_can_be_converted {};
    Mode m_mode {Mode::ValidatingCorrectFormat};
    unsigned m_sample_rate {};
    unsigned m_bit_depth {};
};
