#include "signet_interface.h"

#include <functional>

#include "doctest.hpp"

#include "audio_file.h"
#include "subcommands/auto_tuner/auto_tuner.h"
#include "subcommands/converter/converter.h"
#include "subcommands/fader/fader.h"
#include "subcommands/folderiser/folderiser.h"
#include "subcommands/normaliser/normaliser.h"
#include "subcommands/pitch_detector/pitch_detector.h"
#include "subcommands/renamer/renamer.h"
#include "subcommands/silence_remover/silence_remover.h"
#include "subcommands/trimmer/trimmer.h"
#include "subcommands/tuner/tuner.h"
#include "subcommands/zcross_offsetter/zcross_offsetter.h"
#include "test_helpers.h"
#include "tests_config.h"

SignetInterface::SignetInterface() {
    m_subcommands.push_back(std::make_unique<Fader>());
    m_subcommands.push_back(std::make_unique<Normaliser>());
    m_subcommands.push_back(std::make_unique<ZeroCrossingOffsetter>());
    m_subcommands.push_back(std::make_unique<Converter>());
    m_subcommands.push_back(std::make_unique<SilenceRemover>());
    m_subcommands.push_back(std::make_unique<Trimmer>());
    m_subcommands.push_back(std::make_unique<PitchDetector>());
    m_subcommands.push_back(std::make_unique<Tuner>());
    m_subcommands.push_back(std::make_unique<AutoTuner>());
    m_subcommands.push_back(std::make_unique<Renamer>());
    m_subcommands.push_back(std::make_unique<Folderiser>());
}

int SignetInterface::Main(const int argc, const char *const argv[]) {
    CLI::App app {"Signet is a command-line program designed for bulk editing audio files. It features "
                  "common editing functions such as normalisation and fade-out, but also organisation "
                  "functions such as renaming files."};

    app.require_subcommand();
    app.set_help_all_flag("--help-all", "Print help message for all subcommands");
    app.add_flag_callback(
        "--load-backup",
        [this]() {
            MessageWithNewLine("Signet", "Loading most recent backup...");
            m_backup.LoadBackup();
            MessageWithNewLine("Signet", "Done.");
            throw CLI::Success();
        },
        "Load the most recent backup");

    app.add_flag("--recursive", m_recursive_directory_search,
                 "When the input is a directory, scan for files in it recursively");

    app.add_option_function<std::string>(
           "input-files",
           [&](const std::string &input) {
               m_input_audio_files = InputAudioFiles(input, m_recursive_directory_search);
           },
           "The audio files to process")
        ->required()
        ->type_name("STRING - a file, directory or glob pattern. To use multiple, separate each one with a "
                    "comma. You can exclude a pattern too by beginning it with a -. e.g. \"-*.wav\" to "
                    "exclude all .wav files.");

    std::vector<CLI::App *> subcommand_clis;
    for (auto &subcommand : m_subcommands) {
        auto s = subcommand->CreateSubcommandCLI(app);
        s->final_callback([&] { subcommand->Run(*this); });
    }

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        if (e.get_exit_code() != 0) {
            std::atexit([]() { std::cout << rang::style::reset; });
            std::cout << rang::fg::red;
        }
        return app.exit(e);
    }

    m_backup.ResetBackup(); // if we have gotten here we must not be wanting to load a backup

    if (m_num_files_processed) {
        if (!m_input_audio_files.WriteAllAudioFiles(m_backup)) {
            return 1;
        }
    }

    return (m_num_files_processed != 0) ? 0 : 1;
}

void SignetInterface::ProcessAllFiles(Subcommand &subcommand) {
    for (auto &file : m_input_audio_files.GetAllFiles()) {
        if (subcommand.ProcessAudio(file.file, file.new_filename)) {
            file.file_edited = true;
        }
        if (subcommand.ProcessFilename(file.path, file.file)) {
            file.renamed = true;
        }
        if (file.renamed || file.file_edited) {
            m_num_files_processed++;
        }
    }
}

