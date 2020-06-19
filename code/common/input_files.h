#pragma once

#include "CLI11.hpp"

#include "audio_file.h"
#include "backup.h"
#include "common.h"
#include "pathname_expansion.h"
#include "string_utils.h"
#include "types.h"

struct InputAudioFile {
    InputAudioFile(const fs::path &path)
        : filename(GetJustFilenameWithNoExtension(path)), original_path(path), m_path(path) {}

    AudioFile &GetWritableAudio() {
        m_file_edited = true;
        return const_cast<AudioFile &>(GetAudio());
    }

    const AudioFile &GetAudio() {
        if (!m_file_loaded && m_file_valid) {
            if (const auto file = ReadAudioFile(original_path)) {
                LoadAudioData(*file);
            } else {
                ErrorWithNewLine("could not load audio file ", original_path);
                m_file_valid = false;
            }
        }
        return m_file;
    }

    const fs::path &GetPath() const { return m_path; }

    void SetPath(const fs::path &path) {
        m_path_edited = true;
        m_path = path;
    }

    const std::string filename;
    const fs::path original_path;

    bool AudioChanged() const { return m_file_edited && m_file_valid; }
    bool FilepathChanged() const { return m_path_edited; }
    bool FormatChanged() const { return m_original_file_format != m_file.format; }

    void LoadAudioData(const AudioFile &file) {
        m_file = file;
        m_original_file_format = m_file.format;
        m_file_loaded = true;
    }

  private:
    AudioFileFormat m_original_file_format {};
    fs::path m_path {};
    AudioFile m_file {};
    bool m_file_loaded = false;
    bool m_file_valid = true;

    bool m_file_edited = false;
    bool m_path_edited = false;
};

class InputAudioFiles {
  public:
    InputAudioFiles() {}
    InputAudioFiles(const std::string &pathnames_comma_delimed, const bool recursive_directory_search);

    bool IsSingleFile() const { return m_is_single_file; }
    std::vector<InputAudioFile> &GetAllFiles() { return m_all_files; }
    usize NumFiles() const { return m_all_files.size(); }

    bool WriteAllAudioFiles(SignetBackup &backup);
    int GetNumFilesProcessed() const {
        int n = 0;
        for (const auto &f : m_all_files) {
            if (f.AudioChanged() || f.FilepathChanged() || f.FormatChanged()) {
                n++;
            }
        }
        return n;
    }

  private:
    void ReadAllAudioFiles(const FilePathSet &paths);
    bool WouldWritingAllFilesCreateConflicts();

    bool m_is_single_file {};
    std::vector<InputAudioFile> m_all_files {};
};
