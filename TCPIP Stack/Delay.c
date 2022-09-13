/*
 * Delay.c
 *
 *  Created on: Jul 27, 2022
 *      Author: eddie
 */
#include "TCPIP Stack/TCPIP.h"

#include "ch32v30x.h"

#define DELAY_TIMER_RCC     RCC_APB1Periph_TIM6
#define DELAY_TIMER         TIM6

#define TIMER_FREQ               (72000000ul * 2 / 16)

#define FACTOR                   (TIMER_FREQ / 1000000)

void DelayInit(void)
{
    RCC_APB1PeriphClockCmd(DELAY_TIMER_RCC, ENABLE);

    RCC_ClocksTypeDef RCC_Clocks;
    RCC_GetClocksFreq(&RCC_Clocks);

    if (RCC_Clocks.PCLK1_Frequency * 2 != TIMER_FREQ)
    {
        while (1);
    }
}

void DelayUs(UINT32 us)
{
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;

    TIM_TimeBaseStructure.TIM_Period = us * FACTOR - 1;
    TIM_TimeBaseStructure.TIM_Prescaler = 0;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(DELAY_TIMER, &TIM_TimeBaseStructure);

    DELAY_TIMER->CNT = 0;
    DELAY_TIMER->INTFR = (uint16_t)~TIM_IT_Update;
    DELAY_TIMER->CTLR1 |= TIM_CEN;

    while ((DELAY_TIMER->INTFR & TIM_IT_Update) == RESET);

    DELAY_TIMER->CTLR1 &= (uint16_t)(~((uint16_t)TIM_CEN));
}

void DelayMs(UINT32 ms)
{
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;

    TIM_TimeBaseStructure.TIM_Period = ms * FACTOR - 1;
    TIM_TimeBaseStructure.TIM_Prescaler = 1000 - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(DELAY_TIMER, &TIM_TimeBaseStructure);

    DELAY_TIMER->CNT = 0;
    DELAY_TIMER->INTFR = (uint16_t)~TIM_IT_Update;
    DELAY_TIMER->CTLR1 |= TIM_CEN;

    while ((DELAY_TIMER->INTFR & TIM_IT_Update) == RESET);

    DELAY_TIMER->CTLR1 &= (uint16_t)(~((uint16_t)TIM_CEN));
}
