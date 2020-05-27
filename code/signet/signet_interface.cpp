#include "signet_interface.h"

#include <cassert>
#include <functional>

#include "doctest.hpp"

#include "audio_file.h"
#include "subcommands/fader/fader.h"
#include "subcommands/normalise/normaliser.h"
#include "subcommands/zcross_offsetter/zcross_offsetter.h"
#include "tests_config.h"

SignetInterface::SignetInterface() {
    m_subcommands.push_back(std::make_unique<Fader>());
    m_subcommands.push_back(std::make_unique<Normaliser>());
    m_subcommands.push_back(std::make_unique<ZeroCrossingOffsetter>());
}

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

static auto GetAllMatchingPaths(MultiplePatternMatchingFilenames<> &patterns) {
    std::vector<ghc::filesystem::path> result;
    REQUIRE(patterns.GetNumPatterns() != 0);
    for (usize i = 0; i < patterns.GetNumPatterns(); ++i) {
        switch (patterns.GetMode(i)) {
            case PatternMode::Pattern:
            case PatternMode::Directory: {
                const auto root_dir = patterns.GetRootDirectory(i);
                std::cout << root_dir << " root dir\n";
                const auto paths = GetAllAudioFilesInDirectoryRecursively(root_dir);
                for (const auto &p : paths) {
                    if (patterns.Matches(i, p) == MultiplePatternMatchingFilenames<>::MatchResult::Yes)
                        result.push_back(p);
                }
                break;
            }
            case PatternMode::File: {
                result.push_back(patterns.GetPattern(i));
                break;
            }
            default: WarningWithNewLine("pattern is not valid ", patterns.GetPattern(i), "\n");
        }
    }
    return result;
}

int SignetInterface::Main(const int argc, const char *const argv[]) {
    std::cout << "\n\n";
    CLI::App app {"Tools for processing audio files"};

    app.require_subcommand();
    app.set_help_all_flag("--help-all", "Print help message for all subcommands");
    app.add_flag_callback(
        "--load-backup",
        [this]() {
            std::cout << "Loading most recent backup...\n";
            m_backup.LoadBackup();
            std::cout << "Done.\n";
            throw CLI::Success();
        },
        "Load the most recent backup");

    app.add_option("input-file-or-directory", m_input_filepath_pattern, "The file or directory to read from")
        ->required();
    app.add_option("output-wave-filename", m_output_filepath,
                   "The filename to write to - only relevant if the input file is not a directory");

    std::vector<CLI::App *> subcommand_clis;
    for (auto &subcommand : m_subcommands) {
        subcommand_clis.push_back(subcommand->CreateSubcommandCLI(app));
    }

    CLI11_PARSE(app, argc, argv);

    m_backup.ResetBackup(); // if we have gotten here we must not be wanting to load a backup

    if (IsProcessingMultipleFiles()) {
        if (m_output_filepath) {
            FatalErrorWithNewLine(
                "the input path is a directory or pattern, there must be no output filepath");
        }
    } else {
        if (m_output_filepath) {
            if (ghc::filesystem::is_directory(*m_output_filepath)) {
                FatalErrorWithNewLine(
                    "output filepath cannot be a directory if the input filepath is a file");
            } else if (m_output_filepath->extension() != ".wav") {
                FatalErrorWithNewLine("only WAV files can be written");
            }
        }
    }

    m_all_matched_files = GetAllMatchingPaths(m_input_filepath_pattern);
    if (!m_all_matched_files.size()) {
        WarningWithNewLine("There are no matching files for this input\n");
        return 1;
    }

    for (size_t i = 0; i < m_subcommands.size(); ++i) {
        if (subcommand_clis[i]->parsed()) {
            m_subcommands[i]->Run(*this);
        }
    }

    return m_num_files_processed != 0 ? 0 : 1;
}

void SignetInterface::ProcessAllFiles(Subcommand &subcommand) {
    for (const auto &p : m_all_matched_files) {
        ProcessFile(subcommand, p, m_output_filepath);
    }
}

