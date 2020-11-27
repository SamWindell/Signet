#include "audio_file.h"

#include <cstdint>
#include <iostream>

#define DR_WAV_IMPLEMENTATION
#include "FLAC/all.h"
#include "FLAC/stream_encoder.h"
#include "doctest.hpp"
#include "dr_wav.h"
#include "magic_enum.hpp"

#include "common.h"
#include "flac_decoder.h"
#include "test_helpers.h"
#include "types.h"

static constexpr unsigned valid_wave_bit_depths[] = {8, 16, 24, 32, 64};
static constexpr unsigned valid_flac_bit_depths[] = {8, 16, 20, 24};

constexpr bool IsThisMachineLittleEndian() {
    int num = 1;
    return *(char *)&num == 1;
}

bool CanFileBeConvertedToBitDepth(AudioFileFormat file, const unsigned bit_depth) {
    switch (file) {
        case AudioFileFormat::Wav: {
            const auto &arr = valid_wave_bit_depths;
            return std::find(std::begin(arr), std::end(arr), bit_depth) != std::end(arr);
        }
        case AudioFileFormat::Flac: {
            const auto &arr = valid_flac_bit_depths;
            return std::find(std::begin(arr), std::end(arr), bit_depth) != std::end(arr);
        }
        default: REQUIRE(false);
    }
    return false;
}

std::string GetLowercaseExtension(AudioFileFormat format) {
    std::string result {magic_enum::enum_name(format)};
    Lowercase(result);
    return result;
}

bool IsAudioFileReadable(const fs::path &path) {
    if (StartsWith(path.filename().generic_string(), ".")) return false;
    const auto ext = path.extension();
    return ext == ".wav" || ext == ".flac";
}

static size_t OnReadFile(void *file, void *buffer_out, size_t bytes_to_read) {
    return std::fread(buffer_out, 1, bytes_to_read, (FILE *)file);
}

template <typename SeekOrigin>
static drwav_bool32 OnSeekFile(void *file, int offset, SeekOrigin origin) {
    constexpr int fseek_success = 0;
    if (std::fseek((FILE *)file, offset, (origin == (int)drwav_seek_origin_current) ? SEEK_CUR : SEEK_SET) ==
        fseek_success) {
        return 1;
    }
    WarningWithNewLine("failed to seek file");
    return 0;
}

static u64 OnWaveChunk(void *chunk_user_data,
                       drwav_read_proc on_read,
                       drwav_seek_proc on_seek,
                       void *read_seek_user_data,
                       const drwav_chunk_header *chunk_header) {
    auto &audio_data = *(AudioData *)chunk_user_data;

    if (drwav__fourcc_equal(chunk_header->id.fourcc, "ltxt")) {
        constexpr size_t num_bytes_in_inst_chunk = 7;
        if (chunk_header->sizeInBytes != num_bytes_in_inst_chunk) {
            WarningWithNewLine("WAVE file has an incorrectly formatted Inst chunk - ignoring it");
            return 0;
        }
        std::array<u8, num_bytes_in_inst_chunk> bytes;
        const auto bytes_read = on_read(read_seek_user_data, bytes.data(), bytes.size());
        if (bytes_read == bytes.size()) {
            InstrumentData inst;
            inst.midi_note = (s8)bytes[0];
            inst.fine_tune_db = (s8)bytes[1];
            inst.gain = (s8)bytes[2];
            inst.low_note = (s8)bytes[3];
            inst.high_note = (s8)bytes[4];
            inst.low_velocity = (s8)bytes[5];
            inst.high_velocity = (s8)bytes[6];
            audio_data.instrument_data = inst;
        }
        return bytes_read;
    } else if (drwav__fourcc_equal(chunk_header->id.fourcc, "smpl") ||
               drwav__fourcc_equal(chunk_header->id.fourcc, "cue ") ||
               drwav__fourcc_equal(chunk_header->id.fourcc, "plst") ||
               drwav__fourcc_equal(chunk_header->id.fourcc, "list")) {
        std::vector<u8> chunk;

        const auto WriteU32 = [&](u32 val) {
            // TODO this assumes this machine is little endian
            chunk.resize(chunk.size() + sizeof(val));
            std::memcpy(chunk.data(), &val, sizeof(val));
        };

        chunk.push_back(chunk_header->id.fourcc[0]);
        chunk.push_back(chunk_header->id.fourcc[1]);
        chunk.push_back(chunk_header->id.fourcc[2]);
        chunk.push_back(chunk_header->id.fourcc[3]);
        WriteU32((u32)chunk_header->sizeInBytes);

        audio_data.metadata.push_back(chunk);
    } else {
        if (!drwav__fourcc_equal(chunk_header->id.fourcc, "data") &&
            !drwav__fourcc_equal(chunk_header->id.fourcc, "fmt ")) {
            char type[5];
            type[0] = (char)chunk_header->id.fourcc[0];
            type[1] = (char)chunk_header->id.fourcc[1];
            type[2] = (char)chunk_header->id.fourcc[2];
            type[3] = (char)chunk_header->id.fourcc[3];
            type[4] = '\0';

            WarningWithNewLine("Unsupported WAVE file chunk: '", type, "', this will be deleted");
        }
    }

    return 0;
}

