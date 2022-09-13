/*********************************************************************
 *
 *                  Tick Manager for Timekeeping
 *
 *********************************************************************
 * FileName:        Tick.c
 * Dependencies:    Timer 0 (PIC18) or Timer 1 (PIC24F, PIC24H, 
 *					dsPIC30F, dsPIC33F, PIC32)
 * Processor:       PIC18, PIC24F, PIC24H, dsPIC30F, dsPIC33F, PIC32
 * Compiler:        Microchip C32 v1.00 or higher
 *					Microchip C30 v3.01 or higher
 *					Microchip C18 v3.13 or higher
 *					HI-TECH PICC-18 STD 9.50PL3 or higher
 * Company:         Microchip Technology, Inc.
 *
 * Software License Agreement
 *
 * Copyright (C) 2002-2008 Microchip Technology Inc.  All rights 
 * reserved.
 *
 * Microchip licenses to you the right to use, modify, copy, and 
 * distribute: 
 * (i)  the Software when embedded on a Microchip microcontroller or 
 *      digital signal controller product ("Device") which is 
 *      integrated into Licensee's product; or
 * (ii) ONLY the Software driver source files ENC28J60.c and 
 *      ENC28J60.h ported to a non-Microchip device used in 
 *      conjunction with a Microchip ethernet controller for the 
 *      sole purpose of interfacing with the ethernet controller. 
 *
 * You should refer to the license agreement accompanying this 
 * Software for additional information regarding your rights and 
 * obligations.
 *
 * THE SOFTWARE AND DOCUMENTATION ARE PROVIDED "AS IS" WITHOUT 
 * WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT 
 * LIMITATION, ANY WARRANTY OF MERCHANTABILITY, FITNESS FOR A 
 * PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT SHALL 
 * MICROCHIP BE LIABLE FOR ANY INCIDENTAL, SPECIAL, INDIRECT OR 
 * CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF 
 * PROCUREMENT OF SUBSTITUTE GOODS, TECHNOLOGY OR SERVICES, ANY CLAIMS 
 * BY THIRD PARTIES (INCLUDING BUT NOT LIMITED TO ANY DEFENSE 
 * THEREOF), ANY CLAIMS FOR INDEMNITY OR CONTRIBUTION, OR OTHER 
 * SIMILAR COSTS, WHETHER ASSERTED ON THE BASIS OF CONTRACT, TORT 
 * (INCLUDING NEGLIGENCE), BREACH OF WARRANTY, OR OTHERWISE.
 *
 *
 * Author               Date        Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Nilesh Rajbharti     6/28/01     Original        (Rev 1.0)
 * Nilesh Rajbharti     2/9/02      Cleanup
 * Nilesh Rajbharti     5/22/02     Rev 2.0 (See version.log for detail)
 * Howard Schlunder		6/13/07		Changed to use timer without 
 *									writing for perfect accuracy.
********************************************************************/
#define __TICK_C

#include "TCPIP Stack/TCPIP.h"

#include "ch32v30x.h"

// Internal counter to store Ticks.
static DWORD dwInternalTicks = 0;

// 6-byte value to store Ticks.  Allows for use over longer periods of time.
static BYTE vTickReading[6];

static void GetTickCopy(void);

#define TICK_TIMER_RCC     RCC_APB1Periph_TIM5
#define TICK_TIMER         TIM5
#define TICK_TIMER_IRQ     TIM5_IRQn
#define TICK_TIMER_ISR     TIM5_IRQHandler

/*****************************************************************************
  Function:
	void TickInit(void)

  Summary:
	Initializes the Tick manager module.

  Description:
	Configures the Tick module and any necessary hardware resources.

  Precondition:
	None

  Parameters:
	None

  Returns:
  	None
  	
  Remarks:
	This function is called only one during lifetime of the application.
  ***************************************************************************/
void TickInit(void)
{
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(TICK_TIMER_RCC, ENABLE);

    TIM_TimeBaseStructure.TIM_Period = 0x10000ul - 1;
    TIM_TimeBaseStructure.TIM_Prescaler = 256 - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TICK_TIMER, &TIM_TimeBaseStructure);

    TIM_ITConfig(TICK_TIMER, TIM_IT_Update, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = TICK_TIMER_IRQ;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0xff;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    TIM_SetCounter(TICK_TIMER, 0);

    TIM_Cmd(TICK_TIMER, ENABLE);

    dwInternalTicks = 0;
}

/*****************************************************************************
  Function:
	static void GetTickCopy(void)

  Summary:
	Reads the tick value.

  Description:
	This function performs an interrupt-safe and synchronized read of the 
	48-bit Tick value.

  Precondition:
	None

  Parameters:
	None

  Returns:
  	None
  ***************************************************************************/
