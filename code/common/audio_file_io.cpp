#include "audio_file_io.h"

#include <cstdint>
#include <iostream>

#define DR_WAV_IMPLEMENTATION
#include "FLAC/all.h"
#include "FLAC/stream_encoder.h"
#include "doctest.hpp"
#include "dr_wav.h"
#include "magic_enum.hpp"
#include <cereal/archives/json.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include "common.h"
#include "flac_decoder.h"
#include "test_helpers.h"
#include "tests_config.h"
#include "types.h"

static constexpr unsigned valid_wave_bit_depths[] = {8, 16, 24, 32, 64};
static constexpr unsigned valid_flac_bit_depths[] = {8, 16, 20, 24};

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

bool IsPathReadableAudioFile(const fs::path &path) {
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
    WarningWithNewLine("Wav", {}, "failed to seek file");
    return 0;
}

static size_t OnWrite(void *pUserData, const void *pData, size_t bytesToWrite) {
    return fwrite(pData, 1, bytesToWrite, (FILE *)pUserData);
}

std::ostream &operator<<(std::ostream &os, const drwav_smpl &s) {
    os << "{\n";
    os << "  manufacturer: " << s.manufacturerId << "\n";
    os << "  product: " << s.productId << "\n";
    os << "  samplePeriodNanoseconds: " << s.samplePeriodNanoseconds << "\n";
    os << "  midiUnityNote: " << s.midiUnityNote << "\n";
    os << "  midiPitchFraction: " << s.midiPitchFraction << "\n";
    os << "  smpteFormat: " << s.smpteFormat << "\n";
    os << "  smpteOffset: " << s.smpteOffset << "\n";
    os << "  sampleLoopCount: " << s.sampleLoopCount << "\n";
    os << "  numBytesOfSamplerSpecificData: " << s.samplerSpecificDataSizeInBytes << "\n";

    os << "  loops: [\n";
    for (u32 i = 0; i < s.sampleLoopCount; ++i) {
        auto &smpl_loop = s.pLoops[i];

        os << "    {\n";
        os << "      cuePointId: " << smpl_loop.cuePointId << "\n";
        os << "      type: " << smpl_loop.type << "\n";
        os << "      firstSampleByteOffset: " << smpl_loop.firstSampleByteOffset << "\n";
        os << "      lastSampleByteOffset: " << smpl_loop.lastSampleByteOffset << "\n";
        os << "      sampleFraction: " << smpl_loop.sampleFraction << "\n";
        os << "      playCount: " << smpl_loop.playCount << "\n";
        os << "    }\n";
    }
    os << "  ]\n";
    os << "}\n";
    return os;
}

std::ostream &operator<<(std::ostream &os, const drwav_cue &c) {
    os << "{\n";
    os << "  cuePointCount: " << c.cuePointCount << "\n";

    os << "  cuePoints: [\n";
    for (u32 i = 0; i < c.cuePointCount; ++i) {
        auto &cue = c.pCuePoints[i];
        os << "    {\n";
        os << "      id: " << cue.id << "\n";
        os << "      position: " << cue.playOrderPosition << "\n";
        os << "      dataChunkId: " << (char)cue.dataChunkId[0] << (char)cue.dataChunkId[1]
           << (char)cue.dataChunkId[2] << (char)cue.dataChunkId[3] << "\n";
        os << "      chunkStart: " << cue.chunkStart << "\n";
        os << "      blockStart: " << cue.blockStart << "\n";
        os << "      sampleByteOffset: " << cue.sampleByteOffset << "\n";
        os << "    }\n";
    }
    os << "  ]\n";
    os << "}\n";
    return os;
}

std::ostream &operator<<(std::ostream &os, const drwav_inst &i) {
    os << "{\n";
    os << "  midiUnityNote: " << (int)i.midiUnityNote << "\n";
    os << "  fine_tune_cents: " << (int)i.fineTuneCents << "\n";
    os << "  gainDecibels: " << (int)i.gainDecibels << "\n";
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
    os << "  midiUnityNote: " << a.midiUnityNote << "\n";
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
    os << "  stringSize: " << l.stringLength << "\n";
    os << "  string: " << l.pString << "\n";
    os << "}\n";
    return os;
}

std::ostream &operator<<(std::ostream &os, const drwav_list_labelled_cue_region &l) {
    os << "{\n";
    os << "  cuePointId: " << l.cuePointId << "\n";
    os << "  sampleLength: " << l.sampleLength << "\n";
    os << "  purposeId: " << (char)l.purposeId[0] << (char)l.purposeId[1] << (char)l.purposeId[2]
       << (char)l.purposeId[3] << "\n";
    os << "  country: " << l.country << "\n";
    os << "  language: " << l.language << "\n";
    os << "  dialect: " << l.dialect << "\n";
    os << "  codePage: " << l.codePage << "\n";
    os << "  stringLength: " << l.stringLength << "\n";
    os << "  string: ";
    if (l.pString) os << l.pString;
    os << "\n";
    os << "}\n";
    return os;
}

