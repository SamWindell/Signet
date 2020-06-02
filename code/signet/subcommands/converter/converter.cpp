#include "converter.h"

#include "test_helpers.h"

CLI::App *Converter::CreateSubcommandCLI(CLI::App &app) {
    auto convert = app.add_subcommand("convert", "Convert the bit depth and sample rate.");
    // TODO: add checks for valid bit rates and sample rates
    convert->add_option("sample_rate", m_sample_rate, "The target sample rate in Hz")->required();
    convert->add_option("bit_depth", m_bit_depth, "The target bit depth")->required();
    return convert;
}

std::optional<AudioFile> Converter::Process(const AudioFile &input, ghc::filesystem::path &output_filename) {
    auto result = input;
    result.bits_per_sample = m_bit_depth;
    return result;
}

TEST_CASE("[Converter]") {
    SUBCASE("args") {
        SUBCASE("requires both") {
            TestHelpers::ProcessBufferWithSubcommand<Converter>("convert", {}, true);
            TestHelpers::ProcessBufferWithSubcommand<Converter>("convert 44100", {}, true);
            TestHelpers::ProcessBufferWithSubcommand<Converter>("convert 44100 16", {}, false);
        }
    }
}
