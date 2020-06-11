#include "folderiser.h"

#include <regex>

#include "audio_file.h"
#include "common.h"
#include "midi_pitches.h"
#include "string_utils.h"
#include "subcommands/pitch_detector/pitch_detector.h"
#include "test_helpers.h"

CLI::App *Folderiser::CreateSubcommandCLI(CLI::App &app) {
    auto folderiser =
        app.add_subcommand("folderise", "Put files that match a regex pattern into folders. These folders "
                                        "are created if they do not already exist.");

    folderiser
        ->add_option(
            "filename-regex", m_filename_pattern,
            "The ECMAScript regex pattern used to match filenames against. The file extension is ignored.")
        ->required();

    folderiser
        ->add_option("out-folder", m_out_folder,
                     "The output folder that the matching files should be put into. This will be created if "
                     "it does not exist. It can contain numbers in angle brackets to signify where groups "
                     "from the matching regex should be inserted. These means files can end up in multiple "
                     "folders. For example, 'folderise file(\\d+).wav folder<1>' would create folders "
                     "'folder1' and 'folder2' if there were files 'file1.wav' and 'file2.wav'.")
        ->required();

    return folderiser;
}

bool Folderiser::ProcessFilename(fs::path &path, const AudioFile &input) {
    std::string filename = GetJustFilenameWithNoExtension(path);

    const std::regex r {m_filename_pattern};
    std::smatch pieces_match;
    std::cout << "comparing " << filename << " to " << m_filename_pattern << "\n";
    if (std::regex_match(filename, pieces_match, r)) {
        std::cout << "matched\n";
        std::string output_folder = m_out_folder;
        for (size_t i = 0; i < pieces_match.size(); ++i) {
            const std::ssub_match sub_match = pieces_match[i];
            const std::string piece = sub_match.str();
            Replace(output_folder, PutNumberInAngleBracket(i), piece);
        }

        fs::path new_path = output_folder;
        new_path /= path.filename();
        path = new_path;
        return true;
    }

    return false;
}

TEST_CASE("Folderiser") {
    SUBCASE("requires args") {
        REQUIRE_THROWS(TestHelpers::ProcessPathWithSubcommand<Folderiser>("folderise", {}, "file.wav"));
        REQUIRE_THROWS(TestHelpers::ProcessPathWithSubcommand<Folderiser>("folderise foo", {}, "file.wav"));
    }

    SUBCASE("no-regex") {
        const auto f =
            TestHelpers::ProcessPathWithSubcommand<Folderiser>("folderise file folder", {}, "file.wav");
        REQUIRE(f);
        REQUIRE(*f == "folder/file.wav");
    }

    SUBCASE("regex") {
        const auto f =
            TestHelpers::ProcessPathWithSubcommand<Folderiser>("folderise (fi).* <1>", {}, "file.wav");
        REQUIRE(f);
        REQUIRE(*f == "fi/file.wav");
    }

    SUBCASE("regex no match") {
        const auto f =
            TestHelpers::ProcessPathWithSubcommand<Folderiser>("folderise (\\d*) <1>", {}, "file.wav");
        REQUIRE(!f);
    }

    SUBCASE("regex 2 groups") {
        const auto f = TestHelpers::ProcessPathWithSubcommand<Folderiser>("folderise (\\w*)-(\\w*) <1><2>",
                                                                          {}, "unprocessed-piano.wav");
        REQUIRE(f);
        REQUIRE(*f == "unprocessedpiano/unprocessed-piano.wav");
    }
}
