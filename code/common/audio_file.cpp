#include "audio_file.h"

#include <cstdint>
#include <iostream>

#define DR_WAV_IMPLEMENTATION
#define DR_FLAC_IMPLEMENTATION
#include "doctest.hpp"
#include "dr_flac.h"
#include "dr_wav.h"
#include "flac/all.h"
#include "flac/stream_encoder.h"

#include "common.h"
#include "test_helpers.h"

std::optional<AudioFile> ReadAudioFile(const ghc::filesystem::path &path) {
    std::cout << "Reading file " << path << "\n";
    AudioFile result {};
    const auto ext = path.extension();
    std::vector<float> f32_buf {};
    if (ext == ".wav") {
        std::unique_ptr<drwav, decltype(&drwav_close)> wav(drwav_open_file(path.generic_string().data()),
                                                           &drwav_close);
        if (!wav) {
            WarningWithNewLine("could not open the WAV file ", path);
            return {};
        }

        result.num_channels = wav->channels;
        result.sample_rate = wav->sampleRate;
        result.bits_per_sample = wav->bitsPerSample;
        result.interleaved_samples.resize(wav->totalPCMFrameCount * wav->channels);
        f32_buf.resize(result.interleaved_samples.size());
        const auto frames_read =
            drwav_read_pcm_frames_f32(wav.get(), wav->totalPCMFrameCount, f32_buf.data());
        if (frames_read != wav->totalPCMFrameCount) {
            WarningWithNewLine("failed to get all the frames from file ", path);
            return {};
        }
    } else if (ext == ".flac") {
        std::unique_ptr<drflac, decltype(&drflac_close)> flac(
            drflac_open_file(path.generic_string().data(), nullptr), &drflac_close);
        if (!flac) {
            WarningWithNewLine("could not open the FLAC file ", path);
            return {};
        }

        result.num_channels = flac->channels;
        result.sample_rate = flac->sampleRate;
        result.bits_per_sample = flac->bitsPerSample;
        result.interleaved_samples.resize(flac->totalPCMFrameCount * flac->channels);
        f32_buf.resize(result.interleaved_samples.size());
        const auto frames_read =
            drflac_read_pcm_frames_f32(flac.get(), flac->totalPCMFrameCount, f32_buf.data());
        if (frames_read != flac->totalPCMFrameCount) {
            WarningWithNewLine("failed to get all the frames from file ", path);
            return {};
        }
    } else {
        WarningWithNewLine("file ", path, " is not a WAV or a FLAC");
        return {};
    }

    // TODO: would be nice to have a way to get double values direct rather than just casting floats...
    for (usize sample = 0; sample < f32_buf.size(); ++sample) {
        result.interleaved_samples[sample] = (double)f32_buf[sample];
    }

    return result;
}

template <typename SignedIntType>
SignedIntType ScaleSampleToSignedInt(const double s, const unsigned bits_per_sample) {
    const auto negative_max = std::pow(2, bits_per_sample) / 2;
    const auto positive_max = negative_max - 1;
    return static_cast<SignedIntType>(std::round(s < 0 ? s * negative_max : s * positive_max));
}

template <typename SignedIntType>
std::vector<SignedIntType> CreateSignedIntSamplesFromFloat(const std::vector<double> &buf,
                                                           const unsigned bits_per_sample) {
    std::vector<SignedIntType> result;
    result.reserve(buf.size());
    for (const auto s : buf) {
        assert(s >= -1);
        assert(s <= 1);
        result.push_back(ScaleSampleToSignedInt<SignedIntType>(s, bits_per_sample));
    }
    return result;
}

template <typename UnsignedIntType>
std::vector<UnsignedIntType> CreateUnsignedIntSamplesFromFloat(const std::vector<double> &buf,
                                                               const unsigned bits_per_sample) {
    std::vector<UnsignedIntType> result;
    result.reserve(buf.size());
    for (const auto s : buf) {
        assert(s >= -1);
        assert(s <= 1);
        const auto scaled_val = ((s + 1.0) / 2.0f) * ((1 << bits_per_sample) - 1);
        result.push_back(static_cast<UnsignedIntType>(scaled_val));
    }
    return result;
}