TEST_CASE("[SignetInterface]") {
    SignetInterface signet;

    const auto in_file = std::string(TEST_DATA_DIRECTORY "/white-noise.wav");
    std::string test_folder = "test-folder";
    if (!fs::is_directory(test_folder)) {
        fs::create_directory(test_folder);
    }
    REQUIRE(fs::copy_file(TEST_DATA_DIRECTORY "/white-noise.wav", "test-folder/tf1.wav",
                          fs::copy_options::skip_existing));
    REQUIRE(fs::copy_file(TEST_DATA_DIRECTORY "/white-noise.wav", "test-folder/tf2.wav",
                          fs::copy_options::skip_existing));

    SUBCASE("args") {

        SUBCASE("single file absolute filename") {
            const auto args = TestHelpers::StringToArgs {"signet test-folder/tf1.wav fade in 50smp"};
            REQUIRE(signet.Main(args.Size(), args.Args()) == 0);
        }

        SUBCASE("single file relative filename") {
            const auto args = TestHelpers::StringToArgs {"signet test-folder/../test-folder/tf1.wav norm -3"};
            REQUIRE(signet.Main(args.Size(), args.Args()) == 0);
        }

        SUBCASE("match all WAVs in a dir by using a wildcard") {
            const auto args = TestHelpers::StringToArgs {"signet test-folder/*wav norm -3"};
            REQUIRE(signet.Main(args.Size(), args.Args()) == 0);
        }

        SUBCASE("when input path is a patternless directory scan all files in that") {
            const auto wildcard = test_folder;
            const auto args = TestHelpers::StringToArgs {"signet test-folder norm -3"};
            REQUIRE(signet.Main(args.Size(), args.Args()) == 0);
        }

        SUBCASE("when input path is a patternless directory with ending slash scan all files in that") {
            const auto args = TestHelpers::StringToArgs {"signet test-folder/ norm -3"};
            REQUIRE(signet.Main(args.Size(), args.Args()) == 0);
        }

        SUBCASE("load backup") {
            const auto args = TestHelpers::StringToArgs {"signet --load-backup"};
            REQUIRE(signet.Main(args.Size(), args.Args()) == 0);
        }

        SUBCASE("multiple comma separated files") {
            const auto args =
                TestHelpers::StringToArgs {"signet test-folder/test.wav,test-folder/test-out.wav norm -3"};
            REQUIRE(signet.Main(args.Size(), args.Args()) == 0);
        }
    }

    SUBCASE("backups") {
        SUBCASE("backup of tf1.wav") {
            auto args = TestHelpers::StringToArgs {"signet test-folder/tf1.wav trim start 50%"};
            REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

            auto f = ReadAudioFile("test-folder/tf1.wav");
            REQUIRE(f);
            auto starting_size = f->interleaved_samples.size();

            args = TestHelpers::StringToArgs {"signet --load-backup"};
            REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

            f = ReadAudioFile("test-folder/tf1.wav");
            REQUIRE(f);
            REQUIRE(f->interleaved_samples.size() > starting_size);
        }

        SUBCASE("backup of tf1.wav and tf2.wav") {
            usize trimmed_size_tf1 = 0;
            usize trimmed_size_tf2 = 0;
            {
                const auto args = TestHelpers::StringToArgs {"signet test-folder/tf*.wav trim start 50%"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                {
                    auto f = ReadAudioFile("test-folder/tf1.wav");
                    REQUIRE(f);
                    trimmed_size_tf1 = f->interleaved_samples.size();

                    f = ReadAudioFile("test-folder/tf2.wav");
                    REQUIRE(f);
                    trimmed_size_tf2 = f->interleaved_samples.size();
                }
            }

            {
                const auto args = TestHelpers::StringToArgs {"signet --load-backup"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                {
                    auto f = ReadAudioFile("test-folder/tf1.wav");
                    REQUIRE(f);
                    REQUIRE(f->interleaved_samples.size() > trimmed_size_tf1);

                    f = ReadAudioFile("test-folder/tf2.wav");
                    REQUIRE(f);
                    REQUIRE(f->interleaved_samples.size() > trimmed_size_tf2);
                }
            }
        }

        SUBCASE("backup of renaming tf1.wav") {
            {
                const auto args = TestHelpers::StringToArgs {"signet test-folder/tf1.wav rename prefix foo_"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                REQUIRE(fs::is_regular_file("test-folder/foo_tf1.wav"));
                REQUIRE(!fs::exists("test-folder/tf1.wav"));
            }

            {
                const auto args = TestHelpers::StringToArgs {"signet --load-backup"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                REQUIRE(fs::is_regular_file("test-folder/tf1.wav"));
                REQUIRE(!fs::exists("test-folder/foo_tf1.wav"));
            }
        }

        SUBCASE("backing up of changing file format") {
            {
                const auto args =
                    TestHelpers::StringToArgs {"signet test-folder/tf1.wav convert file-format flac"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                REQUIRE(fs::is_regular_file("test-folder/tf1.flac"));
                REQUIRE(ReadAudioFile("test-folder/tf1.flac"));
                REQUIRE(!fs::exists("test-folder/tf1.wav"));
            }

            {
                const auto args = TestHelpers::StringToArgs {"signet --load-backup"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                REQUIRE(fs::is_regular_file("test-folder/tf1.wav"));
                REQUIRE(ReadAudioFile("test-folder/tf1.wav"));
                REQUIRE(!fs::exists("test-folder/tf1.flac"));
            }
        }

        SUBCASE("backing up of changing format and renaming") {
            {
                const auto args = TestHelpers::StringToArgs {
                    "signet test-folder/tf1.wav convert file-format flac rename prefix foo_"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                REQUIRE(fs::is_regular_file("test-folder/foo_tf1.flac"));
                REQUIRE(ReadAudioFile("test-folder/foo_tf1.flac"));
                REQUIRE(!fs::exists("test-folder/tf1.wav"));
            }

            {
                const auto args = TestHelpers::StringToArgs {"signet --load-backup"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                REQUIRE(fs::is_regular_file("test-folder/tf1.wav"));
                REQUIRE(ReadAudioFile("test-folder/tf1.wav"));
                REQUIRE(!fs::exists("test-folder/foo_tf1.flac"));
            }
        }

        SUBCASE("backing up of renaming and changing the format") {
            {
                const auto args = TestHelpers::StringToArgs {
                    "signet test-folder/tf1.wav rename prefix foo_ convert file-format flac"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                REQUIRE(fs::is_regular_file("test-folder/foo_tf1.flac"));
                REQUIRE(ReadAudioFile("test-folder/foo_tf1.flac"));
                REQUIRE(!fs::exists("test-folder/tf1.wav"));
            }

            {
                const auto args = TestHelpers::StringToArgs {"signet --load-backup"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                REQUIRE(fs::is_regular_file("test-folder/tf1.wav"));
                REQUIRE(ReadAudioFile("test-folder/tf1.wav"));
                REQUIRE(!fs::exists("test-folder/foo_tf1.flac"));
            }
        }

        SUBCASE("backing up of renaming, changing the format and changing the data") {
            usize trimmed_size = 0;
            {
                const auto args = TestHelpers::StringToArgs {
                    "signet test-folder/tf1.wav rename prefix foo_ convert file-format flac trim start 50%"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                REQUIRE(fs::is_regular_file("test-folder/foo_tf1.flac"));
                const auto f = ReadAudioFile("test-folder/foo_tf1.flac");
                REQUIRE(f);
                trimmed_size = f->interleaved_samples.size();
                REQUIRE(!fs::exists("test-folder/tf1.wav"));
            }

            {
                const auto args = TestHelpers::StringToArgs {"signet --load-backup"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                REQUIRE(fs::is_regular_file("test-folder/tf1.wav"));
                const auto f = ReadAudioFile("test-folder/tf1.wav");
                REQUIRE(f);
                REQUIRE(f->interleaved_samples.size() > trimmed_size);
                REQUIRE(!fs::exists("test-folder/foo_tf1.flac"));
            }
        }
    }
}