void SignetInterface::ProcessFile(Subcommand &subcommand,
                                  const ghc::filesystem::path &input_filepath,
                                  std::optional<ghc::filesystem::path> output_filepath) {
    if (!output_filepath) {
        *output_filepath = input_filepath;
        output_filepath->replace_extension(".wav");
    }
    assert(!input_filepath.empty());

    if (const auto audio_file = ReadAudioFile(input_filepath)) {
        if (const auto new_audio_file = subcommand.Process(*audio_file, *output_filepath)) {
            if (*output_filepath == input_filepath) {
                m_backup.AddFileToBackup(input_filepath);
            }
            if (!WriteWaveFile(*output_filepath, *new_audio_file)) {
                FatalErrorWithNewLine("could not write the wave file ", *output_filepath);
            }
            std::cout << "Successfully wrote file " << *output_filepath << "\n";
            m_num_files_processed++;
        }
    }

    std::cout << "\n";
}

TEST_CASE("[SignetInterface]") {
    SignetInterface signet;

    SUBCASE("args") {
        SUBCASE("single file absolute filename writing to output file") {
            const auto args = {
                "signet", TEST_DATA_DIRECTORY "/test.wav", TEST_DATA_DIRECTORY "/test-out.wav", "fade", "in",
                "50smp"};
            REQUIRE(signet.Main((int)args.size(), args.begin()) == 0);
        }

        SUBCASE("single file relative filename overwrite") {
            const auto args = {"signet", "../test_data/test-out.wav", "norm", "3"};
            REQUIRE(signet.Main((int)args.size(), args.begin()) == 0);
        }

        SUBCASE("single file with single output that is not a wav") {
            const auto args = {
                "signet", TEST_DATA_DIRECTORY "/test.wav", TEST_DATA_DIRECTORY "/test-out.ogg", "fade", "in",
                "50smp"};
            REQUIRE_THROWS(signet.Main((int)args.size(), args.begin()));
        }

        std::string test_folder = "test-folder";
        if (!ghc::filesystem::is_directory(test_folder)) {
            ghc::filesystem::create_directory(test_folder);
        }
        ghc::filesystem::copy_file(TEST_DATA_DIRECTORY "/test.wav", test_folder + "/test.wav",
                                   ghc::filesystem::copy_options::update_existing);
        ghc::filesystem::copy_file(TEST_DATA_DIRECTORY "/test.wav", test_folder + "/test_other.wav",
                                   ghc::filesystem::copy_options::update_existing);
        ghc::filesystem::copy_file(TEST_DATA_DIRECTORY "/test.wav", test_folder + "/test_other2.wav",
                                   ghc::filesystem::copy_options::update_existing);

        SUBCASE("when the input file is a single file the output cannot be a directory") {
            const auto args = {"signet", "../test_data/test-out.wav", "test-folder", "norm", "3"};
            REQUIRE_THROWS(signet.Main((int)args.size(), args.begin()));
        }

        SUBCASE("match all WAVs in a dir by using a wildcard") {
            const auto wildcard = test_folder + "/*.wav";
            const auto args = {"signet", wildcard.data(), "norm", "3"};
            REQUIRE(signet.Main((int)args.size(), args.begin()) == 0);
        }

        SUBCASE("match all WAVs by using a wildcard") {
            const auto args = {"signet", "*.wav", "norm", "3"};
            REQUIRE(signet.Main((int)args.size(), args.begin()) == 0);
        }

        SUBCASE("when the input path is a pattern there cannot be an output file") {
            const auto wildcard = test_folder + "/*.wav";
            const auto args = {"signet", wildcard.data(), "output.wav", "norm", "3"};
            REQUIRE_THROWS(signet.Main((int)args.size(), args.begin()));
        }

        SUBCASE("when input path is a patternless directory scan all files in that") {
            const auto wildcard = test_folder;
            const auto args = {"signet", wildcard.data(), "norm", "3"};
            REQUIRE(signet.Main((int)args.size(), args.begin()) == 0);
        }

        SUBCASE("when input path is a patternless directory with ending slash scan all files in that") {
            const auto wildcard = test_folder + "/";
            const auto args = {"signet", wildcard.data(), "norm", "3"};
            REQUIRE(signet.Main((int)args.size(), args.begin()) == 0);
        }

        SUBCASE("load backup") {
            const auto args = {"signet", "--load-backup"};
            REQUIRE(signet.Main((int)args.size(), args.begin()) == 0);
        }

        SUBCASE("multiple comma separated files") {
            const auto wildcard = test_folder + "/test.wav," + test_folder + "/test_other.wav";
            const auto args = {"signet", wildcard.data(), "norm", "3"};
            REQUIRE(signet.Main((int)args.size(), args.begin()) == 0);
        }
    }
}

