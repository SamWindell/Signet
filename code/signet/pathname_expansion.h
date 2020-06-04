#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "CLI11.hpp"
#include "doctest.hpp"
#include "filesystem.hpp"

static void
ForEachAudioFilesInDirectoryRecursively(const std::string &directory,
                                        std::function<void(const ghc::filesystem::path &)> callback) {
    for (const auto &entry : ghc::filesystem::recursive_directory_iterator(directory)) {
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

    usize Size() const { return paths.size(); }

    auto begin() { return paths.begin(); }
    auto end() { return paths.end(); }
    auto begin() const { return paths.begin(); }
    auto end() const { return paths.end(); }

  private:
    std::unordered_set<std::string> paths {};
};

class ExpandablePathname {
  public:
    ExpandablePathname(const std::string_view str) : m_pathname(str) {}
    virtual ~ExpandablePathname() {}
    virtual void AddMatchingPathsToSet(CanonicalPathSet &paths) = 0;
    virtual bool IsSingleFile() const { return false; }

  protected:
    std::string m_pathname;
};

class ExpandablePatternPathname final : public ExpandablePathname {
  public:
    ExpandablePatternPathname(const std::string_view str) : ExpandablePathname(str) {}
    void AddMatchingPathsToSet(CanonicalPathSet &paths) override {
        ForEachAudioFilesInDirectoryRecursively(GetDirectoryToStartSearch(), [&](auto path) {
            if (PatternMatch(m_pathname, path.generic_string())) {
                paths.Add(path);
            }
        });
    }

  private:
    std::string GetDirectoryToStartSearch() {
        if (const auto slash_pos = m_pathname.rfind('/'); slash_pos != std::string::npos) {
            const auto pattern_pos = m_pathname.find('*');
            REQUIRE(pattern_pos != std::string::npos);
            if (pattern_pos < slash_pos) {
                return ".";
            } else {
                return m_pathname.substr(0, slash_pos);
            }
        } else {
            return ".";
        }
    }
};

class SingleFilePathname final : public ExpandablePathname {
  public:
    SingleFilePathname(const std::string_view str) : ExpandablePathname(str) {}
    void AddMatchingPathsToSet(CanonicalPathSet &paths) override { paths.Add(m_pathname); }
    bool IsSingleFile() const override { return true; }
};

class ExpandableDirectoryPathname final : public ExpandablePathname {
  public:
    ExpandableDirectoryPathname(const std::string_view str) : ExpandablePathname(str) {}
    void AddMatchingPathsToSet(CanonicalPathSet &paths) override {
        const auto parent_directory = ghc::filesystem::path(m_pathname);
        ForEachAudioFilesInDirectoryRecursively(m_pathname, [&](auto path) {
            if (parent_directory.compare(path) < 0) {
                paths.Add(path);
            }
        });
    }
};

class ExpandedPathnames {
  public:
    ExpandedPathnames() {}
    ExpandedPathnames(const std::string &pathnames) {
        m_expandables = std::move(Parse(pathnames));

        for (auto &[is_exclude, e] : m_expandables) {
            if (!is_exclude) {
                e->AddMatchingPathsToSet(m_all_matched_filesnames);
            } else {
                e->AddMatchingPathsToSet(m_all_exclude_matched_filesnames);
            }
        }

        for (const auto &path : m_all_matched_filesnames) {
            if (!m_all_exclude_matched_filesnames.Contains(path)) {
                if (auto file = ReadAudioFile(path)) {
                    m_all_files.push_back({*file, path});
                }
            }
        }

        if (m_all_files.size() == 0) {
            throw CLI::ValidationError("input files",
                                       "there are no files that match the pattern " + pathnames);
        }
    }

    bool IsSingleFile() const { return m_expandables.size() == 1 && m_expandables[0].second->IsSingleFile(); }
    std::vector<std::pair<AudioFile, ghc::filesystem::path>> &GetAllFiles() { return m_all_files; }

  private:
    static std::vector<std::pair<bool, std::unique_ptr<ExpandablePathname>>>
    Parse(std::string_view pathnames) {
        std::vector<std::pair<bool, std::unique_ptr<ExpandablePathname>>> result;
        ForEachCommaDelimitedSection(pathnames, [&](std::string_view section) {
            const bool is_exclude = section[0] == '-';
            if (is_exclude) {
                section.remove_prefix(1);
            }

            if (section.find('*') != std::string::npos) {
                result.push_back({is_exclude, std::make_unique<ExpandablePatternPathname>(section)});
            } else if (ghc::filesystem::is_directory(std::string(section))) {
                result.push_back({is_exclude, std::make_unique<ExpandableDirectoryPathname>(section)});
            } else if (ghc::filesystem::is_regular_file(std::string(section))) {
                result.push_back({is_exclude, std::make_unique<SingleFilePathname>(section)});
            } else {
                throw CLI::ValidationError("Input filename", "The input filename " + std::string(section) +
                                                                 " is neither a file, directory, or pattern");
            }
        });
        return result;
    }

    static void ForEachCommaDelimitedSection(std::string_view s,
                                             std::function<void(std::string_view)> callback) {
        const auto RegisterSection = [&](std::string_view section) {
            if (section.size() >= 2 && (section[0] == '"' || section[0] == '\'') &&
                (section.back() == '"' || section.back() == '\'')) {
                section.remove_prefix(1);
                section.remove_suffix(1);
            }
            callback(section);
        };

        size_t pos = 0;
        while ((pos = s.find(',')) != std::string::npos) {
            RegisterSection(s.substr(0, pos));
            s.remove_prefix(pos + 1);
        }
        RegisterSection(s);
    }

    std::vector<std::pair<AudioFile, ghc::filesystem::path>> m_all_files {};
    CanonicalPathSet m_all_matched_filesnames {};
    CanonicalPathSet m_all_exclude_matched_filesnames {};
    std::vector<std::pair<bool, std::unique_ptr<ExpandablePathname>>> m_expandables;
};
