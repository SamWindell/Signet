#pragma once

#include "filesystem.hpp"
#include "json.hpp"

struct AudioData;

class SignetBackup {
  public:
    SignetBackup();
    bool LoadBackup() const;
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

    void ClearOldBackIfNeeded();

    bool m_old_backup_cleared {false};
    const fs::path m_backup_dir;
    const fs::path m_backup_files_dir;
    const fs::path m_database_file;
    nlohmann::json m_database {};
};
