#ifndef WM8978_H
#define WM8978_H

#include <stdbool.h>
#include <stdint.h>

bool wm8978_init(void);
void wm8978_set_headphone(int16_t volume_db256, bool mute);
bool wm8978_set_clock_master(bool master);

#endif
