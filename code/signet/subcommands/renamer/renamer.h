#pragma once

#include <optional>
#include <unordered_map>

#include "auto_mapper.h"
#include "note_to_midi.h"
#include "subcommand.h"
#include "types.h"

class Renamer final : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
    std::string GetName() const override { return "Renamer"; }

  private:
    AutoMapper m_auto_mapper;
    NoteToMIDIConverter m_note_to_midi_processor;

    std::optional<std::string> m_prefix;
    std::optional<std::string> m_suffix;
    std::optional<std::string> m_regex_pattern;
    std::string m_regex_replacement;

    int m_counter {};
};
