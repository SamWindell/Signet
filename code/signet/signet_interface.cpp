#include "signet_interface.h"

#include <functional>

#include "doctest.hpp"
#include "rang.hpp"

#include "audio_file.h"
#include "cli_formatter.h"
#include "subcommands/auto_tuner/auto_tuner.h"
#include "subcommands/converter/converter.h"
#include "subcommands/fader/fader.h"
#include "subcommands/filters/filters.h"
#include "subcommands/folderiser/folderiser.h"
#include "subcommands/gainer/gainer.h"
#include "subcommands/normaliser/normaliser.h"
#include "subcommands/pitch_detector/pitch_detector.h"
#include "subcommands/renamer/renamer.h"
#include "subcommands/sample_blender/sample_blender.h"
#include "subcommands/silence_remover/silence_remover.h"
#include "subcommands/trimmer/trimmer.h"
#include "subcommands/tuner/tuner.h"
#include "subcommands/zcross_offsetter/zcross_offsetter.h"
#include "test_helpers.h"
#include "tests_config.h"

SignetInterface::SignetInterface() {
    m_subcommands.push_back(std::make_unique<AutoTuner>());
    m_subcommands.push_back(std::make_unique<Converter>());
    m_subcommands.push_back(std::make_unique<Fader>());
    m_subcommands.push_back(std::make_unique<Folderiser>());
    m_subcommands.push_back(std::make_unique<Gainer>());
    m_subcommands.push_back(std::make_unique<Highpass>());
    m_subcommands.push_back(std::make_unique<Lowpass>());
    m_subcommands.push_back(std::make_unique<Normaliser>());
    m_subcommands.push_back(std::make_unique<PitchDetector>());
    m_subcommands.push_back(std::make_unique<Renamer>());
    m_subcommands.push_back(std::make_unique<SampleBlender>());
    m_subcommands.push_back(std::make_unique<SilenceRemover>());
    m_subcommands.push_back(std::make_unique<Trimmer>());
    m_subcommands.push_back(std::make_unique<Tuner>());
    m_subcommands.push_back(std::make_unique<ZeroCrossingOffsetter>());
}

int SignetInterface::Main(const int argc, const char *const argv[]) {
    CLI::App app {
        R"^^(Signet is a command-line program designed for bulk editing audio files. It has commands for converting, editing, renaming and moving WAV and FLAC files. It also features commands that generate audio files. Signet was primarily designed for people make sample libraries, but its features can be useful for any bulk processing.)^^"};

    app.require_subcommand();
    app.set_help_all_flag("--help-all", "Print help message for all subcommands");
    app.formatter(std::make_shared<WrappedFormatter>());

    app.add_flag_callback(
        "--undo",
        [this]() {
            MessageWithNewLine("Signet", "Undoing changes made by the last run of Signet...");
            m_backup.LoadBackup();
            MessageWithNewLine("Signet", "Done.");
            throw CLI::Success();
        },
        "Undoes any changes made by the last run of Signet; files that were overwritten are restored, new "
        "files that were created are destroyed, and files that were renamed are un-renamed. You can only "
        "undo once - you cannot keep going back in history.");

    app.add_flag_callback(
        "--clear-backup",
        [this]() {
            MessageWithNewLine("Signet", "Clearing all backed-up files...");
            m_backup.ClearBackup();
            MessageWithNewLine("Signet", "Done.");
            throw CLI::Success();
        },
        "Deletes all temporary files created by Signet. These files are needed for the undo system and are "
        "saved to your OS's temporary folder. These files are cleared and new ones created every time you "
        "run Signet. This option is only really useful if you have just processed lots of files and you "
        "won't be using Signet for a long time afterwards. You cannot use --undo directly after clearing the "
        "backup.");

    app.add_flag_callback(
        "--silent", []() { SetMessagesEnabled(true); }, "Disable all messages");

    app.add_flag("--recursive", m_recursive_directory_search,
                 "When the input is a directory, scan for files in it recursively");

    app.add_option_function<std::string>(
           "input-files",
           [&](const std::string &input) {
               m_input_audio_files = InputAudioFiles(input, m_recursive_directory_search);
           },
           R"aa(The audio files to process. This is a file, directory or glob pattern. To use multiple, separate each one with a comma. You can exclude a pattern by beginning it with a dash. e.g. "-*.wav" would exclude all .wav files from the current directory.)aa")
        ->required();

    std::vector<CLI::App *> subcommand_clis;
    for (auto &subcommand : m_subcommands) {
        auto s = subcommand->CreateSubcommandCLI(app);
        s->final_callback([&] {
            subcommand->ProcessFiles(m_input_audio_files.GetAllFiles());
            subcommand->GenerateFiles(m_input_audio_files.GetAllFiles(), m_backup);
        });
    }

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        PrintSignetHeading();
        if (e.get_exit_code() != 0) {
            std::cout << rang::fg::red;
            std::atexit([]() { std::cout << rang::fg::reset; });
            std::cout << "ERROR:\n";
            return app.exit(e);
        } else {
            std::stringstream ss;
            const auto result = app.exit(e, ss);
            PrintSignetCLI(ss.str());
            return result;
        }
    }

    m_backup.ClearBackup(); // if we have gotten here we must not be wanting to undo

    if (m_input_audio_files.GetNumFilesProcessed()) {
        if (!m_input_audio_files.WriteAllAudioFiles(m_backup)) {
            return 1;
        }
    }

    return (m_input_audio_files.GetNumFilesProcessed() != 0) ? 0 : 1;
}

