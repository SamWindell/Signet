#pragma once

#include "audio_file_io.h"
#include "signet_interface.h"

class ConvertCommand final : public Command {
  public:
    CLI::App *CreateCommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
    std::string GetName() const override { return "Convert"; }

  private:
    bool m_files_can_be_converted {};
    std::optional<unsigned> m_sample_rate {};
    std::optional<unsigned> m_bit_depth {};
    std::optional<AudioFileFormat> m_file_format {};
};
