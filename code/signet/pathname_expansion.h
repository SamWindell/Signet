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

class ExpandablePathnameListParser {
  public:
    ExpandablePathnameListParser(std::string_view list) : m_list(list) {}

    std::vector<std::unique_ptr<ExpandablePathname>> Parse() {
        std::vector<std::unique_ptr<ExpandablePathname>> result;
        ForEachCommaDelimitedSection(m_list, [&](std::string_view section) {
            if (section.find('*') != std::string::npos) {
                result.push_back(std::make_unique<ExpandablePatternPathname>(section));
            } else if (ghc::filesystem::is_directory(std::string(section))) {
                result.push_back(std::make_unique<ExpandableDirectoryPathname>(section));
            } else if (ghc::filesystem::is_regular_file(std::string(section))) {
                result.push_back(std::make_unique<SingleFilePathname>(section));
            } else {
                throw CLI::ValidationError("Input filename", "The input filename " + std::string(section) +
                                                                 " is neither a file, directory, or pattern");
            }
        });
        return result;
    }

  private:
    static void ForEachCommaDelimitedSection(std::string_view s,
                                             std::function<void(std::string_view)> callback) {
        size_t pos = 0;
        while ((pos = s.find(',')) != std::string::npos) {
            const auto section = s.substr(0, pos);
            callback(section);
            s.remove_prefix(pos + 1);
        }
        callback(s);
    }

    std::string_view m_list;
};

class ExpandedPathnames {
  public:
    ExpandedPathnames() {}
    ExpandedPathnames(std::string_view pathnames) : m_whole_list(pathnames) {
        ExpandablePathnameListParser parser(pathnames);
        m_expandables = std::move(parser.Parse());
    }

    bool IsSingleFile() const { return m_expandables.size() == 1 && m_expandables[0]->IsSingleFile(); }

    void BuildAllMatchesFilenames() {
        for (auto &e : m_expandables) {
            e->AddMatchingPathsToSet(m_all_matched_filesnames);
        }
    }

    const CanonicalPathSet &GetAllMatchedFilenames() const { return m_all_matched_filesnames; }

    std::string GetWholePattern() const { return m_whole_list; }

  private:
    CanonicalPathSet m_all_matched_filesnames {};
    std::string m_whole_list;
    std::vector<std::unique_ptr<ExpandablePathname>> m_expandables;
};
