#pragma once

#include "audio_file.h"
#include "signet_interface.h"

class Converter final : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    void ProcessFiles(const tcb::span<InputAudioFile> files) override;

    static void ConvertSampleRate(std::vector<double> &buffer,
                                  const unsigned num_channels,
                                  const double input_sample_rate,
                                  const double new_sample_rate);

  private:
    bool m_files_can_be_converted {};
    std::optional<unsigned> m_sample_rate {};
    std::optional<unsigned> m_bit_depth {};
    std::optional<AudioFileFormat> m_file_format {};
};
