#include "platform.h"

#include "stm32f407xx.h"

uint32_t SystemCoreClock = 16000000u;
static volatile uint32_t system_millis;

#define HSI_TIMEOUT 1000000u
#define HSE_TIMEOUT 500000u
#define PLL_TIMEOUT 1000000u
#define CLOCK_SWITCH_TIMEOUT 1000000u

/* Newlib calls these from __libc_init_array/__libc_fini_array. */
void _init(void) {}
void _fini(void) {}

void SystemInit(void)
{
    SCB->CPACR |= (3u << (10u * 2u)) | (3u << (11u * 2u));
    SCB->VTOR = FLASH_BASE;
}

uint32_t board_clock_init(void)
{
    uint32_t timeout;
    bool hse_ready;

    RCC->CR |= RCC_CR_HSION;
    timeout = HSI_TIMEOUT;
    while (((RCC->CR & RCC_CR_HSIRDY) == 0u) && (--timeout != 0u)) {
    }
    if (timeout == 0u) {
        return 0u;
    }

    RCC->CFGR = 0u;
    RCC->CR &= ~(RCC_CR_PLLON | RCC_CR_HSEON | RCC_CR_CSSON);
    timeout = PLL_TIMEOUT;
    while (((RCC->CR & RCC_CR_PLLRDY) != 0u) && (--timeout != 0u)) {
    }

    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_VOS;
    FLASH->ACR = FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_PRFTEN |
                 FLASH_ACR_LATENCY_5WS;

    RCC->CR |= RCC_CR_HSEON;
    /* A crystal normally starts within a few milliseconds. Keep a bounded
       fallback delay so a missing X3 does not postpone USB attach by seconds. */
    timeout = HSE_TIMEOUT;
    while (((RCC->CR & RCC_CR_HSERDY) == 0u) && (--timeout != 0u)) {
    }
    hse_ready = timeout != 0u;

    if (hse_ready) {
        /* HSE=8 MHz -> SYSCLK 168 MHz, USB clock 48 MHz. */
        RCC->PLLCFGR = 8u |
                       (336u << RCC_PLLCFGR_PLLN_Pos) |
                       RCC_PLLCFGR_PLLSRC_HSE |
                       (7u << RCC_PLLCFGR_PLLQ_Pos);
        SystemCoreClock = 168000000u;
    } else {
        /* The board can still be tested when its 8 MHz crystal is absent or
           not oscillating: HSI=16 MHz -> SYSCLK 144 MHz, USB nominal 48 MHz. */
        RCC->CR &= ~RCC_CR_HSEON;
        RCC->PLLCFGR = 16u |
                       (288u << RCC_PLLCFGR_PLLN_Pos) |
                       (6u << RCC_PLLCFGR_PLLQ_Pos);
        SystemCoreClock = 144000000u;
    }
    RCC->CFGR = RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV4 |
                RCC_CFGR_PPRE2_DIV2;
    RCC->CR |= RCC_CR_PLLON;
    timeout = PLL_TIMEOUT;
    while (((RCC->CR & RCC_CR_PLLRDY) == 0u) && (--timeout != 0u)) {
    }
    if (timeout == 0u) {
        return 0u;
    }

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    timeout = CLOCK_SWITCH_TIMEOUT;
    while (((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) &&
           (--timeout != 0u)) {
    }
    if (timeout == 0u) {
        return 0u;
    }

    if (SysTick_Config(SystemCoreClock / 1000u) != 0u) {
        return 0u;
    }
    NVIC_SetPriority(SysTick_IRQn, (1u << __NVIC_PRIO_BITS) - 1u);
    return hse_ready ? 1u : 2u;
}

void board_gpio_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    (void)RCC->AHB1ENR;

    /* LED4: PC0, active low. */
    GPIOC->BSRR = GPIO_BSRR_BS_0;
    GPIOC->MODER = (GPIOC->MODER & ~GPIO_MODER_MODER0) |
                   GPIO_MODER_MODER0_0;
    GPIOC->OTYPER &= ~GPIO_OTYPER_OT_0;
    GPIOC->OSPEEDR &= ~GPIO_OSPEEDER_OSPEEDR0;
    GPIOC->PUPDR &= ~GPIO_PUPDR_PUPDR0;
}

void board_usb_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    (void)RCC->AHB1ENR;

    /* PA11=USB_DM and PA12=USB_DP, alternate function 10. */
    GPIOA->MODER = (GPIOA->MODER &
                    ~(GPIO_MODER_MODER11 | GPIO_MODER_MODER12)) |
                   GPIO_MODER_MODER11_1 | GPIO_MODER_MODER12_1;
    GPIOA->OTYPER &= ~(GPIO_OTYPER_OT_11 | GPIO_OTYPER_OT_12);
    GPIOA->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR11 |
                      GPIO_OSPEEDER_OSPEEDR12;
    GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPDR11 | GPIO_PUPDR_PUPDR12);
    GPIOA->AFR[1] = (GPIOA->AFR[1] & ~((0xFu << 12u) | (0xFu << 16u))) |
                    (10u << 12u) | (10u << 16u);

    RCC->AHB2ENR |= RCC_AHB2ENR_OTGFSEN;
    (void)RCC->AHB2ENR;
    RCC->AHB2RSTR |= RCC_AHB2RSTR_OTGFSRST;
    RCC->AHB2RSTR &= ~RCC_AHB2RSTR_OTGFSRST;
    NVIC_SetPriority(OTG_FS_IRQn, 5u);
}

void board_delay_ms(uint32_t milliseconds)
{
    const uint32_t start = system_millis;
    while ((uint32_t)(system_millis - start) < milliseconds) {
        __NOP();
    }
}

uint32_t board_millis(void)
{
    return system_millis;
}

uint32_t tusb_time_millis_api(void)
{
    return system_millis;
}

void board_led_set(bool on)
{
    GPIOC->BSRR = on ? GPIO_BSRR_BR_0 : GPIO_BSRR_BS_0;
}

void SysTick_Handler(void)
{
    ++system_millis;
}