static bool WriteWaveFile(const ghc::filesystem::path &path,
                          const AudioFile &audio_file,
                          const unsigned bits_per_sample) {
    if (std::find(std::begin(valid_wave_bit_depths), std::end(valid_wave_bit_depths), bits_per_sample) ==
        std::end(valid_wave_bit_depths)) {
        WarningWithNewLine("could not write wave file - the given bit depth is invalid");
        return false;
    }

    drwav_data_format format {};
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_PCM;
    format.channels = audio_file.num_channels;
    format.sampleRate = audio_file.sample_rate;
    format.bitsPerSample = bits_per_sample;
    std::unique_ptr<drwav, decltype(&drwav_close)> wav {
        drwav_open_file_write(path.generic_string().data(), &format), &drwav_close};
    if (!wav) {
        WarningWithNewLine("could not write wave file - could not open file ", path);
        return false;
    }

    u64 num_written = 0;
    switch (bits_per_sample) {
        case 8: {
            // 8-bit is the exception in that it uses unsigned ints
            const auto buf = CreateUnsignedIntSamplesFromFloat<u8>(audio_file.interleaved_samples, 8);
            num_written = drwav_write_pcm_frames(wav.get(), audio_file.NumFrames(), buf.data());
            break;
        }
        case 16: {
            const auto buf = CreateSignedIntSamplesFromFloat<s16>(audio_file.interleaved_samples, 16);
            num_written = drwav_write_pcm_frames(wav.get(), audio_file.NumFrames(), buf.data());
            break;
        }
        case 24: {
            std::vector<u8> buf;
            buf.reserve(audio_file.interleaved_samples.size() * 3);
            for (const auto &s : audio_file.interleaved_samples) {
                const auto sample = ScaleSampleToSignedInt<s32>(s, 24);

                u8 bytes[3];
                bytes[0] = (u8)sample & 0xFF;
                bytes[1] = (u8)(sample >> 8) & 0xFF;
                bytes[2] = (u8)(sample >> 16) & 0xFF;

                for (const auto byte : bytes) {
                    buf.push_back(byte);
                }
            }
            num_written = drwav_write_pcm_frames(wav.get(), audio_file.NumFrames(), buf.data());
            break;
        }
        case 32: {
            format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
            std::vector<float> buf;
            buf.reserve(audio_file.interleaved_samples.size());
            for (const auto s : audio_file.interleaved_samples) {
                buf.push_back(static_cast<float>(s));
            }
            num_written = drwav_write_pcm_frames(wav.get(), audio_file.NumFrames(), buf.data());
            break;
        }
        case 64: {
            format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
            num_written = drwav_write_pcm_frames(wav.get(), audio_file.NumFrames(),
                                                 audio_file.interleaved_samples.data());
            break;
        }
        default: {
            REQUIRE(0);
            return false;
        }
    }
    if (num_written != audio_file.NumFrames()) {
        WarningWithNewLine("could not write wave file - could not write all samples");
        return false;
    }

    return true;
}

