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

int SignetInterface::Main(const int argc, const char *const argv[]) {
    CLI::App app {"Tools for processing audio files"};

    app.require_subcommand();
    app.set_help_all_flag("--help-all", "Print help message for all subcommands");

    app.add_option("input-file-or-directory", m_input_filepath_pattern, "The file or directory to read from")
        ->required();
    app.add_option("output-wave-filename", m_output_filepath,
                   "The filename to write to - only relevant if the input file is not a directory");
    app.add_flag("-o,--overwrite", m_overwrite, "Overwrite files with their processed versions");

    std::vector<CLI::App *> subcommand_clis;
    for (auto &subcommand : m_subcommands) {
        subcommand_clis.push_back(subcommand->CreateSubcommandCLI(app));
    }

    CLI11_PARSE(app, argc, argv);

    if (IsProcessingMultipleFiles()) {
        if (m_output_filepath) {
            FatalErrorWithNewLine("the input path is a directory, there must be no output filepath - output "
                                  "files will be placed adjacent to originals");
        }
    } else {
        if (!m_overwrite && !m_output_filepath) {
            FatalErrorWithNewLine(
                "with a single input file you must specify either the --overwrite flag or an output file");
        }

        if (m_output_filepath) {
            if (ghc::filesystem::is_directory(*m_output_filepath)) {
                FatalErrorWithNewLine(
                    "output filepath cannot be a directory if the input filepath is a file");
            } else if (m_output_filepath->extension() != ".wav") {
                FatalErrorWithNewLine("only WAV files can be written");
            }
        }
    }

    for (size_t i = 0; i < m_subcommands.size(); ++i) {
        if (subcommand_clis[i]->parsed()) {
            m_subcommands[i]->Run(*this);
        }
    }

    return 0;
}

template <typename DirectoryIterator>
static void ForEachAudioFileInDirectory(const std::string &directory,
                                        std::function<void(const ghc::filesystem::path &)> callback) {
    std::vector<ghc::filesystem::path> paths;
    for (const auto &entry : DirectoryIterator(directory)) {
        const auto &path = entry.path();
        const auto ext = path.extension();
        if (ext == ".flac" || ext == ".wav") {
            paths.push_back(path);
        }
    }

    // We do this in a separate loop because the callback might write a file. We do not want to then process
    // it again.
    for (const auto &path : paths) {
        callback(path);
    }
    std::cout << "Processed " << paths.size() << (paths.size() == 1 ? " file\n\n" : " files\n\n");
}

static void ForEachAudioFileInDirectory(const std::string &directory,
                                        std::function<void(const ghc::filesystem::path &)> callback) {
    ForEachAudioFileInDirectory<ghc::filesystem::recursive_directory_iterator>(directory, callback);
}

void SignetInterface::ProcessAllFiles(Subcommand &subcommand) {
    if (IsProcessingMultipleFiles()) {
        ForEachAudioFileInDirectory(
            m_input_filepath_pattern.GetRootDirectory(),
            [&](const ghc::filesystem::path &path) { ProcessFile(subcommand, path, {}); });
    } else {
        ProcessFile(subcommand, m_input_filepath_pattern.GetPattern(), *m_output_filepath);
    }
}

void SignetInterface::ProcessFile(Subcommand &subcommand,
                                  const ghc::filesystem::path input_filepath,
                                  ghc::filesystem::path output_filepath) {
    if (!m_input_filepath_pattern.Matches(input_filepath.generic_string())) {
        return;
    }

    if (output_filepath.empty()) {
        output_filepath = input_filepath;
        output_filepath.replace_extension(".wav");
    }
    assert(!input_filepath.empty());

    if (const auto audio_file = ReadAudioFile(input_filepath)) {
        if (const auto new_audio_file = subcommand.Process(*audio_file, output_filepath)) {
            if (output_filepath == input_filepath) {
                m_backup.AddFileToBackup(input_filepath);
            }
            if (!WriteWaveFile(output_filepath, *new_audio_file)) {
                FatalErrorWithNewLine("could not write the wave file ", output_filepath);
            }
            std::cout << "Successfully wrote file " << output_filepath << "\n";
        }
    }

    std::cout << "\n";
}

