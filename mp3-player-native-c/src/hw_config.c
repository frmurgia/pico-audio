/**
 * Hardware Configuration for no_OS_FatFS_SDIO
 * Configures SDIO interface with custom pins
 */

#include "hw_config.h"
#include "ff.h"
#include "diskio.h"
#include "sd_card.h"
#include "my_debug.h"

// SDIO Configuration
// CLK  = GP7
// CMD  = GP6
// DAT0 = GP8 (DAT1=9, DAT2=10, DAT3=11 consecutive)

static sd_sdio_if_t sdio_if = {
    .CMD_gpio = 6,
    .D0_gpio = 8,     // D1=9, D2=10, D3=11 automatically
    .CLK_gpio = 7,
    .baud_rate = 20 * 1000 * 1000,  // 20 MHz
    .SDIO_PIO = pio1,
    .DMA_IRQ_num = DMA_IRQ_1,
    .use_exclusive_DMA_IRQ_handler = true,
    .no_miso_gpio_pull_up = false
};

static sd_card_t sd_card = {
    .type = SD_IF_SDIO,
    .sdio_if_p = &sdio_if,
    .use_card_detect = false,  // No card detect pin
    .card_detect_gpio = 0,
    .card_detected_true = 0,
    .card_detect_use_pull = false,
    .card_detect_pull_hi = false
};

// FatFS objects
static FATFS fatfs;
static bool sd_mounted = false;

/* ********************************************************************** */

size_t sd_get_num(void) {
    return 1;  // Single SD card
}

sd_card_t *sd_get_by_num(size_t num) {
    if (num == 0) {
        return &sd_card;
    }
    return NULL;
}

/* ********************************************************************** */

bool hw_config_init_sd(void) {
    // Initialize SD card driver
    sd_init_driver();

    // Mount filesystem
    FRESULT fr = f_mount(&fatfs, "", 1);
    if (fr != FR_OK) {
        printf("SD: Mount failed (error %d)\n", fr);
        return false;
    }

    sd_mounted = true;
    printf("SD: Mounted successfully\n");

    // Print card info
    FATFS *fs;
    DWORD fre_clust;

    fr = f_getfree("", &fre_clust, &fs);
    if (fr == FR_OK) {
        uint64_t total = (uint64_t)(fs->n_fatent - 2) * fs->csize * 512;
        uint64_t free = (uint64_t)fre_clust * fs->csize * 512;

        printf("SD: Total %llu MB, Free %llu MB\n",
               total / (1024 * 1024),
               free / (1024 * 1024));
    }

    return true;
}

bool hw_config_is_sd_mounted(void) {
    return sd_mounted;
}

void hw_config_deinit_sd(void) {
    if (sd_mounted) {
        f_unmount("");
        sd_mounted = false;
    }
}
