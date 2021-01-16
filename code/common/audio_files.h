#pragma once
#include <map>

#include "edit_tracked_audio_file.h"
#include "pathname_expansion.h"
#include "types.h"

class SignetBackup;

class AudioFiles {
  public:
    AudioFiles() {}
    AudioFiles(const std::vector<std::string> &path_items, const bool recursive_directory_search);
    AudioFiles(const tcb::span<EditTrackedAudioFile> files);

    //
    // Files are typically read from the underlying vector with these methods.
    //
    usize Size() const { return m_all_files.size(); }
    const auto &Files() { return m_all_files; }

    auto begin() { return m_all_files.begin(); }
    auto end() { return m_all_files.end(); }
    auto begin() const { return m_all_files.begin(); }
    auto end() const { return m_all_files.end(); }
    EditTrackedAudioFile &operator[](size_t index) { return m_all_files[index]; }

    //
    // You can also access the files in separate vectors based on each files parent folder.
    //
    const auto &Folders() { return m_folders; }

    //
    //
    bool WriteFilesThatHaveBeenEdited(SignetBackup &backup);
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
    void CreateFoldersDataStructure();

    std::vector<EditTrackedAudioFile> m_all_files {};
    std::map<fs::path, std::vector<EditTrackedAudioFile *>> m_folders {};
};