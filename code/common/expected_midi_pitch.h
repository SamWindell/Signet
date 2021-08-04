#pragma once

#include <optional>
#include <string>

#include "CLI11_Fwd.hpp"
#include "midi_pitches.h"

struct EditTrackedAudioFile;

class ExpectedMidiPitch {
  public:
    void AddCli(CLI::App &command, bool accept_any_octave);
    std::optional<MIDIPitch> GetExpectedMidiPitch(const std::string &command_name, EditTrackedAudioFile &f);

  private:
    std::optional<std::string> m_expected_note_capture {};
    int m_expected_note_capture_midi_zero_octave {-1};
};