#include "normalise.h"

#include "CLI11.hpp"
#include "doctest.hpp"

#include "test_helpers.h"

CLI::App *NormaliseCommand::CreateCommandCLI(CLI::App &app) {
    auto norm = app.add_subcommand(
        "norm",
        "Sets the peak amplitude to a given level (normalisation). When this is used on multiple files, each file is altered by the same amount; preserving their volume levels relative to each other (sometimes known as common-gain normalisation). Alternatively, you can make each file always normalise to the target by specifying the flag --independently.");

    norm->add_option("target-decibels", m_target_decibels,
                     "The target level in decibels, where 0dB is the max volume.")
        ->required()
        ->check(CLI::Range(-200, 0));

    norm->add_flag(
        "--independently", m_normalise_independently,
        "When there are multiple files, normalise each one individually rather than by a common gain.");

    norm->add_flag("--independent-channels", m_normalise_channels_separately,
                   "Normalise each channel independently rather than scale them together.");

    norm->add_flag(
        "--rms", m_use_rms,
        "Use average RMS (root mean squared) calculations to work out the required gain amount. In other words, the whole file's loudness is analysed, rather than just the peak. Does not work well with very dynamic-range variable audio.");

    norm->add_option(
            "--mix", m_norm_mix,
            "The mix of the normalised signal, where 100% means normalise to exactly to the target, 50% means no apply a gain to get halfway from the current level to the target.")
        ->check(CLI::Range(0, 100));
    return norm;
}

void NormaliseCommand::ProcessFiles(AudioFiles &files) {
    const auto MakeGainCalculator = [rms = m_use_rms]() -> std::unique_ptr<NormalisationGainCalculator> {
        if (rms) return std::make_unique<RMSGainCalculator>();
        return std::make_unique<PeakGainCalculator>();
    };

    auto gain_calculator = MakeGainCalculator();

    const auto GetMixedGain = [&]() {
        double gain = gain_calculator->GetGain(DBToAmp(m_target_decibels));
        if (m_norm_mix) {
            const auto mix_01 = *m_norm_mix / 100.0;
            if (gain >= 1) {
                gain = 1 + (gain - 1) * mix_01;
            } else {
                gain = gain + (1 - gain) * mix_01;
            }
        }
        return gain;
    };

    bool use_common_gain = false;
    if (files.Size() > 1 && !m_normalise_independently) {
        use_common_gain = true;
        for (auto &f : files) {
            if (!gain_calculator->RegisterBufferMagnitudes(f.GetAudio(), {})) {
                use_common_gain = false;
                break;
            }
        }
        if (!use_common_gain) {
            ErrorWithNewLine(
                GetName(), {},
                "unable to perform normalisation because the common gain was not successfully found");
            return;
        }
    }

    for (auto &f : files) {
        auto &audio = f.GetWritableAudio();
        if (!m_normalise_channels_separately) {
            if (!use_common_gain) {
                gain_calculator->Reset();
                gain_calculator->RegisterBufferMagnitudes(audio, {});
            }
            const auto gain = GetMixedGain();
            MessageWithNewLine(GetName(), f, "Applying a gain of {:.2f}", gain);
            audio.MultiplyByScalar(gain);
        } else {
            if (!use_common_gain) {
                for (unsigned chan = 0; chan < audio.num_channels; ++chan) {
                    gain_calculator->Reset();
                    gain_calculator->RegisterBufferMagnitudes(audio, chan);

                    const auto gain = GetMixedGain();
                    MessageWithNewLine(GetName(), f, "Applying a gain of {:.2f} to channel {}", gain, chan);
                    audio.MultiplyByScalar(chan, gain);
                }
            } else {
                auto channels_gain_calculator = MakeGainCalculator();

                std::vector<double> channel_peaks;
                for (unsigned chan = 0; chan < audio.num_channels; ++chan) {
                    channels_gain_calculator->Reset();
                    channels_gain_calculator->RegisterBufferMagnitudes(audio, chan);
                    channel_peaks.push_back(channels_gain_calculator->GetLargestRegisteredMagnitude());
                }
                const auto max_channel_gain = *std::max_element(channel_peaks.begin(), channel_peaks.end());

                const auto gain = GetMixedGain();
                for (unsigned chan = 0; chan < audio.num_channels; ++chan) {
                    const auto channel_gain = gain * (max_channel_gain / channel_peaks[chan]);

                    MessageWithNewLine(GetName(), f, "Applying a gain of {:.2f} to channel {}", channel_gain,
                                       chan);
                    audio.MultiplyByScalar(chan, channel_gain);
                }
            }
        }
    }
}

