/*********************************************************************
 *
 *                  MAC Module Defs for Microchip Stack
 *
 *********************************************************************
 * FileName:        MAC.h
 * Dependencies:    StackTsk.h
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
 * Nilesh Rajbharti     4/27/01     Original        (Rev 1.0)
 * Nilesh Rajbharti     11/27/01    Added SLIP
 * Nilesh Rajbharti     2/9/02      Cleanup
 * Nilesh Rajbharti     5/22/02     Rev 2.0 (See version.log for detail)
 * Howard Schlunder     6/28/04     Added ENC28J60 specific features
 * Howard Schlunder		11/29/04	Added Get/SetLEDConfig macros
 ********************************************************************/
#ifndef __MAC_H
#define __MAC_H

#include "TCPIP Stack/ENC28J60.h"

#define MAC_TX_BUFFER_SIZE			(1500ul)

// A generic structure representing the Ethernet header starting all Ethernet 
// frames
typedef struct  __attribute__((aligned(2), packed))
{
	MAC_ADDR        DestMACAddr;
	MAC_ADDR        SourceMACAddr;
	WORD_VAL        Type;
} ETHER_HEADER;


#define MAC_IP      	(0x00u)
#define MAC_ARP     	(0x06u)
#define MAC_UNKNOWN 	(0xFFu)

/*
 * Microchip Ethernet controller specific MAC items
 */
#if !defined(STACK_USE_HTTP2_SERVER)
	#define RESERVED_HTTP_MEMORY 0ul
#endif

#if !defined(STACK_USE_SSL)
	#define RESERVED_SSL_MEMORY 0ul
#endif

#define BASE_TX_ADDR    0

#define BASE_TCB_ADDR	(BASE_TX_ADDR + (1514ul+7ul))
#define BASE_HTTPB_ADDR (BASE_TCB_ADDR + TCP_PIC_RAM_SIZE)
#define BASE_SSLB_ADDR	(BASE_HTTPB_ADDR + RESERVED_HTTP_MEMORY)


WORD MACCalcRxChecksum(WORD offset, WORD len);
WORD CalcIPBufferChecksum(WORD len);

void MACInit(void);
BOOL MACIsLinked(void);

BOOL MACGetHeader(MAC_ADDR *remote, BYTE* type);
void MACSetReadPtrInRx(WORD offset);
WORD MACSetWritePtr(WORD address);
WORD MACSetReadPtr(WORD address);
BOOL MACSetReadPtrToRx(BOOL toRx);

BYTE MACGet(void);
WORD MACGetArray(BYTE *val, WORD len);
void MACDiscardRx(void);
WORD MACGetFreeRxSize(void);
void MACMemCopyAsync(DWORD destAddr, DWORD sourceAddr, WORD len);
BOOL MACIsMemCopyDone(void);

void MACPutHeader(MAC_ADDR *remote, BYTE type, WORD dataLen);
BOOL MACIsTxReady(void);
void MACPut(BYTE val);
void MACPutArray(BYTE *val, WORD len);
void MACFlush(void);

BOOL MACIsDataTransceiving(void);
void MACSetDataTransceiving(BOOL transceiving);

// ROM function variants for PIC18
#if defined(__18CXX)
	void MACPutROMArray(ROM BYTE *val, WORD len);
#else
	#define MACPutROMArray(a,b)	MACPutArray((BYTE*)a,b)
#endif

#endif
