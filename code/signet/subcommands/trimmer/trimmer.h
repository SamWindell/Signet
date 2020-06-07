#pragma once

#include <optional>

#include "audio_duration.h"
#include "subcommand.h"

class Trimmer final : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    bool ProcessAudio(AudioFile &input, const std::string_view filename) override;
    void Run(SubcommandProcessor &processor) override { processor.ProcessAllFiles(*this); }
    bool ProcessesAudio() const override { return true; }

  private:
    std::optional<AudioDuration> m_start_duration;
    std::optional<AudioDuration> m_end_duration;
};