std::optional<AudioData> ReadAudioFile(const fs::path &path) {
    MessageWithNewLine("Signet", "Reading file ", GetJustFilenameWithNoExtension(path));
    const auto file = OpenFile(path, "rb");
    if (!file) return {};

    AudioData result {};
    const auto ext = path.extension();
    if (ext == ".wav") {
        std::vector<float> f32_buf {};
        drwav wav;
        if (!drwav_init_ex(&wav, OnReadFile, OnSeekFile, OnWaveChunk, file.get(), &result, 0)) {
            WarningWithNewLine("could not init the WAV file ", path);
            return {};
        }

        result.num_channels = wav.channels;
        result.sample_rate = wav.sampleRate;
        result.bits_per_sample = wav.bitsPerSample;
        result.interleaved_samples.resize(wav.totalPCMFrameCount * wav.channels);
        f32_buf.resize(result.interleaved_samples.size());
        const auto frames_read = drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, f32_buf.data());
        if (frames_read != wav.totalPCMFrameCount) {
            WarningWithNewLine("failed to get all the frames from file ", path);
            return {};
        }
        result.format = AudioFileFormat::Wav;

        // TODO: would be nice to have a way to get double values direct rather than just casting floats...
        for (usize sample = 0; sample < f32_buf.size(); ++sample) {
            result.interleaved_samples[sample] = (double)f32_buf[sample];
        }
    } else if (ext == ".flac") {
        const bool decoded = DecodeFlacFile(file.get(), result);
        if (!decoded) {
            WarningWithNewLine("failed to decode flac file ", path);
            return {};
        }
        result.format = AudioFileFormat::Flac;
    } else {
        WarningWithNewLine("file ", path, " is not a WAV or a FLAC");
        return {};
    }

    return result;
}

template <typename SignedIntType>
SignedIntType ScaleSampleToSignedInt(const double s, const unsigned bits_per_sample) {
    const auto negative_max = std::pow(2, bits_per_sample) / 2;
    const auto positive_max = negative_max - 1;
    return static_cast<SignedIntType>(std::round(s < 0 ? s * negative_max : s * positive_max));
}

double GetScaleToAvoidClipping(const std::vector<double> &buf) {
    double max = 0;
    for (const auto s : buf) {
        max = std::max(max, std::abs(s));
    }
    if (max <= 1) return 1;
    return 1.0 / max;
}

template <typename SignedIntType>
std::vector<SignedIntType> CreateSignedIntSamplesFromFloat(const std::vector<double> &buf,
                                                           const unsigned bits_per_sample) {
    std::vector<SignedIntType> result;
    result.reserve(buf.size());
    const auto multiplier = GetScaleToAvoidClipping(buf);
    for (const auto s : buf) {
        result.push_back(ScaleSampleToSignedInt<SignedIntType>(s * multiplier, bits_per_sample));
    }

    if (multiplier != 1.0) {
        WarningWithNewLine("this audio file contained samples outside of the valid range, to avoid "
                           "distortion, the whole file was scaled down in volume");
    }

    return result;
}

