#pragma once

#include <memory>

#include "doctest.hpp"
#include "json.hpp"

#include "common.h"
#include "filesystem.hpp"
#include "subcommand.h"

class Subcommand;

enum class PatternMode {
    Invalid,
    Pattern,
    File,
    Directory,
};

struct CheckFilesystem {
    static bool IsDirectory(std::string str) { return ghc::filesystem::is_directory(str); }
    static bool IsRegularFile(std::string str) { return ghc::filesystem::is_regular_file(str); }
};

struct CheckDummyFilesystem {
    static bool IsDirectory(std::string str) { return str.find('/') != std::string::npos; }
    static bool IsRegularFile(std::string str) { return ghc::filesystem::path(str).has_extension(); }
};

template <typename FilepathCheck = CheckFilesystem>
class PatternMatchingFilename {
  public:
    PatternMatchingFilename() {}
    PatternMatchingFilename(std::string_view str) : m_str(str) {
        if (m_str.find('*') != std::string::npos) {
            m_mode = PatternMode::Pattern;
        } else if (FilepathCheck::IsDirectory(m_str)) {
            m_mode = PatternMode::Directory;
        } else if (FilepathCheck::IsRegularFile(m_str)) {
            m_mode = PatternMode::File;
        } else {
            m_mode = PatternMode::Invalid;
        }
        std::cout << __FUNCTION__ << " mode: " << (int)m_mode << " string " << str << "\n";
    }
    bool MatchesRaw(std::string_view filepath) const { return PatternMatch(m_str, filepath); }
    PatternMode GetMode() const { return m_mode; }

    bool Matches(const ghc::filesystem::path &filepath) const {
        switch (m_mode) {
            case PatternMode::File: {
                return filepath.generic_string() == m_str;
            }
            case PatternMode::Pattern: {
                return MatchesRaw(filepath.generic_string());
            }
            case PatternMode::Directory: {
                const auto pattern_path = ghc::filesystem::path(m_str);
                std::cout << "comparing " << filepath << " and " << pattern_path << "\n";
                const auto comparison = pattern_path.compare(filepath);
                return comparison < 0;
            }
            case PatternMode::Invalid: break;
        }
        REQUIRE(0);
        return false;
    }
    std::string GetPattern() const { return std::string(m_str); }
    std::string GetRootDirectory() const {
        switch (m_mode) {
            case PatternMode::Invalid: break;
            case PatternMode::Directory: {
                return m_str;
            }
            case PatternMode::File: {
                return ".";
            }
            case PatternMode::Pattern: {
                if (const auto slash_pos = m_str.rfind('/'); slash_pos != std::string::npos) {
                    const auto pattern_pos = m_str.find('*');
                    assert(pattern_pos != std::string::npos);
                    if (pattern_pos < slash_pos) {
                        return ".";
                    } else {
                        return m_str.substr(0, slash_pos);
                    }
                } else {
                    return ".";
                }
            }
        }
        return "<invalid>";

        // const auto slash_pos = m_str.rfind('/');
        // const auto pattern_pos = m_str.find('*');
        // const auto has_slash = slash_pos != std::string::npos;
        // const auto has_pattern = pattern_pos != std::string::npos;

        // if (has_slash && !has_pattern) {
        //     return std::string(m_str);
        // }

        // if (!has_slash && !has_pattern) {
        //     return ".";
        // }

        // if (!has_slash && has_pattern) {
        //     return ".";
        // }

        // assert(has_pattern && has_slash);
        // if (pattern_pos < slash_pos) {
        //     return ".";
        // }
        // return std::string(m_str.substr(0, slash_pos));
    }

  private:
    PatternMode m_mode {};
    std::string m_str {};
};

template <typename FilepathCheck = CheckFilesystem>
class MultiplePatternMatchingFilenames {
  public:
    enum class MatchResult {
        Yes,
        No,
        AlreadyMatched,
    };

