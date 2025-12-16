#pragma once

#include "command.h"
#include <vector>

struct PopLocation {
    size_t frame;
    unsigned channel;
    double deviation;
};

class DetectPopsCommand : public Command {
  public:
    CLI::App *CreateCommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
    std::string GetName() const override { return "DetectPops"; }

    bool AllowsOutputFolder() const override { return m_fix; }
    bool AllowsSingleOutputFile() const override { return m_fix; }

  private:
    double m_threshold = 30.0;
    bool m_fix = false;
    bool m_zero_only = false;

    std::vector<PopLocation> DetectPops(const AudioData &audio) const;
    void RepairPops(AudioData &audio, const std::vector<PopLocation> &pops) const;
    void ReportDetections(EditTrackedAudioFile &f, const std::vector<PopLocation> &pops) const;
    void ReportRepairs(EditTrackedAudioFile &f, const std::vector<PopLocation> &pops) const;
};
