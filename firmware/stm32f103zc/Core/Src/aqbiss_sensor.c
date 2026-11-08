#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_usart.h"
#include "misc.h"

#include "console.h"
#include "aqbiss_sensor.h"

#define BISS_MA_PORT GPIOA
#define BISS_MA_PIN  GPIO_Pin_0
#define BISS_SL_PORT GPIOA
#define BISS_SL_PIN  GPIO_Pin_1

static void delay_us(uint16_t us)
{
    uint16_t start = TIM_GetCounter(TIM2);
    while ((uint16_t)(TIM_GetCounter(TIM2) - start) < us) {
        /* 1 MHz timer tick. */
    }
}

static void clock_and_sample(uint8_t *bit)
{
    GPIO_SetBits(BISS_MA_PORT, BISS_MA_PIN);
    delay_us(1u);
    *bit = GPIO_ReadInputDataBit(BISS_SL_PORT, BISS_SL_PIN) ? 1u : 0u;
    GPIO_ResetBits(BISS_MA_PORT, BISS_MA_PIN);
    delay_us(1u);
}

void aqbiss_init(void)
{
    GPIO_InitTypeDef gpio;
    TIM_TimeBaseInitTypeDef tim;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = BISS_MA_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BISS_MA_PORT, &gpio);
    GPIO_ResetBits(BISS_MA_PORT, BISS_MA_PIN);

    gpio.GPIO_Pin = BISS_SL_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(BISS_SL_PORT, &gpio);

    TIM_TimeBaseStructInit(&tim);
    tim.TIM_Prescaler = 71u;
    tim.TIM_CounterMode = TIM_CounterMode_Up;
    tim.TIM_Period = 0xFFFFu;
    tim.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TIM2, &tim);
    TIM_SetCounter(TIM2, 0u);
    TIM_Cmd(TIM2, ENABLE);
}

static uint8_t crc5_compute(uint32_t value17)
{
    uint8_t crc = BISS_CRC_INIT & 0x1Fu;
    uint8_t i;
    for (i = 0u; i < 17u; ++i) {
        uint8_t bit = (uint8_t)((value17 >> (16u - i)) & 1u);
        uint8_t feedback = ((crc >> 4) ^ bit) & 1u;
        crc = (uint8_t)((crc << 1) & 0x1Fu);
        if (feedback) crc ^= BISS_CRC_POLY;
    }
    return (uint8_t)(crc ^ 0x1Fu);
}

int aqbiss_read(aqbiss_frame_t *frame)
{
    uint8_t bits[AQBISS_FRAME_BITS];
    uint8_t i;
    uint32_t data17 = 0u;
    uint8_t received_crc = 0u;

    if (frame == NULL) return -1;

    /* Sensor mode frame: Start + 15 position bits + E2 + CRC5 + M + Stop */
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

    received_crc = 0u;
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