template <typename UnsignedIntType>
std::vector<UnsignedIntType> CreateUnsignedIntSamplesFromFloat(const std::vector<double> &buf,
                                                               const unsigned bits_per_sample) {
    std::vector<UnsignedIntType> result;
    result.reserve(buf.size());
    const auto multiplier = GetScaleToAvoidClipping(buf);
    for (auto s : buf) {
        s *= multiplier;
        const auto scaled_val = ((s + 1.0) / 2.0f) * ((1 << bits_per_sample) - 1);
        result.push_back(static_cast<UnsignedIntType>(scaled_val));
    }

    if (multiplier != 1.0) {
        WarningWithNewLine("this audio file contained samples outside of the valid range, to avoid "
                           "distortion, the whole file was scaled down in volume");
    }

    return result;
}

static std::array<u8, 3> Convert24BitIntToBytes(const s32 sample) {
    std::array<u8, 3> bytes;
    bytes[0] = (u8)sample & 0xFF;
    bytes[1] = (u8)(sample >> 8) & 0xFF;
    bytes[2] = (u8)(sample >> 16) & 0xFF;
    return bytes;
}

static void GetAudioDataConvertedAndScaledToBitDepth(const std::vector<double> f64_buf,
                                                     drwav_uint32 &format,
                                                     unsigned bits_per_sample,
                                                     std::function<void(const void *)> callback) {
    switch (bits_per_sample) {
        case 8: {
            // 8-bit is the exception in that it uses unsigned ints
            const auto buf = CreateUnsignedIntSamplesFromFloat<u8>(f64_buf, 8);
            format = DR_WAVE_FORMAT_PCM;
            callback(buf.data());
            break;
        }
        case 16: {
            const auto buf = CreateSignedIntSamplesFromFloat<s16>(f64_buf, 16);
            format = DR_WAVE_FORMAT_PCM;
            callback(buf.data());
            break;
        }
        case 24: {
            std::vector<u8> buf;
            buf.reserve(f64_buf.size() * 3);
            for (const auto &s : f64_buf) {
                const auto sample = ScaleSampleToSignedInt<s32>(s, 24);
                for (const auto byte : Convert24BitIntToBytes(sample)) {
                    buf.push_back(byte);
                }
            }
            format = DR_WAVE_FORMAT_PCM;
            callback(buf.data());
            break;
        }
        case 32: {
            format = DR_WAVE_FORMAT_IEEE_FLOAT;
            std::vector<float> buf;
            buf.reserve(f64_buf.size());
            for (const auto s : f64_buf) {
                buf.push_back(static_cast<float>(s));
            }
            callback(buf.data());
            break;
        }
        case 64: {
            format = DR_WAVE_FORMAT_IEEE_FLOAT;
            callback(f64_buf.data());
            break;
        }
        default: {
            REQUIRE(0);
            return;
        }
    }
}

static bool WriteWaveFile(const fs::path &path, const AudioData &audio_data, const unsigned bits_per_sample) {
    if (std::find(std::begin(valid_wave_bit_depths), std::end(valid_wave_bit_depths), bits_per_sample) ==
        std::end(valid_wave_bit_depths)) {
        WarningWithNewLine("could not write wave file - the given bit depth is invalid");
        return false;
    }

    assert(IsThisMachineLittleEndian()); // TODO: add ability to write a wave file from a big endian machine

    const auto file = OpenFile(path, "wb");
    if (!file) return false;

    const auto WriteU32 = [&](u32 val) { std::fwrite(&val, sizeof(val), 1, file.get()); };
    const auto WriteU16 = [&](u16 val) { std::fwrite(&val, sizeof(val), 1, file.get()); };
    const auto WriteU8 = [&](u8 val) { std::fwrite(&val, sizeof(val), 1, file.get()); };
    const auto WriteChars = [&](std::string_view str) { std::fwrite(str.data(), 1, str.size(), file.get()); };

    WriteChars("RIFF");
    WriteU32(0); // placeholder for the file size - 8, will be filled at end
    WriteChars("WAVE");

    // fmt chunk
    {
        WriteChars("fmt ");
        WriteU32(16);

        u16 compression_code = 0;
        switch (bits_per_sample) {
            case 8:
            case 16:
            case 24:
                compression_code = 1; // pcm
                break;
            case 32:
            case 64:
                compression_code = 3; // float
                break;
            default: break;
        }
        WriteU16(compression_code);
        WriteU16((u16)audio_data.num_channels);
        WriteU32(audio_data.sample_rate);

        const u32 block_align = bits_per_sample / 8 * audio_data.num_channels;
        WriteU32(block_align * audio_data.sample_rate);
        WriteU16((u16)block_align);
        WriteU16((u16)bits_per_sample);
    }

    // inst chunk
    {
        if (audio_data.instrument_data) {
            WriteChars("ltxt");
            WriteU32(7);
            WriteU8(audio_data.instrument_data->midi_note);
            WriteU8(audio_data.instrument_data->fine_tune_db);
            WriteU8(audio_data.instrument_data->gain);
            WriteU8(audio_data.instrument_data->low_note);
            WriteU8(audio_data.instrument_data->high_note);
            WriteU8(audio_data.instrument_data->low_velocity);
            WriteU8(audio_data.instrument_data->high_velocity);

            WriteU8(0); // padding
        }
    }

    // any other chunks that we saved
    {
        for (auto &chunk : audio_data.metadata) {
            std::fwrite(chunk.data(), 1, chunk.size(), file.get());
            if ((chunk.size() % 2) == 1) {
                WriteU8(0); // padding
            }
        }
    }

    // data chunk
    {
        const auto total_num_bytes = bits_per_sample / 8 * (u32)audio_data.interleaved_samples.size();

        WriteChars("data");
        WriteU32(total_num_bytes);

        u32 format;
        GetAudioDataConvertedAndScaledToBitDepth(
            audio_data.interleaved_samples, format, bits_per_sample, [&](const void *raw_data) {
                std::fwrite(raw_data, bits_per_sample / 8, audio_data.interleaved_samples.size(), file.get());
                if ((total_num_bytes % 2) == 1) {
                    WriteU8(0); // add padding because we must must be 2 byte aligned
                }
            });
    }

    // go back and fill in the file size
    {
        const auto file_size = std::ftell(file.get());
        std::fseek(file.get(), 4, SEEK_SET);
        WriteU32(file_size - 8);
    }

    return true;
}

