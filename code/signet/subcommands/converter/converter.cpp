#include "converter.h"

#include "r8brain-resampler/CDSPResampler.h"

#include "test_helpers.h"

CLI::App *Converter::CreateSubcommandCLI(CLI::App &app) {
    auto convert = app.add_subcommand("convert", "Convert the bit depth and sample rate.");
    // TODO: add checks for valid bit rates and sample rates
    convert->add_option("sample_rate", m_sample_rate, "The target sample rate in Hz")->required();
    convert->add_option("bit_depth", m_bit_depth, "The target bit depth")->required();
    return convert;
}

std::optional<AudioFile> Converter::Process(const AudioFile &input, ghc::filesystem::path &output_filename) {
    if (!input.interleaved_samples.size()) return {};

    AudioFile result;
    result.bits_per_sample = m_bit_depth;
    result.num_channels = input.num_channels;
    if (result.sample_rate == m_sample_rate) {
        result.sample_rate = m_sample_rate;
        result.interleaved_samples = input.interleaved_samples;
        return result;
    }
    result.sample_rate = m_sample_rate;

    const auto result_num_frames =
        (usize)(input.NumFrames() * ((double)m_sample_rate / (double)input.sample_rate));
    result.interleaved_samples.resize(input.num_channels * result_num_frames);

    r8b::CDSPResampler24 resampler(input.sample_rate, m_sample_rate, input.NumFrames());

    std::vector<double> channel_buffer;
    channel_buffer.reserve(input.NumFrames());
    for (unsigned chan = 0; chan < input.num_channels; ++chan) {
        channel_buffer.clear();
        for (size_t frame = 0; frame < input.NumFrames(); ++frame) {
            channel_buffer.push_back(input.GetSample(chan, frame));
        }

        std::vector<double> output_buffer(result_num_frames);
#if 0
        resampler.oneshot(channel_buffer.data(), (int)channel_buffer.size(), output_buffer.data(),
                          (int)result_num_frames);
#else
        double *out;
        auto num_written = resampler.process(channel_buffer.data(), (int)channel_buffer.size(), out);
        MESSAGE("num_written=" << num_written);
        for (int i = 0; i < num_written; ++i) {
            output_buffer[i] = out[i];
        }
#endif
        for (size_t frame = 0; frame < result_num_frames; ++frame) {
            result.GetSample(chan, frame) = output_buffer[frame];
        }

        resampler.clear();
    }

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
    SUBCASE("conversion") {
        AudioFile buf;
        buf.interleaved_samples = {0.0, 0.2, 0.4, 0.6, 0.8, 1.0};
        buf.num_channels = 1;
        buf.sample_rate = 48000;
        buf.bits_per_sample = 16;

        auto out = TestHelpers::ProcessBufferWithSubcommand<Converter>("convert 96000 16", buf);
        REQUIRE(out);
        REQUIRE(out->NumFrames() == 12);

        for (auto s : out->interleaved_samples) {
            MESSAGE(s);
        }
    }
}
