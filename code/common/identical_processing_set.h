#pragma once

#include <string>
#include <vector>

#include "CLI11_Fwd.hpp"

#include "audio_files.h"
#include "edit_tracked_audio_file.h"

class IdenticalProcessingSet {
  public:
    void AddCli(CLI::App &command);
    bool ShouldProcessInSets() const { return !m_sample_set_args.empty(); }
    void ProcessSets(AudioFiles &files,
                     std::string_view command_name,
                     const std::function<void(EditTrackedAudioFile *authority,
                                              const std::vector<EditTrackedAudioFile *> &set)> &callback);

  private:
    std::vector<std::string> m_sample_set_args;
};
