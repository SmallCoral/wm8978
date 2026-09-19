#include <stdbool.h>
#include <stdint.h>

#include "app_status.h"
#include "audio_i2s.h"
#include "platform.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "wm8978.h"

volatile app_status_t app_status
    __attribute__((section(".status"), used));

static uint8_t mute_state[3];
static int16_t volume_state[3] = {-12 * 256, -12 * 256, -12 * 256};
static const uint32_t sample_rate = 48000u;

static void set_status_defaults(void)
{
    volatile uint32_t *word = (volatile uint32_t *)&app_status;
    for (uint32_t i = 0; i < sizeof(app_status) / sizeof(uint32_t); ++i) {
        word[i] = 0u;
    }
    app_status.magic = APP_STATUS_MAGIC;
    app_status.version = 2u;
    app_status.stage = 1u;
    app_status.sample_rate = sample_rate;
    app_status.volume_db256 = (uint32_t)(int32_t)volume_state[0];
}

static void update_codec_volume(void)
{
    const bool muted = mute_state[0] || mute_state[1] || mute_state[2];
    wm8978_set_headphone(volume_state[0], muted);
    app_status.volume_db256 = (uint32_t)(int32_t)volume_state[0];
    app_status.muted = muted ? 1u : 0u;
}

static void led_task(void)
{
    static uint32_t last_change;
    static bool on;
    const uint32_t interval = app_status.streaming ? 80u :
                              (app_status.usb_mounted ? 500u : 125u);
    const uint32_t now = board_millis();
    if ((uint32_t)(now - last_change) >= interval) {
        last_change = now;
        on = !on;
        board_led_set(on);
    }
}

int main(void)
{
    set_status_defaults();
    board_gpio_init();
    const uint32_t clock_result = board_clock_init();
    if (clock_result == 0u) {
        app_status.stage = 0xE001u;
        for (;;) {
        }
    }
    app_status.clock_ok = clock_result;
    app_status.stage = 2u;

    app_status.wm8978_detected = wm8978_init() ? 1u : 0u;
    if (app_status.wm8978_detected != 0u) {
        if (!audio_i2s_init()) {
            app_status.stage = 0xE004u;
        } else {
            app_status.stage = 4u;
        }
    } else {
        /* Keep USB alive for diagnosis even if the codec control bus fails. */
        app_status.stage = 0xE002u;
    }

    board_usb_init();
    const tusb_rhport_init_t device_init = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_FULL,
    };
    if (!tusb_init(0u, &device_init)) {
        app_status.stage = 0xE003u;
        for (;;) {
            led_task();
        }
    }
    /* Stage 5 means the USB stack is running. Codec/I2S health remains
       visible through wm8978_detected and i2s_dma_started. */
    app_status.stage = 5u;

    for (;;) {
        tud_task();
        audio_i2s_service();
        led_task();
    }
}

void OTG_FS_IRQHandler(void)
{
    tud_int_handler(0u);
}

void tud_mount_cb(void)
{
    app_status.usb_mounted = 1u;
}

void tud_umount_cb(void)
{
    app_status.usb_mounted = 0u;
    app_status.streaming = 0u;
}

void tud_suspend_cb(bool remote_wakeup_enabled)
{
    (void)remote_wakeup_enabled;
}

void tud_resume_cb(void)
{
}

bool tud_audio_set_itf_cb(uint8_t rhport,
                          tusb_control_request_t const *request)
{
    (void)rhport;
    const uint8_t interface_number = tu_u16_low(request->wIndex);
    const uint8_t alternate_setting = tu_u16_low(request->wValue);
    if (interface_number == ITF_NUM_AUDIO_STREAMING) {
        app_status.streaming = alternate_setting != 0u ? 1u : 0u;
    }
    return true;
}

bool tud_audio_set_itf_close_ep_cb(uint8_t rhport,
                                   tusb_control_request_t const *request)
{
    (void)rhport;
    if (tu_u16_low(request->wIndex) == ITF_NUM_AUDIO_STREAMING) {
        app_status.streaming = 0u;
    }
    return true;
}

void tud_audio_feedback_params_cb(uint8_t function_id, uint8_t alternate,
                                  audio_feedback_params_t *parameters)
{
    (void)function_id;
    (void)alternate;
    parameters->method = AUDIO_FEEDBACK_METHOD_FIFO_COUNT;
    parameters->sample_freq = sample_rate;
}

