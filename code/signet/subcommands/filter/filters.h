#pragma once

#include "filter.h"
#include "subcommand.h"

void FilterProcessFiles(const tcb::span<EditTrackedAudioFile> files,
                        Filter::RBJType type,
                        double cutoff,
                        double Q,
                        double gain_db);

class HighpassCommand final : public Command {
  public:
    CLI::App *CreateCommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
    std::string GetName() const override { return "Highpass"; }

  private:
    double m_cutoff;
};

class LowpassCommand final : public Command {
  public:
    CLI::App *CreateCommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
    std::string GetName() const override { return "Lowpass"; }

  private:
    double m_cutoff;
};
