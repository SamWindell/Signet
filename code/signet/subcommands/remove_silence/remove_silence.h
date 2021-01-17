#pragma once

#include "subcommand.h"

class RemoveSilenceCommand final : public Command {
  public:
    CLI::App *CreateCommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
    std::string GetName() const override { return "RemoveSilence"; }

  private:
    enum class Region { Start, End, Both };
    float m_silence_threshold_db = -90;
    Region m_region {Region::Both};
};