TEST_CASE("[PatternMatchingFilename]") {
    SUBCASE("absolute path with pattern in final dir") {
        PatternMatchingFilename<CheckDummyFilesystem> p("/foo/bar/*.wav");
        REQUIRE(p.GetMode() == PatternMode::Pattern);
        REQUIRE(p.MatchesRaw("/foo/bar/file.wav"));
        REQUIRE(!p.MatchesRaw("/foo/bbar/file.wav"));
        REQUIRE(p.GetRootDirectory() == "/foo/bar");
    }
    SUBCASE("match all wavs") {
        PatternMatchingFilename<CheckDummyFilesystem> p("*.wav");
        REQUIRE(p.GetMode() == PatternMode::Pattern);
        REQUIRE(p.MatchesRaw("foodledoo.wav"));
        REQUIRE(p.MatchesRaw("inside/dirs/foo.wav"));
        REQUIRE(!p.MatchesRaw("notawav.flac"));
        REQUIRE(p.GetRootDirectory() == ".");
    }
    SUBCASE("no pattern") {
        PatternMatchingFilename<CheckDummyFilesystem> p("file.wav");
        REQUIRE(p.GetMode() == PatternMode::File);
        REQUIRE(p.MatchesRaw("file.wav"));
        REQUIRE(!p.MatchesRaw("dir/file.wav"));
        REQUIRE(p.GetRootDirectory() == ".");
    }
    SUBCASE("dirs that have a subfolder called subdir") {
        PatternMatchingFilename<CheckDummyFilesystem> p("*/subdir/*");
        REQUIRE(p.GetMode() == PatternMode::Pattern);
        REQUIRE(p.MatchesRaw("foo/subdir/file.wav"));
        REQUIRE(p.MatchesRaw("bar/subdir/file.wav"));
        REQUIRE(p.MatchesRaw("bar/subdir/subsubdir/file.wav"));
        REQUIRE(!p.MatchesRaw("subdir/subsubdir/file.wav"));
        REQUIRE(!p.MatchesRaw("foo/subdir"));
        REQUIRE(!p.MatchesRaw("subdir/file.wav"));
        REQUIRE(p.GetRootDirectory() == ".");
    }
    SUBCASE("dir with no pattern") {
        PatternMatchingFilename<CheckDummyFilesystem> p("c:/tools");
        REQUIRE(p.GetMode() == PatternMode::Directory);
        REQUIRE(p.MatchesRaw("c:/tools"));
        REQUIRE(!p.MatchesRaw("c:/tools/file.wav"));
        REQUIRE(!p.MatchesRaw("c:/tool"));
        REQUIRE(p.GetRootDirectory() == "c:/tools");
    }
    SUBCASE("multiple filenames") {
        MultiplePatternMatchingFilenames<CheckDummyFilesystem> p("foo.wav,bar.wav");
        REQUIRE(p.IsPattern());
        REQUIRE(!p.IsSingleFile());
        REQUIRE(p.GetNumPatterns() == 2);
        REQUIRE(p.Matches(0, "foo.wav") ==
                MultiplePatternMatchingFilenames<CheckDummyFilesystem>::MatchResult::Yes);
        REQUIRE(p.Matches(0, "foo.wav") ==
                MultiplePatternMatchingFilenames<CheckDummyFilesystem>::MatchResult::AlreadyMatched);
        REQUIRE(p.Matches(1, "bar.wav") ==
                MultiplePatternMatchingFilenames<CheckDummyFilesystem>::MatchResult::Yes);
        REQUIRE(p.Matches(1, "barrr.wav") ==
                MultiplePatternMatchingFilenames<CheckDummyFilesystem>::MatchResult::No);
        REQUIRE(p.GetRootDirectory(0) == ".");
        REQUIRE(p.GetRootDirectory(1) == ".");
    }
    SUBCASE("multiple patterns") {
        MultiplePatternMatchingFilenames<CheckDummyFilesystem> p("code/subdirs/*,build/*.wav,*.flac");
        REQUIRE(p.IsPattern());
        REQUIRE(!p.IsSingleFile());
        REQUIRE(p.GetNumPatterns() == 3);
        REQUIRE(p.GetRootDirectory(0) == "code/subdirs");
        REQUIRE(p.GetRootDirectory(1) == "build");
        REQUIRE(p.GetRootDirectory(2) == ".");
        REQUIRE(p.Matches(0, "code/subdirs/file.flac") ==
                MultiplePatternMatchingFilenames<CheckDummyFilesystem>::MatchResult::Yes);
        REQUIRE(p.Matches(2, "code/subdirs/file.flac") ==
                MultiplePatternMatchingFilenames<CheckDummyFilesystem>::MatchResult::AlreadyMatched);
        REQUIRE(p.Matches(1, "build/file.wav") ==
                MultiplePatternMatchingFilenames<CheckDummyFilesystem>::MatchResult::Yes);
        REQUIRE(p.Matches(2, "foo.flac") ==
                MultiplePatternMatchingFilenames<CheckDummyFilesystem>::MatchResult::Yes);
        REQUIRE(p.Matches(2, "foo.wav") ==
                MultiplePatternMatchingFilenames<CheckDummyFilesystem>::MatchResult::No);
    }
    SUBCASE("multiple-pattern object with just a single unpatterned filename") {
        MultiplePatternMatchingFilenames<CheckDummyFilesystem> p("file.wav");
        REQUIRE(p.GetNumPatterns() == 1);
        REQUIRE(!p.IsPattern());
        REQUIRE(p.Matches(0, "file.wav") ==
                MultiplePatternMatchingFilenames<CheckDummyFilesystem>::MatchResult::Yes);
        REQUIRE(p.Matches(0, "dir/file.wav") ==
                MultiplePatternMatchingFilenames<CheckDummyFilesystem>::MatchResult::No);
        REQUIRE(p.GetRootDirectory(0) == ".");
        REQUIRE(p.IsSingleFile());
    }
    SUBCASE("multiple-pattern object with just a single pattern") {
        MultiplePatternMatchingFilenames<CheckDummyFilesystem> p("test-folder/*.wav");
        REQUIRE(p.GetNumPatterns() == 1);
        REQUIRE(p.IsPattern());
        REQUIRE(p.Matches(0, "test-folder/file.wav") ==
                MultiplePatternMatchingFilenames<CheckDummyFilesystem>::MatchResult::Yes);
        REQUIRE(p.GetRootDirectory(0) == "test-folder");
        REQUIRE(!p.IsSingleFile());
    }
    SUBCASE("pattern with an ending slash") {
        MultiplePatternMatchingFilenames<CheckDummyFilesystem> p("test-folder/");
        REQUIRE(p.GetNumPatterns() == 1);
        REQUIRE(!p.IsSingleFile());
        REQUIRE(!p.IsPattern());
        REQUIRE(p.GetRootDirectory(0) == "test-folder/");
        REQUIRE(p.Matches(0, "test-folder/file.wav") ==
                MultiplePatternMatchingFilenames<CheckDummyFilesystem>::MatchResult::Yes);
        REQUIRE(p.Matches(0, "test-folder/foo.wav") ==
                MultiplePatternMatchingFilenames<CheckDummyFilesystem>::MatchResult::Yes);
    }
}

TEST_CASE("[SignetBackup]") {
    {
        SignetBackup b;
        b.ResetBackup();
        b.AddFileToBackup(TEST_DATA_DIRECTORY "/test.wav");
    }
    {
        SignetBackup b;
        REQUIRE(b.LoadBackup());
    }
}
