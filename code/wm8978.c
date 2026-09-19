#include "wm8978.h"

#include "platform.h"
#include "stm32f407xx.h"

#define WM8978_ADDRESS_WRITE (0x1Au << 1u)
#define SCL_PIN 6u
#define SDA_PIN 7u

static uint16_t register_shadow[58];
static bool codec_ready;

static void bus_delay(void)
{
    for (volatile uint32_t i = 0; i < 180u; ++i) {
        __NOP();
    }
}

static void line_high(uint32_t pin) { GPIOD->BSRR = 1u << pin; }
static void line_low(uint32_t pin) { GPIOD->BSRR = 1u << (pin + 16u); }
static bool line_read(uint32_t pin) { return ((GPIOD->IDR >> pin) & 1u) != 0u; }

static void bus_start(void)
{
    line_high(SDA_PIN);
    line_high(SCL_PIN);
    bus_delay();
    line_low(SDA_PIN);
    bus_delay();
    line_low(SCL_PIN);
}

static void bus_stop(void)
{
    line_low(SDA_PIN);
    bus_delay();
    line_high(SCL_PIN);
    bus_delay();
    line_high(SDA_PIN);
    bus_delay();
}

static bool bus_write_byte(uint8_t value)
{
    for (uint32_t bit = 0; bit < 8u; ++bit) {
        if ((value & 0x80u) != 0u) {
            line_high(SDA_PIN);
        } else {
            line_low(SDA_PIN);
        }
        bus_delay();
        line_high(SCL_PIN);
        bus_delay();
        line_low(SCL_PIN);
        value <<= 1u;
    }

    line_high(SDA_PIN);
    bus_delay();
    line_high(SCL_PIN);
    bus_delay();
    const bool acknowledged = !line_read(SDA_PIN);
    line_low(SCL_PIN);
    bus_delay();
    return acknowledged;
}

static bool write_register(uint8_t reg, uint16_t value)
{
    if (reg >= 58u) {
        return false;
    }
    value &= 0x01FFu;
    bus_start();
    const bool ack_address = bus_write_byte(WM8978_ADDRESS_WRITE);
    const bool ack_register = bus_write_byte((uint8_t)((reg << 1u) |
                                                       ((value >> 8u) & 1u)));
    const bool ack_data = bus_write_byte((uint8_t)value);
    bus_stop();
    if (ack_address && ack_register && ack_data) {
        register_shadow[reg] = value;
        return true;
    }
    return false;
}

static bool configure_control_gpio(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
    (void)RCC->AHB1ENR;
    line_high(SCL_PIN);
    line_high(SDA_PIN);
    GPIOD->MODER = (GPIOD->MODER &
                     ~((3u << (SCL_PIN * 2u)) | (3u << (SDA_PIN * 2u)))) |
                    (1u << (SCL_PIN * 2u)) | (1u << (SDA_PIN * 2u));
    GPIOD->OTYPER |= (1u << SCL_PIN) | (1u << SDA_PIN);
    GPIOD->OSPEEDR |= (3u << (SCL_PIN * 2u)) |
                      (3u << (SDA_PIN * 2u));
    GPIOD->PUPDR &= ~((3u << (SCL_PIN * 2u)) |
                      (3u << (SDA_PIN * 2u)));
    bus_delay();
    return line_read(SCL_PIN) && line_read(SDA_PIN);
}

bool wm8978_init(void)
{
    codec_ready = false;
    if (!configure_control_gpio()) {
        return false;
    }
    for (uint32_t i = 0; i < 58u; ++i) {
        register_shadow[i] = 0u;
    }

    if (!write_register(0u, 0u)) {
        return false;
    }
    board_delay_ms(10u);
    if (!write_register(1u, 0x00Fu)) { /* VMID 5k, analog bias/buffers */
        return false;
    }
    board_delay_ms(100u);

    /* 24.576 MHz / 2 / 4 / 64 = 48 kHz; codec is I2S clock master. */
    if (!write_register(6u, 0x049u) ||
        !write_register(4u, 0x010u) ||
        !write_register(7u, 0x000u) ||
        !write_register(10u, 0x008u) ||
        !write_register(11u, 0x0FFu) ||
        !write_register(12u, 0x1FFu) ||
        !write_register(3u, 0x00Fu) ||
        !write_register(50u, 0x001u) ||
        !write_register(51u, 0x001u) ||
        !write_register(2u, 0x180u)) {
        return false;
    }

    codec_ready = true;
    wm8978_set_headphone(-12 * 256, false);
    board_delay_ms(20u);
    return true;
}

void wm8978_set_headphone(int16_t volume_db256, bool mute)
{
    if (!codec_ready) {
        return;
    }
    int32_t db = volume_db256 / 256;
    if (db > 0) {
        db = 0;
    } else if (db < -57) {
        db = -57;
    }
    uint16_t level = (uint16_t)(57 + db);
    if (mute) {
        level |= 0x040u;
    }
    (void)write_register(52u, level);
    (void)write_register(53u, level | 0x100u);
}

bool wm8978_set_clock_master(bool master)
{
    if (!codec_ready) {
        return false;
    }
    /* Master: external 24.576 MHz / 2 SYSCLK, /4 BCLK. Slave: external
       256fs MCLK from STM32, with BCLK/LRCLK also supplied by STM32. */
    return write_register(6u, master ? 0x049u : 0x000u);
}
