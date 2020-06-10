#include "converter.h"

#include "magic_enum.hpp"
#include "r8brain-resampler/CDSPResampler.h"

#include "test_helpers.h"

void Converter::Run(SubcommandHost &processor) {
    m_files_can_be_converted = true;
    m_mode = Mode::ValidatingCorrectFormat;
    processor.ProcessAllFiles(*this);

    if (m_files_can_be_converted) {
        m_mode = Mode::Converting;
        processor.ProcessAllFiles(*this);
    } else {
        WarningWithNewLine("one or more files cannot be converted therefore no conversion will take place");
    }
}

CLI::App *Converter::CreateSubcommandCLI(CLI::App &app) {
    auto convert = app.add_subcommand("convert", "Convert the bit depth and sample rate.");
    convert->add_option("sample_rate", m_sample_rate, "The target sample rate in Hz")
        ->required()
        ->check(CLI::Range(1llu, 4300000000llu));
    convert->add_option("bit_depth", m_bit_depth, "The target bit depth")
        ->required()
        ->check(CLI::IsMember({8, 16, 20, 24, 32, 64}));
    return convert;
}

void Converter::ConvertSampleRate(std::vector<double> &buffer,
                                  const unsigned num_channels,
                                  const double input_sample_rate,
                                  const double new_sample_rate) {
    if (input_sample_rate == new_sample_rate) return;
    const auto num_frames = buffer.size() / num_channels;

    const auto result_num_frames = (usize)(num_frames * (new_sample_rate / (double)input_sample_rate));
    std::vector<double> result_interleaved_samples(num_channels * result_num_frames);

    r8b::CDSPResampler24 resampler(input_sample_rate, new_sample_rate, (int)num_frames);

    ForEachDeinterleavedChannel(buffer, num_channels, [&](const auto &channel_buffer, auto channel) {
        std::vector<double> output_buffer(result_num_frames);
        resampler.oneshot(channel_buffer.data(), (int)channel_buffer.size(), output_buffer.data(),
                          (int)result_num_frames);
        for (size_t frame = 0; frame < result_num_frames; ++frame) {
            result_interleaved_samples[frame * num_channels + channel] = output_buffer[frame];
        }

        resampler.clear();
    });

    buffer = result_interleaved_samples;
}

bool Converter::ProcessAudio(AudioFile &input, const std::string_view filename) {
    switch (m_mode) {
        case Mode::ValidatingCorrectFormat: {
            if (!CanFileBeConvertedToBitDepth(input, m_bit_depth)) {
                WarningWithNewLine("files of type ", magic_enum::enum_name(input.format),
                                   " cannot be converted to a bit depth of ", m_bit_depth);
                m_files_can_be_converted = false;
            }
            return false;
        }
        case Mode::Converting: {
            if (!input.interleaved_samples.size()) return false;
            if (m_bit_depth == input.bits_per_sample && m_sample_rate == input.sample_rate) {
                MessageWithNewLine("Converter", "No conversion necessary");
                return false;
            }
            input.bits_per_sample = m_bit_depth;
            if (input.sample_rate == m_sample_rate) {
                MessageWithNewLine("Converter", "Seting the bit rate from ", input.bits_per_sample, " to ",
                                   m_bit_depth);
                return true;
            }

            MessageWithNewLine("Converter", "Converting sample rate from ", input.sample_rate, " to ",
                               m_sample_rate);
            ConvertSampleRate(input.interleaved_samples, input.num_channels, input.sample_rate,
                              (double)m_sample_rate);
            input.sample_rate = m_sample_rate;
            return true;
        }
    }

    REQUIRE(false);
    return false;
}

TEST_CASE("[Converter]") {
    SUBCASE("args") {
        SUBCASE("requires both") {
            REQUIRE_THROWS(TestHelpers::ProcessBufferWithSubcommand<Converter>("convert", {}));
            REQUIRE_THROWS(TestHelpers::ProcessBufferWithSubcommand<Converter>("convert 44100", {}));
            REQUIRE_NOTHROW(TestHelpers::ProcessBufferWithSubcommand<Converter>("convert 44100 16", {}));
        }
        SUBCASE("invalid vals") {
            REQUIRE_THROWS(TestHelpers::ProcessBufferWithSubcommand<Converter>("convert 0.2 2", {}));
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
            if (!fs::is_directory(folder)) {
                fs::create_directory(folder);
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
                const auto sample_rates = {
                    8000,  11025, 16000, 22050, 32000, 37800, 44056, 44100,
                    47250, 48000, 50000, 50400, 64000, 88200, 96000, 176400,
                };

                for (const auto s1 : sample_rates) {
                    for (const auto s2 : sample_rates) {
                        PerformResampling(s1, s2, 1, true);
                    }
                }
            }
        }
    }
}
