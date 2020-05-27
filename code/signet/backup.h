#pragma once

#include "filesystem.hpp"
#include "json.hpp"

class SignetBackup {
  public:
    SignetBackup();
    bool LoadBackup();
    void ResetBackup();
    void AddFileToBackup(const ghc::filesystem::path &path);

  private:
    ghc::filesystem::path m_database_file {};
    ghc::filesystem::path m_backup_dir {};
    ghc::filesystem::path m_backup_files_dir {};
    nlohmann::json m_database {};
    bool m_parsed_json {};
};
