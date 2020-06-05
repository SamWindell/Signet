#pragma once

#include <optional>

#include "audio_duration.h"
#include "subcommand.h"

class Trimmer final : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    bool Process(AudioFile &input) override;
    void Run(SubcommandProcessor &processor) override { processor.ProcessAllFiles(*this); }

  private:
    std::optional<AudioDuration> m_start_duration;
    std::optional<AudioDuration> m_end_duration;
};