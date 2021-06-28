#include "dr_wav.h"

#include "test_helpers.h"
#include "tests_config.h"
#include "types.h"

bool operator==(const drwav_smpl_loop &a, const drwav_smpl_loop &b) {
    return a.cuePointId == b.cuePointId && a.type == b.type &&
           a.firstSampleByteOffset == b.firstSampleByteOffset &&
           a.lastSampleByteOffset == b.lastSampleByteOffset && a.sampleFraction == b.sampleFraction &&
           a.playCount == b.playCount;
}

bool operator==(const drwav_smpl &a, const drwav_smpl &b) {
    if (!(a.manufacturerId == b.manufacturerId && a.productId == b.productId &&
          a.samplePeriodNanoseconds == b.samplePeriodNanoseconds && a.midiUnityNote == b.midiUnityNote &&
          a.midiPitchFraction == b.midiPitchFraction && a.smpteFormat == b.smpteFormat &&
          a.smpteOffset == b.smpteOffset && a.sampleLoopCount == b.sampleLoopCount &&
          a.samplerSpecificDataSizeInBytes == b.samplerSpecificDataSizeInBytes)) {
        return false;
    }
    if (a.samplerSpecificDataSizeInBytes) {
        if (std::memcmp(a.pSamplerSpecificData, b.pSamplerSpecificData, b.samplerSpecificDataSizeInBytes) !=
            0) {
            return false;
        }
    }
    if (a.sampleLoopCount) {
        for (u32 i = 0; i < a.sampleLoopCount; ++i) {
            if (!(a.pLoops[i] == b.pLoops[i])) return false;
        }
    }

    return true;
}

bool operator==(const drwav_inst &a, const drwav_inst &b) {
    return a.midiUnityNote == b.midiUnityNote && a.fineTuneCents == b.fineTuneCents &&
           a.gainDecibels == b.gainDecibels && a.lowNote == b.lowNote && a.highNote == b.highNote &&
           a.lowVelocity == b.lowVelocity && a.highVelocity == b.highVelocity;
}

bool operator==(const drwav_cue_point &a, const drwav_cue_point &b) {
    if (!(a.id == b.id && a.playOrderPosition == b.playOrderPosition && a.chunkStart == b.chunkStart &&
          a.blockStart == b.blockStart && a.sampleByteOffset == b.sampleByteOffset))
        return false;

    return std::memcmp(a.dataChunkId, b.dataChunkId, sizeof(a.dataChunkId)) == 0;
}

bool operator==(const drwav_cue &a, const drwav_cue &b) {
    if (a.cuePointCount != b.cuePointCount) return false;

    for (u32 i = 0; i < a.cuePointCount; ++i) {
        if (!(a.pCuePoints[i] == b.pCuePoints[i])) return false;
    }
    return true;
}

bool operator==(const drwav_acid &a, const drwav_acid &b) {
    return a.flags == b.flags && a.midiUnityNote == b.midiUnityNote && a.numBeats == b.numBeats &&
           a.meterDenominator == b.meterDenominator && a.meterNumerator == b.meterNumerator &&
           a.tempo == b.tempo;
}

bool operator==(const drwav_bext &a, const drwav_bext &b) {
    if (!strcmp(a.pDescription, b.pDescription)) return false;
    if (!strcmp(a.pOriginatorName, b.pOriginatorName)) return false;
    if (!strcmp(a.pOriginatorReference, b.pOriginatorReference)) return false;
    if (!memcmp(a.pOriginationDate, b.pOriginationDate, sizeof(a.pOriginationDate)) != 0) return false;
    if (!memcmp(a.pOriginationTime, b.pOriginationTime, sizeof(a.pOriginationTime)) != 0) return false;
    if (!(a.codingHistorySize == b.codingHistorySize && a.loudnessValue == b.loudnessValue &&
          a.loudnessRange == b.loudnessRange && a.maxTruePeakLevel == b.maxTruePeakLevel &&
          a.maxMomentaryLoudness == b.maxMomentaryLoudness &&
          a.maxShortTermLoudness == b.maxShortTermLoudness))
        return false;

    if (memcmp(a.pUMID, b.pUMID, 64) != 0) return false;
    if (a.codingHistorySize != 0) {
        if (memcmp(a.pCodingHistory, b.pCodingHistory, a.codingHistorySize) != 0) return false;
    }
    return true;
}

