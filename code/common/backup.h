#pragma once

#include "filesystem.hpp"
#include "json.hpp"

struct AudioData;

class SignetBackup {
  public:
    SignetBackup();
    bool LoadBackup();
    void ClearBackup();

    bool DeleteFile(const fs::path &path);
    bool MoveFile(const fs::path &from, const fs::path &to);
    bool CreateFile(const fs::path &path, const AudioData &data, bool create_directories);
    bool OverwriteFile(const fs::path &path, const AudioData &data);

    bool AddFileToBackup(const fs::path &path);

  private:
    bool AddMovedFileToBackup(const fs::path &from, const fs::path &to);
    bool AddNewlyCreatedFileToBackup(const fs::path &path);

    bool WriteDatabaseFile();
    bool CreateBackupFilesDirIfNeeded();

    void ClearOldBackIfNeeded();

    bool m_old_backup_cleared {false};
    fs::path m_database_file {};
    fs::path m_backup_dir {};
    fs::path m_backup_files_dir {};
    nlohmann::json m_database {};
    bool m_parsed_json {};
};
