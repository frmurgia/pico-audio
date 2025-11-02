#ifndef HW_CONFIG_H
#define HW_CONFIG_H

#include <stdbool.h>

/**
 * Initialize SD card with SDIO
 *
 * @return true if successful, false otherwise
 */
bool hw_config_init_sd(void);

/**
 * Check if SD card is mounted
 *
 * @return true if mounted, false otherwise
 */
bool hw_config_is_sd_mounted(void);

/**
 * Deinitialize SD card
 */
void hw_config_deinit_sd(void);

#endif // HW_CONFIG_H
