#include "filepath_set.h"

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

static void ForEachFileInDirectory(const std::string_view directory,
                                   const bool recursively,
                                   const std::function<void(const fs::path &)> callback) {
    if (recursively) {
        ForEachFileInDirectory<fs::recursive_directory_iterator>(directory, callback);
    } else {
        ForEachFileInDirectory<fs::directory_iterator>(directory, callback);
    }
}

static bool IsPathExcluded(const fs::path &path, const std::vector<std::string> &exclude_patterns) {
    for (const auto &exclude : exclude_patterns) {
        if (WildcardMatch(exclude, path.generic_string())) {
            return true;
        }
    }
    return false;
}

static std::vector<fs::path> GetAllFilepathsInDirectory(const std::string_view dir, const bool recursively) {
    std::vector<fs::path> filepaths;
    const auto parent_directory = fs::path(std::string(dir));
    ForEachFileInDirectory(dir, recursively, [&](const auto filepath) {
        if (parent_directory.compare(filepath) < 0) {
            filepaths.push_back(filepath);
        }
    });
    return filepaths;
}

void FilepathSet::AddNonExcludedPaths(const tcb::span<const fs::path> paths,
                                      const std::vector<std::string> &exclude_patterns) {
    for (const auto &path : paths) {
        if (!IsPathExcluded(path, exclude_patterns)) {
            Add(path);
        }
    }
}

std::optional<FilepathSet>
FilepathSet::CreateFromPaths(const std::vector<std::string> &input_paths,
                             const std::vector<std::string> &exclude_patterns,
                             bool recursive_directory_search,
                             std::string *error) {
    FilepathSet set {};
    for (const auto &input : input_paths) {
        if (fs::is_directory(input)) {
            MessageWithNewLine("Signet", {}, "Searching for files {} in the directory {}",
                               recursive_directory_search ? "recursively" : "non-recursively", input);
            const auto matching_paths = GetAllFilepathsInDirectory(input, recursive_directory_search);
            set.AddNonExcludedPaths(matching_paths, exclude_patterns);
        } else if (fs::is_regular_file(input)) {
            fs::path path {input};
            set.AddNonExcludedPaths({&path, 1}, exclude_patterns);
        } else {
            if (error) {
                *error = "no such file or directory: " + input;
            }
            return {};
        }
    }
    if (!recursive_directory_search && input_paths.size() == 1 &&
        fs::is_directory(std::string(input_paths[0]))) {
        MessageWithNewLine(
            "Signet", {},
            "Use the option --recursive to search in all subdirecties of the given one as well.");
    }

    return set;
}

TEST_CASE("FilepathSet") {
#ifdef CreateFile
#undef CreateFile
#endif
    std::error_code ec;
    fs::remove_all("sandbox", ec);

    const auto CreateDir = [](const char *name) {
        if (!fs::is_directory(name)) {
            fs::create_directory(name);
        }
    };
    const auto CreateFile = [](const char *name) { OpenFile(name, "wb"); };

    CreateDir("sandbox");
    CreateFile("sandbox/file1.wav");
    CreateFile("sandbox/file2.wav");
    CreateFile("sandbox/foo.wav");

    CreateDir("sandbox/sub");
    CreateFile("sandbox/sub/nested.wav");
    CreateFile("sandbox/sub/nested.flac");

    const auto CanonicalSet = [](const FilepathSet &set) {
        std::vector<std::string> out;
        for (const auto &p : set) out.push_back(fs::canonical(p).generic_string());
        std::sort(out.begin(), out.end());
        return out;
    };

    const auto CanonicalExpected = [](std::initializer_list<const std::string> expected) {
        std::vector<std::string> out;
        for (auto e : expected) out.push_back(fs::canonical(e).generic_string());
        std::sort(out.begin(), out.end());
        return out;
    };

    SUBCASE("single file") {
        std::string err;
        auto set = FilepathSet::CreateFromPaths({"sandbox/file1.wav"}, {}, false, &err);
        CAPTURE(err);
        REQUIRE(set);
        REQUIRE(CanonicalSet(*set) == CanonicalExpected({"sandbox/file1.wav"}));
    }

    SUBCASE("multiple files") {
        std::string err;
        auto set =
            FilepathSet::CreateFromPaths({"sandbox/file1.wav", "sandbox/file2.wav"}, {}, false, &err);
        CAPTURE(err);
        REQUIRE(set);
        REQUIRE(CanonicalSet(*set) ==
                CanonicalExpected({"sandbox/file1.wav", "sandbox/file2.wav"}));
    }

    SUBCASE("directory non-recursive") {
        std::string err;
        auto set = FilepathSet::CreateFromPaths({"sandbox"}, {}, false, &err);
        CAPTURE(err);
        REQUIRE(set);
        REQUIRE(CanonicalSet(*set) == CanonicalExpected({"sandbox/file1.wav", "sandbox/file2.wav",
                                                         "sandbox/foo.wav"}));
    }

    SUBCASE("directory recursive") {
        std::string err;
        auto set = FilepathSet::CreateFromPaths({"sandbox"}, {}, true, &err);
        CAPTURE(err);
        REQUIRE(set);
        REQUIRE(CanonicalSet(*set) ==
                CanonicalExpected({"sandbox/file1.wav", "sandbox/file2.wav", "sandbox/foo.wav",
                                   "sandbox/sub/nested.wav", "sandbox/sub/nested.flac"}));
    }

    SUBCASE("exclude by glob pattern against path") {
        std::string err;
        auto set = FilepathSet::CreateFromPaths({"sandbox"}, {"**.wav"}, true, &err);
        CAPTURE(err);
        REQUIRE(set);
        REQUIRE(CanonicalSet(*set) == CanonicalExpected({"sandbox/sub/nested.flac"}));
    }

    SUBCASE("exclude by exact path") {
        std::string err;
        auto set = FilepathSet::CreateFromPaths({"sandbox"}, {"sandbox/foo.wav"}, false, &err);
        CAPTURE(err);
        REQUIRE(set);
        REQUIRE(CanonicalSet(*set) ==
                CanonicalExpected({"sandbox/file1.wav", "sandbox/file2.wav"}));
    }

    SUBCASE("exclude with subdirectory glob") {
        std::string err;
        auto set = FilepathSet::CreateFromPaths({"sandbox"}, {"sandbox/sub/*"}, true, &err);
        CAPTURE(err);
        REQUIRE(set);
        REQUIRE(CanonicalSet(*set) == CanonicalExpected({"sandbox/file1.wav", "sandbox/file2.wav",
                                                         "sandbox/foo.wav"}));
    }

    SUBCASE("nonexistent path produces error") {
        std::string err;
        auto set = FilepathSet::CreateFromPaths({"sandbox/does-not-exist.wav"}, {}, false, &err);
        REQUIRE(!set);
        REQUIRE(err.find("no such file") != std::string::npos);
    }
}
