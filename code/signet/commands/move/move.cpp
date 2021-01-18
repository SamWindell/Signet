#include "move.h"

#include "CLI11.hpp"

CLI::App *MoveCommand::CreateCommandCLI(CLI::App &app) {
    auto move = app.add_subcommand("move", GetName() + ": moves all input files to a given folder.");
    move->add_option("destination-folder", m_destination_dir, "The folder to put all of the input files in.");
    return move;
}

void MoveCommand::ProcessFiles(AudioFiles &files) {
    std::vector<fs::path> dest_paths {};
    for (auto &f : files) {
        const auto p = m_destination_dir / f.GetPath().filename();

        bool already_exists = false;
        for (const auto &dp : dest_paths) {
            if (p == dp) {
                already_exists = true;
                ErrorWithNewLine(
                    GetName(),
                    "There is already another file with the same that will be moved to the destination folder, so this file will be skipped. File: {}",
                    p);
            }
        }

        if (!already_exists) {
            f.SetPath(p);
            dest_paths.push_back(p);
        }
    }
}
