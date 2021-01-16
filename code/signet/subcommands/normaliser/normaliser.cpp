#include "normaliser.h"

#include "CLI11.hpp"
#include "doctest.hpp"

#include "test_helpers.h"

CLI::App *Normaliser::CreateSubcommandCLI(CLI::App &app) {
    auto norm = app.add_subcommand(
        "norm",
        "Normaliser: sets the peak amplitude to a certain level. When this is used on multiple files, each "
        "file is attenuated by the same amount. You can disable this by specifying the flag "
        "--independently.");

    norm->add_option("target-decibels", m_target_decibels,
                     "The target level in decibels, where 0dB is the max volume.")
        ->required()
        ->check(CLI::Range(-200, 0));

    norm->add_flag(
        "--independently", m_normalise_independently,
        "When there are multiple files, normalise each one individually rather than by a common gain.");
    norm->add_flag("--rms", m_use_rms,
                   "Use RMS (root mean squared) calculations to work out the required gain amount.");
    norm->add_option("--mix", m_norm_mix,
                     "The mix of the normalised signal, where 100% means normalised exactly to the target, "
                     "and 0% means no change.")
        ->check(CLI::Range(0, 100));
    return norm;
}

void Normaliser::ProcessFiles(AudioFiles &files) {
    if (m_use_rms) {
        m_processor = std::make_unique<RMSGainCalculator>();
    } else {
        m_processor = std::make_unique<PeakGainCalculator>();
    }

    m_using_common_gain = false;
    if (files.Size() > 1 && !m_normalise_independently) {
        m_using_common_gain = true;
        for (auto &f : files) {
            if (!ReadFileForCommonGain(f.GetAudio())) {
                m_using_common_gain = false;
                break;
            }
        }
        if (!m_using_common_gain) {
            ErrorWithNewLine(
                GetName(),
                "unable to perform normalisation because the common gain was not successfully found");
            return;
        }
    }

    for (auto &f : files) {
        PerformNormalisation(f.GetWritableAudio());
    }
}

bool Normaliser::PerformNormalisation(AudioData &input_audio) const {
    if (!m_using_common_gain) {
        m_processor->Reset();
        m_processor->RegisterBufferMagnitudes(input_audio);
    }
    double gain = m_processor->GetGain(DBToAmp(m_target_decibels));
    if (m_norm_mix) {
        const auto mix_01 = *m_norm_mix / 100.0;
        if (gain >= 1) {
            gain = 1 + (gain - 1) * mix_01;
        } else {
            gain = gain + (1 - gain) * mix_01;
        }
    }

    MessageWithNewLine(GetName(), "Applying a gain of {}", gain);

    for (auto &s : input_audio.interleaved_samples) {
        s *= gain;
    }

    return true;
}

bool Normaliser::ReadFileForCommonGain(const AudioData &audio) {
    return m_processor->RegisterBufferMagnitudes(audio);
}

TEST_CASE("Normaliser") {
    auto sine = TestHelpers::CreateSingleOscillationSineWave(1, 100, 100);
    for (auto &s : sine.interleaved_samples) {
        s *= 0.5;
    }

    const auto FindMaxSample = [](const AudioData &d) {
        double max = 0;
        for (const auto &s : d.interleaved_samples) {
            max = std::max(std::abs(s), max);
        }
        return max;
    };

    SUBCASE("single file") {
        const auto out = TestHelpers::ProcessBufferWithSubcommand<Normaliser>("norm 0", sine);
        REQUIRE(out);
        REQUIRE(FindMaxSample(*out) == doctest::Approx(1.0));
    }

    SUBCASE("single file mix 50% to 0db") {
        const auto out = TestHelpers::ProcessBufferWithSubcommand<Normaliser>("norm 0 --mix=50", sine);
        REQUIRE(out);
        REQUIRE(FindMaxSample(*out) == doctest::Approx(0.75));
    }

    SUBCASE("single file mix 75% to 0db") {
        const auto out = TestHelpers::ProcessBufferWithSubcommand<Normaliser>("norm 0 --mix=75", sine);
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
            TestHelpers::ProcessBuffersWithSubcommand<Normaliser>("norm 0", {sine, sine_3_quarters_vol});
        for (const auto &out : outs) {
            REQUIRE(out);
        }
        REQUIRE(FindMaxSample(*outs[0]) == doctest::Approx(0.666).epsilon(0.1));
        REQUIRE(FindMaxSample(*outs[1]) == doctest::Approx(1.0).epsilon(0.1));
    }
}
