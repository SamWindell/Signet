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
#include "tests_config.h"
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

static drwav_bool32 OnSeekFile(void *file, int offset, drwav_seek_origin origin) {
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
                       const drwav_chunk_header *chunk_header,
                       drwav_container container,
                       const drwav_fmt *pFMT) {
    return 0;
}

std::ostream &operator<<(std::ostream &os, const drwav_smpl &s) {
    os << "{\n";
    os << "  manufacturer: " << s.manufacturer << "\n";
    os << "  product: " << s.product << "\n";
    os << "  samplePeriod: " << s.samplePeriod << "\n";
    os << "  midiUnityNotes: " << s.midiUnityNotes << "\n";
    os << "  midiPitchFraction: " << s.midiPitchFraction << "\n";
    os << "  smpteFormat: " << s.smpteFormat << "\n";
    os << "  smpteOffset: " << s.smpteOffset << "\n";
    os << "  numSampleLoops: " << s.numSampleLoops << "\n";
    os << "  samplerData: " << s.samplerData << "\n";

    os << "  loops: [\n";
    for (u32 i = 0; i < s.numSampleLoops; ++i) {
        auto &smpl_loop = s.loops[i];

        os << "    {\n";
        os << "      cuePointId: " << smpl_loop.cuePointId << "\n";
        os << "      type: " << smpl_loop.type << "\n";
        os << "      start: " << smpl_loop.start << "\n";
        os << "      end: " << smpl_loop.end << "\n";
        os << "      fraction: " << smpl_loop.fraction << "\n";
        os << "      playCount: " << smpl_loop.playCount << "\n";
        os << "    }\n";
    }
    os << "  ]\n";
    os << "}\n";
    return os;
}

std::ostream &operator<<(std::ostream &os, const drwav_cue &c) {
    os << "{\n";
    os << "  numCuePoints: " << c.numCuePoints << "\n";

    os << "  cuePoints: [\n";
    for (u32 i = 0; i < c.numCuePoints; ++i) {
        auto &cue = c.cuePoints[i];
        os << "    {\n";
        os << "      id: " << cue.id << "\n";
        os << "      position: " << cue.position << "\n";
        os << "      dataChunkId: " << (char)cue.dataChunkId[0] << (char)cue.dataChunkId[1]
           << (char)cue.dataChunkId[2] << (char)cue.dataChunkId[3] << "\n";
        os << "      chunkStart: " << cue.chunkStart << "\n";
        os << "      blockStart: " << cue.blockStart << "\n";
        os << "      sampleOffset: " << cue.sampleOffset << "\n";
        os << "    }\n";
    }
    os << "  ]\n";
    os << "}\n";
    return os;
}

std::ostream &operator<<(std::ostream &os, const drwav_inst &i) {
    os << "{\n";
    os << "  midi_note: " << (int)i.midiNote << "\n";
    os << "  fine_tune_db: " << (int)i.fineTuneDb << "\n";
    os << "  gain: " << (int)i.gain << "\n";
    os << "  low_note: " << (int)i.lowNote << "\n";
    os << "  high_note: " << (int)i.highNote << "\n";
    os << "  low_velocity: " << (int)i.lowVelocity << "\n";
    os << "  high_velocity: " << (int)i.highVelocity << "\n";
    os << "}\n";
    return os;
}

std::ostream &operator<<(std::ostream &os, const drwav_acid &a) {
    os << "{\n";
    os << "  flags: " << a.flags << "\n";
    os << "  rootNote: " << a.rootNote << "\n";
    os << "  unknown1: " << a.reserved1 << "\n";
    os << "  unknown2: " << a.reserved2 << "\n";
    os << "  numBeats: " << a.numBeats << "\n";
    os << "  meterDenominator: " << a.meterDenominator << "\n";
    os << "  meterNumerator: " << a.meterNumerator << "\n";
    os << "  tempo: " << a.tempo << "\n";
    os << "}\n";
    return os;
}

std::ostream &operator<<(std::ostream &os, const drwav_list_label_or_note &l) {
    os << "{\n";
    os << "  cuePointId: " << l.cuePointId << "\n";
    os << "  stringSize: " << l.stringSize << "\n";
    os << "  string: " << l.string << "\n";
    os << "}\n";
    return os;
}

