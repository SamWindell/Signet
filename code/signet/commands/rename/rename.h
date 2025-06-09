#pragma once

#include <optional>
#include <unordered_map>

#include "auto_mapper.h"
#include "command.h"
#include "note_to_midi.h"
#include "types.h"

class RenameCommand final : public Command {
  public:
    CLI::App *CreateCommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
    std::string GetName() const override { return "Rename"; }
    bool AllowsSingleOutputFile() const override { return false; }

  private:
    AutoMapper m_auto_mapper;
    NoteToMIDIConverter m_note_to_midi_processor;

    bool dry_run {};

    std::optional<std::string> m_prefix;
    std::optional<std::string> m_suffix;
    std::optional<std::string> m_regex_pattern;
    std::string m_regex_replacement;

    int m_counter {};
};
