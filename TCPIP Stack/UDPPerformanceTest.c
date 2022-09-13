/*********************************************************************
 *
 *	UDP Performance Test
 *  Module for Microchip TCP/IP Stack
 *	 -Sends out dummy packets from ROM memory
 *	 -Reference: None.  This is for testing only.
 *
 *********************************************************************
 * FileName:        UDPPerformanceTest.c
 * Dependencies:    UDP, Ethernet (ENC28J60.c or ETH97J60.c)
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
 * Howard Schlunder     01/29/07	Original
 ********************************************************************/
#define __UDPPERFORMANCETEST_C

#include "TCPIP Stack/TCPIP.h"
#if defined(STACK_USE_UDP_PERFORMANCE_TEST)

// Which UDP port to broadcast from for the UDP tests
#define PERFORMANCE_PORT	8888


/*****************************************************************************
  Function:
	void UDPPerformanceTask(void)

  Summary:
	Tests the transmit performance of the UDP module.

  Description:
	This function tests the transmit performance of the UDP module.  At boot,
	this module will transmit 1024 large UDP broadcast packets of 1024 bytes
	each.  Using a packet sniffer, one can determine how long this process 
	takes and calculate the transmit rate of the stack.  This function tests 
	true UDP performance in that it will open a socket, transmit one packet, 
	and close the socket for each loop.  After this initial transmission, the
	module can be re-enabled by holding button 3.
	
	This function is particularly useful after development to determine the
	impact of your application code on the stack's performance.  A before and
	after comparison will indicate if your application is unacceptably
	blocking the processor or taking too long to execute.

  Precondition:
	UDP is initialized.

  Parameters:
	None

  Returns:
	None
  ***************************************************************************/
static TICK tick = 0;

void UDPPerformanceTask(void)
{
	UDP_SOCKET	MySocket;
	NODE_INFO	Remote;

	DWORD dwCounter = 1;

	if (TickGet() - tick < TICK_SECOND)
	    return;

	tick = TickGet();

	// Suppress transmissions if we don't have an Ethernet link so our counter starts correctly at 0x00000001
	if(!MACIsLinked())
		return;
	
	// Set the socket's destination to be a broadcast over our IP 
	// subnet
	// Set the MAC destination to be a broadcast
	memset(&Remote, 0xFF, sizeof(Remote));

	// Open a UDP socket for outbound transmission
	MySocket = UDPOpen(8887, &Remote, PERFORMANCE_PORT);
	
	// Abort operation if no UDP sockets are available
	// If this ever happens, incrementing MAX_UDP_SOCKETS in 
	// StackTsk.h may help (at the expense of more global memory 
	// resources).
	if(MySocket == INVALID_UDP_SOCKET)
		return;
	
	// Make certain the socket can be written to
	if(!UDPIsPutReady(MySocket))
	{
		UDPClose(MySocket);
		return;
	}
	
	// Put counter value into first 4 bytes of the packet
	UDPPutArray((BYTE*)&dwCounter, sizeof(dwCounter));
	
	/*wTemp = UDPPutROMArray((ROM BYTE*)
			"The quick brown fox tried to jump over the yellow dog.  Unfortunately, the yellow dog stood up while the fox was in mid-jump.  As a result, the two collided.  Then, the dog, being the omnivore that it is, ate the quick brown fox.  This line is 256 bytes.\r\n"
			"The quick brown fox tried to jump over the yellow dog.  Unfortunately, the yellow dog stood up while the fox was in mid-jump.  As a result, the two collided.  Then, the dog, being the omnivore that it is, ate the quick brown fox.  This line is 256 bytes.\r\n"
			"The quick brown fox tried to jump over the yellow dog.  Unfortunately, the yellow dog stood up while the fox was in mid-jump.  As a result, the two collided.  Then, the dog, being the omnivore that it is, ate the quick brown fox.  This line is 256 bytes.\r\n"
			"The quick brown fox tried to jump over the yellow dog.  Unfortunately, the yellow dog stood up while the fox was in mid-jump.  As a result, the two collided.  Then, the dog, being the omnivore that it is, ate the quick brown fox.  This line is 252b. \r\n", 1020);
    */
	// Send the packet
	UDPFlush();
	
	// Close the socket so it can be used by other modules
	UDPClose(MySocket);

	TRACE("udp sent\n");
}

#endif //#if defined(STACK_USE_UDP_PERFORMANCE_TEST)
