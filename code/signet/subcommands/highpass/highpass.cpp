#include "highpass.h"

#include "audio_file.h"
#include "common.h"
#include "filter.h"

CLI::App *Highpass::CreateSubcommandCLI(CLI::App &app) {
    auto hp = app.add_subcommand("highpass", R"aa(Highpass: removes frequencies below the given cutoff.)aa");

    hp->add_option("cutoff-freq-hz", m_cutoff,
                   "The cutoff point where frequencies below this should be removed.")
        ->required();

    return hp;
}

void Highpass::ProcessFiles(const tcb::span<InputAudioFile> files) {
    for (auto &f : files) {
        auto &audio = f.GetWritableAudio();

        Filter::Params params;
        Filter::Coeffs coeffs;
        Filter::SetParamsAndCoeffs(Filter::Type::RBJ, params, coeffs,
                                   static_cast<int>(Filter::RBJType::HighPass), (double)audio.sample_rate,
                                   m_cutoff, 1, 0);

        for (unsigned chan = 0; chan < audio.num_channels; ++chan) {
            Filter::Data data {};
            for (auto frame = 0; frame < audio.NumFrames(); ++frame) {
                auto &v = audio.GetSample(chan, frame);
                v = Filter::Process(data, coeffs, v);
            }
        }
    }
}
