#ifndef APP_STATUS_H
#define APP_STATUS_H

#include <stdint.h>

#define APP_STATUS_MAGIC 0x55414331u /* ASCII "UAC1" */

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t stage;
    uint32_t clock_ok;
    uint32_t wm8978_detected;
    uint32_t i2s_dma_started;
    uint32_t usb_mounted;
    uint32_t streaming;
    uint32_t dma_half_irqs;
    uint32_t dma_full_irqs;
    uint32_t audio_underruns;
    uint32_t usb_rx_bytes;
    uint32_t sample_rate;
    uint32_t volume_db256;
    uint32_t muted;
    uint32_t audio_fifo_bytes;
    uint32_t mclk_hz;
    uint32_t lrclk_hz;
    uint32_t audio_clock_mode;
} app_status_t;

extern volatile app_status_t app_status;

#endif
