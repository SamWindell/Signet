#pragma once

#include "command.h"
#include "gain_calculators.h"
#include "identical_processing_set.h"
#include "magic_enum.hpp"

class TrimSilenceCommand final : public Command {
  public:
    CLI::App *CreateCommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
    std::string GetName() const override { return "TrimSilence"; }

  private:
    std::pair<usize, usize> GetLoudRegion(EditTrackedAudioFile &f) const;
    void ProcessFile(EditTrackedAudioFile &f, usize loud_region_start, usize loud_region_end) const;

    IdenticalProcessingSet m_identical_processing_set;
    enum class Region { Start, End, Both };
    float m_silence_threshold_db {-90};
    bool m_relative_to_peak {false};
    Region m_region {Region::Both};
};
