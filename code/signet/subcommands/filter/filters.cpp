#include "filters.h"

#include "CLI11.hpp"

#include "audio_files.h"
#include "common.h"
#include "filter.h"

void FilterProcessFiles(AudioFiles &files,
                        const Filter::RBJType type,
                        const double cutoff,
                        const double Q,
                        const double gain_db) {
    for (auto &f : files) {
        auto &audio = f.GetWritableAudio();

        Filter::Params params;
        Filter::Coeffs coeffs;
        Filter::SetParamsAndCoeffs(Filter::Type::RBJ, params, coeffs, (int)type, (double)audio.sample_rate,
                                   cutoff, Q, gain_db);

        for (unsigned chan = 0; chan < audio.num_channels; ++chan) {
            Filter::Data data {};
            for (auto frame = 0; frame < audio.NumFrames(); ++frame) {
                auto &v = audio.GetSample(chan, frame);
                v = Filter::Process(data, coeffs, v);
            }
        }
    }
}

CLI::App *HighpassCommand::CreateCommandCLI(CLI::App &app) {
    auto hp =
        app.add_subcommand("highpass", R"aa(HighpassCommand: removes frequencies below the given cutoff.)aa");

    hp->add_option("cutoff-freq-hz", m_cutoff,
                   "The cutoff point where frequencies below this should be removed.")
        ->required();

    return hp;
}

void HighpassCommand::ProcessFiles(AudioFiles &files) {
    FilterProcessFiles(files, Filter::RBJType::HighPass, m_cutoff, Filter::default_q_factor, 0);
}

CLI::App *LowpassCommand::CreateCommandCLI(CLI::App &app) {
    auto lp =
        app.add_subcommand("lowpass", R"aa(LowpassCommand: removes frequencies above the given cutoff.)aa");

    lp->add_option("cutoff-freq-hz", m_cutoff,
                   "The cutoff point where frequencies above this should be removed.")
        ->required();

    return lp;
}

void LowpassCommand::ProcessFiles(AudioFiles &files) {
    FilterProcessFiles(files, Filter::RBJType::LowPass, m_cutoff, Filter::default_q_factor, 0);
}
