#include "pathname_expansion.h"

#include "doctest.hpp"

#include "common.h"
#include "string_utils.h"
#include "tests_config.h"

template <typename DirectoryIterator>
static void ForEachFileInDirectory(const std::string_view directory,
                                   const std::function<void(const fs::path &)> callback) {
    for (const auto &entry : DirectoryIterator(directory)) {
        const auto &path = entry.path();
        if (!fs::is_directory(path)) {
            callback(path);
        }
    }
}

void ForEachFileInDirectory(const std::string_view directory,
                            const bool recursively,
                            const std::function<void(const fs::path &)> callback) {
    if (recursively) {
        ForEachFileInDirectory<fs::recursive_directory_iterator>(directory, callback);
    } else {
        ForEachFileInDirectory<fs::directory_iterator>(directory, callback);
    }
}

const bool IsPathExcluded(const fs::path &path, const std::vector<std::string_view> &exclude_patterns) {
    for (const auto exclude : exclude_patterns) {
        if (WildcardMatch(exclude, path.generic_string())) {
            return true;
        }
    }
    return false;
}

static std::vector<fs::path> GetAudioFilePathsThatMatchPattern(std::string_view pattern) {
    namespace fs = fs;

    std::string generic_pattern {pattern};
    Replace(generic_pattern, '\\', '/');
    pattern = generic_pattern;

    std::string added_slash;
    if (pattern.find('/') == std::string_view::npos) {
        added_slash = "./" + std::string(pattern);
        pattern = added_slash;
    }

    std::vector<std::string> possible_folders;

    size_t pos = 0;
    size_t prev_pos = 0;
    while ((pos = pattern.find('/', pos)) != std::string_view::npos) {
        const auto folder = pattern.substr(0, pos);
        const auto part = pattern.substr(prev_pos, pos - prev_pos);

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
                            if (WildcardMatch(folder, path)) {
                                new_possible_folders.push_back(path);
                            }
                        }
                    }
                } else if (part.find('*') != std::string_view::npos) {
                    for (const auto &entry : fs::directory_iterator(f)) {
                        if (entry.is_directory()) {
                            const auto path = entry.path().generic_string();
                            if (WildcardMatch(folder, path)) {
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

    const std::string_view last_file_section = pattern.substr(prev_pos);

    std::vector<fs::path> result;
    const auto CheckAndRegisterFile = [&](auto path) {
        const auto generic = path.generic_string();
        if (WildcardMatch(pattern, generic)) {
            result.push_back(path);
        }
    };

    for (const auto f : possible_folders) {
        if (last_file_section.find("**") != std::string_view::npos) {
            ForEachFileInDirectory<fs::recursive_directory_iterator>(f, CheckAndRegisterFile);
        } else if (last_file_section.find("*") != std::string_view::npos) {
            ForEachFileInDirectory<fs::directory_iterator>(f, CheckAndRegisterFile);
        } else {
            fs::path path = f;
            path /= std::string(last_file_section);
            CheckAndRegisterFile(path);
        }
    }

    return result;
}

static std::vector<fs::path> GetAudioFilePathsInDirectory(const std::string_view dir,
                                                          const bool recursively) {
    std::vector<fs::path> result;
    const auto parent_directory = fs::path(std::string(dir));
    ForEachFileInDirectory(dir, recursively, [&](const auto path) {
        if (parent_directory.compare(path) < 0) {
            result.push_back(path);
        }
    });
    return result;
}

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

void FilePathSet::AddNonExcludedPaths(const tcb::span<const fs::path> paths,
                                      const std::vector<std::string_view> &exclude_patterns) {
    for (const auto &path : paths) {
        if (!IsPathExcluded(path, exclude_patterns)) {
            Add(path);
        }
    }
}

std::optional<FilePathSet> FilePathSet::CreateFromPatterns(const std::string_view comma_delimed_parts,
                                                           bool recursive_directory_search,
                                                           std::string *error) {
    std::vector<std::string_view> include_parts;
    std::vector<std::string_view> exclude_paths;
    GetAllCommaDelimitedSections(comma_delimed_parts, include_parts, exclude_paths);

    FilePathSet set {};
    for (const auto &include_part : include_parts) {
        if (include_part.find('*') != std::string_view::npos) {
            MessageWithNewLine("Signet", "Searching for audio files using the pattern ", include_part);
            const auto matching_paths = GetAudioFilePathsThatMatchPattern(include_part);
            set.AddNonExcludedPaths(matching_paths, exclude_paths);
            set.m_num_wildcard_parts++;
        } else if (fs::is_directory(std::string(include_part))) {
            MessageWithNewLine("Signet", "Searching for audio files ",
                               recursive_directory_search ? "recursively" : "non-recursively",
                               " in the directory ", include_part);
            const auto matching_paths =
                GetAudioFilePathsInDirectory(include_part, recursive_directory_search);
            set.AddNonExcludedPaths(matching_paths, exclude_paths);
            set.m_num_directory_parts++;
        } else if (fs::is_regular_file(std::string(include_part))) {
            fs::path path {include_part};
            set.AddNonExcludedPaths({&path, 1}, exclude_paths);
            set.m_num_file_parts++;
        } else {
            if (error) {
                *error = "The input part " + std::string(include_part) +
                         " is neither a file, directory, or pattern";
            }
            return {};
        }
    }
    MessageWithNewLine("Signet", "Found ", set.Size(), " matching audio files");

    return set;
}

TEST_CASE("Pathname Expansion") {
#ifdef CreateFile
#undef CreateFile
#endif
    namespace fs = fs;
    const auto CreateDir = [](const char *name) {
        if (!fs::is_directory(name)) {
            fs::create_directory(name);
        }
    };
    const auto CreateFile = [](const char *name) { OpenFile(name, "wb"); };

    CreateDir("sandbox");
    CreateFile("sandbox/file1.wav");
    CreateFile("sandbox/file2.wav");
    CreateFile("sandbox/file3.wav");
    CreateFile("sandbox/foo.wav");

    CreateDir("sandbox/unprocessed-piano");
    CreateFile("sandbox/unprocessed-piano/hello.wav");
    CreateFile("sandbox/unprocessed-piano/there.wav");

    CreateDir("sandbox/unprocessed-piano/copies");
    CreateDir("sandbox/unprocessed-piano/copies/session1");
    CreateDir("sandbox/unprocessed-piano/copies/foo");
    CreateFile("sandbox/unprocessed-piano/copies/foo/file.flac");
    CreateFile("sandbox/unprocessed-piano/copies/session1/file.wav");

    CreateDir("sandbox/unprocessed-keys");
    CreateDir("sandbox/unprocessed-keys/copies");
    CreateDir("sandbox/unprocessed-keys/copies/session1");
    CreateFile("sandbox/unprocessed-keys/copies/session1/file.wav");

    CreateDir("sandbox/unprocessed-keys/copies/foo");

    CreateDir("sandbox/processed");
    CreateFile("sandbox/processed/file.wav");
    CreateFile("sandbox/processed/file.flac");

    const auto CheckMatches = [](const std::string pattern,
                                 const std::initializer_list<const std::string> expected_matches) {
        std::string parse_error;
        const auto matches = FilePathSet::CreateFromPatterns(pattern, false, &parse_error);
        CAPTURE(parse_error);
        CAPTURE(pattern);
        REQUIRE(matches);

        std::vector<std::string> canonical_matches;
        for (const auto &match : *matches) {
            canonical_matches.push_back(fs::canonical(match).generic_string());
        }
        std::sort(canonical_matches.begin(), canonical_matches.end());

        std::vector<std::string> canonical_expected;
        for (auto expected : expected_matches) {
            canonical_expected.push_back(fs::canonical(expected).generic_string());
        }
        std::sort(canonical_expected.begin(), canonical_expected.end());

        REQUIRE(canonical_matches.size() == canonical_expected.size());
        for (usize i = 0; i < canonical_expected.size(); i++) {
            REQUIRE(canonical_matches[i] == canonical_expected[i]);
        }
    };

    SUBCASE("all wavs in a folder") {
        CheckMatches("sandbox/*.wav",
                     {"sandbox/file1.wav", "sandbox/file2.wav", "sandbox/file3.wav", "sandbox/foo.wav"});
    }
    SUBCASE("all wavs in a folder recursively") {
        CheckMatches("sandbox/**.wav", {
                                           "sandbox/file1.wav",
                                           "sandbox/file2.wav",
                                           "sandbox/file3.wav",
                                           "sandbox/foo.wav",
                                           "sandbox/unprocessed-piano/hello.wav",
                                           "sandbox/unprocessed-piano/there.wav",
                                           "sandbox/unprocessed-keys/copies/session1/file.wav",
                                           "sandbox/unprocessed-piano/copies/session1/file.wav",
                                           "sandbox/processed/file.wav",
                                       });
    }
    SUBCASE("two non recursive wildcards") {
        CheckMatches("sandbox/*/*.wav",
                     {"sandbox/unprocessed-piano/hello.wav", "sandbox/unprocessed-piano/there.wav",
                      "sandbox/processed/file.wav"});
    }
    SUBCASE("two complicated non recursive wildcards") {
        CheckMatches("sandbox/unprocessed-*/*.wav",
                     {"sandbox/unprocessed-piano/hello.wav", "sandbox/unprocessed-piano/there.wav"});
    }
    SUBCASE("three complicated non recursive wildcards") {
        CheckMatches("sandbox/unprocessed-*/*/session*/*.wav",
                     {"sandbox/unprocessed-piano/copies/session1/file.wav",
                      "sandbox/unprocessed-keys/copies/session1/file.wav"});
    }
    SUBCASE("recursive in the middle") {
        CheckMatches("sandbox/**/*.wav", {
                                             "sandbox/unprocessed-piano/hello.wav",
                                             "sandbox/unprocessed-piano/there.wav",
                                             "sandbox/unprocessed-keys/copies/session1/file.wav",
                                             "sandbox/unprocessed-piano/copies/session1/file.wav",
                                             "sandbox/processed/file.wav",
                                         });
    }
    SUBCASE("two recursive in the middle") {
        CheckMatches("sandbox/**/**/*.wav", {
                                                "sandbox/unprocessed-keys/copies/session1/file.wav",
                                                "sandbox/unprocessed-piano/copies/session1/file.wav",
                                            });
    }

    SUBCASE("canonical two recursive in the middle") {
        CheckMatches(fs::canonical("sandbox").generic_string() + "/**/**/*.wav",
                     {
                         "sandbox/unprocessed-keys/copies/session1/file.wav",
                         "sandbox/unprocessed-piano/copies/session1/file.wav",
                     });
    }

    SUBCASE("all wavs in a folder rescursively with absolute path") {
        CheckMatches(BUILD_DIRECTORY "/sandbox/**.wav",
                     {
                         "sandbox/file1.wav",
                         "sandbox/file2.wav",
                         "sandbox/file3.wav",
                         "sandbox/foo.wav",
                         "sandbox/unprocessed-piano/hello.wav",
                         "sandbox/unprocessed-piano/there.wav",
                         "sandbox/unprocessed-keys/copies/session1/file.wav",
                         "sandbox/unprocessed-piano/copies/session1/file.wav",
                         "sandbox/processed/file.wav",
                     });
    }
    SUBCASE("all files in a folder recursively excluding wavs") {
        CheckMatches("sandbox/**.*,-sandbox/**.wav", {
                                                         "sandbox/unprocessed-piano/copies/foo/file.flac",
                                                         "sandbox/processed/file.flac",
                                                     });
    }
    SUBCASE("all files in a folder recursively excluding wavs only in the top dir") {
        CheckMatches("sandbox/**.*,-sandbox/*.wav", {
                                                        "sandbox/unprocessed-piano/copies/foo/file.flac",
                                                        "sandbox/processed/file.flac",
                                                        "sandbox/unprocessed-piano/hello.wav",
                                                        "sandbox/unprocessed-piano/there.wav",
                                                        "sandbox/unprocessed-keys/copies/session1/file.wav",
                                                        "sandbox/unprocessed-piano/copies/session1/file.wav",
                                                        "sandbox/processed/file.wav",
                                                    });
    }
    SUBCASE("two non recursive wildcards excluding 1 dir") {
        CheckMatches("sandbox/*/*.wav,-sandbox/processed/*",
                     {"sandbox/unprocessed-piano/hello.wav", "sandbox/unprocessed-piano/there.wav"});
    }
}
