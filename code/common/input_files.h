#pragma once

#include "CLI11.hpp"

#include "audio_file.h"
#include "backup.h"
#include "pathname_expansion.h"
#include "string_utils.h"
#include "types.h"

struct InputAudioFile {
    InputAudioFile(const AudioFile &_file, const fs::path &_path)
        : filename(GetJustFilenameWithNoExtension(_path))
        , original_path(_path)
        , original_file_format(_file.format) {
        file = _file;
        path = _path;
    }

    AudioFile file {};
    fs::path path {};
    bool renamed {};
    bool file_edited {};

    const std::string filename;
    const fs::path original_path;
    const AudioFileFormat original_file_format;
};

class InputAudioFiles {
  public:
    InputAudioFiles() {}
    InputAudioFiles(const std::string &pathnames_comma_delimed, const bool recursive_directory_search);

    bool IsSingleFile() const { return m_is_single_file; }
    std::vector<InputAudioFile> &GetAllFiles() { return m_all_files; }
    usize NumFiles() const { return m_all_files.size(); }

    bool WriteAllAudioFiles(SignetBackup &backup);

  private:
    void ReadAllAudioFiles(const FilePathSet &paths);
    bool WouldWritingAllFilesCreateConflicts();

    bool m_is_single_file {};
    std::vector<InputAudioFile> m_all_files {};
};