static void PrintFlacStatusCode(const FLAC__StreamEncoderInitStatus code) {
    switch (code) {
        case FLAC__STREAM_ENCODER_INIT_STATUS_ENCODER_ERROR:
            std::cout << "FLAC__STREAM_ENCODER_INIT_STATUS_ENCODER_ERROR\n";
            return;
            /**< General failure to set up encoder; call FLAC__stream_encoder_get_state() for cause. */

        case FLAC__STREAM_ENCODER_INIT_STATUS_UNSUPPORTED_CONTAINER:
            std::cout << "FLAC__STREAM_ENCODER_INIT_STATUS_UNSUPPORTED_CONTAINER\n";
            return;
            /**< The library was not compiled with support for the given container
             * format.
             */

        case FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_CALLBACKS:
            std::cout << "FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_CALLBACKS\n";
            return;
            /**< A required callback was not supplied. */

        case FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_NUMBER_OF_CHANNELS:
            std::cout << "FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_NUMBER_OF_CHANNELS\n";
            return;
            /**< The encoder has an invalid setting for number of channels. */

        case FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_BITS_PER_SAMPLE:
            std::cout << "FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_BITS_PER_SAMPLE\n";
            return;
            /**< The encoder has an invalid setting for bits-per-sample.
             * FLAC supports 4-32 bps but the reference encoder currently supports
             * only up to 24 bps.
             */

        case FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_SAMPLE_RATE:
            std::cout << "FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_SAMPLE_RATE\n";
            return;
            /**< The encoder has an invalid setting for the input sample rate. */

        case FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_BLOCK_SIZE:
            std::cout << "FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_BLOCK_SIZE\n";
            return;
            /**< The encoder has an invalid setting for the block size. */

        case FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_MAX_LPC_ORDER:
            std::cout << "FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_MAX_LPC_ORDER\n";
            return;
            /**< The encoder has an invalid setting for the maximum LPC order. */

        case FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_QLP_COEFF_PRECISION:
            std::cout << "FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_QLP_COEFF_PRECISION\n";
            return;
            /**< The encoder has an invalid setting for the precision of the quantized linear predictor
             * coefficients. */

        case FLAC__STREAM_ENCODER_INIT_STATUS_BLOCK_SIZE_TOO_SMALL_FOR_LPC_ORDER:
            std::cout << "FLAC__STREAM_ENCODER_INIT_STATUS_BLOCK_SIZE_TOO_SMALL_FOR_LPC_ORDER\n";
            return;
            /**< The specified block size is less than the maximum LPC order. */

        case FLAC__STREAM_ENCODER_INIT_STATUS_NOT_STREAMABLE:
            std::cout << "FLAC__STREAM_ENCODER_INIT_STATUS_NOT_STREAMABLE\n";
            return;
            /**< The encoder is bound to the <A HREF="../format.html#subset">Subset</A> but other settings
             * violate it. */

        case FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_METADATA:
            std::cout << "FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_METADATA\n";
            return;
            /**< The metadata input to the encoder is invalid, in one of the following ways:
             * - FLAC__stream_encoder_set_metadata() was called with a null pointer but a block count > 0
             * - One of the metadata blocks contains an undefined type
             * - It contains an illegal CUESHEET as checked by FLAC__format_cuesheet_is_legal()
             * - It contains an illegal SEEKTABLE as checked by FLAC__format_seektable_is_legal()
             * - It contains more than one SEEKTABLE block or more than one VORBIS_COMMENT block
             */

        case FLAC__STREAM_ENCODER_INIT_STATUS_ALREADY_INITIALIZED:
            std::cout << "FLAC__STREAM_ENCODER_INIT_STATUS_ALREADY_INITIALIZED\n";
            return;
            /**< FLAC__stream_encoder_init_*() was called when the encoder was
             * already initialized, usually because
             * FLAC__stream_encoder_finish() was not called.*/
    }
}

static bool WriteFlacFile(const ghc::filesystem::path &filename,
                          const AudioFile &audio_file,
                          const unsigned bits_per_sample) {
    if (std::find(std::begin(valid_flac_bit_depths), std::end(valid_flac_bit_depths), bits_per_sample) ==
        std::end(valid_flac_bit_depths)) {
        WarningWithNewLine("could not write flac file - the given bit depth is invalid");
        return false;
    }

    std::unique_ptr<FLAC__StreamEncoder, decltype(&FLAC__stream_encoder_delete)> encoder {
        FLAC__stream_encoder_new(), &FLAC__stream_encoder_delete};
    if (!encoder) {
        WarningWithNewLine("could not write flac file - no memory");
        return false;
    }

    FLAC__stream_encoder_set_channels(encoder.get(), audio_file.num_channels);
    FLAC__stream_encoder_set_bits_per_sample(encoder.get(), bits_per_sample);
    FLAC__stream_encoder_set_sample_rate(encoder.get(), audio_file.sample_rate);
    FLAC__stream_encoder_set_total_samples_estimate(encoder.get(), audio_file.interleaved_samples.size());

    auto f = OpenFile(filename, "w+b");
    if (!f) {
        WarningWithNewLine("could not write flac file - could not open file ", filename);
        return false;
    }

    if (const auto o = FLAC__stream_encoder_init_FILE(encoder.get(), f.get(), nullptr, nullptr);
        o != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
        WarningWithNewLine("could not write flac file:");
        PrintFlacStatusCode(o);
        return false;
    }

    const auto int32_buffer =
        CreateSignedIntSamplesFromFloat<s32>(audio_file.interleaved_samples, bits_per_sample);
    if (!FLAC__stream_encoder_process_interleaved(encoder.get(), int32_buffer.data(),
                                                  (unsigned)audio_file.NumFrames())) {
        WarningWithNewLine("could not write flac file - failed encoding samples");
        return false;
    }

    if (!FLAC__stream_encoder_finish(encoder.get())) {
        WarningWithNewLine("could not write flac file - error finishing encoding");
        return false;
    }
    return true;
}

