#pragma once

#include "filesystem.hpp"
#include "json.hpp"

struct AudioFile;

class SignetBackup {
  public:
    SignetBackup();
    bool LoadBackup();
    void ClearBackup();
    void AddFileToBackup(const fs::path &path);
    void AddMovedFileToBackup(const fs::path &from, const fs::path &to);
    void AddNewlyCreatedFileToBackup(const fs::path &path);

    bool DeleteFile(const fs::path &path);
    bool MoveFile(const fs::path &from, const fs::path &to);
    bool CreateFile(const fs::path &path, const AudioFile &file);
    bool OverwriteFile(const fs::path &path, const AudioFile &file);

  private:
    void WriteDatabaseFile();

    fs::path m_database_file {};
    fs::path m_backup_dir {};
    fs::path m_backup_files_dir {};
    nlohmann::json m_database {};
    bool m_parsed_json {};
};
