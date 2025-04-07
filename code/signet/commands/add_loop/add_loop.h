#pragma once

#include "audio_duration.h"
#include "command.h"

class AddLoopCommand final : public Command {
  public:
    CLI::App *CreateCommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
    std::string GetName() const override { return "AddLoop"; }

  private:
    AudioDuration m_start_point {AudioDuration::Unit::Samples, 0};
    std::optional<AudioDuration> m_end_point {};
    std::optional<AudioDuration> m_num_frames;
    std::optional<std::string> m_loop_name;
    MetadataItems::LoopType m_loop_type = MetadataItems::LoopType::Forward;
    unsigned m_num_times_to_loop = 0; // 0 = infinite
};