bool operator==(const drwav_list_label_or_note &a, const drwav_list_label_or_note &b) {
    if (a.cuePointId != b.cuePointId || a.stringLength != b.stringLength) {
        return false;
    }
    return memcmp(a.pString, b.pString, a.stringLength) == 0;
}

bool operator==(const drwav_list_labelled_cue_region &a, const drwav_list_labelled_cue_region &b) {
    if (a.cuePointId != b.cuePointId || a.sampleLength != b.sampleLength || a.country != b.country ||
        a.language != b.language || a.dialect != b.dialect || a.codePage != b.codePage ||
        a.stringLength != b.stringLength)
        return false;

    if (memcmp(a.purposeId, b.purposeId, sizeof(a.purposeId)) != 0) return false;
    if (b.stringLength && memcmp(a.pString, b.pString, sizeof(a.stringLength)) != 0) return false;
    return true;
}

bool operator==(const drwav_list_info_text &a, const drwav_list_info_text &b) {
    if (a.stringLength != b.stringLength) return false;
    if (a.stringLength && memcmp(a.pString, b.pString, a.stringLength) != 0) return false;
    return true;
}

bool operator==(const drwav_metadata &a, const drwav_metadata &b) {
    if (a.type != b.type) return false;

    switch (a.type) {
        case drwav_metadata_type_smpl: {
            return a.data.smpl == b.data.smpl;
        }
        case drwav_metadata_type_inst: {
            return a.data.inst == b.data.inst;
        }
        case drwav_metadata_type_cue: {
            return a.data.cue == b.data.cue;
        }
        case drwav_metadata_type_acid: {
            return a.data.acid == b.data.acid;
        }
        case drwav_metadata_type_bext: {
            return a.data.bext == b.data.bext;
        }

        case drwav_metadata_type_list_label: {
            case drwav_metadata_type_list_note: return a.data.labelOrNote == b.data.labelOrNote;
        }

        case drwav_metadata_type_list_labelled_cue_region: {
            return a.data.labelledCueRegion == b.data.labelledCueRegion;
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
            return a.data.infoText == b.data.infoText;
        }
    }
    return false;
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

TEST_CASE("dr_wav: wave with marker and loops") {
    const char *in_filename = TEST_DATA_DIRECTORY "/wave_with_markers_and_loop.wav";
    const char *out_filename = "out_wave_with_markers_and_loop.wav";

    drwav in_wav {};
    REQUIRE(drwav_init_file_with_metadata(&in_wav, in_filename, 0, nullptr));
    REQUIRE(in_wav.metadataCount != 0);

    const auto num_bytes_audio = in_wav.bytesRemaining;
    auto audio_data = std::make_unique<u8[]>((size_t)num_bytes_audio);
    REQUIRE(drwav_read_raw(&in_wav, num_bytes_audio, (void *)audio_data.get()) == num_bytes_audio);

    const auto num_metadata = in_wav.metadataCount;
    auto metadata = drwav_take_ownership_of_metadata(&in_wav);

    {
        auto out_file = OpenFile(out_filename, "wb");
        REQUIRE(out_file);

        drwav_data_format format {};
        format.container = in_wav.container;
        format.bitsPerSample = in_wav.bitsPerSample;
        format.channels = in_wav.channels;
        format.format = in_wav.translatedFormatTag;
        format.sampleRate = in_wav.sampleRate;

        drwav out_wav {};
        REQUIRE(drwav_init_write_with_metadata(&out_wav, &format, OnWrite, OnSeekFile, out_file.get(),
                                               nullptr, metadata, num_metadata));

        REQUIRE(drwav_write_raw(&out_wav, num_bytes_audio, (const void *)audio_data.get()) ==
                num_bytes_audio);

        REQUIRE(drwav_uninit(&out_wav) == DRWAV_SUCCESS);
    }

    REQUIRE(drwav_uninit(&in_wav) == DRWAV_SUCCESS);

    {
        drwav in2_wav {};
        REQUIRE(drwav_init_file_with_metadata(&in2_wav, out_filename, 0, nullptr));

        REQUIRE(num_metadata == in2_wav.metadataCount);
        for (u32 i = 0; i < in2_wav.metadataCount; ++i) {
            const auto &a = in2_wav.pMetadata[i];
            if (a.type == drwav_metadata_type_unknown) continue;

            bool found_metadata_in_gen_file = false;
            for (u32 j = 0; j < num_metadata; ++j) {
                const auto &b = metadata[j];
                if (a == b) {
                    found_metadata_in_gen_file = true;
                    break;
                }
            }
            REQUIRE(found_metadata_in_gen_file);
        }

        REQUIRE(drwav_uninit(&in2_wav) == DRWAV_SUCCESS);
    }

    drwav_free(metadata, nullptr);
}

#if 0
TEST_CASE("drwav: file with every type of metadata") {
    drwav_metadata meta[17] = {};

    // smpl
    meta[0].type = drwav_metadata_type_smpl;
    drwav_smpl_loop loops[1] = {};
    loops[0].cuePointId = 1;
    loops[0].type = drwav_smpl_loop_type_pingpong;
    loops[0].firstSampleByteOffset = 0;
    loops[0].firstSampleByteOffset = 1;
    loops[0].sampleFraction = 0;
    loops[0].playCount = 55;

    meta[0].data.smpl.manufacturerId = 1;
    meta[0].data.smpl.productId = 2;
    meta[0].data.smpl.samplePeriodNanoseconds = 0;
    meta[0].data.smpl.midiUnityNote = 0;
    meta[0].data.smpl.midiPitchFraction = 0;
    meta[0].data.smpl.smpteFormat = 0;
    meta[0].data.smpl.smpteOffset = 0;
    meta[0].data.smpl.sampleLoopCount = 1;
    meta[0].data.smpl.samplerSpecificDataSizeInBytes = 1;

    u8 byte = 0;
    meta[0].data.smpl.pSamplerSpecificData = &byte;
    meta[0].data.smpl.pLoops = loops;

    // inst
    meta[1].type = drwav_metadata_type_inst;
    meta[1].data.inst = {};

    // cue
    drwav_cue_point cue_point {};
    cue_point.id = 1;
    cue_point.playOrderPosition = 0;
    cue_point.dataChunkId[0] = 'd';
    cue_point.dataChunkId[1] = 'a';
    cue_point.dataChunkId[2] = 't';
    cue_point.dataChunkId[3] = 'a';
    cue_point.chunkStart = 0;
    cue_point.blockStart = 0;
    cue_point.sampleByteOffset = 0;

    meta[1].type = drwav_metadata_type_cue;
    meta[1].data.cue.cuePointCount = 1;
    meta[1].data.cue.pCuePoints = &cue_point;

    // acid
    meta[1].type = drwav_metadata_type_acid;
    meta[1].data.acid = {};

    // bext
    meta[1].type = drwav_metadata_type_bext;
    meta[1].data.bext = {};

    meta[1].data.bext.pDescription = "Description";
    meta[1].data.bext.pOriginatorName = "Name";
    meta[1].data.bext.pOriginatorReference = "Ref";
    memcpy(meta[1].data.bext.pOriginationDate, "1066:01:01", 10);
    memcpy(meta[1].data.bext.pOriginationTime, "10:01:01", 8);
    meta[1].data.bext.timeReference = {};

    meta[1].data.bext.pCodingHistory = "Coding history";
    meta[1].data.bext.codingHistorySize = strlen("Coding history");
    u8 umid[64] = {1, 1, 1, 1, 1, 1};
    meta[1].data.bext.pUMID = &umid;
    meta[1].data.bext.loudnessValue = {};
    meta[1].data.bext.loudnessRange = {};
    meta[1].data.bext.maxTruePeakLevel = {};
    meta[1].data.bext.maxMomentaryLoudness = {};
    meta[1].data.bext.maxShortTermLoudness = {};

    // list label
    meta[1].type = drwav_metadata_type_list_label;


}
#endif