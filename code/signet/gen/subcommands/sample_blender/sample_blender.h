#pragma once

#include "CLI11.hpp"
#include "filesystem.hpp"

#include "audio_file.h"
#include "backup.h"

class SampleBlender {
  public:
    SampleBlender(SignetBackup &backup) : m_backup(backup) {}
    static void Create(CLI::App &app, SignetBackup &backup);

  private:
    struct BaseBlendFiles {
        fs::path path;
        int root_note;
        AudioFile file;
    };

    void Run();
    void GenerateSamplesByBlending(const BaseBlendFiles &f1, const BaseBlendFiles &f2);

    SignetBackup &m_backup;
    std::string m_regex;
    fs::path m_directory;
    int m_semitone_interval;
    std::string m_out_filename;
};
