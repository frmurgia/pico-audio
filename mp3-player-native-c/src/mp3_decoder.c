/**
 * MP3 Decoder Wrapper - minimp3
 */

#include "mp3_decoder.h"
#include <string.h>

// minimp3 implementation
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_SIMD
#include "minimp3.h"

void mp3_decoder_init(mp3_decoder_t* decoder) {
    mp3dec_init(&decoder->dec);
    decoder->sample_rate = 0;
    decoder->channels = 0;
    decoder->bitrate = 0;
}

int mp3_decoder_decode_frame(mp3_decoder_t* decoder,
                              const uint8_t* mp3_data,
                              uint32_t mp3_size,
                              int16_t* pcm_out,
                              mp3_frame_info_t* info) {

    mp3dec_frame_info_t frame_info;

    int samples = mp3dec_decode_frame(&decoder->dec,
                                       mp3_data,
                                       mp3_size,
                                       pcm_out,
                                       &frame_info);

    if (samples > 0) {
        // Store info
        decoder->sample_rate = frame_info.hz;
        decoder->channels = frame_info.channels;
        decoder->bitrate = frame_info.bitrate_kbps;

        if (info) {
            info->sample_rate = frame_info.hz;
            info->channels = frame_info.channels;
            info->bitrate_kbps = frame_info.bitrate_kbps;
            info->frame_bytes = frame_info.frame_bytes;
        }
    }

    return samples;
}

void mp3_decoder_convert_to_mono(int16_t* stereo_buffer, uint32_t stereo_samples, int16_t* mono_out) {
    uint32_t mono_samples = stereo_samples / 2;

    for (uint32_t i = 0; i < mono_samples; i++) {
        int32_t left = stereo_buffer[i * 2];
        int32_t right = stereo_buffer[i * 2 + 1];
        mono_out[i] = (int16_t)((left + right) / 2);
    }
}