bool WriteAudioFile(const ghc::filesystem::path &filename,
                    const AudioFile &audio_file,
                    std::optional<unsigned> new_bits_per_sample) {
    auto bits_per_sample = audio_file.bits_per_sample;
    if (new_bits_per_sample) bits_per_sample = *new_bits_per_sample;

    const auto ext = filename.extension();
    if (ext == ".flac") {
        return WriteFlacFile(filename, audio_file, bits_per_sample);
    } else if (ext == ".wav") {
        return WriteWaveFile(filename, audio_file, bits_per_sample);
    }
    return false;
}

TEST_CASE("[AudioFile]") {
    SUBCASE("conversion") {
        SUBCASE("signed single samples") {
            REQUIRE(ScaleSampleToSignedInt<s16>(1, 16) == INT16_MAX);
            REQUIRE(ScaleSampleToSignedInt<s16>(-1, 16) == INT16_MIN);
            REQUIRE(ScaleSampleToSignedInt<s32>(1, 32) == INT32_MAX);
            REQUIRE(ScaleSampleToSignedInt<s32>(-1, 32) == INT32_MIN);
        }
        SUBCASE("to signed buffer") {
            SUBCASE("s32") {
                const auto s = CreateSignedIntSamplesFromFloat<s32>({-1.0, 1.0, 0}, 32);
                REQUIRE(s.size() == 3);
                REQUIRE(s[0] == INT32_MIN);
                REQUIRE(s[1] == INT32_MAX);
                REQUIRE(s[2] == 0);
            }
            SUBCASE("s16") {
                const auto s = CreateSignedIntSamplesFromFloat<s16>({-1.0, 1.0, 0}, 16);
                REQUIRE(s.size() == 3);
                REQUIRE(s[0] == INT16_MIN);
                REQUIRE(s[1] == INT16_MAX);
                REQUIRE(s[2] == 0);
            }
        }
        SUBCASE("to unsigned buffer") {
            const auto s = CreateUnsignedIntSamplesFromFloat<u8>({-1.0, 1.0}, 8);
            REQUIRE(s.size() == 2);
            REQUIRE(s[0] == 0);
            REQUIRE(s[1] == UINT8_MAX);
        }
    }

    const auto sine_wave_440 = TestHelpers::CreateSineWaveAtFrequency(2, 44100, 1, 440);
    SUBCASE("wave") {
        for (const auto bit_depth : valid_wave_bit_depths) {
            CAPTURE(bit_depth);
            const ghc::filesystem::path filename = "test_sine_440.wav";
            REQUIRE(WriteAudioFile(filename, sine_wave_440, bit_depth));
            REQUIRE(ghc::filesystem::is_regular_file(filename));
            REQUIRE(ReadAudioFile(filename));
        }
    }
    SUBCASE("flac") {
        for (const auto bit_depth : valid_flac_bit_depths) {
            CAPTURE(bit_depth);
            const ghc::filesystem::path filename = "test_sine_440.flac";
            REQUIRE(WriteAudioFile(filename, sine_wave_440, bit_depth));
            REQUIRE(ghc::filesystem::is_regular_file(filename));
            REQUIRE(ReadAudioFile(filename));
        }
    }
}
