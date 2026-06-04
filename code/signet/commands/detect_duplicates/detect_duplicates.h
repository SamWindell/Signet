#pragma once

#include "command.h"

class DetectDuplicatesCommand : public Command {
  public:
    CLI::App *CreateCommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
    std::string GetName() const override { return "DetectDuplicates"; }

    bool AllowsOutputFolder() const override { return false; }
    bool AllowsSingleOutputFile() const override { return false; }
    bool IsReadOnly() const override { return true; }

  private:
    double m_threshold = 0.5;
    double m_min_overlap_seconds = 0.5;
    bool m_show_matrix = false;
};
