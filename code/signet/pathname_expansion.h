#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "CLI11.hpp"
#include "doctest.hpp"
#include "filesystem.hpp"

#include "audio_file.h"
#include "common.h"

static void
ForEachAudioFilesInDirectoryRecursively(const std::string_view directory,
                                        std::function<void(const ghc::filesystem::path &)> callback) {
    for (const auto &entry : ghc::filesystem::recursive_directory_iterator(std::string(directory))) {
        const auto &path = entry.path();
        const auto ext = path.extension();
        if (ext == ".flac" || ext == ".wav") {
            callback(path);
        }
    }
}

static void ForEachAudioFilesInDirectory(const std::string_view directory,
                                         std::function<void(const ghc::filesystem::path &)> callback) {
    for (const auto &entry : ghc::filesystem::directory_iterator(std::string(directory))) {
        const auto &path = entry.path();
        const auto ext = path.extension();
        if (ext == ".flac" || ext == ".wav") {
            callback(path);
        }
    }
}

class CanonicalPathSet {
  public:
    void Add(const ghc::filesystem::path &path) {
        paths.insert(ghc::filesystem::canonical(path).generic_string());
    }

    bool Contains(const std::string &path) const { return paths.find(path) != paths.end(); }

    auto Size() const { return paths.size(); }

    auto begin() { return paths.begin(); }
    auto end() { return paths.end(); }
    auto begin() const { return paths.begin(); }
    auto end() const { return paths.end(); }

  private:
    std::unordered_set<std::string> paths {};
};

struct ExpandablePatternPathname {
    static void AddMatchingPathsToSet(std::string_view str,
                                      CanonicalPathSet &paths,
                                      const std::vector<std::string_view> &exclude_filters) {
        namespace fs = ghc::filesystem;

        std::string added_slash;
        if (str.find('/') == std::string_view::npos) {
            added_slash = "./" + std::string(str);
            str = added_slash;
        }

        std::vector<std::string> possible_folders;

        size_t pos = 0;
        size_t prev_pos = 0;
        std::string_view prev_part = {};
        while ((pos = str.find('/', pos)) != std::string_view::npos) {
            const auto folder = str.substr(0, pos);
            const auto part = str.substr(prev_pos, pos - prev_pos);

            if (!possible_folders.size()) {
                possible_folders.push_back(std::string(folder));
            } else {
                std::vector<std::string> new_possible_folders;

                for (const auto &f : possible_folders) {
                    const std::string with_part = f + "/" + std::string(part);

                    if (part.find("**") != std::string_view::npos) {
                        for (const auto &entry : fs::recursive_directory_iterator(f)) {
                            if (entry.is_directory()) {
                                const auto path = entry.path().generic_string();
                                if (PatternMatch(folder, path)) {
                                    new_possible_folders.push_back(path);
                                }
                            }
                        }
                    } else if (part.find('*') != std::string_view::npos) {
                        for (const auto &entry : fs::directory_iterator(f)) {
                            if (entry.is_directory()) {
                                const auto path = entry.path().generic_string();
                                if (PatternMatch(folder, path)) {
                                    new_possible_folders.push_back(path);
                                }
                            }
                        }
                    } else {
                        new_possible_folders.push_back(with_part);
                    }
                }

                possible_folders = new_possible_folders;
            }

            pos += 1;
            prev_pos = pos;
        }

        const std::string_view last_file_section = str.substr(prev_pos);

        const auto CheckAndRegisterFile = [&](auto path) {
            const auto generic = path.generic_string();
            if (PatternMatch(str, generic)) {
                bool is_valid = true;
                for (const auto exclude : exclude_filters) {
                    if (PatternMatch(exclude, generic)) {
                        is_valid = false;
                        break;
                    }
                }
                if (is_valid) {
                    paths.Add(path);
                }
            }
        };

        for (const auto f : possible_folders) {
            if (last_file_section.find("**") != std::string_view::npos) {
                ForEachAudioFilesInDirectoryRecursively(f, CheckAndRegisterFile);
            } else if (last_file_section.find("*") != std::string_view::npos) {
                ForEachAudioFilesInDirectory(f, CheckAndRegisterFile);
            } else {
                fs::path path = f;
                path /= std::string(last_file_section);
                CheckAndRegisterFile(path);
            }
        }
    }
};

