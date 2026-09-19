#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

/* Returns 1 for HSE, 2 for the HSI fallback, or 0 on failure. */
uint32_t board_clock_init(void);
void board_gpio_init(void);
void board_usb_init(void);
void board_delay_ms(uint32_t milliseconds);
uint32_t board_millis(void);
void board_led_set(bool on);

#endif