static void PrintFlacStatusCode(const FLAC__StreamEncoderInitStatus code) {
    switch (code) {
        case FLAC__STREAM_ENCODER_INIT_STATUS_OK: return;
        case FLAC__STREAM_ENCODER_INIT_STATUS_ENCODER_ERROR:
            ErrorWithNewLine("FLAC__STREAM_ENCODER_INIT_STATUS_ENCODER_ERROR");
            return;
            /**< General failure to set up encoder; call FLAC__stream_encoder_get_state() for cause. */

        case FLAC__STREAM_ENCODER_INIT_STATUS_UNSUPPORTED_CONTAINER:
            ErrorWithNewLine("FLAC__STREAM_ENCODER_INIT_STATUS_UNSUPPORTED_CONTAINER");
            return;
            /**< The library was not compiled with support for the given container
             * format.
             */

        case FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_CALLBACKS:
            ErrorWithNewLine("FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_CALLBACKS");
            return;
            /**< A required callback was not supplied. */

        case FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_NUMBER_OF_CHANNELS:
            ErrorWithNewLine("FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_NUMBER_OF_CHANNELS");
            return;
            /**< The encoder has an invalid setting for number of channels. */

        case FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_BITS_PER_SAMPLE:
            ErrorWithNewLine("FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_BITS_PER_SAMPLE");
            return;
            /**< The encoder has an invalid setting for bits-per-sample.
             * FLAC supports 4-32 bps but the reference encoder currently supports
             * only up to 24 bps.
             */

        case FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_SAMPLE_RATE:
            ErrorWithNewLine("FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_SAMPLE_RATE");
            return;
            /**< The encoder has an invalid setting for the input sample rate. */

        case FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_BLOCK_SIZE:
            ErrorWithNewLine("FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_BLOCK_SIZE");
            return;
            /**< The encoder has an invalid setting for the block size. */

        case FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_MAX_LPC_ORDER:
            ErrorWithNewLine("FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_MAX_LPC_ORDER");
            return;
            /**< The encoder has an invalid setting for the maximum LPC order. */

        case FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_QLP_COEFF_PRECISION:
            ErrorWithNewLine("FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_QLP_COEFF_PRECISION");
            return;
            /**< The encoder has an invalid setting for the precision of the quantized linear predictor
             * coefficients. */

        case FLAC__STREAM_ENCODER_INIT_STATUS_BLOCK_SIZE_TOO_SMALL_FOR_LPC_ORDER:
            ErrorWithNewLine("FLAC__STREAM_ENCODER_INIT_STATUS_BLOCK_SIZE_TOO_SMALL_FOR_LPC_ORDER");
            return;
            /**< The specified block size is less than the maximum LPC order. */

        case FLAC__STREAM_ENCODER_INIT_STATUS_NOT_STREAMABLE:
            ErrorWithNewLine("FLAC__STREAM_ENCODER_INIT_STATUS_NOT_STREAMABLE");
            return;
            /**< The encoder is bound to the <A HREF="../format.html#subset">Subset</A> but other settings
             * violate it. */

        case FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_METADATA:
            ErrorWithNewLine("FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_METADATA");
            return;
            /**< The metadata input to the encoder is invalid, in one of the following ways:
             * - FLAC__stream_encoder_set_metadata() was called with a null pointer but a block count > 0
             * - One of the metadata blocks contains an undefined type
             * - It contains an illegal CUESHEET as checked by FLAC__format_cuesheet_is_legal()
             * - It contains an illegal SEEKTABLE as checked by FLAC__format_seektable_is_legal()
             * - It contains more than one SEEKTABLE block or more than one VORBIS_COMMENT block
             */

        case FLAC__STREAM_ENCODER_INIT_STATUS_ALREADY_INITIALIZED:
            ErrorWithNewLine("FLAC__STREAM_ENCODER_INIT_STATUS_ALREADY_INITIALIZED");
            return;
            /**< FLAC__stream_encoder_init_*() was called when the encoder was
             * already initialized, usually because
             * FLAC__stream_encoder_finish() was not called.*/
    }
}

