#include "folderise.h"

#include <regex>

#include "audio_file_io.h"
#include "common.h"
#include "midi_pitches.h"
#include "string_utils.h"
#include "test_helpers.h"

CLI::App *FolderiseCommand::CreateCommandCLI(CLI::App &app) {
    auto folderise = app.add_subcommand(
        "folderise",
        GetName() +
            ": moves files into folders based on their names. This is done by specifying "
            "a regex pattern to match the name against. The folder in which the matched file should be "
            "moved to can be based off of the name. These folders are created if they do not already exist.");

    folderise
        ->add_option("filename-regex", m_filename_pattern,
                     "The ECMAScript-style regex pattern used to match filenames against. The file extension "
                     "is not part of this comparison.")
        ->required();

    folderise
        ->add_option_function<std::string>(
            "out-folder",
            [&](const std::string &input) {
                if (fs::path(input).is_relative()) {
                    WarningWithNewLine(
                        "Folderise",
                        "input folder {} is not absolute. The resulting folder will be {} (ignoring any <n> expansion)",
                        input, fs::absolute(input));
                }
                m_out_folder = input;
            },
            "The output folder that the matching files should be put into. This will be created if "
            "it does not exist. It can contain numbers in angle brackets to signify where groups "
            "from the matching regex should be inserted. These means files can end up in multiple "
            "folders. For example, 'folderise file(\\d+).wav C:/folder<1>' would create folders "
            "'C:/folder1' and 'C:/folder2' if there were files 'file1.wav' and 'file2.wav'.")
        ->required();

    return folderise;
}

void FolderiseCommand::ProcessFiles(AudioFiles &files) {
    int num_matches = 0;
    for (auto &f : files) {
        const auto filename = GetJustFilenameWithNoExtension(f.GetPath());

        const std::regex r {m_filename_pattern};
        std::smatch pieces_match;
        if (std::regex_match(filename, pieces_match, r)) {
            std::string output_folder = m_out_folder;
            for (size_t i = 0; i < pieces_match.size(); ++i) {
                const std::ssub_match sub_match = pieces_match[i];
                const std::string piece = sub_match.str();
                Replace(output_folder, PutNumberInAngleBracket(i), piece);
            }

            fs::path new_path {output_folder};
            new_path /= f.GetPath().filename();
            f.SetPath(new_path);
            ++num_matches;
        }
    }

    if (num_matches == 0) {
        ErrorWithNewLine(GetName(), "No files matched the given filename regex.");
        MessageWithNewLine(GetName(), "    The given filename regex: {}", m_filename_pattern);
        MessageWithNewLine(GetName(), "    An example of a filename that was attempted to match to: {}",
                           GetJustFilenameWithNoExtension(files[0].GetPath()));
    }
}

TEST_CASE("FolderiseCommand") {
    SUBCASE("requires args") {
        REQUIRE_THROWS(TestHelpers::ProcessPathWithCommand<FolderiseCommand>("folderise", {}, "file.wav"));
        REQUIRE_THROWS(
            TestHelpers::ProcessPathWithCommand<FolderiseCommand>("folderise foo", {}, "file.wav"));
    }

    SUBCASE("no-regex") {
        const auto f =
            TestHelpers::ProcessPathWithCommand<FolderiseCommand>("folderise file folder", {}, "file.wav");
        REQUIRE(f);
        REQUIRE(*f == "folder/file.wav");
    }

    SUBCASE("regex") {
        const auto f =
            TestHelpers::ProcessPathWithCommand<FolderiseCommand>("folderise (fi).* <1>", {}, "file.wav");
        REQUIRE(f);
        REQUIRE(*f == "fi/file.wav");
    }

    SUBCASE("regex no match") {
        const auto f =
            TestHelpers::ProcessPathWithCommand<FolderiseCommand>("folderise (\\d*) <1>", {}, "file.wav");
        REQUIRE(!f);
    }

    SUBCASE("regex 2 groups") {
        const auto f = TestHelpers::ProcessPathWithCommand<FolderiseCommand>("folderise (\\w*)-(\\w*) <1><2>",
                                                                             {}, "unprocessed-piano.wav");
        REQUIRE(f);
        REQUIRE(*f == "unprocessedpiano/unprocessed-piano.wav");
    }
}
