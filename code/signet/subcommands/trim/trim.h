#pragma once

#include <optional>

#include "audio_duration.h"
#include "subcommand.h"

class TrimCommand final : public Command {
  public:
    CLI::App *CreateCommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
    std::string GetName() const override { return "Trim"; }

  private:
    std::optional<AudioDuration> m_start_duration;
    std::optional<AudioDuration> m_end_duration;
};
