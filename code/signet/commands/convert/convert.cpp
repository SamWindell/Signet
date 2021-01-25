#include "convert.h"

#include "magic_enum.hpp"

#include "test_helpers.h"

CLI::App *ConvertCommand::CreateCommandCLI(CLI::App &app) {
    auto convert = app.add_subcommand(
        "convert", "Converts the file format, bit-depth or sample "
                   "rate. Features a high quality resampling algorithm. This command has subcommands; it "
                   "requires at least one of sample-rate, bit-depth or file-format to be specified.");
    convert->require_subcommand();

    auto sample_rate =
        convert->add_subcommand("sample-rate", "Change the sample rate using a high quality resampler.");
    sample_rate
        ->add_option<decltype(m_sample_rate), unsigned>("sample-rate", m_sample_rate,
                                                        "The target sample rate in Hz. For example 44100")
        ->required()
        ->check(CLI::Range(1llu, 4300000000llu));

    auto bit_depth = convert->add_subcommand("bit-depth", "Change the bit depth of the file(s).");
    bit_depth
        ->add_option<decltype(m_sample_rate), unsigned>("bit-depth", m_bit_depth, "The target bit depth.")
        ->required()
        ->check(CLI::IsMember({8, 16, 20, 24, 32, 64}));

    std::map<std::string, AudioFileFormat> file_format_name_dictionary;
    for (const auto &e : magic_enum::enum_entries<AudioFileFormat>()) {
        file_format_name_dictionary[std::string(e.second)] = e.first;
    }

    auto file_format = convert->add_subcommand("file-format", "Change the file format.");
    file_format
        ->add_option_function<AudioFileFormat>(
            "file-format", [this](AudioFileFormat f) { m_file_format = f; }, "The output file format.")
        ->required()
        ->transform(CLI::CheckedTransformer(file_format_name_dictionary, CLI::ignore_case));

    return convert;
}

void ConvertCommand::ProcessFiles(AudioFiles &files) {
    m_files_can_be_converted = true;
    if (m_bit_depth) {
        if (!m_file_format) {
            for (auto &f : files) {
                auto &audio = f.GetAudio();
                if (!CanFileBeConvertedToBitDepth(audio.format, *m_bit_depth)) {
                    WarningWithNewLine(GetName(), "files of type {} cannot be converted to a bit depth of {}",
                                       magic_enum::enum_name(audio.format), *m_bit_depth);
                    m_files_can_be_converted = false;
                }
            }
        } else {
            m_files_can_be_converted = CanFileBeConvertedToBitDepth(*m_file_format, *m_bit_depth);
            if (!m_files_can_be_converted) {
                WarningWithNewLine(GetName(), "file format {} cannot be converted to bit depths {}",
                                   magic_enum::enum_name(*m_file_format), *m_bit_depth);
            }
        }
    } else if (m_file_format) {
        for (auto &f : files) {
            auto &audio = f.GetAudio();
            if (!CanFileBeConvertedToBitDepth(*m_file_format, audio.bits_per_sample)) {
                WarningWithNewLine(GetName(), "files of type {} cannot be converted to a bit depth of {}",
                                   magic_enum::enum_name(*m_file_format), audio.bits_per_sample);
                m_files_can_be_converted = false;
            }
        }
    }

    if (m_files_can_be_converted) {
        for (auto &f : files) {
            const auto &audio = f.GetAudio();
            bool edited = false;
            if (m_bit_depth) {
                MessageWithNewLine(GetName(), "Seting the bit rate from {} to {}", audio.bits_per_sample,
                                   *m_bit_depth);
                f.GetWritableAudio().bits_per_sample = *m_bit_depth;
                edited = true;
            }
            if (m_sample_rate && audio.sample_rate != *m_sample_rate) {
                MessageWithNewLine(GetName(), "Converting sample rate from {} to {}", audio.sample_rate,
                                   *m_sample_rate);
                f.GetWritableAudio().Resample((double)*m_sample_rate);
                edited = true;
            }
            if (m_file_format && audio.format != *m_file_format) {
                const auto from_name = magic_enum::enum_name(audio.format);
                const auto to_name = magic_enum::enum_name(*m_file_format);
                MessageWithNewLine(GetName(), "Converting file format from {} to {}", from_name, to_name);
                f.GetWritableAudio().format = *m_file_format;
                edited = true;
            }

            if (!edited) {
                MessageWithNewLine(GetName(), "No conversion necessary");
            }
        }
    } else {
        WarningWithNewLine(GetName(),
                           "one or more files cannot be converted therefore no conversion will take place");
    }
}

TEST_CASE("[ConvertCommand]") {
    SUBCASE("args") {
        SUBCASE("requires a subcommand") {
            REQUIRE_THROWS(TestHelpers::ProcessBufferWithCommand<ConvertCommand>("convert", {}));
        }
    }
    SUBCASE("conversion") {
        SUBCASE("generated") {
            AudioData buf;
            buf.interleaved_samples = {0.0, 0.2, 0.4, 0.6, 0.8, 1.0};
            buf.num_channels = 1;
            buf.sample_rate = 48000;
            buf.bits_per_sample = 16;

            auto out = TestHelpers::ProcessBufferWithCommand<ConvertCommand>(
                "convert sample-rate 96000 bit-depth 16", buf);
            REQUIRE(out);
            REQUIRE(out->NumFrames() == 12);
        }

        SUBCASE("change file-format to a file format that does not support the bit depth") {
            AudioData buf;
            buf.interleaved_samples = {0.0, 0.2, 0.4, 0.6, 0.8, 1.0};
            buf.num_channels = 1;
            buf.sample_rate = 48000;
            buf.bits_per_sample = 32;

            auto out = TestHelpers::ProcessBufferWithCommand<ConvertCommand>("convert file-format flac", buf);
            REQUIRE(!out);
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
                const auto out = TestHelpers::ProcessBufferWithCommand<ConvertCommand>(
                    "convert sample-rate " + target_str + " bit-depth 16", buf);
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
