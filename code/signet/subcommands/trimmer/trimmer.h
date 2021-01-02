#pragma once

#include <optional>

#include "audio_duration.h"
#include "subcommand.h"

class Trimmer final : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    void ProcessFiles(const tcb::span<EditTrackedAudioFile> files) override;
    std::string GetName() const override { return "Trimmer"; }

  private:
    std::optional<AudioDuration> m_start_duration;
    std::optional<AudioDuration> m_end_duration;
};
