#include "audio_i2s.h"

#include <stdint.h>
#include <string.h>

#include "app_status.h"
#include "platform.h"
#include "stm32f407xx.h"
#include "tusb.h"
#include "wm8978.h"

#define FRAMES_PER_MILLISECOND 48u
#define CHANNEL_COUNT 2u
#define HALF_SAMPLES (FRAMES_PER_MILLISECOND * CHANNEL_COUNT)
#define HALF_BYTES (HALF_SAMPLES * sizeof(int16_t))
#define BUFFER_SAMPLES (HALF_SAMPLES * 2u)
#define PLLI2S_TIMEOUT 5000000u

static int16_t audio_dma_buffer[BUFFER_SAMPLES] __attribute__((aligned(4)));
static volatile uint32_t refill_flags;

static void gpio_set_af(GPIO_TypeDef *gpio, uint32_t pin, uint32_t af)
{
    gpio->MODER = (gpio->MODER & ~(3u << (pin * 2u))) |
                  (2u << (pin * 2u));
    gpio->OTYPER &= ~(1u << pin);
    gpio->OSPEEDR |= 3u << (pin * 2u);
    gpio->PUPDR &= ~(3u << (pin * 2u));
    const uint32_t index = pin >> 3u;
    const uint32_t shift = (pin & 7u) * 4u;
    gpio->AFR[index] = (gpio->AFR[index] & ~(0xFu << shift)) | (af << shift);
}

static uint32_t measure_mclk(void)
{
    /* PC6 is TIM3_CH1. TIM3 is only 16-bit on STM32F407, therefore the
       24.576 MHz input must be measured for less than 2.66 ms. DWT gives us
       a precise 1 ms gate without depending on SysTick phase. */
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    (void)RCC->APB1ENR;
    RCC->APB1RSTR |= RCC_APB1RSTR_TIM3RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_TIM3RST;
    gpio_set_af(GPIOC, 6u, 2u);
    TIM3->CR1 = 0u;
    TIM3->SMCR = 0u;
    TIM3->CCMR1 = TIM_CCMR1_CC1S_0;
    TIM3->CCER = TIM_CCER_CC1E;
    TIM3->PSC = 0u;
    TIM3->ARR = 0xFFFFu;
    TIM3->CNT = 0u;
    TIM3->SMCR = TIM_SMCR_TS_2 | TIM_SMCR_TS_0 |
                 TIM_SMCR_SMS_2 | TIM_SMCR_SMS_1 | TIM_SMCR_SMS_0;

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    TIM3->CR1 = TIM_CR1_CEN;
    const uint32_t gate_cycles = SystemCoreClock / 1000u;
    const uint32_t start = DWT->CYCCNT;
    while ((uint32_t)(DWT->CYCCNT - start) < gate_cycles) {
    }
    const uint32_t edges = TIM3->CNT;
    TIM3->CR1 = 0u;
    return edges * 1000u;
}

static uint32_t measure_lrclk(void)
{
    /* LRCLK is only 48 kHz, so direct edge polling is ample for diagnosis. */
    GPIOB->MODER &= ~GPIO_MODER_MODER12;
    GPIOB->PUPDR &= ~GPIO_PUPDR_PUPDR12;
    uint32_t previous = GPIOB->IDR & GPIO_IDR_IDR_12;
    uint32_t edges = 0u;
    const uint32_t start = board_millis();
    while ((uint32_t)(board_millis() - start) < 10u) {
        const uint32_t current = GPIOB->IDR & GPIO_IDR_IDR_12;
        if (current != previous) {
            previous = current;
            ++edges;
        }
    }
    return edges * 50u;
}