    MultiplePatternMatchingFilenames() {}
    MultiplePatternMatchingFilenames(const std::string &pattern_list) : m_whole_str(pattern_list) {
        std::string_view s = m_whole_str;
        size_t pos = 0;
        while ((pos = s.find(',')) != std::string::npos) {
            const auto pattern = s.substr(0, pos);
            m_patterns.push_back(PatternMatchingFilename<FilepathCheck>(pattern));
            s.remove_prefix(pos + 1);
        }
        m_patterns.push_back(PatternMatchingFilename<FilepathCheck>(s));
    }
    size_t GetNumPatterns() const { return m_patterns.size(); }
    std::string GetPattern(const usize pattern_index) const {
        assert(pattern_index < m_patterns.size());
        return m_patterns[pattern_index].GetPattern();
    }
    PatternMode GetMode(const usize pattern_index) const { return m_patterns[pattern_index].GetMode(); }
    std::string GetWholePattern() const { return m_whole_str; }
    std::string GetRootDirectory(const size_t pattern_index) const {
        return m_patterns[pattern_index].GetRootDirectory();
    }
    MatchResult Matches(const size_t pattern_index, const ghc::filesystem::path &filepath) {
        const auto hash = ghc::filesystem::hash_value(filepath);
        if (IsPathHashPresent(hash)) {
            return MatchResult::AlreadyMatched; // already matched this with another pattern
        }
        if (m_patterns[pattern_index].Matches(filepath)) {
            m_already_matched_path_hashes.push_back(hash);
            return MatchResult::Yes;
        }
        return MatchResult::No;
    }
    bool IsSingleFile() const {
        return m_patterns.size() == 1 && m_patterns[0].GetMode() == PatternMode::File;
    }
    bool IsPattern() const {
        return m_patterns.size() > 1 || m_patterns[0].GetMode() == PatternMode::Pattern;
    }

  private:
    bool IsPathHashPresent(const usize hash) const {
        return std::find(m_already_matched_path_hashes.begin(), m_already_matched_path_hashes.end(), hash) !=
               m_already_matched_path_hashes.end();
    }

    std::vector<size_t> m_already_matched_path_hashes {};
    std::vector<PatternMatchingFilename<FilepathCheck>> m_patterns {};
    std::string m_whole_str {};
};

class SignetBackup {
  public:
    SignetBackup() {
        m_backup_dir = "signet-backup";
        if (!ghc::filesystem::is_directory(m_backup_dir)) {
            ghc::filesystem::create_directory(m_backup_dir);
        }

        m_backup_files_dir = m_backup_dir / "files";
        m_database_file = m_backup_dir / "backup.json";
        try {
            std::ifstream i(m_database_file.generic_string(), std::ofstream::in | std::ofstream::binary);
            i >> m_database;
            m_parsed_json = true;
            i.close();
        } catch (const nlohmann::detail::parse_error &e) {
            std::cout << e.what() << "\n";
        } catch (...) {
            std::cout << "other exception\n";
            throw;
        }
    }

    bool LoadBackup() {
        if (!m_parsed_json) return false;
        for (auto [hash, path] : m_database["files"].items()) {
            std::cout << "Loading backed-up file " << path << "\n";
            ghc::filesystem::copy_file(m_backup_files_dir / hash, path,
                                       ghc::filesystem::copy_options::update_existing);
        }
        return true;
    }

    void ResetBackup() {
        if (ghc::filesystem::is_directory(m_backup_files_dir)) {
            ghc::filesystem::remove_all(m_backup_files_dir);
            ghc::filesystem::create_directory(m_backup_files_dir);
        }
        if (ghc::filesystem::is_regular_file(m_database_file)) {
            ghc::filesystem::remove(m_database_file);
        }
        m_database = {};
    }

    void AddFileToBackup(const ghc::filesystem::path &path) {
        if (!ghc::filesystem::is_directory(m_backup_files_dir)) {
            ghc::filesystem::create_directory(m_backup_files_dir);
        }

        const auto hash_string = std::to_string(ghc::filesystem::hash_value(path));
        ghc::filesystem::copy_file(path, m_backup_files_dir / hash_string,
                                   ghc::filesystem::copy_options::update_existing);
        m_database["files"][hash_string] = path.generic_string();

        std::cout << "Backing-up file " << path << "\n";
        std::ofstream o(m_database_file.generic_string(), std::ofstream::out | std::ofstream::binary);
        o << std::setw(2) << m_database << std::endl;
        o.close();
    }

  private:
    ghc::filesystem::path m_database_file {};
    ghc::filesystem::path m_backup_dir {};
    ghc::filesystem::path m_backup_files_dir {};
    nlohmann::json m_database {};
    bool m_parsed_json {};
};

class SignetInterface {
  public:
    SignetInterface();

    int Main(const int argc, const char *const argv[]);
    void ProcessAllFiles(Subcommand &subcommand);
    bool IsProcessingMultipleFiles() const { return !m_input_filepath_pattern.IsSingleFile(); }

  private:
    void ProcessFile(Subcommand &subcommand,
                     const ghc::filesystem::path &input_filepath,
                     std::optional<ghc::filesystem::path> output_filepath);

    std::vector<std::unique_ptr<Subcommand>> m_subcommands {};

    SignetBackup m_backup {};
    std::vector<ghc::filesystem::path> m_all_matched_files {};

    int m_num_files_processed = 0;
    std::optional<ghc::filesystem::path> m_output_filepath {};
    MultiplePatternMatchingFilenames<> m_input_filepath_pattern {};
};