bool tud_audio_rx_done_isr(uint8_t rhport, uint16_t bytes_received,
                           uint8_t function_id, uint8_t endpoint,
                           uint8_t alternate_setting)
{
    (void)rhport;
    (void)function_id;
    (void)endpoint;
    (void)alternate_setting;
    app_status.usb_rx_bytes += bytes_received;
    return true;
}

bool tud_audio_set_req_ep_cb(uint8_t rhport,
                             tusb_control_request_t const *request,
                             uint8_t *buffer)
{
    (void)rhport;
    if (TU_U16_HIGH(request->wValue) != AUDIO10_EP_CTRL_SAMPLING_FREQ ||
        request->bRequest != AUDIO10_CS_REQ_SET_CUR || request->wLength != 3u) {
        return false;
    }
    const uint32_t requested = (uint32_t)buffer[0] |
                               ((uint32_t)buffer[1] << 8u) |
                               ((uint32_t)buffer[2] << 16u);
    return requested == sample_rate;
}

bool tud_audio_get_req_ep_cb(uint8_t rhport,
                             tusb_control_request_t const *request)
{
    if (TU_U16_HIGH(request->wValue) == AUDIO10_EP_CTRL_SAMPLING_FREQ &&
        request->bRequest == AUDIO10_CS_REQ_GET_CUR) {
        const uint8_t frequency[3] = {
            (uint8_t)sample_rate,
            (uint8_t)(sample_rate >> 8u),
            (uint8_t)(sample_rate >> 16u),
        };
        return tud_audio_buffer_and_schedule_control_xfer(
            rhport, request, (void *)frequency, sizeof(frequency));
    }
    return false;
}

bool tud_audio_set_req_entity_cb(uint8_t rhport,
                                 tusb_control_request_t const *request,
                                 uint8_t *buffer)
{
    (void)rhport;
    const uint8_t entity = TU_U16_HIGH(request->wIndex);
    const uint8_t control = TU_U16_HIGH(request->wValue);
    const uint8_t channel = TU_U16_LOW(request->wValue);
    if (entity != UAC1_ENTITY_FEATURE_UNIT || channel >= 3u ||
        request->bRequest != AUDIO10_CS_REQ_SET_CUR) {
        return false;
    }

    if (control == AUDIO10_FU_CTRL_MUTE && request->wLength == 1u) {
        mute_state[channel] = buffer[0] != 0u ? 1u : 0u;
        update_codec_volume();
        return true;
    }
    if (control == AUDIO10_FU_CTRL_VOLUME && request->wLength == 2u) {
        const int16_t value = (int16_t)((uint16_t)buffer[0] |
                                        ((uint16_t)buffer[1] << 8u));
        volume_state[channel] = value;
        if (channel == 0u) {
            volume_state[1] = value;
            volume_state[2] = value;
        } else {
            volume_state[0] = value;
        }
        update_codec_volume();
        return true;
    }
    return false;
}

static bool send_i16(uint8_t rhport, tusb_control_request_t const *request,
                     int16_t value)
{
    const uint8_t data[2] = {(uint8_t)value, (uint8_t)((uint16_t)value >> 8u)};
    return tud_audio_buffer_and_schedule_control_xfer(
        rhport, request, (void *)data, sizeof(data));
}

bool tud_audio_get_req_entity_cb(uint8_t rhport,
                                 tusb_control_request_t const *request)
{
    const uint8_t entity = TU_U16_HIGH(request->wIndex);
    const uint8_t control = TU_U16_HIGH(request->wValue);
    const uint8_t channel = TU_U16_LOW(request->wValue);
    if (entity != UAC1_ENTITY_FEATURE_UNIT || channel >= 3u) {
        return false;
    }

    if (control == AUDIO10_FU_CTRL_MUTE &&
        request->bRequest == AUDIO10_CS_REQ_GET_CUR) {
        return tud_audio_buffer_and_schedule_control_xfer(
            rhport, request, &mute_state[channel], 1u);
    }
    if (control == AUDIO10_FU_CTRL_VOLUME) {
        switch (request->bRequest) {
        case AUDIO10_CS_REQ_GET_CUR:
            return send_i16(rhport, request, volume_state[channel]);
        case AUDIO10_CS_REQ_GET_MIN:
            return send_i16(rhport, request, -57 * 256);
        case AUDIO10_CS_REQ_GET_MAX:
            return send_i16(rhport, request, 0);
        case AUDIO10_CS_REQ_GET_RES:
            return send_i16(rhport, request, 256);
        default:
            break;
        }
    }
    return false;
}
