#pragma once

#include <memory>

#include "doctest.hpp"
#include "json.hpp"

#include "backup.h"
#include "common.h"
#include "filesystem.hpp"
#include "pathname_expansion.h"
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

    auto GetAllMatchingPaths() {
        std::vector<ghc::filesystem::path> result;
        REQUIRE(GetNumPatterns() != 0);
        for (usize i = 0; i < GetNumPatterns(); ++i) {
            switch (GetMode(i)) {
                case PatternMode::Pattern:
                case PatternMode::Directory: {
                    const auto root_dir = GetRootDirectory(i);
                    std::cout << root_dir << " root dir\n";
                    const auto paths = GetAllAudioFilesInDirectoryRecursively(root_dir);
                    for (const auto &p : paths) {
                        if (Matches(i, p) == MultiplePatternMatchingFilenames<>::MatchResult::Yes)
                            result.push_back(p);
                    }
                    break;
                }
                case PatternMode::File: {
                    result.push_back(GetPattern(i));
                    break;
                }
                default: WarningWithNewLine("pattern is not valid ", GetPattern(i), "\n");
            }
        }
        return result;
    }

  private:
    static std::vector<ghc::filesystem::path>
    GetAllAudioFilesInDirectoryRecursively(const std::string &directory) {
        std::vector<ghc::filesystem::path> paths;
        for (const auto &entry : ghc::filesystem::recursive_directory_iterator(directory)) {
            const auto &path = entry.path();
            const auto ext = path.extension();
            if (ext == ".flac" || ext == ".wav") {
                paths.push_back(path);
            }
        }
        return paths;
    }

    bool IsPathHashPresent(const usize hash) const {
        return std::find(m_already_matched_path_hashes.begin(), m_already_matched_path_hashes.end(), hash) !=
               m_already_matched_path_hashes.end();
    }

    std::vector<size_t> m_already_matched_path_hashes {};
    std::vector<PatternMatchingFilename<FilepathCheck>> m_patterns {};
    std::string m_whole_str {};
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
