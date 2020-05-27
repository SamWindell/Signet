#include "pathname_expansion.h"

#include "doctest.hpp"

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