static bool
WriteFlacFile(const fs::path &filename, const AudioData &audio_data, const unsigned bits_per_sample) {
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

    FLAC__stream_encoder_set_channels(encoder.get(), audio_data.num_channels);
    FLAC__stream_encoder_set_bits_per_sample(encoder.get(), bits_per_sample);
    FLAC__stream_encoder_set_sample_rate(encoder.get(), audio_data.sample_rate);
    FLAC__stream_encoder_set_total_samples_estimate(encoder.get(), audio_data.interleaved_samples.size());

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
        CreateSignedIntSamplesFromFloat<s32>(audio_data.interleaved_samples, bits_per_sample);
    if (!FLAC__stream_encoder_process_interleaved(encoder.get(), int32_buffer.data(),
                                                  (unsigned)audio_data.NumFrames())) {
        WarningWithNewLine("could not write flac file - failed encoding samples");
        return false;
    }

    if (!FLAC__stream_encoder_finish(encoder.get())) {
        WarningWithNewLine("could not write flac file - error finishing encoding");
        return false;
    }
    return true;
}

bool WriteAudioFile(const fs::path &filename,
                    const AudioData &audio_data,
                    std::optional<unsigned> new_bits_per_sample) {
    auto bits_per_sample = audio_data.bits_per_sample;
    if (new_bits_per_sample) bits_per_sample = *new_bits_per_sample;

    bool result = false;
    const auto ext = filename.extension();
    if (ext == ".flac") {
        result = WriteFlacFile(filename, audio_data, bits_per_sample);
    } else if (ext == ".wav") {
        result = WriteWaveFile(filename, audio_data, bits_per_sample);
    }

    if (result) MessageWithNewLine("Signet", "Successfully wrote file ", filename);
    return result;
}

struct BufferConversionTest {
    template <typename T>
    static void Check(const std::vector<double> &buf,
                      const unsigned bits_per_sample,
                      const std::vector<T> expected,
                      const u32 expected_format) {
        u32 format;
        GetAudioDataConvertedAndScaledToBitDepth(buf, format, bits_per_sample, [&](const void *raw_data) {
            const auto data = (const T *)raw_data;
            for (usize i = 0; i < expected.size(); ++i) {
                REQUIRE(data[i] == expected[i]);
            }
        });
        REQUIRE(format == expected_format);
    };
};