TEST_CASE("[SignetInterface]") {
    SignetInterface signet;

    const auto in_file = std::string(TEST_DATA_DIRECTORY "/white-noise.wav");
    std::string test_folder = "test-folder";
    if (!fs::is_directory(test_folder)) {
        fs::create_directory(test_folder);
    }
    REQUIRE(fs::copy_file(TEST_DATA_DIRECTORY "/white-noise.wav", "test-folder/tf1.wav",
                          fs::copy_options::overwrite_existing));
    REQUIRE(fs::copy_file(TEST_DATA_DIRECTORY "/white-noise.wav", "test-folder/tf2.wav",
                          fs::copy_options::overwrite_existing));
    REQUIRE(fs::copy_file(TEST_DATA_DIRECTORY "/test.wav", "test-folder/test.wav",
                          fs::copy_options::overwrite_existing));

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

        SUBCASE("load undo") {
            const auto args = TestHelpers::StringToArgs {"signet --undo"};
            REQUIRE(signet.Main(args.Size(), args.Args()) == 0);
        }

        SUBCASE("multiple comma separated files") {
            const auto args =
                TestHelpers::StringToArgs {"signet test-folder/test.wav,test-folder/tf1.wav norm -3"};
            REQUIRE(signet.Main(args.Size(), args.Args()) == 0);
        }
    }

    SUBCASE("undos") {
        SUBCASE("undo of tf1.wav") {
            auto args = TestHelpers::StringToArgs {"signet test-folder/tf1.wav trim start 50%"};
            REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

            auto f = ReadAudioFile("test-folder/tf1.wav");
            REQUIRE(f);
            auto starting_size = f->interleaved_samples.size();

            args = TestHelpers::StringToArgs {"signet --undo"};
            REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

            f = ReadAudioFile("test-folder/tf1.wav");
            REQUIRE(f);
            REQUIRE(f->interleaved_samples.size() > starting_size);
        }

        SUBCASE("undo of tf1.wav and tf2.wav") {
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
                const auto args = TestHelpers::StringToArgs {"signet --undo"};
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

        SUBCASE("undo of renaming tf1.wav") {
            {
                const auto args = TestHelpers::StringToArgs {"signet test-folder/tf1.wav rename prefix foo_"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                REQUIRE(fs::is_regular_file("test-folder/foo_tf1.wav"));
                REQUIRE(!fs::exists("test-folder/tf1.wav"));
            }

            {
                const auto args = TestHelpers::StringToArgs {"signet --undo"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                REQUIRE(fs::is_regular_file("test-folder/tf1.wav"));
                REQUIRE(!fs::exists("test-folder/foo_tf1.wav"));
            }
        }

        SUBCASE("undoing changing file format") {
            {
                const auto args =
                    TestHelpers::StringToArgs {"signet test-folder/tf1.wav convert file-format flac"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                REQUIRE(fs::is_regular_file("test-folder/tf1.flac"));
                REQUIRE(ReadAudioFile("test-folder/tf1.flac"));
                REQUIRE(!fs::exists("test-folder/tf1.wav"));
            }

            {
                const auto args = TestHelpers::StringToArgs {"signet --undo"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                REQUIRE(fs::is_regular_file("test-folder/tf1.wav"));
                REQUIRE(ReadAudioFile("test-folder/tf1.wav"));
                REQUIRE(!fs::exists("test-folder/tf1.flac"));
            }
        }

        SUBCASE("undoing changing format and renaming") {
            {
                const auto args = TestHelpers::StringToArgs {
                    "signet test-folder/tf1.wav convert file-format flac rename prefix foo_"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                REQUIRE(fs::is_regular_file("test-folder/foo_tf1.flac"));
                REQUIRE(ReadAudioFile("test-folder/foo_tf1.flac"));
                REQUIRE(!fs::exists("test-folder/tf1.wav"));
            }

            {
                const auto args = TestHelpers::StringToArgs {"signet --undo"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                REQUIRE(fs::is_regular_file("test-folder/tf1.wav"));
                REQUIRE(ReadAudioFile("test-folder/tf1.wav"));
                REQUIRE(!fs::exists("test-folder/foo_tf1.flac"));
            }
        }

        SUBCASE("undoing renaming and changing the format") {
            {
                const auto args = TestHelpers::StringToArgs {
                    "signet test-folder/tf1.wav rename prefix foo_ convert file-format flac"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                REQUIRE(fs::is_regular_file("test-folder/foo_tf1.flac"));
                REQUIRE(ReadAudioFile("test-folder/foo_tf1.flac"));
                REQUIRE(!fs::exists("test-folder/tf1.wav"));
            }

            {
                const auto args = TestHelpers::StringToArgs {"signet --undo"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                REQUIRE(fs::is_regular_file("test-folder/tf1.wav"));
                REQUIRE(ReadAudioFile("test-folder/tf1.wav"));
                REQUIRE(!fs::exists("test-folder/foo_tf1.flac"));
            }
        }

        SUBCASE("undoing folderising") {
            {
                const auto args =
                    TestHelpers::StringToArgs {"signet test-folder/tf1.wav folderise .* folderise-output"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                REQUIRE(fs::is_regular_file("folderise-output/tf1.wav"));
                REQUIRE(!fs::exists("test-folder/tf1.wav"));
            }

            {
                const auto args = TestHelpers::StringToArgs {"signet --undo"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                REQUIRE(fs::is_regular_file("test-folder/tf1.wav"));
                REQUIRE(!fs::exists("folderise-output/tf1.wav"));
            }
        }

        SUBCASE("undoing renaming, changing the format and changing the data") {
            usize trimmed_size = 0;
            {
                const auto args = TestHelpers::StringToArgs {"signet test-folder/tf1.wav rename prefix "
                                                             "foo_ convert file-format flac trim start 50%"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                REQUIRE(fs::is_regular_file("test-folder/foo_tf1.flac"));
                const auto f = ReadAudioFile("test-folder/foo_tf1.flac");
                REQUIRE(f);
                trimmed_size = f->interleaved_samples.size();
                REQUIRE(!fs::exists("test-folder/tf1.wav"));
            }

            {
                const auto args = TestHelpers::StringToArgs {"signet --undo"};
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
