/*********************************************************************
 *
 *  Internet Control Message Protocol (ICMP) Server
 *  Module for Microchip TCP/IP Stack
 *   -Provides "ping" diagnostics
 *	 -Reference: RFC 792
 *
 *********************************************************************
 * FileName:        ICMP.c
 * Dependencies:    IP, ARP
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
 * Author               Date    	Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Howard Schlunder		03/16/07	Original
 ********************************************************************/
#define __ICMP_C

#include "TCPIP Stack/TCPIP.h"

#if defined(STACK_USE_ICMP_SERVER) || defined(STACK_USE_ICMP_CLIENT)

#if defined(STACK_USE_ICMP_CLIENT)

#define ICMP_TIMEOUT	(4ul*TICK_SECOND)

typedef struct _ICMP_HEADER
{
	BYTE vType;
	BYTE vCode;
	WORD_VAL wvChecksum;
	WORD_VAL wvIdentifier;
	WORD_VAL wvSequenceNumber;
} ICMP_HEADER;

static ICMP_HEADER ICMPHeader;
static TICK ICMPTimer;
static struct
{
	unsigned char bICMPInUse:1;
	unsigned char bReplyValid:1;
} ICMPFlags = {0x00};
static NODE_INFO ICMPRemote;
static enum
{
	SM_IDLE = 0,
	SM_ARP_RESOLVE,
	SM_GET_ECHO
} ICMPState;
#endif	//#if defined(STACK_USE_ICMP_CLIENT)

/*********************************************************************
 * Function:        void ICMPProcess(void)
 *
 * PreCondition:    MAC buffer contains ICMP type packet.
 *
 * Input:           *remote: Pointer to a NODE_INFO structure of the 
 *					ping requester
 *					len: Count of how many bytes the ping header and 
 *					payload are in this IP packet
 *
 * Output:          Generates an echo reply, if requested
 *					Validates and sets ICMPFlags.bReplyValid if a 
 *					correct ping response to one of ours is received.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 ********************************************************************/
void ICMPProcess(NODE_INFO *remote, WORD len)
{
	DWORD_VAL dwVal;

    // Obtain the ICMP header Type, Code, and Checksum fields
    MACGetArray((BYTE*)&dwVal, sizeof(dwVal));
	
	// See if this is an ICMP echo (ping) request
	if(dwVal.w[0] == 0x0008u)
	{
		// Validate the checksum using the Microchip MAC's DMA module
		// The checksum data includes the precomputed checksum in the 
		// header, so a valid packet will always have a checksum of 
		// 0x0000 if the packet is not disturbed.
		if(MACCalcRxChecksum(0+sizeof(IP_HEADER), len))
			return;
	
		// Calculate new Type, Code, and Checksum values
		dwVal.v[0] = 0x00;	// Type: 0 (ICMP echo/ping reply)
		dwVal.v[2] += 8;	// Subtract 0x0800 from the checksum
		if(dwVal.v[2] < 8u)
		{
			dwVal.v[3]++;
			if(dwVal.v[3] == 0u)
				dwVal.v[2]++;
		}
	
		// Position the write pointer for the next IPPutHeader operation
	    MACSetWritePtr(BASE_TX_ADDR + sizeof(ETHER_HEADER));
	
	    // Wait for TX hardware to become available (finish transmitting 
	    // any previous packet)
	    while(!IPIsTxReady());
	
		// Create IP header in TX memory
		IPPutHeader(remote, IP_PROT_ICMP, len);
	
		// Copy ICMP response into the TX memory
		MACPutArray((BYTE*)&dwVal, sizeof(dwVal));
		MACMemCopyAsync(-1, -1, len-4);
		while(!MACIsMemCopyDone());
	
		// Transmit the echo reply packet
	    MACFlush();
	}
#if defined(STACK_USE_ICMP_CLIENT)
	else if(dwVal.w[0] == 0x0000u)	// See if this an ICMP Echo reply to our request
	{
		// Get the sequence number and identifier fields
		MACGetArray((BYTE*)&dwVal, sizeof(dwVal));
		
		// See if the identifier matches the one we sent
		if(dwVal.w[0] != 0xEFBE)	
			return;
	
		if(dwVal.w[1] != ICMPHeader.wvSequenceNumber.Val)
			return;

		// Validate the ICMP checksum field
	    IPSetRxBuffer(0);
	    BYTE rxToRxSave = MACSetReadPtrToRx(FALSE);
		if(CalcIPBufferChecksum(sizeof(ICMP_HEADER) + 2))	// Two bytes of payload were sent in the echo request
		{
		    MACSetReadPtrToRx(rxToRxSave);

		    return;
		}
		
		MACSetReadPtrToRx(rxToRxSave);

		// Flag that we received the response and stop the timer ticking
		ICMPFlags.bReplyValid = 1;
		ICMPTimer = TickGet() - ICMPTimer;
	}
#endif
}

