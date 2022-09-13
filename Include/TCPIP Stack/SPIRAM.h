/*********************************************************************
 *
 * Data SPI RAM Access Routines
 *  -Tested with AMI Semiconductor N256S0830HDA
 *
 *********************************************************************
 * FileName:        SPIRAM.h
 * Dependencies:    Compiler.h
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
 * \file SPIRAM.h
 * \author Howard Henry Schlunder
 * \date 25 July 2007
********************************************************************/
#ifndef __SPIRAM_H
#define __SPIRAM_H

void SPIRAMInit(void);
void SPIRAMGetArray(WORD wAddress, BYTE *vData, WORD wLength);
void SPIRAMPutArray(WORD wAddress, BYTE *vData, WORD wLength);

#define SPIRAMPutString(a,b)			SPIRAMPutArray(a, strlen((char*)b))

#if defined(__18CXX)
	void SPIRAMPutROMArray(WORD wAddress, ROM BYTE *vData, WORD wLength);
	#define SPIRAMPutROMString(a,b)		SPIRAMPutROMArray(a, strlenpgm((ROM char*)b))
#else
	#define SPIRAMPutROMString(a,b)		SPIRAMPutArray(a, strlen((char*)b))
	#define SPIRAMPutROMArray(a,b,c)	SPIRAMPutROMArray(a, b, c)
#endif


#endif
