#pragma once

#include <cstdio>
#include <memory>

#include "FLAC/stream_decoder.h"
#include "audio_data.h"
#include "common.h"

struct FlacFileDataContext {
    FlacFileDataContext(FILE *f, AudioData &a) : file(f), data(a) {}
    FILE *file;
    AudioData &data;
};

FLAC__StreamDecoderReadStatus FlacDecodeReadCallback(const FLAC__StreamDecoder *decoder,
                                                     FLAC__byte buffer[],
                                                     size_t *bytes,
                                                     void *client_data) {
    auto &context = *((FlacFileDataContext *)client_data);

    if (*bytes > 0) {
        *bytes = std::fread(buffer, sizeof(FLAC__byte), *bytes, context.file);
        if (std::ferror(context.file))
            return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
        else if (*bytes == 0)
            return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
        else
            return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
    } else
        return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
}

FLAC__StreamDecoderSeekStatus FlacDecodeSeekCallback(const FLAC__StreamDecoder *decoder,
                                                     FLAC__uint64 absolute_byte_offset,
                                                     void *client_data) {
    auto &context = *((FlacFileDataContext *)client_data);

    // TODO use fseeko to get 64 bit offset
    if (std::fseek(context.file, (long)absolute_byte_offset, SEEK_SET) < 0)
        return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
    else
        return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

FLAC__StreamDecoderTellStatus FlacDecodeTellCallback(const FLAC__StreamDecoder *decoder,
                                                     FLAC__uint64 *absolute_byte_offset,
                                                     void *client_data) {
    auto &context = *((FlacFileDataContext *)client_data);

    off_t pos;
    if ((pos = std::ftell(context.file)) < 0)
        return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
    else {
        *absolute_byte_offset = (FLAC__uint64)pos;
        return FLAC__STREAM_DECODER_TELL_STATUS_OK;
    }
}

FLAC__StreamDecoderLengthStatus
FlacDecodeLengthCallback(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data) {
    auto &context = *((FlacFileDataContext *)client_data);

    struct stat filestats;

    if (fstat(_fileno(context.file), &filestats) != 0)
        return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
    else {
        *stream_length = (FLAC__uint64)filestats.st_size;
        return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
    }
}

FLAC__bool FlacDecodeIsEndOfFile(const FLAC__StreamDecoder *decoder, void *client_data) {
    auto &context = *((FlacFileDataContext *)client_data);
    return feof(context.file) ? true : false;
}

FLAC__StreamDecoderWriteStatus FlacDecoderWriteCallback(const FLAC__StreamDecoder *decoder,
                                                        const FLAC__Frame *flac_frame,
                                                        const FLAC__int32 *const buffer[],
                                                        void *client_data) {
    auto &context = *((FlacFileDataContext *)client_data);

    context.data.num_channels = flac_frame->header.channels;
    context.data.bits_per_sample = flac_frame->header.bits_per_sample;

    double divisor;
    switch (flac_frame->header.bits_per_sample) {
        case 8: divisor = std::pow(2, 7); break;
        case 16:
            divisor = 32768.0; // 2^15
            break;
        case 20: divisor = std::pow(2, 19); break;
        case 24:
            divisor = 8388608.0; // 2^23
            break;
        default: divisor = 9999999.0; assert(false);
    }

    for (unsigned int frame = 0; frame < flac_frame->header.blocksize; ++frame) {
        for (unsigned int chan = 0; chan < flac_frame->header.channels; ++chan) {
            const auto val = buffer[chan][frame] / divisor;
            context.data.interleaved_samples.push_back(val);
        }
    }

    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void FlacDecoderMetadataCallback(const FLAC__StreamDecoder *decoder,
                                 const FLAC__StreamMetadata *metadata,
                                 void *client_data) {
    auto &context = *((FlacFileDataContext *)client_data);

    switch (metadata->type) {
        case FLAC__METADATA_TYPE_APPLICATION: {
            if (memcmp(metadata->data.application.id, flac_custom_signet_application_id, 4) == 0) {
                const auto id_bytes = (FLAC__STREAM_METADATA_APPLICATION_ID_LEN / 8);
                if (metadata->length > id_bytes) {
                    std::string data {(char *)metadata->data.application.data, metadata->length - id_bytes};
                    std::stringstream ss(data);
                    try {
                        cereal::JSONInputArchive archive(ss);
                        archive(cereal::make_nvp(signet_root_json_object_name, context.data.metadata));
                    } catch (const std::exception &e) {
                        ErrorWithNewLine("Flac", "Error parsing FLAC signet metadata: {}", e.what());
                    }
                }
                return;
            }
            break;
        }
        case FLAC__METADATA_TYPE_STREAMINFO: {
            context.data.num_channels = metadata->data.stream_info.channels;
            context.data.bits_per_sample = metadata->data.stream_info.bits_per_sample;
            context.data.sample_rate = metadata->data.stream_info.sample_rate;
            context.data.interleaved_samples.reserve(metadata->data.stream_info.total_samples);
            return;
        }
        case FLAC__METADATA_TYPE_CUESHEET:
        case FLAC__METADATA_TYPE_SEEKTABLE: {
            const char *type = metadata->type == FLAC__METADATA_TYPE_CUESHEET ? "cuesheet" : "seektable";
            WarningWithNewLine("Flac", "Unsupported FLAC file block '{}', this will be deleted", type);
            // We're discarding these at the moment because this data contains references to particular points
            // in the audio file. We might changing the length of the audio file so this would become
            // invalidated.
            return;
        }
    }
    std::shared_ptr<FLAC__StreamMetadata> ptr {FLAC__metadata_object_clone(metadata),
                                               &FLAC__metadata_object_delete};
    context.data.flac_metadata.push_back(ptr);
}

void FlacStreamDecodeErrorCallback(const FLAC__StreamDecoder *decoder,
                                   FLAC__StreamDecoderErrorStatus status,
                                   void *client_data) {
    auto &context = *((FlacFileDataContext *)client_data);
    ErrorWithNewLine("Flac", "error triggered: {}", FLAC__StreamDecoderErrorStatusString[status]);
}

bool DecodeFlacFile(FILE *file, AudioData &output) {
    std::unique_ptr<FLAC__StreamDecoder, decltype(&FLAC__stream_decoder_delete)> decoder(
        FLAC__stream_decoder_new(), &FLAC__stream_decoder_delete);
    if (decoder == nullptr) {
        ErrorWithNewLine("Flac", "failed to allocate memory for flac decoder");
        return false;
    }

    FlacFileDataContext context(file, output);

    const bool set_respond_all = FLAC__stream_decoder_set_metadata_respond_all(decoder.get());
    assert(set_respond_all);

    const auto init_status = FLAC__stream_decoder_init_stream(
        decoder.get(), FlacDecodeReadCallback, FlacDecodeSeekCallback, FlacDecodeTellCallback,
        FlacDecodeLengthCallback, FlacDecodeIsEndOfFile, FlacDecoderWriteCallback,
        FlacDecoderMetadataCallback, FlacStreamDecodeErrorCallback, &context);
    if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
        ErrorWithNewLine("Flac", "failed to initialise the flac stream: {}",
                         FLAC__StreamDecoderInitStatusString[init_status]);
        return false;
    }

    const auto process_success = FLAC__stream_decoder_process_until_end_of_stream(decoder.get());
    if (!process_success) {
        ErrorWithNewLine("Flac", "failed encoding flac data");
    } else {
        output.sample_rate = FLAC__stream_decoder_get_sample_rate(decoder.get());
    }

    FLAC__stream_decoder_finish(decoder.get());
    return process_success;
}