TEST_CASE("[SignetInterface]") {
    SignetInterface signet;

    SUBCASE("args") {
        SUBCASE("single file absolute filename") {
            const auto args = {
                "signet", TEST_DATA_DIRECTORY "/test.wav", TEST_DATA_DIRECTORY "/test-out.wav", "fade", "in",
                "50smp"};
            REQUIRE(signet.Main((int)args.size(), args.begin()) == 0);
        }

        SUBCASE("single file relative filename") {
            const auto args = {"signet", "--overwrite", "../test_data/test-out.wav", "norm", "3"};
            REQUIRE(signet.Main((int)args.size(), args.begin()) == 0);
        }

        SUBCASE("single file must specify either overwrite or an outfile") {
            const auto args = {"signet", TEST_DATA_DIRECTORY "/test.wav", "norm", "3"};
            REQUIRE_THROWS(signet.Main((int)args.size(), args.begin()));
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
        ghc::filesystem::copy_file(TEST_DATA_DIRECTORY "/test.wav",
                                   ghc::filesystem::path(test_folder) / "test.wav",
                                   ghc::filesystem::copy_options::update_existing);
        ghc::filesystem::copy_file(TEST_DATA_DIRECTORY "/test.wav",
                                   ghc::filesystem::path(test_folder) / "test_other.wav",
                                   ghc::filesystem::copy_options::update_existing);
        ghc::filesystem::copy_file(TEST_DATA_DIRECTORY "/test.wav",
                                   ghc::filesystem::path(test_folder) / "test_other2.wav",
                                   ghc::filesystem::copy_options::update_existing);

        SUBCASE("match all WAVs by using a wildcard") {
            const auto wildcard = test_folder + "/*.wav";
            const auto args = {"signet", wildcard.data(), "norm", "3"};
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
    }
}

TEST_CASE("[PatternMatchingFilename]") {
    SUBCASE("absolute path with pattern in final dir") {
        PatternMatchingFilename p("/foo/bar/*.wav");
        REQUIRE(p.IsPattern());
        REQUIRE(p.MatchesRaw("/foo/bar/file.wav"));
        REQUIRE(!p.MatchesRaw("/foo/bbar/file.wav"));
        REQUIRE(p.GetRootDirectory() == "/foo/bar");
    }
    SUBCASE("match all wavs") {
        PatternMatchingFilename p("*.wav");
        REQUIRE(p.IsPattern());
        REQUIRE(p.MatchesRaw("foodledoo.wav"));
        REQUIRE(p.MatchesRaw("inside/dirs/foo.wav"));
        REQUIRE(!p.MatchesRaw("notawav.flac"));
        REQUIRE(p.GetRootDirectory() == ".");
    }
    SUBCASE("no pattern") {
        PatternMatchingFilename p("file.wav");
        REQUIRE(!p.IsPattern());
        REQUIRE(p.MatchesRaw("file.wav"));
        REQUIRE(!p.MatchesRaw("dir/file.wav"));
        REQUIRE(p.GetRootDirectory() == ".");
    }
    SUBCASE("dirs that have a subfolder called subdir") {
        PatternMatchingFilename p("*/subdir/*");
        REQUIRE(p.IsPattern());
        REQUIRE(p.MatchesRaw("foo/subdir/file.wav"));
        REQUIRE(p.MatchesRaw("bar/subdir/file.wav"));
        REQUIRE(p.MatchesRaw("bar/subdir/subsubdir/file.wav"));
        REQUIRE(!p.MatchesRaw("subdir/subsubdir/file.wav"));
        REQUIRE(!p.MatchesRaw("foo/subdir"));
        REQUIRE(!p.MatchesRaw("subdir/file.wav"));
        REQUIRE(p.GetRootDirectory() == ".");
    }
    SUBCASE("dir with no pattern") {
        PatternMatchingFilename p("c:/tools");
        REQUIRE(!p.IsPattern());
        REQUIRE(p.MatchesRaw("c:/tools"));
        REQUIRE(!p.MatchesRaw("c:/tools/file.wav"));
        REQUIRE(!p.MatchesRaw("c:/tool"));
        REQUIRE(p.GetRootDirectory() == "c:/tools");
    }
}

TEST_CASE("[SignetBackup]") {
    {
        SignetBackup b;
        b.ResetBackup();
        b.AddFileToBackup(TEST_DATA_DIRECTORY "/test(edited).wav");
        b.AddFileToBackup(TEST_DATA_DIRECTORY "/test-out(edited).wav");
    }
    {
        SignetBackup b;
        REQUIRE(b.LoadBackup());
    }
}
