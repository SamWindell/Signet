#include "tuner.h"

#include "audio_file.h"
#include "common.h"
#include "subcommands/converter/converter.h"

CLI::App *Tuner::CreateSubcommandCLI(CLI::App &app) {
    auto tuner = app.add_subcommand("tune", "The the sample by stretching it");
    tuner->add_option("tune cents", m_tune_cents, "The cents to change the pitch by")->required();
    return tuner;
}

bool Tuner::Process(AudioFile &input) {
    if (!input.interleaved_samples.size()) return false;

    const auto cents_in_octave = 100.0 * 12.0;
    const auto multiplier = std::pow(2, -m_tune_cents / cents_in_octave);

    MessageWithNewLine("Tuner", "Tuning sample by ", m_tune_cents, " cents");

    const auto new_sample_rate = (double)input.sample_rate * multiplier;
    Converter::ConvertSampleRate(input.interleaved_samples, input.num_channels, (double)input.sample_rate,
                                 new_sample_rate);

    return true;
}