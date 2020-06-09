#pragma once

#include "filesystem.hpp"
#include "json.hpp"

class SignetBackup {
  public:
    SignetBackup();
    bool LoadBackup();
    void ResetBackup();
    void AddFileToBackup(const fs::path &path);

  private:
    fs::path m_database_file {};
    fs::path m_backup_dir {};
    fs::path m_backup_files_dir {};
    nlohmann::json m_database {};
    bool m_parsed_json {};
};
