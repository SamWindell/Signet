#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "CLI11.hpp"
#include "doctest.hpp"
#include "filesystem.hpp"
#include "span.hpp"

#include "audio_file.h"
#include "common.h"
#include "string_utils.h"

class CanonicalPathSet {
  public:
    void Add(const ghc::filesystem::path &path) {
        m_paths.insert(ghc::filesystem::canonical(path).generic_string());
    }
    void Add(const std::string_view &path) { Add(ghc::filesystem::path(std::string(path))); }
    void Add(const std::vector<ghc::filesystem::path> &paths) {
        for (const auto &p : paths) {
            Add(p);
        }
    }

    bool Contains(const std::string &path) const { return m_paths.find(path) != m_paths.end(); }

    auto Size() const { return m_paths.size(); }

    auto begin() { return m_paths.begin(); }
    auto end() { return m_paths.end(); }
    auto begin() const { return m_paths.begin(); }
    auto end() const { return m_paths.end(); }

  private:
    std::unordered_set<std::string> m_paths {};
};

template <typename DirectoryIterator>
void ForEachAudioFilesInDirectory(const std::string_view directory,
                                  const std::function<void(const ghc::filesystem::path &)> callback) {
    for (const auto &entry : DirectoryIterator(std::string(directory))) {
        const auto &path = entry.path();
        const auto ext = path.extension();
        if (ext == ".flac" || ext == ".wav") {
            callback(path);
        }
    }
}

void ForEachAudioFilesInDirectory(const std::string_view directory,
                                  const bool recursively,
                                  const std::function<void(const ghc::filesystem::path &)> callback);

std::vector<ghc::filesystem::path> GetAudioFilePathsThatMatchPattern(std::string_view pattern);
std::vector<ghc::filesystem::path> GetAudioFilePathsInDirectory(const std::string_view dir,
                                                                const bool recursively);
bool AddNonExcludedPathsToSet(CanonicalPathSet &set,
                              const tcb::span<const ghc::filesystem::path> paths,
                              const std::vector<std::string_view> &exclude_patterns);

struct ProcessedAudioFile {
    ProcessedAudioFile(const AudioFile &_file, ghc::filesystem::path _path) {
        file = _file;
        path = _path;
        new_filename = GetJustFilenameWithNoExtension(path);
    }

    AudioFile file {};
    ghc::filesystem::path path {};
    std::string new_filename {};
    bool renamed {};
    bool file_edited {};
};

class ExpandedPathnames {
  public:
    ExpandedPathnames() {}
    ExpandedPathnames(const std::string &pathnames_comma_delimed, const bool recursive_directory_search) {
        std::vector<std::string_view> include_parts;
        std::vector<std::string_view> exclude_paths;
        CanonicalPathSet all_matched_filesnames {};

        GetAllCommaDelimitedSections(pathnames_comma_delimed, include_parts, exclude_paths);

        for (const auto &include_part : include_parts) {
            if (include_part.find('*') != std::string_view::npos) {
                const auto matching_paths = GetAudioFilePathsThatMatchPattern(include_part);
                AddNonExcludedPathsToSet(all_matched_filesnames, matching_paths, exclude_paths);
            } else if (ghc::filesystem::is_directory(std::string(include_part))) {
                const auto matching_paths =
                    GetAudioFilePathsInDirectory(include_part, recursive_directory_search);
                AddNonExcludedPathsToSet(all_matched_filesnames, matching_paths, exclude_paths);
            } else if (ghc::filesystem::is_regular_file(std::string(include_part))) {
                ghc::filesystem::path path {include_part};
                if (AddNonExcludedPathsToSet(all_matched_filesnames, {&path, 1}, exclude_paths)) {
                    num_single_file_parts++;
                }
            } else {
                throw CLI::ValidationError("Input filename", "The input filename " +
                                                                 std::string(include_part) +
                                                                 " is neither a file, directory, or pattern");
            }
        }

        if (all_matched_filesnames.Size() == 0) {
            throw CLI::ValidationError("input files", "there are no files that match the pattern " +
                                                          pathnames_comma_delimed);
        }

        for (const auto &path : all_matched_filesnames) {
            if (auto file = ReadAudioFile(path)) {
                m_all_files.push_back({*file, path});
            }
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