std::ostream &operator<<(std::ostream &os, const drwav_list_info_text &l) {
    os << "{\n";
    os << "  stringSize: " << l.stringLength << "\n";
    os << "  string: " << l.pString << "\n";
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

std::ostream &operator<<(std::ostream &os, const drwav_bext &b) {
    os << "{\n";
    os << "  description: " << ((b.pDescription) ? b.pDescription : "") << "\n";
    os << "  originatorName: " << ((b.pOriginatorName) ? b.pOriginatorName : "") << "\n";
    os << "  originatorReference: " << ((b.pOriginatorReference) ? b.pOriginatorReference : "") << "\n";
    os << "  originationDate: " << std::string_view(b.pOriginationDate, sizeof(b.pOriginationDate)) << "\n";
    os << "  originationTime: " << std::string_view(b.pOriginationTime, sizeof(b.pOriginationTime)) << "\n";
    os << "  timeReference: " << b.timeReference << "\n";
    os << "  version: " << b.version << "\n";
    os << "  loudnessValue: " << b.loudnessValue << "\n";
    os << "  loudnessRange: " << b.loudnessRange << "\n";
    os << "  maxTruePeakLevel: " << b.maxTruePeakLevel << "\n";
    os << "  maxMomentaryLoudness: " << b.maxMomentaryLoudness << "\n";
    os << "  maxShortTermLoudness: " << b.maxShortTermLoudness << "\n";
    os << "  codingHistory: ";
    if (b.pCodingHistory) {
        os << b.pCodingHistory;
    }
    os << "\n";
    os << "}\n";
    return os;
}

class WaveMetadataToNonSpecificMetadata {
  public:
    WaveMetadataToNonSpecificMetadata(const WaveMetadata &wave_metadata, const AudioData &audio)
        : m_wave_metadata(wave_metadata), m_audio(audio) {}

    Metadata Convert() const {
        Metadata result {};

        // TODO: is this metadata valid if the wave file was compressed, are things like
        // cue_point.sampleByteOffset valid?

        if (const auto root_note = FindMidiRootNote()) {
            result.midi_mapping.emplace();
            result.midi_mapping->root_midi_note = *root_note;
        }

        for (const auto &m : m_wave_metadata) {
            switch (m.type) {
                case drwav_metadata_type_smpl: {
                    std::vector<MetadataItems::Loop> loops;
                    for (u32 i = 0; i < m.data.smpl.sampleLoopCount; ++i) {
                        loops.push_back(CreateSampleLoop(m.data.smpl.pLoops[i]));
                    }
                    result.loops = loops;
                    break;
                }
                case drwav_metadata_type_inst: {
                    MetadataItems::SamplerMapping data;
                    data.fine_tune_cents = m.data.inst.fineTuneCents;
                    data.gain_db = m.data.inst.gainDecibels;
                    data.low_note = m.data.inst.lowNote;
                    data.high_note = m.data.inst.highNote;
                    data.low_velocity = m.data.inst.lowVelocity;
                    data.high_velocity = m.data.inst.highVelocity;
                    assert(result.midi_mapping); // we should have created it for the midi root note after
                                                 // calling FindMidiRootNote
                    result.midi_mapping->sampler_mapping = data;
                    break;
                }
                case drwav_metadata_type_cue: {
                    std::vector<MetadataItems::Marker> markers;
                    for (u32 i = 0; i < m.data.cue.cuePointCount; ++i) {
                        markers.push_back(CreateMarker(m.data.cue.pCuePoints[i]));
                    }
                    result.markers = markers;
                    break;
                }
                case drwav_metadata_type_acid: {
                    MetadataItems::TimingInfo info;
                    info.playback_type = (m.data.acid.flags & drwav_acid_flag_one_shot)
                                             ? MetadataItems::PlaybackType::OneShot
                                             : MetadataItems::PlaybackType::Loop;
                    info.num_beats = m.data.acid.numBeats;
                    info.time_signature_denominator = m.data.acid.meterDenominator;
                    info.time_signature_numerator = m.data.acid.meterNumerator;
                    info.tempo = m.data.acid.tempo;
                    result.timing_info = info;
                    break;
                }
                case drwav_metadata_type_list_labelled_cue_region: {
                    result.regions.push_back(CreateRegion(m.data.labelledCueRegion));
                    break;
                }
            }
        }

        return result;
    }

  private:
    MetadataItems::Loop CreateSampleLoop(const drwav_smpl_loop &loop) const {
        MetadataItems::Loop result {};

        for (const auto &m : m_wave_metadata) {
            if (m.type == drwav_metadata_type_list_label) {
                if (m.data.labelOrNote.cuePointId == loop.cuePointId && m.data.labelOrNote.stringLength) {
                    result.name = std::string(m.data.labelOrNote.pString);
                }
            }
        }

        switch (loop.type) {
            case drwav_smpl_loop_type_forward: result.type = MetadataItems::LoopType::Forward; break;
            case drwav_smpl_loop_type_pingpong: result.type = MetadataItems::LoopType::PingPong; break;
            case drwav_smpl_loop_type_backward: result.type = MetadataItems::LoopType::Backward; break;
            default: result.type = MetadataItems::LoopType::Forward;
        }

        result.start_frame =
            loop.firstSampleByteOffset / (m_audio.bits_per_sample / 8) / m_audio.num_channels;
        const auto end_frame =
            (loop.lastSampleByteOffset / (m_audio.bits_per_sample / 8) / m_audio.num_channels) + 1;
        result.num_frames = end_frame - result.start_frame;
        result.num_times_to_loop = loop.playCount;

        // TODO: handle these cases properly instead of asserting
        assert(result.start_frame < m_audio.NumFrames());
        assert(end_frame <= m_audio.NumFrames());

        return result;
    }

    MetadataItems::Region CreateRegion(const drwav_list_labelled_cue_region &region) const {
        MetadataItems::Region result {};
        if (region.stringLength) result.name = std::string(region.pString);

        bool found_cue = false;

        for (const auto &m : m_wave_metadata) {
            if (m.type == drwav_metadata_type_cue) {
                for (u32 i = 0; i < m.data.cue.cuePointCount; ++i) {
                    const auto &cue_point = m.data.cue.pCuePoints[i];
                    if (cue_point.id == region.cuePointId) {
                        result.start_frame =
                            cue_point.sampleByteOffset / (m_audio.bits_per_sample / 8) / m_audio.num_channels;
                        found_cue = true;
                        break;
                    }
                }
            } else if (m.type == drwav_metadata_type_list_label) {
                if (m.data.labelOrNote.cuePointId == region.cuePointId && m.data.labelOrNote.stringLength) {
                    result.initial_marker_name = std::string(m.data.labelOrNote.pString);
                }
            }
        }

        result.num_frames = region.sampleLength / m_audio.num_channels;

        // TODO: handle these cases properly instead of asserting
        assert(found_cue);
        assert(result.start_frame < m_audio.NumFrames());
        assert((result.start_frame + result.num_frames) <= m_audio.NumFrames());
        (void)found_cue;

        return result;
    }

    MetadataItems::Marker CreateMarker(const drwav_cue_point &cue_point) const {
        MetadataItems::Marker result {};
        for (const auto &m : m_wave_metadata) {
            if (m.type == drwav_metadata_type_list_label) {
                if (m.data.labelOrNote.cuePointId == cue_point.id && m.data.labelOrNote.stringLength) {
                    result.name = std::string(m.data.labelOrNote.pString);
                    break;
                }
            }
        }
        result.start_frame =
            cue_point.sampleByteOffset / (m_audio.bits_per_sample / 8) / m_audio.num_channels;
        // TODO: handle thi cases properly instead of asserting
        assert(result.start_frame < m_audio.NumFrames());
        return result;
    }

    std::optional<int> FindMidiRootNote() const {
        // There are 3 different places this data might be...  so we just kind of arbitrarily pick one
        for (const auto &m : m_wave_metadata) {
            if (m.type == drwav_metadata_type_inst) {
                return m.data.inst.midiUnityNote;
            }
        }

        for (const auto &m : m_wave_metadata) {
            if (m.type == drwav_metadata_type_smpl) {
                return m.data.smpl.midiUnityNote;
            }
        }

        for (const auto &m : m_wave_metadata) {
            if (m.type == drwav_metadata_type_acid && m.data.acid.flags & drwav_acid_flag_root_note_set) {
                return m.data.acid.midiUnityNote;
            }
        }

        return {};
    }

    const WaveMetadata &m_wave_metadata;
    const AudioData &m_audio;
};

void DebugPrintAllMetadata(const WaveMetadata &metadata) {
    DebugWithNewLine("Wav num metadata: {}", metadata.NumItems());
    for (const auto &m : metadata) {
        switch (m.type) {
            case drwav_metadata_type_smpl: {
                DebugWithNewLine("type: smpl");
                std::cout << m.data.smpl;
                break;
            }
            case drwav_metadata_type_inst: {
                DebugWithNewLine("type: inst");
                std::cout << m.data.inst;
                break;
            }
            case drwav_metadata_type_cue: {
                DebugWithNewLine("type: cue");
                std::cout << m.data.cue;
                break;
            }
            case drwav_metadata_type_acid: {
                DebugWithNewLine("type: acid");
                std::cout << m.data.acid;
                break;
            }
            case drwav_metadata_type_bext: {
                DebugWithNewLine("type: bext");
                std::cout << m.data.bext;
                break;
            }
            case drwav_metadata_type_list_label: {
                DebugWithNewLine("type: labelOrNote");
                std::cout << m.data.labelOrNote;
                break;
            }
            case drwav_metadata_type_list_note: {
                DebugWithNewLine("type: list_note");
                std::cout << m.data.labelOrNote;
                break;
            }
            case drwav_metadata_type_list_labelled_cue_region: {
                DebugWithNewLine("type: list_labelled_cue_region");
                std::cout << m.data.labelledCueRegion;
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
                std::cout << m.data.infoText;
                break;
            }
            case drwav_metadata_type_unknown: {
                DebugWithNewLine("type: unknown");
                std::cout << m.data.unknown;
                break;
            }
        }
    }
}

std::optional<AudioData> ReadAudioFile(const fs::path &path) {
    MessageWithNewLine("Signet", path, "Reading file");
    const auto file = OpenFile(path, "rb");
    if (!file) return {};

    AudioData result {};
    const auto ext = path.extension();
    if (ext == ".wav") {
        std::vector<float> f32_buf {};
        drwav wav;

        if (!drwav_init_with_metadata(&wav, OnReadFile, OnSeekFile, file.get(), 0, nullptr)) {
            WarningWithNewLine("Wav", path, "could not init the WAV file");
            return {};
        }
        result.num_channels = wav.channels;
        result.sample_rate = wav.sampleRate;
        result.bits_per_sample = wav.bitsPerSample;
        result.interleaved_samples.resize(wav.totalPCMFrameCount * wav.channels);

        if (wav.metadataCount) {
            const auto num_metadata = wav.metadataCount; // drwav_take_ownership_of_metadata clears it
            result.wave_metadata.Assign(drwav_take_ownership_of_metadata(&wav), num_metadata);
            // DebugPrintAllMetadata(result.wave_metadata);
            WaveMetadataToNonSpecificMetadata converter(result.wave_metadata, result);
            result.metadata = converter.Convert();
        }

        f32_buf.resize(result.interleaved_samples.size());
        const auto frames_read = drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, f32_buf.data());
        if (frames_read != wav.totalPCMFrameCount) {
            WarningWithNewLine("Wav", path, "failed to get all the frames from file");
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
            WarningWithNewLine("Wav", path, "failed to decode flac file");
            return {};
        }
        result.format = AudioFileFormat::Flac;
    } else {
        WarningWithNewLine("Wav", path, "file is not a WAV or a FLAC");
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
        WarningWithNewLine(
            "Signet", {},
            "this audio file contained samples outside of the valid range, to avoid distortion, the whole file was scaled down in volume");
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
        WarningWithNewLine(
            "Signet", {},
            "this audio file contained samples outside of the valid range, to avoid distortion, the whole file was scaled down in volume");
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
                                                     unsigned bits_per_sample,
                                                     std::function<void(const void *)> callback) {
    switch (bits_per_sample) {
        case 8: {
            // 8-bit is the exception in that it uses unsigned ints
            const auto buf = CreateUnsignedIntSamplesFromFloat<u8>(f64_buf, 8);
            callback(buf.data());
            break;
        }
        case 16: {
            const auto buf = CreateSignedIntSamplesFromFloat<s16>(f64_buf, 16);
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
            callback(buf.data());
            break;
        }
        case 32: {
            std::vector<float> buf;
            buf.reserve(f64_buf.size());
            for (const auto s : f64_buf) {
                buf.push_back(static_cast<float>(s));
            }
            callback(buf.data());
            break;
        }
        case 64: {
            callback(f64_buf.data());
            break;
        }
        default: {
            REQUIRE(0);
            return;
        }
    }
}

class NonSpecificMetadataToWaveMetadata {
  public:
    NonSpecificMetadataToWaveMetadata(const AudioData &audio_data, const unsigned bits_per_sample)
        : m_audio_data(audio_data), m_bits_per_sample(bits_per_sample) {}

    const std::vector<drwav_metadata> &BuildMetadata() {
        CopyInfoTextsFromOriginalWaveData();

        bool root_note_written = false;
        if (m_audio_data.metadata.timing_info) {
            AddAcidMetadata(*m_audio_data.metadata.timing_info, m_audio_data.wave_metadata.GetAcid());
            root_note_written = true;
        }
        if (m_audio_data.metadata.midi_mapping && m_audio_data.metadata.midi_mapping->sampler_mapping) {
            AddInstMetadata(*m_audio_data.metadata.midi_mapping);
            root_note_written = true;
        }
        if (m_audio_data.metadata.loops.size()) {
            AddSmplMetadata(m_audio_data.metadata.loops, m_audio_data.wave_metadata.GetSmpl());
            root_note_written = true;
        }
        if (m_audio_data.metadata.regions.size()) {
            AddLabelledCueRegionMetadatas(m_audio_data.metadata.regions);
        }
        if (!root_note_written && m_audio_data.metadata.midi_mapping) {
            AddSmplMetadata(m_audio_data.metadata.loops, {});
        }

        // must be the last as other chunks might have added cues to it
        AddCueMetadata(m_audio_data.metadata.markers);

        return m_wave_metadata;
    }

  private:
    char *AllocateAndCopyString(const char *data, size_t size) {
        if (!size) return nullptr;
        std::shared_ptr<std::string> mem(new std::string(data, size));
        m_allocations.push_back(mem);
        return mem.get()->data();
    };

    template <typename Type>
    Type *AllocateObjects(size_t num_objects) {
        if (!num_objects) return nullptr;
        std::shared_ptr<std::vector<Type>> mem(new std::vector<Type>(num_objects));
        m_allocations.push_back(mem);
        return mem.get()->data();
    }

    u32 FramePosToSampleBytes(size_t frame_pos) {
        return (u32)(frame_pos * m_audio_data.num_channels * (m_bits_per_sample / 8));
    }

    u32 CreateUniqueCuePointId(std::optional<std::string> name, size_t start_frame) {
        const auto id = (u32)m_cue_points_buffer.size();
        m_cue_points_buffer.push_back({id, name, start_frame});
        return id;
    }

    void CopyInfoTextsFromOriginalWaveData() {
        for (const auto &m : m_audio_data.wave_metadata.Items()) {
            if (m.type & drwav_metadata_type_list_all_info_strings && m.data.infoText.stringLength) {
                drwav_metadata item {};
                item.type = m.type;
                item.data.infoText.stringLength = m.data.infoText.stringLength;
                if (m.data.infoText.stringLength) {
                    item.data.infoText.pString =
                        AllocateAndCopyString(m.data.infoText.pString, m.data.infoText.stringLength);
                }
                m_wave_metadata.push_back(item);
            }
        }
    }

    void AddCueMetadata(const std::vector<MetadataItems::Marker> &markers) {
        for (const auto &m : markers) {
            CreateUniqueCuePointId(m.name, m.start_frame);
        }

        if (m_cue_points_buffer.size()) {
            drwav_metadata item {};
            item.type = drwav_metadata_type_cue;

            item.data.cue.cuePointCount = (u32)m_cue_points_buffer.size();
            item.data.cue.pCuePoints = AllocateObjects<drwav_cue_point>(m_cue_points_buffer.size());

            size_t index = 0;
            for (const auto &cue : m_cue_points_buffer) {
                drwav_cue_point p {};
                p.id = cue.id;
                p.dataChunkId[0] = 'd';
                p.dataChunkId[1] = 'a';
                p.dataChunkId[2] = 't';
                p.dataChunkId[3] = 'a';
                p.sampleByteOffset = FramePosToSampleBytes(cue.frame_position);
                item.data.cue.pCuePoints[index++] = p;
            }

            m_wave_metadata.push_back(item);
        }

        for (const auto &c : m_cue_points_buffer) {
            if (c.name) {
                drwav_metadata item {};
                item.type = drwav_metadata_type_list_label;
                item.data.labelOrNote.cuePointId = c.id;
                item.data.labelOrNote.pString = AllocateObjects<char>(c.name->size());
                memcpy(item.data.labelOrNote.pString, c.name->data(), c.name->size());
                item.data.labelOrNote.stringLength = (u32)c.name->size();

                m_wave_metadata.push_back(item);
            }
        }
    }

    void AddAcidMetadata(const MetadataItems::TimingInfo &timing_info,
                         const std::optional<drwav_acid> current_acid) {
        drwav_metadata item {};
        item.type = drwav_metadata_type_acid;
        if (current_acid) item.data.acid = *current_acid;

        if (m_audio_data.metadata.midi_mapping) {
            item.data.acid.flags |= drwav_acid_flag_root_note_set;
            item.data.acid.midiUnityNote =
                (u16)std::clamp(m_audio_data.metadata.midi_mapping->root_midi_note, 0, 127);
        }

        item.data.acid.flags &= ~drwav_acid_flag_one_shot;
        if (timing_info.playback_type == MetadataItems::PlaybackType::OneShot) {
            item.data.acid.flags |= drwav_acid_flag_one_shot;
        }

        item.data.acid.numBeats = timing_info.num_beats;
        item.data.acid.meterDenominator = (u16)timing_info.time_signature_denominator;
        item.data.acid.meterNumerator = (u16)timing_info.time_signature_numerator;
        item.data.acid.tempo = timing_info.tempo;

        m_wave_metadata.push_back(item);
    }

    void AddInstMetadata(const MetadataItems::MidiMapping &midi_mapping) {
        assert(midi_mapping.sampler_mapping);
        drwav_metadata item {};
        item.type = drwav_metadata_type_inst;

        // TODO: notify the user about values that have clamped
        item.data.inst.midiUnityNote = (s8)std::clamp(midi_mapping.root_midi_note, 0, 127);
        item.data.inst.fineTuneCents = (s8)std::clamp(midi_mapping.sampler_mapping->fine_tune_cents, -50, 50);
        item.data.inst.gainDecibels = (s8)std::clamp(midi_mapping.sampler_mapping->gain_db, -64, 64);
        item.data.inst.lowNote = (s8)std::clamp(midi_mapping.sampler_mapping->low_note, 0, 127);
        item.data.inst.highNote = (s8)std::clamp(midi_mapping.sampler_mapping->high_note, 0, 127);
        item.data.inst.lowVelocity = (s8)std::clamp(midi_mapping.sampler_mapping->low_velocity, 1, 127);
        item.data.inst.highVelocity = (s8)std::clamp(midi_mapping.sampler_mapping->high_velocity, 1, 127);

        m_wave_metadata.push_back(item);
    }

    void AddSmplMetadata(const std::vector<MetadataItems::Loop> &loops,
                         const std::optional<drwav_smpl> current_smpl) {
        if (current_smpl) assert(m_audio_data.metadata.midi_mapping);

        drwav_metadata item {};
        item.type = drwav_metadata_type_smpl;
        if (current_smpl) item.data.smpl = *current_smpl;

        item.data.smpl.samplePeriodNanoseconds = u32((1.0 / (double)m_audio_data.sample_rate) * 1000000000.0);
        if (m_audio_data.metadata.midi_mapping) {
            item.data.smpl.midiUnityNote = m_audio_data.metadata.midi_mapping->root_midi_note;
        } else {
            item.data.smpl.midiUnityNote = 60;
        }

        // if it exists, this data is opaque to us, so it's safer to just clear it
        item.data.smpl.samplerSpecificDataSizeInBytes = 0;
        item.data.smpl.pSamplerSpecificData = nullptr;

        item.data.smpl.sampleLoopCount = (u32)loops.size();
        if (item.data.smpl.sampleLoopCount) {
            item.data.smpl.pLoops = AllocateObjects<drwav_smpl_loop>(loops.size());
            size_t smpl_loops_index = 0;
            for (const auto &loop : loops) {
                drwav_smpl_loop smpl_loop {};

                smpl_loop.cuePointId = CreateUniqueCuePointId(loop.name, loop.start_frame);

                switch (loop.type) {
                    case MetadataItems::LoopType::Forward:
                        smpl_loop.type = drwav_smpl_loop_type_forward;
                        break;
                    case MetadataItems::LoopType::Backward:
                        smpl_loop.type = drwav_smpl_loop_type_backward;
                        break;
                    case MetadataItems::LoopType::PingPong:
                        smpl_loop.type = drwav_smpl_loop_type_pingpong;
                        break;
                    default: smpl_loop.type = drwav_smpl_loop_type_forward;
                }
                smpl_loop.firstSampleByteOffset = FramePosToSampleBytes(loop.start_frame);
                smpl_loop.lastSampleByteOffset =
                    FramePosToSampleBytes(loop.start_frame + loop.num_frames - 1);
                smpl_loop.sampleFraction =
                    0; // Note: we are discarding this value even if we have unchaged the loop
                smpl_loop.playCount = loop.num_times_to_loop;

                item.data.smpl.pLoops[smpl_loops_index++] = smpl_loop;
            }
        }

        m_wave_metadata.push_back(item);
    }

    void AddLabelledCueRegionMetadatas(const std::vector<MetadataItems::Region> &regions) {
        for (const auto &r : regions) {
            // Note: we are discarding of the purposeId, and the language items even if we haven't changed the
            // loop
            drwav_metadata item {};
            item.type = drwav_metadata_type_list_labelled_cue_region;

            item.data.labelledCueRegion.cuePointId =
                CreateUniqueCuePointId(r.initial_marker_name, r.start_frame);
            item.data.labelledCueRegion.sampleLength = (u32)(r.num_frames * m_audio_data.num_channels);
            item.data.labelledCueRegion.purposeId[0] = 'b';
            item.data.labelledCueRegion.purposeId[1] = 'e';
            item.data.labelledCueRegion.purposeId[2] = 'a';
            item.data.labelledCueRegion.purposeId[3] = 't';

            if (r.name) {
                item.data.labelledCueRegion.stringLength = (u32)(r.name->size());
                item.data.labelledCueRegion.pString = AllocateAndCopyString(r.name->data(), r.name->size());
            }

            m_wave_metadata.push_back(item);
        }
    }

    const AudioData &m_audio_data;
    unsigned m_bits_per_sample;

    struct CuePoint {
        u32 id;
        std::optional<std::string> name;
        size_t frame_position;
    };
    std::vector<CuePoint> m_cue_points_buffer {};

    std::vector<std::shared_ptr<void>> m_allocations {};
    std::vector<drwav_metadata> m_wave_metadata {};
};

static bool WriteWaveFile(const fs::path &path, const AudioData &audio_data, const unsigned bits_per_sample) {
    if (std::find(std::begin(valid_wave_bit_depths), std::end(valid_wave_bit_depths), bits_per_sample) ==
        std::end(valid_wave_bit_depths)) {
        WarningWithNewLine("Wav", path, "could not write wave file - {} is not a valid bit depth",
                           bits_per_sample);
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

    // NonSpecificMetadataToWaveMetadata must exist for the lifetime of drwav as drwav keeps a pointer to the
    // metadata
    NonSpecificMetadataToWaveMetadata wave_file_metadata(audio_data, bits_per_sample);
    const auto metadata = wave_file_metadata.BuildMetadata();

    drwav wav;
    drwav_init_write_with_metadata(&wav, &format, OnWrite, OnSeekFile, file.get(), nullptr,
                                   metadata.size() ? (drwav_metadata *)metadata.data() : NULL,
                                   (u32)metadata.size());

    bool succeed_writing = true;
    GetAudioDataConvertedAndScaledToBitDepth(
        audio_data.interleaved_samples, bits_per_sample, [&](const void *raw_data) {
            if (!succeed_writing) return;
            const auto frames_written = drwav_write_pcm_frames(&wav, audio_data.NumFrames(), raw_data);
            if (frames_written != audio_data.NumFrames()) {
                ErrorWithNewLine(
                    "Wav", path,
                    "failed to write the correct number of frames, {} were written, {} we requested",
                    frames_written, audio_data.NumFrames());
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
            ErrorWithNewLine("Flac", {}, "FLAC__STREAM_ENCODER_INIT_STATUS_ENCODER_ERROR");
            return;
            /**< General failure to set up encoder; call FLAC__stream_encoder_get_state() for cause. */

        case FLAC__STREAM_ENCODER_INIT_STATUS_UNSUPPORTED_CONTAINER:
            ErrorWithNewLine("Flac", {}, "FLAC__STREAM_ENCODER_INIT_STATUS_UNSUPPORTED_CONTAINER");
            return;
            /**< The library was not compiled with support for the given container
             * format.
             */

        case FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_CALLBACKS:
            ErrorWithNewLine("Flac", {}, "FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_CALLBACKS");
            return;
            /**< A required callback was not supplied. */

        case FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_NUMBER_OF_CHANNELS:
            ErrorWithNewLine("Flac", {}, "FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_NUMBER_OF_CHANNELS");
            return;
            /**< The encoder has an invalid setting for number of channels. */

        case FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_BITS_PER_SAMPLE:
            ErrorWithNewLine("Flac", {}, "FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_BITS_PER_SAMPLE");
            return;
            /**< The encoder has an invalid setting for bits-per-sample.
             * FLAC supports 4-32 bps but the reference encoder currently supports
             * only up to 24 bps.
             */

        case FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_SAMPLE_RATE:
            ErrorWithNewLine("Flac", {}, "FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_SAMPLE_RATE");
            return;
            /**< The encoder has an invalid setting for the input sample rate. */

        case FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_BLOCK_SIZE:
            ErrorWithNewLine("Flac", {}, "FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_BLOCK_SIZE");
            return;
            /**< The encoder has an invalid setting for the block size. */

        case FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_MAX_LPC_ORDER:
            ErrorWithNewLine("Flac", {}, "FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_MAX_LPC_ORDER");
            return;
            /**< The encoder has an invalid setting for the maximum LPC order. */

        case FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_QLP_COEFF_PRECISION:
            ErrorWithNewLine("Flac", {}, "FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_QLP_COEFF_PRECISION");
            return;
            /**< The encoder has an invalid setting for the precision of the quantized linear predictor
             * coefficients. */

        case FLAC__STREAM_ENCODER_INIT_STATUS_BLOCK_SIZE_TOO_SMALL_FOR_LPC_ORDER:
            ErrorWithNewLine("Flac", {},
                             "FLAC__STREAM_ENCODER_INIT_STATUS_BLOCK_SIZE_TOO_SMALL_FOR_LPC_ORDER");
            return;
            /**< The specified block size is less than the maximum LPC order. */

        case FLAC__STREAM_ENCODER_INIT_STATUS_NOT_STREAMABLE:
            ErrorWithNewLine("Flac", {}, "FLAC__STREAM_ENCODER_INIT_STATUS_NOT_STREAMABLE");
            return;
            /**< The encoder is bound to the <A HREF="../format.html#subset">Subset</A> but other settings
             * violate it. */

        case FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_METADATA:
            ErrorWithNewLine("Flac", {}, "FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_METADATA");
            return;
            /**< The metadata input to the encoder is invalid, in one of the following ways:
             * - FLAC__stream_encoder_set_metadata() was called with a null pointer but a block count > 0
             * - One of the metadata blocks contains an undefined type
             * - It contains an illegal CUESHEET as checked by FLAC__format_cuesheet_is_legal()
             * - It contains an illegal SEEKTABLE as checked by FLAC__format_seektable_is_legal()
             * - It contains more than one SEEKTABLE block or more than one VORBIS_COMMENT block
             */

        case FLAC__STREAM_ENCODER_INIT_STATUS_ALREADY_INITIALIZED:
            ErrorWithNewLine("Flac", {}, "FLAC__STREAM_ENCODER_INIT_STATUS_ALREADY_INITIALIZED");
            return;
            /**< FLAC__stream_encoder_init_*() was called when the encoder was
             * already initialized, usually because
             * FLAC__stream_encoder_finish() was not called.*/
    }
}

static void SafeMetadataDelete(FLAC__StreamMetadata *obj) {
    if (obj) FLAC__metadata_object_delete(obj);
}

static bool
WriteFlacFile(const fs::path &filename, const AudioData &audio_data, const unsigned bits_per_sample) {
    if (std::find(std::begin(valid_flac_bit_depths), std::end(valid_flac_bit_depths), bits_per_sample) ==
        std::end(valid_flac_bit_depths)) {
        WarningWithNewLine("Flac", filename, "could not write flac file - {} is not a valid bit depth",
                           bits_per_sample);
        return false;
    }

    std::unique_ptr<FLAC__StreamEncoder, decltype(&FLAC__stream_encoder_delete)> encoder {
        FLAC__stream_encoder_new(), &FLAC__stream_encoder_delete};
    if (!encoder) {
        WarningWithNewLine("Flac", filename, "could not write flac file - no memory");
        return false;
    }

    FLAC__stream_encoder_set_channels(encoder.get(), audio_data.num_channels);
    FLAC__stream_encoder_set_bits_per_sample(encoder.get(), bits_per_sample);
    FLAC__stream_encoder_set_sample_rate(encoder.get(), audio_data.sample_rate);
    FLAC__stream_encoder_set_total_samples_estimate(encoder.get(), audio_data.interleaved_samples.size());

    std::vector<FLAC__StreamMetadata *> metadata;
    for (auto m : audio_data.flac_metadata) {
        metadata.push_back(m.get());
    }

    // Add in our metadata to a custom FLAC block
    std::unique_ptr<FLAC__StreamMetadata, decltype(&SafeMetadataDelete)> signet_metadata {
        nullptr, &SafeMetadataDelete};
    {
        std::stringstream ss;
        try {
            cereal::JSONOutputArchive archive(ss);
            archive(cereal::make_nvp(signet_root_json_object_name, audio_data.metadata));
        } catch (const std::exception &e) {
            ErrorWithNewLine("Flac", {}, "Internal error when writing FLAC signet json metadata: {}",
                             e.what());
        }
        const auto str = ss.str();
        if (str.size()) {
            signet_metadata.reset(FLAC__metadata_object_new(FLAC__METADATA_TYPE_APPLICATION));
            memcpy(signet_metadata->data.application.id, flac_custom_signet_application_id, 4);
            FLAC__metadata_object_application_set_data(signet_metadata.get(), (FLAC__byte *)str.data(),
                                                       (unsigned)str.size(), true);
            metadata.push_back(signet_metadata.get());
        }
    }

    if (metadata.size()) {
        const bool set_metadata =
            FLAC__stream_encoder_set_metadata(encoder.get(), metadata.data(), (unsigned)metadata.size());
        assert(set_metadata);
    }

    auto f = OpenFileRaw(filename, "w+b");
    if (!f) {
        WarningWithNewLine("Flac", filename, "could not write flac file - could not open file {}");
        return false;
    }

    if (const auto o = FLAC__stream_encoder_init_FILE(encoder.get(), f, nullptr, nullptr);
        o != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
        WarningWithNewLine("Flac", filename, "could not write flac file");
        PrintFlacStatusCode(o);
        return false;
    }

    const auto int32_buffer =
        CreateSignedIntSamplesFromFloat<s32>(audio_data.interleaved_samples, bits_per_sample);
    if (!FLAC__stream_encoder_process_interleaved(encoder.get(), int32_buffer.data(),
                                                  (unsigned)audio_data.NumFrames())) {
        WarningWithNewLine("Flac", filename, "could not write flac file - failed encoding samples");
        return false;
    }

    if (!FLAC__stream_encoder_finish(encoder.get())) {
        WarningWithNewLine("Flac", filename, "could not write flac file - error finishing encoding");
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

    return result;
}

struct BufferConversionTest {
    template <typename T>
    static void
    Check(const std::vector<double> &buf, const unsigned bits_per_sample, const std::vector<T> expected) {
        GetAudioDataConvertedAndScaledToBitDepth(buf, bits_per_sample, [&](const void *raw_data) {
            const auto data = (const T *)raw_data;
            for (usize i = 0; i < expected.size(); ++i) {
                REQUIRE(data[i] == expected[i]);
            }
        });
    }
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

            SUBCASE("to unsigned 8-bit data") { BufferConversionTest::Check<u8>(buf, 8, {0, 127, 255, 127}); }

            SUBCASE("to signed 16-bit data") {
                BufferConversionTest::Check<s16>(buf, 16, {INT16_MIN, 0, INT16_MAX, 0});
            }

            SUBCASE("to signed 24-bit data") {
                std::vector<u8> expected_bytes;
                for (const auto s : buf) {
                    for (const auto byte : Convert24BitIntToBytes(ScaleSampleToSignedInt<s32>(s, 24))) {
                        expected_bytes.push_back(byte);
                    }
                }
                BufferConversionTest::Check<u8>(buf, 24, expected_bytes);
            }

            SUBCASE("to 32-bit float data") {
                BufferConversionTest::Check<float>(buf, 32, {-1.0f, 0.0f, 1.0f, 0.0f});
            }

            SUBCASE("to 64-bit float data") {
                BufferConversionTest::Check<double>(buf, 64, {-1.0, 0.0, 1.0, 0.0});
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

    SUBCASE("metadata") {
        SUBCASE("read/write metadata") {
            auto CheckAllMetadataReadingAndWriting = [](const fs::path &out_filename) {
                auto buf = TestHelpers::CreateSineWaveAtFrequency(2, 44100, 0.01, 440);

                SUBCASE("root_midi_note") {
                    buf.metadata.midi_mapping.emplace();
                    buf.metadata.midi_mapping->root_midi_note = 99;
                    REQUIRE(WriteAudioFile(out_filename, buf, 16));

                    auto read_file = ReadAudioFile(out_filename);
                    REQUIRE(read_file);
                    REQUIRE(read_file->metadata.midi_mapping);
                    REQUIRE(read_file->metadata.midi_mapping->root_midi_note == 99);
                }

                SUBCASE("sampler_mapping") {
                    const MetadataItems::SamplerMapping sampler_mapping = {1, 2, 3, 4, 5, 6};

                    buf.metadata.midi_mapping.emplace();
                    buf.metadata.midi_mapping->sampler_mapping = sampler_mapping;
                    REQUIRE(WriteAudioFile(out_filename, buf, 16));

                    auto read_file = ReadAudioFile(out_filename);
                    REQUIRE(read_file);
                    REQUIRE(read_file->metadata.midi_mapping);
                    REQUIRE(read_file->metadata.midi_mapping->sampler_mapping == sampler_mapping);
                }

                SUBCASE("timing info") {
                    const MetadataItems::TimingInfo timing_info = {MetadataItems::PlaybackType::Loop, 2, 3, 4,
                                                                   87.0f};

                    buf.metadata.timing_info = timing_info;
                    REQUIRE(WriteAudioFile(out_filename, buf, 16));

                    auto read_file = ReadAudioFile(out_filename);
                    REQUIRE(read_file);
                    REQUIRE(read_file->metadata.timing_info == timing_info);
                }

                SUBCASE("loops") {
                    SUBCASE("single loop") {
                        const MetadataItems::Loop loop {"loop name", MetadataItems::LoopType::Backward, 2, 5,
                                                        80};

                        buf.metadata.loops = {loop};
                        REQUIRE(WriteAudioFile(out_filename, buf, 16));

                        auto read_file = ReadAudioFile(out_filename);
                        REQUIRE(read_file);
                        REQUIRE(read_file->metadata.loops.size() == 1);
                        REQUIRE(read_file->metadata.loops[0] == loop);
                    }

                    SUBCASE("multiple loops") {
                        const MetadataItems::Loop loop1 {"loop name", MetadataItems::LoopType::Backward, 2, 5,
                                                         80};
                        const MetadataItems::Loop loop2 {"loop name2", MetadataItems::LoopType::PingPong, 3,
                                                         8, 2};

                        buf.metadata.loops = {loop1, loop2};
                        REQUIRE(WriteAudioFile(out_filename, buf, 16));

                        auto read_file = ReadAudioFile(out_filename);
                        REQUIRE(read_file);
                        REQUIRE(read_file->metadata.loops.size() == 2);
                        REQUIRE(read_file->metadata.loops[0] == loop1);
                        REQUIRE(read_file->metadata.loops[1] == loop2);
                    }
                }

                SUBCASE("markers") {
                    const std::vector<MetadataItems::Marker> markers {
                        {"m1", 1},
                        {"m2", 3},
                        {"m3", 5},
                    };
                    buf.metadata.markers = markers;
                    REQUIRE(WriteAudioFile(out_filename, buf, 16));

                    auto read_file = ReadAudioFile(out_filename);
                    REQUIRE(read_file);
                    REQUIRE(read_file->metadata.markers.size() == markers.size());
                    for (usize i = 0; i < markers.size(); ++i) {
                        REQUIRE(read_file->metadata.markers[i] == markers[i]);
                    }
                }

                SUBCASE("regions") {
                    const std::vector<MetadataItems::Region> regions {
                        {"marker name", "name", 1, 2},
                        {"marker name2", "name2", 2, 3},
                    };
                    buf.metadata.regions = regions;
                    REQUIRE(WriteAudioFile(out_filename, buf, 16));

                    auto read_file = ReadAudioFile(out_filename);
                    REQUIRE(read_file);
                    REQUIRE(read_file->metadata.regions.size() == regions.size());
                    for (usize i = 0; i < regions.size(); ++i) {
                        REQUIRE(read_file->metadata.regions[i] == regions[i]);
                    }
                }
            };
            SUBCASE("wav") { CheckAllMetadataReadingAndWriting("metadata_test.wav"); }
            SUBCASE("flac") { CheckAllMetadataReadingAndWriting("metadata_test.flac"); }
        }

        SUBCASE("wave with marker and loops") {
            const fs::path in_filename = TEST_DATA_DIRECTORY "/wave_with_markers_and_loop.wav";
            const fs::path out_filename = "out_wave_with_markers_and_loop.wav";
            const fs::path out_flac_filename = "out_flac_with_markers_and_loop.flac";
            auto f = ReadAudioFile(in_filename);
            REQUIRE(f);
            REQUIRE(f->wave_metadata.NumItems() >= 2);

            REQUIRE(WriteAudioFile(out_filename, f.value(), 16));
            REQUIRE(fs::is_regular_file(out_filename));

            auto out_f = ReadAudioFile(out_filename);
            REQUIRE(out_f);
            REQUIRE(out_f->wave_metadata.NumItems() >= 2);

            REQUIRE(WriteAudioFile(out_flac_filename, f.value(), 16));
            REQUIRE(fs::is_regular_file(out_flac_filename));

            auto out_f2 = ReadAudioFile(out_flac_filename);
            REQUIRE(out_f2);
            REQUIRE(out_f2->metadata.loops.size() != 0);
        }

        SUBCASE("wave with bext") {
            const fs::path in_filename = TEST_DATA_DIRECTORY "/wav_with_bext.wav";
            auto f = ReadAudioFile(in_filename);
            REQUIRE(f);
            REQUIRE(f->wave_metadata.NumItems() >= 1);
        }

        SUBCASE("wave with bpm and time sig") {
            const fs::path in_filename = TEST_DATA_DIRECTORY "/wav_metadata_80bpm_5-4_time_sig.wav";
            auto f = ReadAudioFile(in_filename);
            REQUIRE(f);
            REQUIRE(f->wave_metadata.NumItems() >= 1);
        }

        SUBCASE("wave with region and marker") {
            const fs::path in_filename = TEST_DATA_DIRECTORY "/wav_with_region_and_marker.wav";
            auto f = ReadAudioFile(in_filename);
            REQUIRE(f);
            REQUIRE(f->wave_metadata.NumItems() >= 1);
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
}
