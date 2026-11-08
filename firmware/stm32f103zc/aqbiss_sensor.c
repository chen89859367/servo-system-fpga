#include "aqbiss_sensor.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_tim.h"
#include "misc.h"

/* AqBiSS MA -> external SP490 driver input; SL <- SP490 receiver output. */
#define BISS_MA_PORT GPIOA
#define BISS_MA_PIN  GPIO_Pin_0
#define BISS_SL_PORT GPIOA
#define BISS_SL_PIN  GPIO_Pin_1
#define BISS_HALF_PERIOD_US 1u

static void delay_us(uint16_t us)
{
    uint16_t start = (uint16_t)TIM_GetCounter(TIM2);
    while ((uint16_t)((uint16_t)TIM_GetCounter(TIM2) - start) < us) {
        /* TIM2 configured as a 1 MHz free-running counter. */
    }
}

static uint8_t read_sl(void)
{
    return GPIO_ReadInputDataBit(BISS_SL_PORT, BISS_SL_PIN) ? 1u : 0u;
}

static void clock_and_sample(uint8_t *bit)
{
    GPIO_SetBits(BISS_MA_PORT, BISS_MA_PIN);
    delay_us(BISS_HALF_PERIOD_US);
    *bit = read_sl(); /* Sample while MA is high; verify against UG206 timing. */
    GPIO_ResetBits(BISS_MA_PORT, BISS_MA_PIN);
    delay_us(BISS_HALF_PERIOD_US);
}

void aqbiss_init(void)
{
    GPIO_InitTypeDef gpio;
    TIM_TimeBaseInitTypeDef tim;
    RCC_ClocksTypeDef clocks;
    uint32_t tim_clock;
    uint16_t prescaler;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = BISS_MA_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(BISS_MA_PORT, &gpio);
    GPIO_ResetBits(BISS_MA_PORT, BISS_MA_PIN);

    gpio.GPIO_Pin = BISS_SL_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(BISS_SL_PORT, &gpio);

    RCC_GetClocksFreq(&clocks);
    tim_clock = clocks.PCLK1_Frequency;
    /* STM32F1 timer clocks double when the APB prescaler is not 1. */
    if (clocks.HCLK_Frequency != clocks.PCLK1_Frequency) {
        tim_clock *= 2u;
    }
    if (tim_clock < 1000000u) {
        /* Invalid clock setup; keep a safe nonzero prescaler. */
        prescaler = 71u;
    } else {
        prescaler = (uint16_t)(tim_clock / 1000000u - 1u);
    }

    TIM_TimeBaseStructInit(&tim);
    tim.TIM_Prescaler = prescaler;
    tim.TIM_CounterMode = TIM_CounterMode_Up;
    tim.TIM_Period = 0xFFFFu;
    tim.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TIM2, &tim);
    TIM_SetCounter(TIM2, 0u);
    TIM_Cmd(TIM2, ENABLE);
}

static uint8_t crc5_compute(uint32_t value17)
{
    uint8_t crc = (uint8_t)(BISS_CRC_INIT & 0x1Fu);
    int8_t i;

    /* Position (15 bits) followed by E[1:0], MSB first. */
    for (i = 16; i >= 0; --i) {
        uint8_t feedback = (uint8_t)(((crc >> 4) ^ (value17 >> i)) & 1u);
        crc = (uint8_t)((crc << 1) & 0x1Fu);
        if (feedback != 0u) {
            crc ^= BISS_CRC_POLY;
        }
    }
    return (uint8_t)(crc ^ 0x1Fu); /* AqBiSS transmitted CRC is inverted per supplied spec. */
}

int aqbiss_read(aqbiss_frame_t *frame)
{
    uint8_t bits[AQBISS_FRAME_BITS];
    uint32_t data17 = 0u;
    uint8_t i;
    uint8_t received_crc = 0u;

    if (frame == 0) {
        return -1;
    }

    /* Idle low before the frame; each clock samples the next serial bit. */
    GPIO_ResetBits(BISS_MA_PORT, BISS_MA_PIN);
    delay_us(2u);
    for (i = 0u; i < AQBISS_FRAME_BITS; ++i) {
        clock_and_sample(&bits[i]);
    }
    GPIO_SetBits(BISS_MA_PORT, BISS_MA_PIN);

    frame->start = bits[0];
    frame->position = 0u;
    for (i = 1u; i <= 15u; ++i) {
        frame->position = (uint16_t)((frame->position << 1) | bits[i]);
        data17 = (data17 << 1) | bits[i];
    }
    frame->error_bits = (uint8_t)((bits[16] << 1) | bits[17]);
    data17 = (data17 << 2) | frame->error_bits;
    for (i = 18u; i <= 22u; ++i) {
        received_crc = (uint8_t)((received_crc << 1) | bits[i]);
    }
    frame->crc = received_crc;
    frame->m_bit = bits[23];
    frame->stop = bits[24];
    frame->crc_expected = crc5_compute(data17);
    frame->crc_ok = (frame->crc == frame->crc_expected) ? 1u : 0u;
    return 0;
}
