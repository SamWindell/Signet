#pragma once
#include "edit_tracked_audio_file.h"
#include "pathname_expansion.h"
#include "types.h"

class SignetBackup;

class InputAudioFiles {
  public:
    InputAudioFiles() {}
    InputAudioFiles(const std::string &pathnames_comma_delimed, const bool recursive_directory_search);

    bool IsSingleFile() const { return m_is_single_file; }
    std::vector<EditTrackedAudioFile> &GetAllFiles() { return m_all_files; }
    usize NumFiles() const { return m_all_files.size(); }

    auto &GetAllFolders() { return m_folders; }

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
    std::vector<EditTrackedAudioFile> m_all_files {};
    FolderMapType m_folders {};
};
