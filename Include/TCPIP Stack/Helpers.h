/*********************************************************************
 *
 *                  Helper Function Defs for Microchip TCP/IP Stack
 *
 *********************************************************************
 * FileName:        Helpers.h
 * Dependencies:    stacktsk.h
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
 * Author               Date    Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Nilesh Rajbharti     5/17/01 Original        (Rev 1.0)
 * Nilesh Rajbharti     2/9/02  Cleanup
 ********************************************************************/
#ifndef __HELPERS_H
#define __HELPERS_H


#if !defined(__18CXX) || defined(HI_TECH_C)
	char *strupr(char* s);
	void ultoa(DWORD Value, BYTE* Buffer);
#endif

#if defined(DEBUG)
	#define DebugPrint(a)	{putrsUART(a);}
#else
	#define DebugPrint(a)
#endif

DWORD	GenerateRandomDWORD(void);
void 	uitoa(WORD Value, BYTE* Buffer);
void 	UnencodeURL(BYTE* URL);
WORD 	Base64Decode(BYTE* cSourceData, WORD wSourceLen, BYTE* cDestData, WORD wDestLen);
WORD	Base64Encode(BYTE* cSourceData, WORD wSourceLen, BYTE* cDestData, WORD wDestLen);
BOOL	StringToIPAddress(BYTE* str, IP_ADDR* IPAddress);
BYTE 	ReadStringUART(BYTE* Dest, BYTE BufferLen);
BYTE	hexatob(WORD_VAL AsciiChars);
BYTE	btohexa_high(BYTE b);
BYTE	btohexa_low(BYTE b);
signed char stricmppgm2ram(BYTE* a, ROM BYTE* b);

#if defined(__18CXX)
	BOOL	ROMStringToIPAddress(ROM BYTE* str, IP_ADDR* IPAddress);
#else
	// Non-ROM variant for C30 and C32
	#define ROMStringToIPAddress(a,b)	StringToIPAddress((BYTE*)a,b)
#endif


WORD    swaps(WORD v);
DWORD   swapl(DWORD v);

WORD    CalcIPChecksum(BYTE* buffer, WORD len);
WORD    CalcIPBufferChecksum(WORD len);

#if defined(__18CXX)
	DWORD leftRotateDWORD(DWORD val, BYTE bits);
#else
	// Rotations are more efficient in C30 and C32
	#define leftRotateDWORD(x, n) (((x) << (n)) | ((x) >> (32-(n))))
#endif

void FormatNetBIOSName(BYTE Name[16]);

#endif
