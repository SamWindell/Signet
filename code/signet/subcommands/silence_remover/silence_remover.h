#pragma once

#include "subcommand.h"

class SilenceRemover final : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
    std::string GetName() const override { return "SilenceRemover"; }

  private:
    enum class Region { Start, End, Both };
    float m_silence_threshold_db = -90;
    Region m_region {Region::Both};
};
