#pragma once

#include "CLI11.hpp"
#include "filesystem.hpp"

#include "audio_file.h"
#include "subcommand.h"

class SampleBlender : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    void GenerateFiles(const tcb::span<const InputAudioFile> files, SignetBackup &backup) override;

  private:
    struct BaseBlendFiles {
        fs::path path;
        int root_note;
        AudioData data;
    };

    void GenerateSamplesByBlending(SignetBackup &backup, const BaseBlendFiles &f1, const BaseBlendFiles &f2);

    std::string m_regex;
    int m_semitone_interval;
    std::string m_out_filename;
};