bool audio_i2s_init(void)
{
    bool stm32_is_clock_master = false;
    memset(audio_dma_buffer, 0, sizeof(audio_dma_buffer));

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN |
                    RCC_AHB1ENR_DMA1EN;
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
    (void)RCC->APB1ENR;

    app_status.mclk_hz = measure_mclk();
    app_status.lrclk_hz = measure_lrclk();

    if (app_status.mclk_hz < 20000000u || app_status.lrclk_hz < 40000u) {
        /* X4 is not oscillating on the assembled board. Generate a close
           48 kHz clock family with PLLI2S and drive MCLK on PC6 instead. */
        stm32_is_clock_master = true;
        if (!wm8978_set_clock_master(false)) {
            return false;
        }
        RCC->CR &= ~RCC_CR_PLLI2SON;
        uint32_t timeout = PLLI2S_TIMEOUT;
        while (((RCC->CR & RCC_CR_PLLI2SRDY) != 0u) &&
               (--timeout != 0u)) {
        }
        if (timeout == 0u) {
            return false;
        }
        RCC->PLLI2SCFGR = (258u << RCC_PLLI2SCFGR_PLLI2SN_Pos) |
                          (3u << RCC_PLLI2SCFGR_PLLI2SR_Pos);
        RCC->CR |= RCC_CR_PLLI2SON;
        timeout = PLLI2S_TIMEOUT;
        while (((RCC->CR & RCC_CR_PLLI2SRDY) == 0u) &&
               (--timeout != 0u)) {
        }
        if (timeout == 0u) {
            return false;
        }
        gpio_set_af(GPIOC, 6u, 5u); /* I2S2_MCK */
        app_status.audio_clock_mode = 2u;
    } else {
        if (!wm8978_set_clock_master(true)) {
            return false;
        }
        app_status.audio_clock_mode = 1u;
    }

    gpio_set_af(GPIOB, 12u, 5u); /* I2S2_WS */
    gpio_set_af(GPIOB, 13u, 5u); /* I2S2_CK */
    gpio_set_af(GPIOC, 3u, 5u);  /* I2S2_SD */

    /* PC6 remains a TIM3 input with X4, or becomes I2S2_MCK in fallback. */

    DMA1_Stream4->CR = 0u;
    while ((DMA1_Stream4->CR & DMA_SxCR_EN) != 0u) {
    }
    DMA1->HIFCR = DMA_HIFCR_CFEIF4 | DMA_HIFCR_CDMEIF4 |
                  DMA_HIFCR_CTEIF4 | DMA_HIFCR_CHTIF4 |
                  DMA_HIFCR_CTCIF4;
    DMA1_Stream4->PAR = (uint32_t)&SPI2->DR;
    DMA1_Stream4->M0AR = (uint32_t)audio_dma_buffer;
    DMA1_Stream4->NDTR = BUFFER_SAMPLES;
    DMA1_Stream4->FCR = 0u;
    DMA1_Stream4->CR = DMA_SxCR_PL_1 | DMA_SxCR_MSIZE_0 |
                       DMA_SxCR_PSIZE_0 | DMA_SxCR_MINC |
                       DMA_SxCR_DIR_0 | DMA_SxCR_CIRC |
                       DMA_SxCR_HTIE | DMA_SxCR_TCIE;

    /* 16-bit samples in 32-bit channel slots (64 BCLK per stereo frame). */
    SPI2->I2SPR = stm32_is_clock_master ?
                  (SPI_I2SPR_MCKOE | SPI_I2SPR_ODD | 3u) : 2u;
    SPI2->I2SCFGR = SPI_I2SCFGR_I2SMOD | SPI_I2SCFGR_CHLEN |
                    (stm32_is_clock_master ? SPI_I2SCFGR_I2SCFG_1 : 0u);
    SPI2->CR2 = SPI_CR2_TXDMAEN;

    NVIC_SetPriority(DMA1_Stream4_IRQn, 6u);
    NVIC_EnableIRQ(DMA1_Stream4_IRQn);
    DMA1_Stream4->CR |= DMA_SxCR_EN;
    SPI2->I2SCFGR |= SPI_I2SCFGR_I2SE;
    app_status.i2s_dma_started = 1u;
    return true;
}

static void refill_half(uint32_t half)
{
    uint8_t *const destination = (uint8_t *)&audio_dma_buffer[half * HALF_SAMPLES];
    uint32_t bytes_read = 0u;
    if (app_status.streaming != 0u) {
        bytes_read = tud_audio_read(destination, HALF_BYTES);
    }
    if (bytes_read < HALF_BYTES) {
        memset(destination + bytes_read, 0, HALF_BYTES - bytes_read);
        if (app_status.streaming != 0u) {
            ++app_status.audio_underruns;
        }
    }
    app_status.audio_fifo_bytes = tud_audio_available();
}

void audio_i2s_service(void)
{
    uint32_t pending;
    __disable_irq();
    pending = refill_flags;
    refill_flags = 0u;
    __enable_irq();

    if ((pending & 1u) != 0u) {
        refill_half(0u);
    }
    if ((pending & 2u) != 0u) {
        refill_half(1u);
    }
}

void DMA1_Stream4_IRQHandler(void)
{
    const uint32_t status = DMA1->HISR;
    if ((status & DMA_HISR_HTIF4) != 0u) {
        DMA1->HIFCR = DMA_HIFCR_CHTIF4;
        refill_flags |= 1u;
        ++app_status.dma_half_irqs;
    }
    if ((status & DMA_HISR_TCIF4) != 0u) {
        DMA1->HIFCR = DMA_HIFCR_CTCIF4;
        refill_flags |= 2u;
        ++app_status.dma_full_irqs;
    }
    if ((status & (DMA_HISR_TEIF4 | DMA_HISR_DMEIF4 | DMA_HISR_FEIF4)) != 0u) {
        DMA1->HIFCR = DMA_HIFCR_CTEIF4 | DMA_HIFCR_CDMEIF4 |
                      DMA_HIFCR_CFEIF4;
    }
}
