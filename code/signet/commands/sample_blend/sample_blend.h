#pragma once

#include "CLI11.hpp"
#include "filesystem.hpp"

#include "audio_file_io.h"
#include "command.h"

class SampleBlendCommand : public Command {
  public:
    CLI::App *CreateCommandCLI(CLI::App &app) override;
    void GenerateFiles(AudioFiles &files, SignetBackup &backup) override;
    std::string GetName() const override { return "SampleBlend"; }

    bool AllowsOutputFolder() const override { return false; }
    bool AllowsSingleOutputFile() const override { return true; }

  private:
    struct BaseBlendFile {
        BaseBlendFile(EditTrackedAudioFile *file_, const int root_note_)
            : root_note(root_note_), file(file_) {}
        int root_note;
        EditTrackedAudioFile *file;
    };

    void GenerateSamplesByBlending(SignetBackup &backup, BaseBlendFile &f1, BaseBlendFile &f2);

    bool m_make_same_length {false};
    std::string m_regex {};
    int m_semitone_interval {};
    std::string m_out_filename {};
};
