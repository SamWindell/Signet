#include "tuner.h"

#include "common.h"
#include "subcommands/converter/converter.h"

CLI::App *Tuner::CreateSubcommandCLI(CLI::App &app) {
    auto tuner = app.add_subcommand("tune", "The the sample by stretching it");
    tuner->add_option("tune cents", m_tune_cents, "The cents to change the pitch by")->required();
    return tuner;
}

void Tuner::ChangePitch(AudioFile &input, const double cents) {
    constexpr auto cents_in_octave = 100.0 * 12.0;
    const auto multiplier = std::pow(2, -cents / cents_in_octave);
    const auto new_sample_rate = (double)input.sample_rate * multiplier;
    Converter::ConvertSampleRate(input.interleaved_samples, input.num_channels, (double)input.sample_rate,
                                 new_sample_rate);
}

bool Tuner::ProcessAudio(AudioFile &input, const std::string_view filename) {
    if (!input.interleaved_samples.size()) return false;
    MessageWithNewLine("Tuner", "Tuning sample by ", m_tune_cents, " cents");
    ChangePitch(input, m_tune_cents);
    return true;
}