static void GetTickCopy(void)
{
	// Perform an Interrupt safe and synchronized read of the 48-bit 
	// tick value
    do
    {
        DWORD dwTempTicks;

        NVIC_EnableIRQ(TICK_TIMER_IRQ);          // Enable interrupt
        __NOP();
        NVIC_DisableIRQ(TICK_TIMER_IRQ);          // Disable interrupt

        // Get low 2 bytes
        ((WORD*)vTickReading)[0] = TICK_TIMER->CNT;

        dwTempTicks = dwInternalTicks;

        // Get high 4 bytes
        vTickReading[2] = ((BYTE*)&dwTempTicks)[0];
        vTickReading[3] = ((BYTE*)&dwTempTicks)[1];
        vTickReading[4] = ((BYTE*)&dwTempTicks)[2];
        vTickReading[5] = ((BYTE*)&dwTempTicks)[3];
    } while(TICK_TIMER->INTFR & TIM_IT_Update);
    NVIC_EnableIRQ(TICK_TIMER_IRQ);              // Enable interrupt
}


/*****************************************************************************
  Function:
	DWORD TickGet(void)

  Summary:
	Obtains the current Tick value.

  Description:
	This function retrieves the current Tick value, allowing timing and
	measurement code to be written in a non-blocking fashion.  This function
	retrieves the least significant 32 bits of the internal tick counter, 
	and is useful for measuring time increments ranging from a few 
	microseconds to a few hours.  Use TickGetDiv256 or TickGetDiv64K for
	longer periods of time.

  Precondition:
	None

  Parameters:
	None

  Returns:
  	Lower 32 bits of the current Tick value.
  ***************************************************************************/
DWORD TickGet(void)
{
	GetTickCopy();

	return *((DWORD*)&vTickReading[0]);
}

/*****************************************************************************
  Function:
	DWORD TickGetDiv256(void)

  Summary:
	Obtains the current Tick value divided by 256.

  Description:
	This function retrieves the current Tick value, allowing timing and
	measurement code to be written in a non-blocking fashion.  This function
	retrieves the middle 32 bits of the internal tick counter, 
	and is useful for measuring time increments ranging from a few 
	minutes to a few weeks.  Use TickGet for shorter periods or TickGetDiv64K
	for longer ones.

  Precondition:
	None

  Parameters:
	None

  Returns:
  	Middle 32 bits of the current Tick value.
  ***************************************************************************/
DWORD TickGetDiv256(void)
{
    DWORD_VAL ret;

    GetTickCopy();
    ret.v[0] = vTickReading[1]; // Note: This copy must be done one
    ret.v[1] = vTickReading[2]; // byte at a time to prevent misaligned
    ret.v[2] = vTickReading[3]; // memory reads, which will reset the PIC.
    ret.v[3] = vTickReading[4];

    return ret.Val;
}

/*****************************************************************************
  Function:
	DWORD TickGetDiv64K(void)

  Summary:
	Obtains the current Tick value divided by 64K.

  Description:
	This function retrieves the current Tick value, allowing timing and
	measurement code to be written in a non-blocking fashion.  This function
	retrieves the most significant 32 bits of the internal tick counter, 
	and is useful for measuring time increments ranging from a few 
	days to a few years, or for absolute time measurements.  Use TickGet or
	TickGetDiv256 for shorter periods of time.

  Precondition:
	None

  Parameters:
	None

  Returns:
  	Upper 32 bits of the current Tick value.
  ***************************************************************************/
DWORD TickGetDiv64K(void)
{
    DWORD_VAL ret;

    GetTickCopy();
    ret.v[0] = vTickReading[2]; // Note: This copy must be done one
    ret.v[1] = vTickReading[3]; // byte at a time to prevent misaligned
    ret.v[2] = vTickReading[4]; // memory reads, which will reset the PIC.
    ret.v[3] = vTickReading[5];

    return ret.Val;
}


/*****************************************************************************
  Function:
	DWORD TickConvertToMilliseconds(DWORD dwTickValue)

  Summary:
	Converts a Tick value or difference to milliseconds.

  Description:
	This function converts a Tick value or difference to milliseconds.  For
	example, TickConvertToMilliseconds(32768) returns 1000 when a 32.768kHz 
	clock with no prescaler drives the Tick module interrupt.

  Precondition:
	None

  Parameters:
	dwTickValue	- Value to convert to milliseconds

  Returns:
  	Input value expressed in milliseconds.

  Remarks:
	This function performs division on DWORDs, which is slow.  Avoid using
	it unless you absolutely must (such as displaying data to a user).  For
	timeout comparisons, compare the current value to a multiple or fraction 
	of TICK_SECOND, which will be calculated only once at compile time.
  ***************************************************************************/
DWORD TickConvertToMilliseconds(DWORD dwTickValue)
{
	return (dwTickValue+(TICKS_PER_SECOND/2000ul))/((DWORD)(TICKS_PER_SECOND/1000ul));
}


/*****************************************************************************
  Function:
	void TickUpdate(void)

  Description:
	Updates the tick value when an interrupt occurs.

  Precondition:
	None

  Parameters:
	None

  Returns:
  	None
  ***************************************************************************/

void TICK_TIMER_ISR(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void TICK_TIMER_ISR(void)
{
    if (TIM_GetITStatus(TICK_TIMER, TIM_IT_Update) != RESET)
    {
        dwInternalTicks++;

        TIM_ClearITPendingBit(TICK_TIMER, TIM_IT_Update);
    }
}
