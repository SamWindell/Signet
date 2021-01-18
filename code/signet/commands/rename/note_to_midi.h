#pragma once
#include <optional>
#include <string>

#include "CLI11.hpp"

#include "edit_tracked_audio_file.h"

class NoteToMIDIConverter {
  public:
    bool Rename(const EditTrackedAudioFile &f, std::string &filename);
    void CreateCLI(CLI::App &rename);

  private:
    bool m_on {};
    std::string m_midi_0_note {"C-1"};
};
