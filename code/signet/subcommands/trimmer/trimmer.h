#pragma once

#include <optional>

#include "audio_duration.h"
#include "subcommand.h"

class Trimmer final : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
    std::string GetName() const override { return "Trimmer"; }

  private:
    std::optional<AudioDuration> m_start_duration;
    std::optional<AudioDuration> m_end_duration;
};
