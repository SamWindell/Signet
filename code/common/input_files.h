#pragma once

#include "audio_file.h"
#include "common.h"
#include "pathname_expansion.h"
#include "string_utils.h"
#include "types.h"

class SignetBackup;

struct InputAudioFile {
    InputAudioFile(const fs::path &path)
        : filename(GetJustFilenameWithNoExtension(path)), original_path(path), m_path(path) {}

    AudioData &GetWritableAudio() {
        m_file_edited = true;
        return const_cast<AudioData &>(GetAudio());
    }

    const AudioData &GetAudio() {
        if (!m_file_loaded && m_file_valid) {
            if (const auto data = ReadAudioFile(original_path)) {
                LoadAudioData(*data);
            } else {
                ErrorWithNewLine("could not load audio file ", original_path);
                m_file_valid = false;
            }
        }
        return m_data;
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
    bool FormatChanged() const { return m_file_loaded && m_original_file_format != m_data.format; }

    void LoadAudioData(const AudioData &data) {
        m_data = data;
        m_original_file_format = m_data.format;
        m_file_loaded = true;
    }

  private:
    AudioFileFormat m_original_file_format {};
    fs::path m_path {};
    AudioData m_data {};
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