TEST_CASE("[AudioData]") {
    SUBCASE("AudioData object") {
        SUBCASE("MultiplyByScalar") {
            AudioData file;
            file.interleaved_samples = {1, 1};
            file.MultiplyByScalar(0.5);
            REQUIRE(file.interleaved_samples[0] == doctest::Approx(0.5));
            REQUIRE(file.interleaved_samples[1] == doctest::Approx(0.5));
        }
        SUBCASE("AddOther") {
            AudioData file;
            file.interleaved_samples = {1, 1};
            SUBCASE("larger") {
                AudioData file2;
                file2.interleaved_samples = {1, 1, 1};
                file.AddOther(file2);
                REQUIRE(file.interleaved_samples[0] == 2);
                REQUIRE(file.interleaved_samples[1] == 2);
                REQUIRE(file.interleaved_samples[2] == 1);
            }
            SUBCASE("smaller") {
                AudioData file2;
                file2.interleaved_samples = {1};
                file.AddOther(file2);
                REQUIRE(file.interleaved_samples[0] == 2);
                REQUIRE(file.interleaved_samples[1] == 1);
            }
            SUBCASE("equal") {
                AudioData file2;
                file2.interleaved_samples = {1, 1};
                file.AddOther(file2);
                REQUIRE(file.interleaved_samples[0] == 2);
                REQUIRE(file.interleaved_samples[1] == 2);
            }
        }
    }

    SUBCASE("conversion") {
        SUBCASE("signed single samples") {
            REQUIRE(ScaleSampleToSignedInt<s16>(1, 16) == INT16_MAX);
            REQUIRE(ScaleSampleToSignedInt<s16>(-1, 16) == INT16_MIN);
            REQUIRE(ScaleSampleToSignedInt<s16>(0, 16) == 0);

            REQUIRE(ScaleSampleToSignedInt<s32>(1, 32) == INT32_MAX);
            REQUIRE(ScaleSampleToSignedInt<s32>(-1, 32) == INT32_MIN);
            REQUIRE(ScaleSampleToSignedInt<s32>(0, 32) == 0);

            REQUIRE(ScaleSampleToSignedInt<s32>(-1, 24) == -8388608);
            REQUIRE(ScaleSampleToSignedInt<s32>(1, 24) == 8388607);
            REQUIRE(ScaleSampleToSignedInt<s32>(0, 24) == 0);
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

        SUBCASE("wave f64 to other bit depth conversion") {
            std::vector<double> buf = {-1.0, 0, 1.0, 0};

            SUBCASE("to unsigned 8-bit data") {
                BufferConversionTest::Check<u8>(buf, 8, {0, 127, 255, 127}, DR_WAVE_FORMAT_PCM);
            }

            SUBCASE("to signed 16-bit data") {
                BufferConversionTest::Check<s16>(buf, 16, {INT16_MIN, 0, INT16_MAX, 0}, DR_WAVE_FORMAT_PCM);
            }

            SUBCASE("to signed 24-bit data") {
                std::vector<u8> expected_bytes;
                for (const auto s : buf) {
                    for (const auto byte : Convert24BitIntToBytes(ScaleSampleToSignedInt<s32>(s, 24))) {
                        expected_bytes.push_back(byte);
                    }
                }
                BufferConversionTest::Check<u8>(buf, 24, expected_bytes, DR_WAVE_FORMAT_PCM);
            }

            SUBCASE("to 32-bit float data") {
                BufferConversionTest::Check<float>(buf, 32, {-1.0f, 0.0f, 1.0f, 0.0f},
                                                   DR_WAVE_FORMAT_IEEE_FLOAT);
            }

            SUBCASE("to 64-bit float data") {
                BufferConversionTest::Check<double>(buf, 64, {-1.0, 0.0, 1.0, 0.0},
                                                    DR_WAVE_FORMAT_IEEE_FLOAT);
            }
        }
    }

    SUBCASE("writing and reading all bitdepths") {
        const auto sine_wave_440 = TestHelpers::CreateSineWaveAtFrequency(2, 44100, 1, 440);
        SUBCASE("wave") {
            for (const auto bit_depth : valid_wave_bit_depths) {
                CAPTURE(bit_depth);
                const fs::path filename = "test_sine_440.wav";
                REQUIRE(WriteAudioFile(filename, sine_wave_440, bit_depth));
                REQUIRE(fs::is_regular_file(filename));
                REQUIRE(ReadAudioFile(filename));
            }
        }
        SUBCASE("flac") {
            for (const auto bit_depth : valid_flac_bit_depths) {
                CAPTURE(bit_depth);
                const fs::path filename = "test_sine_440.flac";
                REQUIRE(WriteAudioFile(filename, sine_wave_440, bit_depth));
                REQUIRE(fs::is_regular_file(filename));
                REQUIRE(ReadAudioFile(filename));
            }
        }
    }
}
