#include "mir_report.h"

#include "CLI11.hpp"
#include "doctest.hpp"
#include "fmt/format.h"
#include "json.hpp"

#include "audio_files.h"
#include "common.h"
#include "edit_tracked_audio_file.h"
#include "mir_analysis.h"
#include "test_helpers.h"

CLI::App *MirReportCommand::CreateCommandCLI(CLI::App &app) {
    auto cmd = app.add_subcommand(
        "mir-report",
        "Analyses the input audio file(s) and prints a JSON report to stdout containing "
        "length, channels, detected pitch (with confidence), phase correlation, an "
        "averaged spectral profile, a waveform envelope, and a pitch-over-time track. Intended as input for an AI "
        "agent to characterise samples for a library.");

    return cmd;
}

void MirReportCommand::ProcessFiles(AudioFiles &files) {
    if (files.Size() == 0) {
        MessageWithNewLine(GetName(), {}, "No input files");
        return;
    }

    auto report = nlohmann::json::array();
    for (auto &f : files) {
        MessageWithNewLine(GetName(), f, "Analysing");
        auto entry = mir::Analyse(f.GetAudio());
        entry["path"] = f.OriginalPath().u8string();
        report.push_back(std::move(entry));
    }

    fmt::print(stdout, "{}\n", report.dump(2));
}

TEST_CASE("MirReportCommand") {
    auto test_audio = TestHelpers::CreateSineWaveAtFrequency(2, 44100, 1.0, 440.0);
    TestHelpers::ProcessBufferWithCommand<MirReportCommand>("mir-report", test_audio);
}
