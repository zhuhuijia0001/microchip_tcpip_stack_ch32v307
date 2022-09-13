/*********************************************************************
 *
 *     UART access routines for C18 and C30
 *
 *********************************************************************
 * FileName:        UART.h
 * Processor:       PIC18, PIC24F, PIC24H, dsPIC30F, dsPIC33F
 * Compiler:        Microchip C30 v3.01 or higher
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
 * Author               Date   		Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Howard Schlunder		6/16/06		Original
********************************************************************/
#ifndef __UART_H
#define __UART_H

#include "Compiler.h"
#include "HardwareProfile.h"


#if defined(__18CXX)	// PIC18
	char BusyUSART(void);
	void CloseUSART(void);
	char DataRdyUSART(void);
	char ReadUSART(void);
	void WriteUSART(char data);
	void getsUSART(char *buffer, unsigned char len);
	void putsUSART(char *data);
	void putrsUSART(const rom char *data);
	
#elif defined(__C30__) // PIC24F, PIC24H, dsPIC30, dsPIC33
	void putsUART2(unsigned int *buffer);
	unsigned int getsUART2(unsigned int length,unsigned int *buffer,
	                       unsigned int uart_data_wait);
	char DataRdyUART2(void);
	char BusyUART2(void);
	unsigned int ReadUART2(void);
	void WriteUART2(unsigned int data);
#endif


#endif