/*********************************************************************
 * Function:        void ICMPSendPing(DWORD dwRemoteIP)
 *
 * PreCondition:    ICMPBeginUsage() returned TRUE
 *
 * Input:           dwRemoteIP: IP Address to ping.  Must be stored 
 *								big endian.  Ex: 192.168.0.1 should be
 *								passed as 0xC0A80001.
 *
 * Output:          Begins the process of transmitting an ICMP echo 
 *					request.  This normally involves an ARP 
 *					resolution procedure first.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 ********************************************************************/
#if defined(STACK_USE_ICMP_CLIENT)
void ICMPSendPing(DWORD dwRemoteIP)
{
	// Figure out the MAC address to send to
	ICMPRemote.IPAddr.Val = dwRemoteIP;
	ARPResolve(&ICMPRemote.IPAddr);

	// Set up the ping packet
	ICMPHeader.vType = 0x08;	// 0x08: Echo (ping) request
	ICMPHeader.vCode = 0x00;
	ICMPHeader.wvChecksum.Val = 0x0000;
	ICMPHeader.wvIdentifier.Val = 0xEFBE;
	ICMPHeader.wvSequenceNumber.Val++;
	ICMPHeader.wvChecksum.Val = CalcIPChecksum((BYTE*)&ICMPHeader, sizeof(ICMPHeader));

	// Kick off the ICMPGetReply() state machine
	ICMPTimer = TickGet();
	ICMPFlags.bReplyValid = 0;
	ICMPState = SM_ARP_RESOLVE;
}


/*********************************************************************
 * Function:        LONG ICMPGetReply(void)
 *
 * PreCondition:    ICMPBeginUsage() returned TRUE and ICMPSendPing() 
 *					was called
 *
 * Input:           None
 *
 * Output:          -2: No response received yet
 *					-1: Operation timed out (longer than ICMP_TIMEOUT) 
 *						has elapsed.
 *					>=0: Number of TICKs that elapsed between 
 *						 initial ICMP transmission and reception of 
 *						 a valid echo.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 ********************************************************************/
LONG ICMPGetReply(void)
{
	switch(ICMPState)
	{
		case SM_ARP_RESOLVE:
			// See if the ARP reponse was successfully received
			if(ARPIsResolved(&ICMPRemote.IPAddr, &ICMPRemote.MACAddr))
			{
				// Position the write pointer for the next IPPutHeader operation
			    MACSetWritePtr(BASE_TX_ADDR + sizeof(ETHER_HEADER));
			
			    // Wait for TX hardware to become available (finish transmitting 
			    // any previous packet)
			    while(!IPIsTxReady());
			
				// Create IP header in TX memory
				IPPutHeader(&ICMPRemote, IP_PROT_ICMP, sizeof(ICMP_HEADER) + 2);
				MACPutArray((BYTE*)&ICMPHeader, sizeof(ICMPHeader));
				MACPut(0x00);	// Send two dummy bytes as ping payload 
				MACPut(0x00);	// (needed for compatibility with some buggy NAT routers)
				MACFlush();

				// MAC Address resolved and echo sent, advance state
				ICMPState = SM_GET_ECHO;
				return -2;
			}

			// See if the ARP/echo request timed out
			if(TickGet() - ICMPTimer > ICMP_TIMEOUT)
			{
				ICMPState = SM_IDLE;
				return -1;
			}

			// No ARP response back yet
			return -2;

		case SM_GET_ECHO:
			// See if the echo was successfully received
			if(ICMPFlags.bReplyValid)
			{
				return (LONG)ICMPTimer;
			}
		
			// See if the ARP/echo request timed out
			if(TickGet() - ICMPTimer > ICMP_TIMEOUT)
			{
				ICMPState = SM_IDLE;
				return -1;
			}
		
			// No echo response back yet
			return -2;
		
		// SM_IDLE or illegal/impossible state:
		default:
			return -1;
	}
}


/*********************************************************************
 * Function:        BOOL ICMPBeginUsage(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          TRUE: You have successfully gained ownership of 
 *						  the ICMP client module and can now use the 
 *						  ICMPSendPing() and ICMPGetReply() functions.
 *					FALSE: Some other application is using the ICMP 
 *						   client module.  Calling ICMPSendPing() 
 *						   will corrupt the other application's ping 
 *						   result.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 ********************************************************************/
BOOL ICMPBeginUsage(void)
{
	if(ICMPFlags.bICMPInUse)
		return FALSE;

	ICMPFlags.bICMPInUse = TRUE;
	return TRUE;
}


/*********************************************************************
 * Function:        void ICMPEndUsage(void)
 *
 * PreCondition:    ICMPBeginUsage() was called by you and it 
 *					returned TRUE.
 *
 * Input:           None
 *
 * Output:          Your ownership of the ICMP module is released.  
 *					You can no longer use ICMPSendPing().
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 ********************************************************************/
void ICMPEndUsage(void)
{
	ICMPFlags.bICMPInUse = FALSE;
}
#endif //#if defined(STACK_USE_ICMP_CLIENT)

#endif //#if defined(STACK_USE_ICMP_SERVER) || defined(STACK_USE_ICMP_CLIENT)
