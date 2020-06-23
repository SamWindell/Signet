#pragma once

#include "CLI11.hpp"
#include "filesystem.hpp"

#include "audio_file.h"
#include "subcommand.h"

class SampleBlender : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    void GenerateFiles(const tcb::span<EditTrackedAudioFile> files, SignetBackup &backup) override;

  private:
    struct BaseBlendFile {
        BaseBlendFile(EditTrackedAudioFile *file, const int root_note) : root_note(root_note), file(file) {}
        int root_note;
        EditTrackedAudioFile *file;
    };

    void GenerateSamplesByBlending(SignetBackup &backup, BaseBlendFile &f1, BaseBlendFile &f2);

    std::string m_regex;
    int m_semitone_interval;
    std::string m_out_filename;
};
