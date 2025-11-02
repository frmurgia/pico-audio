#ifndef MP3_DECODER_H
#define MP3_DECODER_H

#include <stdint.h>
#include "minimp3.h"

typedef struct {
    mp3dec_t dec;
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t bitrate;
} mp3_decoder_t;

typedef struct {
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t bitrate_kbps;
    uint32_t frame_bytes;
} mp3_frame_info_t;

/**
 * Initialize MP3 decoder
 */
void mp3_decoder_init(mp3_decoder_t* decoder);

/**
 * Decode one MP3 frame
 *
 * @param decoder Decoder instance
 * @param mp3_data MP3 data buffer
 * @param mp3_size Size of MP3 data
 * @param pcm_out Output PCM buffer (must be at least 1152*2 samples)
 * @param info Output frame info (can be NULL)
 * @return Number of PCM samples decoded, or 0 on error
 */
int mp3_decoder_decode_frame(mp3_decoder_t* decoder,
                              const uint8_t* mp3_data,
                              uint32_t mp3_size,
                              int16_t* pcm_out,
                              mp3_frame_info_t* info);

/**
 * Convert stereo to mono
 *
 * @param stereo_buffer Input stereo buffer
 * @param stereo_samples Number of stereo samples (L+R pairs)
 * @param mono_out Output mono buffer
 */
void mp3_decoder_convert_to_mono(int16_t* stereo_buffer, uint32_t stereo_samples, int16_t* mono_out);

#endif // MP3_DECODER_H
