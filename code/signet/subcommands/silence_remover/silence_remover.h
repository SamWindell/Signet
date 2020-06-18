#pragma once

#include "subcommand.h"

class SilenceRemover final : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    bool ProcessAudio(AudioFile &input, const std::string_view filename) override;
    void Run(SubcommandHost &processor) override { processor.ProcessAllFiles(*this); }

  private:
    enum class Region { Start, End, Both };
    float m_silence_threshold_db = -90;
    Region m_region {Region::Both};
};
