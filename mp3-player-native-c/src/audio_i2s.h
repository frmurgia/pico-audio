#ifndef AUDIO_I2S_H
#define AUDIO_I2S_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Initialize I2S audio output
 *
 * @param bck_pin Bit clock pin
 * @param lrck_pin Left/Right clock pin (Word Select)
 * @param din_pin Data input pin
 * @param sample_rate Sample rate in Hz (e.g. 44100)
 */
void audio_i2s_init(uint bck_pin, uint lrck_pin, uint din_pin, uint32_t sample_rate);

/**
 * Write audio samples to I2S
 *
 * @param buffer Pointer to 16-bit PCM samples
 * @param num_samples Number of samples to write
 */
void audio_i2s_write(int16_t* buffer, uint32_t num_samples);

/**
 * Check if I2S is ready for more data
 *
 * @return true if ready, false if busy
 */
bool audio_i2s_is_ready(void);

#endif // AUDIO_I2S_H