struct SingleFilePathname {
    static void AddMatchingPathsToSet(const std::string_view str,
                                      CanonicalPathSet &paths,
                                      const std::vector<std::string_view> &exclude_filters) {
        paths.Add(str);
    }
};

struct ExpandableDirectoryPathname {
    static void AddMatchingPathsToSet(const std::string_view str,
                                      CanonicalPathSet &paths,
                                      const std::vector<std::string_view> &exclude_filters) {
        const auto parent_directory = ghc::filesystem::path(str);
        ForEachAudioFilesInDirectory(str, [&](auto path) {
            if (parent_directory.compare(path) < 0) {
                paths.Add(path);
            }
        });
    }
};

struct ProcessedAudioFile {
    AudioFile file;
    ghc::filesystem::path path;
    bool file_edited;
    bool renamed;
};

class ExpandedPathnames {
  public:
    ExpandedPathnames() {}
    ExpandedPathnames(const std::string &pathnames) {
        std::vector<std::string_view> include_parts;
        std::vector<std::string_view> exclude_parts;
        CanonicalPathSet all_matched_filesnames {};

        GetAllCommaDelimitedSections(pathnames, include_parts, exclude_parts);

        for (const auto &include_part : include_parts) {
            if (include_part.find('*') != std::string_view::npos) {
                ExpandablePatternPathname::AddMatchingPathsToSet(include_part, all_matched_filesnames,
                                                                 exclude_parts);
            } else if (ghc::filesystem::is_directory(std::string(include_part))) {
                ExpandableDirectoryPathname::AddMatchingPathsToSet(include_part, all_matched_filesnames,
                                                                   exclude_parts);
            } else if (ghc::filesystem::is_regular_file(std::string(include_part))) {
                SingleFilePathname::AddMatchingPathsToSet(include_part, all_matched_filesnames,
                                                          exclude_parts);
                num_single_file_parts++;
            } else {
                throw CLI::ValidationError("Input filename", "The input filename " +
                                                                 std::string(include_part) +
                                                                 " is neither a file, directory, or pattern");
            }
        }

        for (const auto &path : all_matched_filesnames) {
            if (auto file = ReadAudioFile(path)) {
                m_all_files.push_back({*file, path, false, false});
            }
        }

        if (m_all_files.size() == 0) {
            throw CLI::ValidationError("input files",
                                       "there are no files that match the pattern " + pathnames);
        }
    }

    bool IsSingleFile() const { return num_single_file_parts == 1; }
    std::vector<ProcessedAudioFile> &GetAllFiles() { return m_all_files; }

  private:
    static void GetAllCommaDelimitedSections(std::string_view s,
                                             std::vector<std::string_view> &include_parts,
                                             std::vector<std::string_view> &exclude_parts) {
        const auto RegisterSection = [&](std::string_view section) {
            if (section.size() >= 2 && (section[0] == '"' || section[0] == '\'') &&
                (section.back() == '"' || section.back() == '\'')) {
                section.remove_prefix(1);
                section.remove_suffix(1);
            }

            if (section[0] == '-') {
                section.remove_prefix(1);
                exclude_parts.push_back(section);
            } else {
                include_parts.push_back(section);
            }
        };

        size_t pos = 0;
        while ((pos = s.find(',')) != std::string::npos) {
            RegisterSection(s.substr(0, pos));
            s.remove_prefix(pos + 1);
        }
        RegisterSection(s);
    }

    std::vector<ProcessedAudioFile> m_all_files {};
    usize num_single_file_parts = 0;
};