TEST_CASE("NormaliseCommand") {
    auto sine = TestHelpers::CreateSingleOscillationSineWave(1, 100, 100);
    sine.MultiplyByScalar(0.5);

    const auto FindMaxSample = [](const AudioData &d) {
        double max = 0;
        for (const auto &s : d.interleaved_samples) {
            max = std::max(std::abs(s), max);
        }
        return max;
    };

    const auto FindMaxSampleChannel = [](const AudioData &d, unsigned channel) {
        double max = 0;
        for (size_t frame = 0; frame < d.NumFrames(); ++frame) {
            max = std::max(std::abs(d.GetSample(channel, frame)), max);
        }
        return max;
    };

    SUBCASE("single file") {
        const auto out = TestHelpers::ProcessBufferWithCommand<NormaliseCommand>("norm 0", sine);
        REQUIRE(out);
        REQUIRE(FindMaxSample(*out) == doctest::Approx(1.0));
    }

    SUBCASE("single file mix 50% to 0db") {
        const auto out = TestHelpers::ProcessBufferWithCommand<NormaliseCommand>("norm 0 --mix=50", sine);
        REQUIRE(out);
        REQUIRE(FindMaxSample(*out) == doctest::Approx(0.75));
    }

    SUBCASE("single file mix 75% to 0db") {
        const auto out = TestHelpers::ProcessBufferWithCommand<NormaliseCommand>("norm 0 --mix=75", sine);
        REQUIRE(out);
        REQUIRE(FindMaxSample(*out) == doctest::Approx(0.875));
    }

    SUBCASE("multiple files common gain") {
        auto sine_half_vol = sine;
        auto sine_3_quarters_vol = sine;
        for (auto &s : sine_3_quarters_vol.interleaved_samples) {
            s *= 1.5;
        }
        const auto outs =
            TestHelpers::ProcessBuffersWithCommand<NormaliseCommand>("norm 0", {sine, sine_3_quarters_vol});
        for (const auto &out : outs) {
            REQUIRE(out);
        }
        REQUIRE(FindMaxSample(*outs[0]) == doctest::Approx(0.666).epsilon(0.1));
        REQUIRE(FindMaxSample(*outs[1]) == doctest::Approx(1.0).epsilon(0.1));
    }

    SUBCASE("independant channels") {
        auto stereo_sine_a = TestHelpers::CreateSingleOscillationSineWave(2, 100, 100);
        auto stereo_sine_b = TestHelpers::CreateSingleOscillationSineWave(2, 100, 100);

        stereo_sine_a.MultiplyByScalar(0, 0.8);
        stereo_sine_a.MultiplyByScalar(1, 0.6);
        stereo_sine_b.MultiplyByScalar(0, 0.4);
        stereo_sine_b.MultiplyByScalar(1, 0.2);

        SUBCASE("non-common with independent channels") {
            const auto outs = TestHelpers::ProcessBuffersWithCommand<NormaliseCommand>(
                "norm 0 --independently --independent-channels", {stereo_sine_a, stereo_sine_b});
            for (const auto &out : outs) {
                REQUIRE(out);
            }

            REQUIRE(FindMaxSampleChannel(*outs[0], 0) == doctest::Approx(1.0));
            REQUIRE(FindMaxSampleChannel(*outs[0], 1) == doctest::Approx(1.0));
            REQUIRE(FindMaxSampleChannel(*outs[1], 0) == doctest::Approx(1.0));
            REQUIRE(FindMaxSampleChannel(*outs[1], 1) == doctest::Approx(1.0));
        }

        SUBCASE("common with independent channels") {
            const auto outs = TestHelpers::ProcessBuffersWithCommand<NormaliseCommand>(
                "norm 0 --independent-channels", {stereo_sine_a, stereo_sine_b});
            for (const auto &out : outs) {
                REQUIRE(out);
            }

            REQUIRE(FindMaxSampleChannel(*outs[0], 0) == doctest::Approx(1.0));
            REQUIRE(FindMaxSampleChannel(*outs[0], 1) == doctest::Approx(1.0));
            REQUIRE(FindMaxSampleChannel(*outs[1], 0) == doctest::Approx(0.5));
            REQUIRE(FindMaxSampleChannel(*outs[1], 1) == doctest::Approx(0.5));
        }

        SUBCASE("common with independent channels mix 50%") {
            const auto outs = TestHelpers::ProcessBuffersWithCommand<NormaliseCommand>(
                "norm 0 --independent-channels --mix=50", {stereo_sine_a, stereo_sine_b});
            for (const auto &out : outs) {
                REQUIRE(out);
            }

            REQUIRE(FindMaxSampleChannel(*outs[0], 0) == doctest::Approx(0.9));
            REQUIRE(FindMaxSampleChannel(*outs[0], 1) == doctest::Approx(0.9));
            REQUIRE(FindMaxSampleChannel(*outs[1], 0) == doctest::Approx(0.45));
            REQUIRE(FindMaxSampleChannel(*outs[1], 1) == doctest::Approx(0.45));
        }
    }
}