std::ostream &operator<<(std::ostream &os, const drwav_list_labelled_text &l) {
    os << "{\n";
    os << "  cuePointId: " << l.cuePointId << "\n";
    os << "  sampleLength: " << l.sampleLength << "\n";
    os << "  purposeId: " << (char)l.purposeId[0] << (char)l.purposeId[1] << (char)l.purposeId[2]
       << (char)l.purposeId[3] << "\n";
    os << "  country: " << l.country << "\n";
    os << "  language: " << l.language << "\n";
    os << "  dialect: " << l.dialect << "\n";
    os << "  codePage: " << l.codePage << "\n";
    os << "  stringSize: " << l.stringSize << "\n";
    os << "  string: ";
    if (l.string) os << l.string;
    os << "\n";
    os << "}\n";
    return os;
}

std::ostream &operator<<(std::ostream &os, const drwav_list_info_text &l) {
    os << "{\n";
    os << "  stringSize: " << l.stringSize << "\n";
    os << "  string: " << l.string << "\n";
    os << "}\n";
    return os;
}

std::ostream &operator<<(std::ostream &os, const drwav_unknown_metadata &u) {
    os << "{\n";
    os << "  id: " << (char)u.id[0] << (char)u.id[1] << (char)u.id[2] << (char)u.id[3] << "\n";
    if (u.chunkLocation == drwav_metadata_location_invalid) {
        os << "  chunkLocation: invalid\n";
    } else if (u.chunkLocation == drwav_metadata_location_inside_info_list) {
        os << "  chunkLocation: info list\n";
    } else if (u.chunkLocation == drwav_metadata_location_inside_adtl_list) {
        os << "  chunkLocation: adtl list\n";
    }
    os << "  dataSizeInBytes: " << u.dataSizeInBytes << "\n";
    os << "}\n";
    return os;
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

        DebugWithNewLine("=================");
        if (!drwav_init_ex_with_metadata(&wav, OnReadFile, OnSeekFile, OnWaveChunk, file.get(), nullptr, 0,
                                         nullptr, (u64)drwav_metadata_type_all)) {
            WarningWithNewLine("could not init the WAV file ", path);
            return {};
        }

        if (wav.numMetadata) {
            DebugWithNewLine("Wav num metadata: ", wav.numMetadata);

            for (size_t i = 0; i < wav.numMetadata; ++i) {
                auto &m = wav.metadata[i];
                switch (m.type) {
                    case drwav_metadata_type_smpl: {
                        DebugWithNewLine("type: smpl");
                        DebugWithNewLine(m.smpl);
                        break;
                    }
                    case drwav_metadata_type_inst: {
                        DebugWithNewLine("type: inst");
                        DebugWithNewLine(m.inst);
                        break;
                    }
                    case drwav_metadata_type_cue: {
                        DebugWithNewLine("type: cue");
                        DebugWithNewLine(m.cue);
                        break;
                    }
                    case drwav_metadata_type_acid: {
                        DebugWithNewLine("type: acid");
                        DebugWithNewLine(m.acid);
                        break;
                    }
                    case drwav_metadata_type_list_label: {
                        DebugWithNewLine("type: labelOrNote");
                        DebugWithNewLine(m.labelOrNote);
                        break;
                    }
                    case drwav_metadata_type_list_note: {
                        DebugWithNewLine("type: list_note");
                        DebugWithNewLine(m.labelOrNote);
                        break;
                    }
                    case drwav_metadata_type_list_labelled_text: {
                        DebugWithNewLine("type: list_labelled_text");
                        DebugWithNewLine(m.labelledText);
                        break;
                    }
                    case drwav_metadata_type_list_info_software:
                    case drwav_metadata_type_list_info_copyright:
                    case drwav_metadata_type_list_info_title:
                    case drwav_metadata_type_list_info_artist:
                    case drwav_metadata_type_list_info_comment:
                    case drwav_metadata_type_list_info_date:
                    case drwav_metadata_type_list_info_genre:
                    case drwav_metadata_type_list_info_album:
                    case drwav_metadata_type_list_info_tracknumber: {
                        DebugWithNewLine("type: list info");
                        DebugWithNewLine(m.infoText);
                        break;
                    }
                    case drwav_metadata_type_unknown: {
                        DebugWithNewLine("type: unknown");
                        DebugWithNewLine(m.unknown);
                        break;
                    }
                }
            }

            result.num_wave_metadata = wav.numMetadata;
            result.wave_metadata = {drwav_take_ownership_of_metadata(&wav),
                                    [](drwav_metadata *m) { drwav_free(m, NULL); }};
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

static size_t OnWrite(void *pUserData, const void *pData, size_t bytesToWrite) {
    return fwrite(pData, 1, bytesToWrite, (FILE *)pUserData);
}

static bool WriteWaveFile(const fs::path &path, const AudioData &audio_data, const unsigned bits_per_sample) {
    if (std::find(std::begin(valid_wave_bit_depths), std::end(valid_wave_bit_depths), bits_per_sample) ==
        std::end(valid_wave_bit_depths)) {
        WarningWithNewLine("could not write wave file - the given bit depth is invalid");
        return false;
    }

    const auto file = OpenFile(path, "wb");
    if (!file) return false;

    drwav_data_format format {};
    format.container = drwav_container_riff; // Use rf64 for large files?
    format.format =
        (bits_per_sample == 32 || bits_per_sample == 64) ? DR_WAVE_FORMAT_IEEE_FLOAT : DR_WAVE_FORMAT_PCM;
    format.channels = audio_data.num_channels;
    format.sampleRate = audio_data.sample_rate;
    format.bitsPerSample = bits_per_sample;

    if (audio_data.num_wave_metadata) {
        DebugWithNewLine("\n\n\nWRITE\n\n\n");
        for (size_t i = 0; i < audio_data.num_wave_metadata; ++i) {
            auto &m = audio_data.wave_metadata.get()[i];
            switch (m.type) {
                case drwav_metadata_type_smpl: {
                    DebugWithNewLine("type: smpl");
                    DebugWithNewLine(m.smpl);
                    break;
                }
                case drwav_metadata_type_inst: {
                    DebugWithNewLine("type: inst");
                    DebugWithNewLine(m.inst);
                    break;
                }
                case drwav_metadata_type_cue: {
                    DebugWithNewLine("type: cue");
                    DebugWithNewLine(m.cue);
                    break;
                }
                case drwav_metadata_type_acid: {
                    DebugWithNewLine("type: acid");
                    DebugWithNewLine(m.acid);
                    break;
                }
                case drwav_metadata_type_list_label: {
                    DebugWithNewLine("type: labelOrNote");
                    DebugWithNewLine(m.labelOrNote);
                    break;
                }
                case drwav_metadata_type_list_note: {
                    DebugWithNewLine("type: list_note");
                    DebugWithNewLine(m.labelOrNote);
                    break;
                }
                case drwav_metadata_type_list_labelled_text: {
                    DebugWithNewLine("type: list_labelled_text");
                    DebugWithNewLine(m.labelledText);
                    break;
                }
                case drwav_metadata_type_list_info_software:
                case drwav_metadata_type_list_info_copyright:
                case drwav_metadata_type_list_info_title:
                case drwav_metadata_type_list_info_artist:
                case drwav_metadata_type_list_info_comment:
                case drwav_metadata_type_list_info_date:
                case drwav_metadata_type_list_info_genre:
                case drwav_metadata_type_list_info_album:
                case drwav_metadata_type_list_info_tracknumber: {
                    DebugWithNewLine("type: list info");
                    DebugWithNewLine(m.infoText);
                    break;
                }
                case drwav_metadata_type_unknown: {
                    DebugWithNewLine("type: unknown");
                    DebugWithNewLine(m.unknown);
                    break;
                }
            }
        }
    }

    drwav wav;
    drwav_init_write_with_metadata(&wav, &format, OnWrite, OnSeekFile, file.get(), nullptr,
                                   audio_data.num_wave_metadata ? audio_data.wave_metadata.get() : NULL,
                                   audio_data.num_wave_metadata);

    u32 f;
    bool succeed_writing = true;
    GetAudioDataConvertedAndScaledToBitDepth(
        audio_data.interleaved_samples, f, bits_per_sample, [&](const void *raw_data) {
            if (!succeed_writing) return;
            u64 frames_written = 0;
            if (format.format == DR_WAVE_FORMAT_PCM) {
                frames_written = drwav_write_pcm_frames(&wav, audio_data.NumFrames(), raw_data);
            } else if (format.format == DR_WAVE_FORMAT_IEEE_FLOAT) {
                frames_written = drwav_write_float_frames(&wav, audio_data.NumFrames(), raw_data);
            }
            if (frames_written != audio_data.NumFrames()) {
                ErrorWithNewLine("frames_written != audio_data.interleaved_samples.size()");
                succeed_writing = true;
            }
        });

    drwav_uninit(&wav);
    return succeed_writing;
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

    std::vector<FLAC__StreamMetadata *> metadata;
    std::shared_ptr<FLAC__StreamMetadata> vorbis_comment_meta {};
    for (auto m : audio_data.flac_metadata) {
        if (m->type == FLAC__METADATA_TYPE_VORBIS_COMMENT) {
            vorbis_comment_meta = m;
        } else {
            metadata.push_back(m.get());
        }
    }

    if (audio_data.instrument_data) {
        if (!vorbis_comment_meta) {
            vorbis_comment_meta = {FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT),
                                   &FLAC__metadata_object_delete};
        }

        std::string buffer;
        auto AddComment = [&](std::string_view key, u8 value) {
            buffer = std::string(key);
            buffer.append("=");
            buffer.append(std::to_string((int)value));

            FLAC__StreamMetadata_VorbisComment_Entry entry;
            entry.length = (u32)buffer.size();
            entry.entry = (FLAC__byte *)buffer.data();
            FLAC__metadata_object_vorbiscomment_append_comment(vorbis_comment_meta.get(), entry, true);
        };

        AddComment("MIDI_NOTE", audio_data.instrument_data->midi_note);
        AddComment("FINE_TUNE_DB", audio_data.instrument_data->fine_tune_db);
        AddComment("GAIN", audio_data.instrument_data->gain);
        AddComment("LOW_NOTE", audio_data.instrument_data->low_note);
        AddComment("HIGH_NOTE", audio_data.instrument_data->high_note);
        AddComment("LOW_VELOCITY", audio_data.instrument_data->low_velocity);
        AddComment("HIGH_VELOCITY", audio_data.instrument_data->high_velocity);
    }

    if (vorbis_comment_meta) {
        metadata.push_back(vorbis_comment_meta.get());
    }

    const bool set_metadata =
        FLAC__stream_encoder_set_metadata(encoder.get(), metadata.data(), (unsigned)metadata.size());
    assert(set_metadata);

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

    SUBCASE("wave with marker and loops") {
        const fs::path in_filename = TEST_DATA_DIRECTORY "/wave_with_markers_and_loop.wav";
        const fs::path out_filename = "out_wave_with_markers_and_loop.wav";
        auto f = ReadAudioFile(in_filename);
        REQUIRE(f);
        REQUIRE(f->num_wave_metadata >= 2);
        DebugWithNewLine("f->wave_metadata ", f->num_wave_metadata);
        REQUIRE(WriteAudioFile(out_filename, f.value(), 16));
        REQUIRE(fs::is_regular_file(out_filename));
        auto out_f = ReadAudioFile(out_filename);
        REQUIRE(out_f);
        REQUIRE(out_f->num_wave_metadata >= 2);
        DebugWithNewLine("out_f->wave_metadata ", out_f->num_wave_metadata);
    }

    SUBCASE("flac with comments") {
        const fs::path in_filename = TEST_DATA_DIRECTORY "/flac_with_comments.flac";
        const fs::path out_filename = "out_flac_with_comments.flac";
        auto f = ReadAudioFile(in_filename);
        REQUIRE(f);
        REQUIRE(f->flac_metadata.size() >= 1);
        REQUIRE(WriteAudioFile(out_filename, f.value(), 16));
        REQUIRE(fs::is_regular_file(out_filename));
        auto out_f = ReadAudioFile(out_filename);
        REQUIRE(out_f);
        REQUIRE(out_f->flac_metadata.size() >= 1);
    }
}
