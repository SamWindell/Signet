#pragma once

#include "filter.h"
#include "subcommand.h"

void FilterProcessFiles(const tcb::span<EditTrackedAudioFile> files,
                        Filter::RBJType type,
                        double cutoff,
                        double Q,
                        double gain_db);

class Highpass final : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    void ProcessFiles(const tcb::span<EditTrackedAudioFile> files) override;
    std::string GetName() const override { return "Highpass"; }

  private:
    double m_cutoff;
};

class Lowpass final : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    void ProcessFiles(const tcb::span<EditTrackedAudioFile> files) override;
    std::string GetName() const override { return "Lowpass"; }

  private:
    double m_cutoff;
};
