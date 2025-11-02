/**
 * I2S Audio Output - Complete Implementation
 * Using PIO and DMA for efficient audio streaming
 */

#include "audio_i2s.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "i2s.pio.h"
#include <string.h>
#include <stdio.h>

// I2S state
static PIO i2s_pio = pio0;
static unsigned int i2s_sm = 0;
static int dma_channel = -1;
static int dma_channel_alt = -1;  // Ping-pong DMA

// DMA buffers (double buffered)
#define DMA_BUFFER_SIZE 512  // Samples per DMA transfer
static int16_t dma_buffer_a[DMA_BUFFER_SIZE];
static int16_t dma_buffer_b[DMA_BUFFER_SIZE];
static volatile bool buffer_a_active = true;

// Callback for filling buffer
static audio_i2s_callback_t user_callback = NULL;
static void* user_callback_data = NULL;

// Forward declarations
static void dma_irq_handler(void);

void audio_i2s_init(unsigned int bck_pin, unsigned int lrck_pin, unsigned int din_pin, uint32_t sample_rate) {
    printf("I2S: Initializing (BCK=%d, LRCK=%d, DIN=%d, SR=%lu)\n",
           bck_pin, lrck_pin, din_pin, sample_rate);

    // Load PIO program
    unsigned int offset = pio_add_program(i2s_pio, &i2s_output_program);
    printf("I2S: PIO program loaded at offset %d\n", offset);

    // Initialize PIO state machine
    i2s_output_program_init(i2s_pio, i2s_sm, offset, din_pin, bck_pin, sample_rate);
    printf("I2S: PIO state machine configured\n");

    // Claim DMA channels
    dma_channel = dma_claim_unused_channel(true);
    dma_channel_alt = dma_claim_unused_channel(true);
    printf("I2S: DMA channels: %d, %d\n", dma_channel, dma_channel_alt);

    // Configure first DMA channel
    dma_channel_config c = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);  // 32-bit transfers (2x16-bit samples)
    channel_config_set_read_increment(&c, true);             // Increment read address
    channel_config_set_write_increment(&c, false);           // Write to same PIO FIFO
    channel_config_set_dreq(&c, pio_get_dreq(i2s_pio, i2s_sm, true));  // Pace by PIO TX FIFO
    channel_config_set_chain_to(&c, dma_channel_alt);       // Chain to alternate channel

    dma_channel_configure(
        dma_channel,
        &c,
        &i2s_pio->txf[i2s_sm],      // Write to PIO TX FIFO
        dma_buffer_a,                // Read from buffer A
        DMA_BUFFER_SIZE / 2,         // Transfer count (32-bit words = samples/2)
        false                        // Don't start yet
    );

    // Configure second DMA channel (identical, but reads from buffer B)
    c = dma_channel_get_default_config(dma_channel_alt);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, pio_get_dreq(i2s_pio, i2s_sm, true));
    channel_config_set_chain_to(&c, dma_channel);  // Chain back to first channel

    dma_channel_configure(
        dma_channel_alt,
        &c,
        &i2s_pio->txf[i2s_sm],
        dma_buffer_b,
        DMA_BUFFER_SIZE / 2,
        false
    );

    // Enable DMA interrupt on channel completion
    dma_channel_set_irq0_enabled(dma_channel, true);
    dma_channel_set_irq0_enabled(dma_channel_alt, true);

    irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    // Clear buffers
    memset(dma_buffer_a, 0, sizeof(dma_buffer_a));
    memset(dma_buffer_b, 0, sizeof(dma_buffer_b));

    printf("I2S: Initialization complete\n");
}

void audio_i2s_start(void) {
    printf("I2S: Starting DMA\n");
    dma_channel_start(dma_channel);
}

void audio_i2s_stop(void) {
    dma_channel_abort(dma_channel);
    dma_channel_abort(dma_channel_alt);
}

void audio_i2s_set_callback(audio_i2s_callback_t callback, void* user_data) {
    user_callback = callback;
    user_callback_data = user_data;
}

bool audio_i2s_is_playing(void) {
    return dma_channel_is_busy(dma_channel) || dma_channel_is_busy(dma_channel_alt);
}

// DMA interrupt handler - called when a buffer is consumed
static void dma_irq_handler(void) {
    // Check which channel completed
    if (dma_channel_get_irq0_status(dma_channel)) {
        dma_channel_acknowledge_irq0(dma_channel);
        buffer_a_active = false;  // Buffer B is now active

        // Fill buffer A for next iteration
        if (user_callback) {
            user_callback(dma_buffer_a, DMA_BUFFER_SIZE, user_callback_data);
        }
    }

    if (dma_channel_get_irq0_status(dma_channel_alt)) {
        dma_channel_acknowledge_irq0(dma_channel_alt);
        buffer_a_active = true;  // Buffer A is now active

        // Fill buffer B for next iteration
        if (user_callback) {
            user_callback(dma_buffer_b, DMA_BUFFER_SIZE, user_callback_data);
        }
    }
}

void audio_i2s_write_blocking(int16_t* buffer, uint32_t num_samples) {
    // Write samples directly to PIO FIFO (blocking)
    for (uint32_t i = 0; i < num_samples; i += 2) {
        // Combine two 16-bit samples into one 32-bit word
        uint32_t word = ((uint32_t)buffer[i]) | (((uint32_t)buffer[i+1]) << 16);
        pio_sm_put_blocking(i2s_pio, i2s_sm, word);
    }
}
