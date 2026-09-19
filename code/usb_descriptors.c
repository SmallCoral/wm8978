#include "usb_descriptors.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "stm32f407xx.h"

static const tusb_desc_device_t device_descriptor = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0xCAFE,
    .idProduct = 0x4010,
    .bcdDevice = 0x0100,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

uint8_t const *tud_descriptor_device_cb(void)
{
    return (uint8_t const *)&device_descriptor;
}

#define EPNUM_AUDIO_OUT 0x01
#define EPNUM_AUDIO_FB 0x81
#define CONFIG_TOTAL_LENGTH \
    (TUD_CONFIG_DESC_LEN + TUD_AUDIO10_SPEAKER_STEREO_FB_DESC_LEN(1))

static const uint8_t configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LENGTH,
                          0x00, 250),
    TUD_AUDIO10_SPEAKER_STEREO_FB_DESCRIPTOR(
        ITF_NUM_AUDIO_CONTROL, 4,
        CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX,
        CFG_TUD_AUDIO_FUNC_1_RESOLUTION_RX,
        EPNUM_AUDIO_OUT, CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_FS,
        EPNUM_AUDIO_FB, 48000),
};

TU_VERIFY_STATIC(sizeof(configuration_descriptor) == CONFIG_TOTAL_LENGTH,
                 "incorrect UAC1 configuration descriptor size");

uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return configuration_descriptor;
}

static const char *const string_descriptors[] = {
    (const char[]){0x09, 0x04},
    "SmallCoral",
    "WM8978 USB Audio",
    NULL,
    "WM8978 Headphones",
};

static uint16_t string_buffer[33];

static size_t append_hex32(uint16_t *destination, uint32_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    for (size_t i = 0; i < 8u; ++i) {
        destination[i] = (uint16_t)hex[(value >> (28u - (uint32_t)i * 4u)) & 0xFu];
    }
    return 8u;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;
    size_t count;

    if (index == 0u) {
        memcpy(&string_buffer[1], string_descriptors[0], 2u);
        count = 1u;
    } else if (index == 3u) {
        const uint32_t *const uid = (const uint32_t *)UID_BASE;
        count = append_hex32(&string_buffer[1], uid[0]);
        count += append_hex32(&string_buffer[1 + count], uid[1]);
        count += append_hex32(&string_buffer[1 + count], uid[2]);
    } else {
        if (index >= (sizeof(string_descriptors) / sizeof(string_descriptors[0])) ||
            string_descriptors[index] == NULL) {
            return NULL;
        }
        const char *text = string_descriptors[index];
        count = strlen(text);
        if (count > 32u) {
            count = 32u;
        }
        for (size_t i = 0; i < count; ++i) {
            string_buffer[1u + i] = (uint16_t)text[i];
        }
    }

    string_buffer[0] = (uint16_t)((TUSB_DESC_STRING << 8u) |
                                  (2u * count + 2u));
    return string_buffer;
}
