#include "audio_file.h"

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

static size_t OnWrite(void *pUserData, const void *pData, size_t bytesToWrite) {
    return fwrite(pData, 1, bytesToWrite, (FILE *)pUserData);
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
    os << "  manufacturer: " << s.manufacturerId << "\n";
    os << "  product: " << s.productId << "\n";
    os << "  samplePeriodNanoseconds: " << s.samplePeriodNanoseconds << "\n";
    os << "  midiUnityNote: " << s.midiUnityNote << "\n";
    os << "  midiPitchFraction: " << s.midiPitchFraction << "\n";
    os << "  smpteFormat: " << s.smpteFormat << "\n";
    os << "  smpteOffset: " << s.smpteOffset << "\n";
    os << "  numSampleLoops: " << s.numSampleLoops << "\n";
    os << "  numBytesOfSamplerSpecificData: " << s.numBytesOfSamplerSpecificData << "\n";

    os << "  loops: [\n";
    for (u32 i = 0; i < s.numSampleLoops; ++i) {
        auto &smpl_loop = s.loops[i];

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
    os << "  numCuePoints: " << c.numCuePoints << "\n";

    os << "  cuePoints: [\n";
    for (u32 i = 0; i < c.numCuePoints; ++i) {
        auto &cue = c.cuePoints[i];
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
    os << "  stringSize: " << l.stringSize << "\n";
    os << "  string: " << l.string << "\n";
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
                    for (u32 i = 0; i < m.smpl.numSampleLoops; ++i) {
                        loops.push_back(CreateSampleLoop(m.smpl.loops[i]));
                    }
                    result.loops = loops;
                    break;
                }
                case drwav_metadata_type_inst: {
                    MetadataItems::SamplerMapping data;
                    data.fine_tune_cents = m.inst.fineTuneCents;
                    data.gain_db = m.inst.gainDecibels;
                    data.low_note = m.inst.lowNote;
                    data.high_note = m.inst.highNote;
                    data.low_velocity = m.inst.lowVelocity;
                    data.high_velocity = m.inst.highVelocity;
                    assert(result.midi_mapping); // we should have created it for the midi root note after
                                                 // calling FindMidiRootNote
                    result.midi_mapping->sampler_mapping = data;
                    break;
                }
                case drwav_metadata_type_cue: {
                    std::vector<MetadataItems::Marker> markers;
                    for (u32 i = 0; i < m.cue.numCuePoints; ++i) {
                        markers.push_back(CreateMarker(m.cue.cuePoints[i]));
                    }
                    result.markers = markers;
                    break;
                }
                case drwav_metadata_type_acid: {
                    MetadataItems::TimingInfo info;
                    info.num_beats = m.acid.numBeats;
                    info.time_signature_denominator = m.acid.meterDenominator;
                    info.time_signature_numerator = m.acid.meterNumerator;
                    info.tempo = m.acid.tempo;
                    result.timing_info = info;
                    break;
                }
                case drwav_metadata_type_list_labelled_cue_region: {
                    result.regions.push_back(CreateRegion(m.labelledCueRegion));
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
                if (m.labelOrNote.cuePointId == loop.cuePointId && m.labelOrNote.stringSize) {
                    result.name = std::string(m.labelOrNote.string);
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
        if (region.stringSize) result.name = std::string(region.string);

        bool found_cue = false;

        for (const auto &m : m_wave_metadata) {
            if (m.type == drwav_metadata_type_cue) {
                for (u32 i = 0; i < m.cue.numCuePoints; ++i) {
                    const auto &cue_point = m.cue.cuePoints[i];
                    if (cue_point.id == region.cuePointId) {
                        result.start_frame =
                            cue_point.sampleByteOffset / (m_audio.bits_per_sample / 8) / m_audio.num_channels;
                        found_cue = true;
                        break;
                    }
                }
            } else if (m.type == drwav_metadata_type_list_label) {
                if (m.labelOrNote.cuePointId == region.cuePointId && m.labelOrNote.stringSize) {
                    result.initial_marker_name = std::string(m.labelOrNote.string);
                }
            }
        }

        result.num_frames = region.sampleLength / m_audio.num_channels;

        // TODO: handle these cases properly instead of asserting
        assert(found_cue);
        assert(result.start_frame < m_audio.NumFrames());
        assert((result.start_frame + result.num_frames) <= m_audio.NumFrames());

        return result;
    }

    MetadataItems::Marker CreateMarker(const drwav_cue_point &cue_point) const {
        MetadataItems::Marker result {};
        for (const auto &m : m_wave_metadata) {
            if (m.type == drwav_metadata_type_list_label) {
                if (m.labelOrNote.cuePointId == cue_point.id && m.labelOrNote.stringSize) {
                    result.name = std::string(m.labelOrNote.string);
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
                return m.inst.midiUnityNote;
            }
        }

        for (const auto &m : m_wave_metadata) {
            if (m.type == drwav_metadata_type_smpl) {
                return m.smpl.midiUnityNote;
            }
        }

        for (const auto &m : m_wave_metadata) {
            if (m.type == drwav_metadata_type_acid && m.acid.flags & drwav_acid_flag_root_note_set) {
                return m.acid.midiUnityNote;
            }
        }

        return {};
    }

    const WaveMetadata &m_wave_metadata;
    const AudioData &m_audio;
};

void DebugPrintAllMetadata(const WaveMetadata &metadata) {
    DebugWithNewLine("Wav num metadata: ", metadata.NumItems());
    for (const auto &m : metadata) {
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
            case drwav_metadata_type_list_labelled_cue_region: {
                DebugWithNewLine("type: list_labelled_cue_region");
                DebugWithNewLine(m.labelledCueRegion);
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
        result.num_channels = wav.channels;
        result.sample_rate = wav.sampleRate;
        result.bits_per_sample = wav.bitsPerSample;
        result.interleaved_samples.resize(wav.totalPCMFrameCount * wav.channels);

        if (wav.numMetadata) {
            const auto num_metadata = wav.numMetadata; // drwav_take_ownership_of_metadata clears it
            result.wave_metadata.Assign(drwav_take_ownership_of_metadata(&wav), num_metadata);
            DebugPrintAllMetadata(result.wave_metadata);
            WaveMetadataToNonSpecificMetadata converter(result.wave_metadata, result);
            result.metadata = converter.Convert();
        }

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

        if (m_audio_data.metadata.timing_info) {
            AddAcidMetadata(*m_audio_data.metadata.timing_info, m_audio_data.wave_metadata.GetAcid());
        }
        if (m_audio_data.metadata.midi_mapping && m_audio_data.metadata.midi_mapping->sampler_mapping) {
            AddInstMetadata(*m_audio_data.metadata.midi_mapping);
        }
        if (m_audio_data.metadata.loops.size()) {
            AddSmplMetadata(m_audio_data.metadata.loops, m_audio_data.wave_metadata.GetSmpl());
        }
        if (m_audio_data.metadata.regions.size()) {
            AddLabelledCueRegionMetadatas(m_audio_data.metadata.regions);
        }

        // must be the last as other chunks might have added cues to it
        AddCueMetadata(m_audio_data.metadata.markers);

        return m_wave_metadata;
    }

  private:
    char *AllocateAndCopyString(const char *data, size_t size) {
        if (!size) return nullptr;
        auto mem = std::shared_ptr<char[]>(new char[size + 1]);
        memcpy(mem.get(), data, size);
        mem.get()[size] = '\0';
        m_allocations.push_back(mem);
        return mem.get();
    };

    template <typename Type>
    Type *AllocateObjects(size_t num_objects) {
        auto mem = std::shared_ptr<Type[]>(new Type[num_objects]);
        m_allocations.push_back(mem);
        return mem.get();
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
            if (m.type & drwav_metadata_type_list_all_info_strings && m.infoText.stringSize) {
                drwav_metadata item {};
                item.type = m.type;
                item.infoText.stringSize = m.infoText.stringSize;
                if (m.infoText.stringSize) {
                    item.infoText.string = AllocateAndCopyString(m.infoText.string, m.infoText.stringSize);
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

            item.cue.numCuePoints = (u32)m_cue_points_buffer.size();
            item.cue.cuePoints = AllocateObjects<drwav_cue_point>(m_cue_points_buffer.size());

            size_t index = 0;
            for (const auto &cue : m_cue_points_buffer) {
                drwav_cue_point p {};
                p.id = cue.id;
                p.dataChunkId[0] = 'd';
                p.dataChunkId[1] = 'a';
                p.dataChunkId[2] = 't';
                p.dataChunkId[3] = 'a';
                p.sampleByteOffset = FramePosToSampleBytes(cue.frame_position);
                item.cue.cuePoints[index++] = p;
            }

            m_wave_metadata.push_back(item);
        }
    }

    void AddAcidMetadata(const MetadataItems::TimingInfo &timing_info,
                         const std::optional<drwav_acid> current_acid) {
        drwav_metadata item {};
        item.type = drwav_metadata_type_acid;
        if (current_acid) item.acid = *current_acid;

        if (m_audio_data.metadata.midi_mapping) {
            item.acid.flags |= drwav_acid_flag_root_note_set;
            item.acid.midiUnityNote =
                (u16)std::clamp(m_audio_data.metadata.midi_mapping->root_midi_note, 0, 127);
        }

        item.acid.flags &= ~drwav_acid_flag_one_shot;
        if (timing_info.playback_type == MetadataItems::PlaybackType::OneShot) {
            item.acid.flags |= drwav_acid_flag_one_shot;
        }

        item.acid.numBeats = timing_info.num_beats;
        item.acid.meterDenominator = (u16)timing_info.time_signature_denominator;
        item.acid.meterNumerator = (u16)timing_info.time_signature_numerator;
        item.acid.tempo = timing_info.tempo;

        m_wave_metadata.push_back(item);
    }

    void AddInstMetadata(const MetadataItems::MidiMapping &midi_mapping) {
        assert(midi_mapping.sampler_mapping);
        drwav_metadata item {};
        item.type = drwav_metadata_type_inst;

        // TODO: notify the user about values that have clamped
        item.inst.midiUnityNote = (s8)std::clamp(midi_mapping.root_midi_note, 0, 127);
        item.inst.fineTuneCents = (s8)std::clamp(midi_mapping.sampler_mapping->fine_tune_cents, -50, 50);
        item.inst.gainDecibels = (s8)std::clamp(midi_mapping.sampler_mapping->gain_db, -64, 64);
        item.inst.lowNote = (s8)std::clamp(midi_mapping.sampler_mapping->low_note, 0, 127);
        item.inst.highNote = (s8)std::clamp(midi_mapping.sampler_mapping->high_note, 0, 127);
        item.inst.lowVelocity = (s8)std::clamp(midi_mapping.sampler_mapping->low_velocity, 1, 127);
        item.inst.highVelocity = (s8)std::clamp(midi_mapping.sampler_mapping->high_velocity, 1, 127);

        m_wave_metadata.push_back(item);
    }

    void AddSmplMetadata(const std::vector<MetadataItems::Loop> &loops,
                         const std::optional<drwav_smpl> current_smpl) {
        if (current_smpl) assert(m_audio_data.metadata.midi_mapping);

        drwav_metadata item {};
        item.type = drwav_metadata_type_smpl;
        if (current_smpl) item.smpl = *current_smpl;

        item.smpl.samplePeriodNanoseconds = u32((1.0 / (double)m_audio_data.sample_rate) * 1000000000.0);
        if (m_audio_data.metadata.midi_mapping)
            item.smpl.midiUnityNote = m_audio_data.metadata.midi_mapping->root_midi_note;
        else
            item.smpl.midiUnityNote = 60;

        // if it exists, this data is opaque to us, so it's safer to just clear it
        item.smpl.numBytesOfSamplerSpecificData = 0;
        item.smpl.samplerSpecificData = nullptr;

        item.smpl.numSampleLoops = (u32)loops.size();
        if (item.smpl.numSampleLoops) {
            item.smpl.loops = AllocateObjects<drwav_smpl_loop>(loops.size());
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

                item.smpl.loops[smpl_loops_index++] = smpl_loop;
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

            item.labelledCueRegion.cuePointId = CreateUniqueCuePointId(r.initial_marker_name, r.start_frame);
            item.labelledCueRegion.sampleLength = (u32)(r.num_frames * m_audio_data.num_channels);
            item.labelledCueRegion.purposeId[0] = 'b';
            item.labelledCueRegion.purposeId[1] = 'e';
            item.labelledCueRegion.purposeId[2] = 'a';
            item.labelledCueRegion.purposeId[3] = 't';

            if (r.name) {
                item.labelledCueRegion.stringSize = (u32)(r.name->size());
                item.labelledCueRegion.string = AllocateAndCopyString(r.name->data(), r.name->size());
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
    std::vector<CuePoint> m_cue_points_buffer;

    std::vector<std::shared_ptr<void>> m_allocations;
    std::vector<drwav_metadata> m_wave_metadata;
};

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

    // Add in our custom metadata to a block we have created with ID SGNT
    FLAC__StreamMetadata signet_metadata {};
    {
        std::stringstream ss;
        {
            // in a separate block becease the archive needs it's destructor called before we can use the
            // string
            cereal::JSONOutputArchive archive(ss);
            archive(cereal::make_nvp(signet_root_json_object_name, audio_data.metadata));
        }

        signet_metadata.type = FLAC__METADATA_TYPE_APPLICATION;
        memcpy(signet_metadata.data.application.id, flac_custom_signet_application_id, 4);
        FLAC__metadata_object_application_set_data(&signet_metadata, (FLAC__byte *)ss.str().data(),
                                                   (unsigned)ss.str().size(), true);
        metadata.push_back(&signet_metadata);
    }

    if (vorbis_comment_meta) {
        metadata.push_back(vorbis_comment_meta.get());
    }

    if (metadata.size()) {
        const bool set_metadata =
            FLAC__stream_encoder_set_metadata(encoder.get(), metadata.data(), (unsigned)metadata.size());
        assert(set_metadata);
    }

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
    static void
    Check(const std::vector<double> &buf, const unsigned bits_per_sample, const std::vector<T> expected) {
        GetAudioDataConvertedAndScaledToBitDepth(buf, bits_per_sample, [&](const void *raw_data) {
            const auto data = (const T *)raw_data;
            for (usize i = 0; i < expected.size(); ++i) {
                REQUIRE(data[i] == expected[i]);
            }
        });
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

    SUBCASE("wave with marker and loops") {
        const fs::path in_filename = TEST_DATA_DIRECTORY "/wave_with_markers_and_loop.wav";
        const fs::path out_filename = "out_wave_with_markers_and_loop.wav";
        const fs::path out_flac_filename = "out_flac_with_markers_and_loop.flac";
        auto f = ReadAudioFile(in_filename);
        REQUIRE(f);
        REQUIRE(f->wave_metadata.NumItems() >= 2);
        MESSAGE(f->wave_metadata.NumItems());

        REQUIRE(WriteAudioFile(out_filename, f.value(), 16));
        REQUIRE(fs::is_regular_file(out_filename));

        auto out_f = ReadAudioFile(out_filename);
        REQUIRE(out_f);
        REQUIRE(out_f->wave_metadata.NumItems() >= 2);
        MESSAGE(out_f->wave_metadata.NumItems());

        REQUIRE(WriteAudioFile(out_flac_filename, f.value(), 16));
        REQUIRE(fs::is_regular_file(out_flac_filename));

        auto out_f2 = ReadAudioFile(out_flac_filename);
        REQUIRE(out_f2);
        REQUIRE(out_f2->metadata.loops.size() != 0);
        MESSAGE(out_f2->metadata.loops.size());
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
