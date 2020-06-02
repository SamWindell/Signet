#include "converter.h"

#include "r8brain-resampler/CDSPResampler.h"

#include "test_helpers.h"

CLI::App *Converter::CreateSubcommandCLI(CLI::App &app) {
    auto convert = app.add_subcommand("convert", "Convert the bit depth and sample rate.");
    convert->add_option("sample_rate", m_sample_rate, "The target sample rate in Hz")
        ->required()
        ->check(CLI::Range(1llu, 4300000000llu));
    convert->add_option("bit_depth", m_bit_depth, "The target bit depth")
        ->required()
        ->check(CLI::IsMember({4, 8, 11, 12, 16, 18, 20, 24, 32, 48, 64}));
    return convert;
}

bool Converter::Process(AudioFile &input) {
    if (!input.interleaved_samples.size()) return false;
    if (m_bit_depth == input.bits_per_sample && m_sample_rate == input.sample_rate) return false;

    input.bits_per_sample = m_bit_depth;
    if (input.sample_rate == m_sample_rate) {
        input.sample_rate = m_sample_rate;
        return true;
    }

    const auto result_num_frames =
        (usize)(input.NumFrames() * ((double)m_sample_rate / (double)input.sample_rate));
    std::vector<double> result_interleaved_samples(input.num_channels * result_num_frames);

    r8b::CDSPResampler24 resampler(input.sample_rate, m_sample_rate, (int)input.NumFrames());

    std::vector<double> channel_buffer;
    channel_buffer.reserve(input.NumFrames());
    for (unsigned chan = 0; chan < input.num_channels; ++chan) {
        channel_buffer.clear();
        for (size_t frame = 0; frame < input.NumFrames(); ++frame) {
            channel_buffer.push_back(input.GetSample(chan, frame));
        }

        std::vector<double> output_buffer(result_num_frames);
        resampler.oneshot(channel_buffer.data(), (int)channel_buffer.size(), output_buffer.data(),
                          (int)result_num_frames);
        for (size_t frame = 0; frame < result_num_frames; ++frame) {
            result_interleaved_samples[frame * input.num_channels + chan] = output_buffer[frame];
        }

        resampler.clear();
    }

    input.sample_rate = m_sample_rate;
    input.interleaved_samples = result_interleaved_samples;
    return true;
}

TEST_CASE("[Converter]") {
    SUBCASE("args") {
        SUBCASE("requires both") {
            TestHelpers::ProcessBufferWithSubcommand<Converter>("convert", {}, true);
            TestHelpers::ProcessBufferWithSubcommand<Converter>("convert 44100", {}, true);
            TestHelpers::ProcessBufferWithSubcommand<Converter>("convert 44100 16", {}, false);
        }
        SUBCASE("invalid vals") {
            TestHelpers::ProcessBufferWithSubcommand<Converter>("convert 0.2 2", {}, true);
        }
    }
    SUBCASE("conversion") {
        SUBCASE("generated") {
            AudioFile buf;
            buf.interleaved_samples = {0.0, 0.2, 0.4, 0.6, 0.8, 1.0};
            buf.num_channels = 1;
            buf.sample_rate = 48000;
            buf.bits_per_sample = 16;

            auto out = TestHelpers::ProcessBufferWithSubcommand<Converter>("convert 96000 16", buf);
            REQUIRE(out);
            REQUIRE(out->NumFrames() == 12);
        }

        SUBCASE("sine") {
            const std::string folder = "resampling-tests";
            if (!ghc::filesystem::is_directory(folder)) {
                ghc::filesystem::create_directory(folder);
            }

            const auto PerformResampling = [&](const unsigned starting_sample_rate,
                                               const unsigned target_sample_rate, const unsigned num_channels,
                                               const bool write_to_file) {
                auto buf =
                    TestHelpers::CreateSineWaveAtFrequency(num_channels, starting_sample_rate, 0.025f, 440);
                for (auto &s : buf.interleaved_samples) {
                    s *= 0.5f; // scale them to avoid clipping
                }

                const auto target_str = std::to_string(target_sample_rate);
                const auto out =
                    TestHelpers::ProcessBufferWithSubcommand<Converter>("convert " + target_str + " 16", buf);
                if (starting_sample_rate != target_sample_rate) {
                    REQUIRE(out);
                    REQUIRE(out->sample_rate == target_sample_rate);
                    REQUIRE(out->bits_per_sample == 16);
                    const auto result_num_frames =
                        (usize)(buf.NumFrames() * ((double)out->sample_rate / (double)buf.sample_rate));
                    REQUIRE(out->NumFrames() == result_num_frames);
                    REQUIRE(out->num_channels == buf.num_channels);
                }

                if (write_to_file) {
                    std::string filename = folder + "/resampled_" + std::to_string(starting_sample_rate) +
                                           "_to_" + target_str + ".wav";
                    WriteAudioFile(filename, *out);
                }
            };

            SUBCASE("multiple channels") {
                SUBCASE("upsample") { PerformResampling(44100, 48000, 4, false); }
                SUBCASE("downsample") { PerformResampling(96000, 22050, 4, false); }
            }

            SUBCASE("resample from all common sample rates") {
                const auto valid_sample_rates = {
                    8000,  11025, 16000, 22050, 32000, 37800, 44056, 44100,
                    47250, 48000, 50000, 50400, 64000, 88200, 96000, 176400,
                };

                for (const auto s1 : valid_sample_rates) {
                    for (const auto s2 : valid_sample_rates) {
                        PerformResampling(s1, s2, 1, true);
                    }
                }
            }
        }
    }
}
