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

bool Highpass::ProcessAudio(AudioFile &input, const std::string_view filename) {
    if (input.interleaved_samples.size() == 0) return false;

    Filter::Params params;
    Filter::Coeffs coeffs;
    Filter::SetParamsAndCoeffs(Filter::Type::RBJ, params, coeffs, static_cast<int>(Filter::RBJType::HighPass),
                               (double)input.sample_rate, m_cutoff, 1, 0);

    for (unsigned chan = 0; chan < input.num_channels; ++chan) {
        Filter::Data data {};
        for (auto frame = 0; frame < input.NumFrames(); ++frame) {
            auto &v = input.GetSample(chan, frame);
            v = Filter::Process(data, coeffs, v);
        }
    }

    return true;
}
