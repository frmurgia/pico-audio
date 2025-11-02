#ifndef AUDIO_I2S_H
#define AUDIO_I2S_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Callback function for filling audio buffer
 * Called by DMA interrupt when buffer needs refilling
 *
 * @param buffer Pointer to buffer to fill with audio samples
 * @param num_samples Number of samples to fill
 * @param user_data User data pointer
 */
typedef void (*audio_i2s_callback_t)(int16_t* buffer, uint32_t num_samples, void* user_data);

/**
 * Initialize I2S audio output with PIO and DMA
 *
 * @param bck_pin Bit clock pin
 * @param lrck_pin Left/Right clock pin (Word Select)
 * @param din_pin Data input pin
 * @param sample_rate Sample rate in Hz (e.g. 44100)
 */
void audio_i2s_init(uint bck_pin, uint lrck_pin, uint din_pin, uint32_t sample_rate);

/**
 * Start I2S DMA streaming
 */
void audio_i2s_start(void);

/**
 * Stop I2S DMA streaming
 */
void audio_i2s_stop(void);

/**
 * Set callback for buffer refilling
 *
 * @param callback Function to call when buffer needs data
 * @param user_data Pointer to pass to callback
 */
void audio_i2s_set_callback(audio_i2s_callback_t callback, void* user_data);

/**
 * Check if I2S is currently playing
 *
 * @return true if playing, false if stopped
 */
bool audio_i2s_is_playing(void);

/**
 * Write audio samples to I2S (blocking)
 * For testing/debugging without DMA
 *
 * @param buffer Pointer to 16-bit PCM samples
 * @param num_samples Number of samples to write
 */
void audio_i2s_write_blocking(int16_t* buffer, uint32_t num_samples);

#endif // AUDIO_I2S_H
