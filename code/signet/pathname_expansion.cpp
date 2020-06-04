#include "pathname_expansion.h"

#include "doctest.hpp"

#include "tests_config.h"

TEST_CASE("Pathname Expansion") {
#ifdef CreateFile
#undef CreateFile
#endif
    namespace fs = ghc::filesystem;
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
                                 const std::vector<std::string_view> &exclude_filters,
                                 const std::initializer_list<const std::string> expected_matches) {
        CanonicalPathSet matches;
        ExpandablePatternPathname::AddMatchingPathsToSet(pattern, matches, exclude_filters);
        CAPTURE(pattern);

        std::vector<std::string> canonical_matches;
        for (const auto &match : matches) {
            canonical_matches.push_back(fs::canonical(match));
        }
        std::sort(canonical_matches.begin(), canonical_matches.end());

        std::vector<std::string> canonical_expected;
        for (auto expected : expected_matches) {
            canonical_expected.push_back(fs::canonical(expected));
        }
        std::sort(canonical_expected.begin(), canonical_expected.end());

        REQUIRE(canonical_matches.size() == canonical_expected.size());
        for (usize i = 0; i < canonical_expected.size(); i++) {
            REQUIRE(canonical_matches[i] == canonical_expected[i]);
        }
    };

    SUBCASE("all wavs in a folder") {
        CheckMatches("sandbox/*.wav", {},
                     {"sandbox/file1.wav", "sandbox/file2.wav", "sandbox/file3.wav", "sandbox/foo.wav"});
    }
    SUBCASE("all wavs in a folder recursively") {
        CheckMatches("sandbox/**.wav", {},
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
    SUBCASE("two non recursive wildcards") {
        CheckMatches("sandbox/*/*.wav", {},
                     {"sandbox/unprocessed-piano/hello.wav", "sandbox/unprocessed-piano/there.wav",
                      "sandbox/processed/file.wav"});
    }
    SUBCASE("two complicated non recursive wildcards") {
        CheckMatches("sandbox/unprocessed-*/*.wav", {},
                     {"sandbox/unprocessed-piano/hello.wav", "sandbox/unprocessed-piano/there.wav"});
    }
    SUBCASE("three complicated non recursive wildcards") {
        CheckMatches("sandbox/unprocessed-*/*/session*/*.wav", {},
                     {"sandbox/unprocessed-piano/copies/session1/file.wav",
                      "sandbox/unprocessed-keys/copies/session1/file.wav"});
    }
    SUBCASE("recursive in the middle") {
        CheckMatches("sandbox/**/*.wav", {},
                     {
                         "sandbox/unprocessed-piano/hello.wav",
                         "sandbox/unprocessed-piano/there.wav",
                         "sandbox/unprocessed-keys/copies/session1/file.wav",
                         "sandbox/unprocessed-piano/copies/session1/file.wav",
                         "sandbox/processed/file.wav",
                     });
    }
    SUBCASE("two recursive in the middle") {
        CheckMatches("sandbox/**/**/*.wav", {},
                     {
                         "sandbox/unprocessed-keys/copies/session1/file.wav",
                         "sandbox/unprocessed-piano/copies/session1/file.wav",
                     });
    }
    SUBCASE("all wavs in a folder rescursively with absolute path") {
        CheckMatches(BUILD_DIRECTORY "/sandbox/**.wav", {},
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
        CheckMatches("sandbox/**.*", {"sandbox/**.wav"},
                     {
                         "sandbox/unprocessed-piano/copies/foo/file.flac",
                         "sandbox/processed/file.flac",
                     });
    }
    SUBCASE("all files in a folder recursively excluding wavs only in the top dir") {
        CheckMatches("sandbox/**.*", {"sandbox/*.wav"},
                     {
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
        CheckMatches("sandbox/*/*.wav", {"sandbox/processed/*"},
                     {"sandbox/unprocessed-piano/hello.wav", "sandbox/unprocessed-piano/there.wav"});
    }
}

// TEST_CASE("[PatternMatchingFilename]") {
//     SUBCASE("absolute path with pattern in final dir") {
//         PatternMatchingFilename<CheckDummyFilesystem> p("/foo/bar/*.wav");
//         REQUIRE(p.GetMode() == PatternMode::Pattern);
//         REQUIRE(p.MatchesRaw("/foo/bar/file.wav"));
//         REQUIRE(!p.MatchesRaw("/foo/bbar/file.wav"));
//         REQUIRE(p.GetRootDirectory() == "/foo/bar");
//     }
//     SUBCASE("match all wavs") {
//         PatternMatchingFilename<CheckDummyFilesystem> p("*.wav");
//         REQUIRE(p.GetMode() == PatternMode::Pattern);
//         REQUIRE(p.MatchesRaw("foodledoo.wav"));
//         REQUIRE(p.MatchesRaw("inside/dirs/foo.wav"));
//         REQUIRE(!p.MatchesRaw("notawav.flac"));
//         REQUIRE(p.GetRootDirectory() == ".");
//     }
//     SUBCASE("no pattern") {
//         PatternMatchingFilename<CheckDummyFilesystem> p("file.wav");
//         REQUIRE(p.GetMode() == PatternMode::File);
//         REQUIRE(p.MatchesRaw("file.wav"));
//         REQUIRE(!p.MatchesRaw("dir/file.wav"));
//         REQUIRE(p.GetRootDirectory() == ".");
//     }
//     SUBCASE("dirs that have a subfolder called subdir") {
//         PatternMatchingFilename<CheckDummyFilesystem> p("*/subdir/*");
//         REQUIRE(p.GetMode() == PatternMode::Pattern);
//         REQUIRE(p.MatchesRaw("foo/subdir/file.wav"));
//         REQUIRE(p.MatchesRaw("bar/subdir/file.wav"));
//         REQUIRE(p.MatchesRaw("bar/subdir/subsubdir/file.wav"));
//         REQUIRE(!p.MatchesRaw("subdir/subsubdir/file.wav"));
//         REQUIRE(!p.MatchesRaw("foo/subdir"));
//         REQUIRE(!p.MatchesRaw("subdir/file.wav"));
//         REQUIRE(p.GetRootDirectory() == ".");
//     }
//     SUBCASE("dir with no pattern") {
//         PatternMatchingFilename<CheckDummyFilesystem> p("c:/tools");
//         REQUIRE(p.GetMode() == PatternMode::Directory);
//         REQUIRE(p.MatchesRaw("c:/tools"));
//         REQUIRE(!p.MatchesRaw("c:/tools/file.wav"));
//         REQUIRE(!p.MatchesRaw("c:/tool"));
//         REQUIRE(p.GetRootDirectory() == "c:/tools");
//     }
//     SUBCASE("multiple filenames") {
//         MultiplePatternMatchingFilenames<CheckDummyFilesystem> p("foo.wav,bar.wav");
//         REQUIRE(!p.IsSingleFile());
//         REQUIRE(p.GetNumPatterns() == 2);
//         REQUIRE(p.Matches(0, "foo.wav") ==
//                 MultiplePatternMatchingFilenames<CheckDummyFilesystem>::MatchResult::Yes);
//         REQUIRE(p.Matches(0, "foo.wav") ==
//                 MultiplePatternMatchingFilenames<CheckDummyFilesystem>::MatchResult::AlreadyMatched);
//         REQUIRE(p.Matches(1, "bar.wav") ==
//                 MultiplePatternMatchingFilenames<CheckDummyFilesystem>::MatchResult::Yes);
//         REQUIRE(p.Matches(1, "barrr.wav") ==
//                 MultiplePatternMatchingFilenames<CheckDummyFilesystem>::MatchResult::No);
//         REQUIRE(p.GetRootDirectory(0) == ".");
//         REQUIRE(p.GetRootDirectory(1) == ".");
//     }
//     SUBCASE("multiple patterns") {
//         MultiplePatternMatchingFilenames<CheckDummyFilesystem> p("code/subdirs/*,build/*.wav,*.flac");
//         REQUIRE(!p.IsSingleFile());
//         REQUIRE(p.GetNumPatterns() == 3);
//         REQUIRE(p.GetRootDirectory(0) == "code/subdirs");
//         REQUIRE(p.GetRootDirectory(1) == "build");
//         REQUIRE(p.GetRootDirectory(2) == ".");
//         REQUIRE(p.Matches(0, "code/subdirs/file.flac") ==
//                 MultiplePatternMatchingFilenames<CheckDummyFilesystem>::MatchResult::Yes);
//         REQUIRE(p.Matches(2, "code/subdirs/file.flac") ==
//                 MultiplePatternMatchingFilenames<CheckDummyFilesystem>::MatchResult::AlreadyMatched);
//         REQUIRE(p.Matches(1, "build/file.wav") ==
//                 MultiplePatternMatchingFilenames<CheckDummyFilesystem>::MatchResult::Yes);
//         REQUIRE(p.Matches(2, "foo.flac") ==
//                 MultiplePatternMatchingFilenames<CheckDummyFilesystem>::MatchResult::Yes);
//         REQUIRE(p.Matches(2, "foo.wav") ==
//                 MultiplePatternMatchingFilenames<CheckDummyFilesystem>::MatchResult::No);
//     }
//     SUBCASE("multiple-pattern object with just a single unpatterned filename") {
//         MultiplePatternMatchingFilenames<CheckDummyFilesystem> p("file.wav");
//         REQUIRE(p.GetNumPatterns() == 1);
//         REQUIRE(p.Matches(0, "file.wav") ==
//                 MultiplePatternMatchingFilenames<CheckDummyFilesystem>::MatchResult::Yes);
//         REQUIRE(p.Matches(0, "dir/file.wav") ==
//                 MultiplePatternMatchingFilenames<CheckDummyFilesystem>::MatchResult::No);
//         REQUIRE(p.GetRootDirectory(0) == ".");
//         REQUIRE(p.IsSingleFile());
//     }
//     SUBCASE("multiple-pattern object with just a single pattern") {
//         MultiplePatternMatchingFilenames<CheckDummyFilesystem> p("test-folder/*.wav");
//         REQUIRE(p.GetNumPatterns() == 1);
//         REQUIRE(p.Matches(0, "test-folder/file.wav") ==
//                 MultiplePatternMatchingFilenames<CheckDummyFilesystem>::MatchResult::Yes);
//         REQUIRE(p.GetRootDirectory(0) == "test-folder");
//         REQUIRE(!p.IsSingleFile());
//     }
//     SUBCASE("pattern with an ending slash") {
//         MultiplePatternMatchingFilenames<CheckDummyFilesystem> p("test-folder/");
//         REQUIRE(p.GetNumPatterns() == 1);
//         REQUIRE(!p.IsSingleFile());
//         REQUIRE(p.GetRootDirectory(0) == "test-folder/");
//         REQUIRE(p.Matches(0, "test-folder/file.wav") ==
//                 MultiplePatternMatchingFilenames<CheckDummyFilesystem>::MatchResult::Yes);
//         REQUIRE(p.Matches(0, "test-folder/foo.wav") ==
//                 MultiplePatternMatchingFilenames<CheckDummyFilesystem>::MatchResult::Yes);
//     }
// }
