/*********************************************************************
 *
 *	Address Resolution Protocol (ARP) Client and Server
 *  Module for Microchip TCP/IP Stack
 *	 -Provides IP address to Ethernet MAC address translation
 *	 -Reference: RFC 826
 *
 *********************************************************************
 * FileName:        ARP.c
 * Dependencies:    string.h
 *                  StackTsk.h
 *                  Helpers.h
 *                  ARP.h
 *                  MAC.h
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
 * Nilesh Rajbharti     5/1/01  Original        (Rev 1.0)
 * Nilesh Rajbharti     2/9/02  Cleanup
 * Nilesh Rajbharti     5/22/02 Rev 2.0 (See version.log for detail)
 * Howard Schlunder		8/17/06	Combined ARP.c and ARPTsk.c into ARP.c; 
 *								rewrote some of it to look more linear
 ********************************************************************/
#define __ARP_C

#include "TCPIP Stack/TCPIP.h"

/****************************************************************************
  Section:
	Constants and Variables
  ***************************************************************************/

#define ARP_OPERATION_REQ       0x01u		// Operation code indicating an ARP Request
#define ARP_OPERATION_RESP      0x02u		// Operation code indicating an ARP Response

#define HW_ETHERNET             (0x0001u)	// ARP Hardware type as defined by IEEE 802.3
#define ARP_IP                  (0x0800u)	// ARP IP packet type as defined by IEEE 802.3

#ifdef STACK_CLIENT_MODE
static NODE_INFO Cache;						// Cache for one ARP response
#endif

// ARP packet structure
typedef struct __attribute__((aligned(2), packed))
{
    WORD        HardwareType;
    WORD        Protocol;
    BYTE        MACAddrLen;
    BYTE        ProtocolLen;
    WORD        Operation;
    MAC_ADDR    SenderMACAddr;
    IP_ADDR     SenderIPAddr;
    MAC_ADDR    TargetMACAddr;
    IP_ADDR     TargetIPAddr;
} ARP_PACKET;



/****************************************************************************
  Section:
	Helper Function Prototypes
  ***************************************************************************/

static void SwapARPPacket(ARP_PACKET* p);
static BOOL ARPPut(ARP_PACKET* packet);



/****************************************************************************
  Section:
	Function Implementations
  ***************************************************************************/

/*****************************************************************************
  Function:
	static BOOL ARPPut(ARP_PACKET* packet)

  Description:
	Writes an ARP packet to the MAC.

  Precondition:
	None

  Parameters:
	packet - A pointer to an ARP_PACKET structure with correct operation
				and target preconfigured.

  Return Values:
  	TRUE - The ARP packet was generated properly
  	FALSE - Not a possible return value
  ***************************************************************************/
static BOOL ARPPut(ARP_PACKET* packet)
{
	while(!MACIsTxReady());
	MACSetWritePtr(BASE_TX_ADDR);
	

    packet->HardwareType  = HW_ETHERNET;
    packet->Protocol      = ARP_IP;
    packet->MACAddrLen    = sizeof(MAC_ADDR);
    packet->ProtocolLen   = sizeof(IP_ADDR);
//    packet->SenderMACAddr = AppConfig.MyMACAddr;	// HI-TECH PICC-18 compiler can't handle this statement, use memcpy() as a workaround
	memcpy(&packet->SenderMACAddr, (void*)&AppConfig.MyMACAddr, sizeof(packet->SenderMACAddr));
    packet->SenderIPAddr  = AppConfig.MyIPAddr;

    SwapARPPacket(packet);

    MACPutHeader(&packet->TargetMACAddr, MAC_ARP, sizeof(*packet));
    MACPutArray((BYTE*)packet, sizeof(*packet));
    MACFlush();
	
	return TRUE;
}



/*****************************************************************************
  Function:
	void ARPInit(void)

  Summary:
	Initializes the ARP module.
	
  Description:
  	Initializes the ARP module.  Call this function once at boot to 
  	invalidate the cached lookup.

  Precondition:
	None

  Parameters:
	None

  Returns:
  	None
  
  Remarks:
  	This function is only required when the stack is a client, and therefore
  	is only enabled when STACK_CLIENT_MODE is enabled.
  ***************************************************************************/
#ifdef STACK_CLIENT_MODE
void ARPInit(void)
{
    Cache.MACAddr.v[0] = 0xff;
    Cache.MACAddr.v[1] = 0xff;
    Cache.MACAddr.v[2] = 0xff;
    Cache.MACAddr.v[3] = 0xff;
    Cache.MACAddr.v[4] = 0xff;
    Cache.MACAddr.v[5] = 0xff;

	Cache.IPAddr.Val = 0x0;
}
#endif



/*****************************************************************************
  Function:
	BOOL ARPProcess(void)

  Summary:
	Processes an incoming ARP packet.
	
  Description:
  	Retrieves an ARP packet from the MAC buffer and determines if it is a
  	response to our request (in which case the ARP is resolved) or if it
  	is a request requiring our response (in which case we transmit one.)

  Precondition:
	ARP packet is ready in the MAC buffer.

  Parameters:
	None

  Return Values:
  	TRUE - All processing of this ARP packet is complete.  Do not call 
  			again until a new ARP packet is waiting in the RX buffer.
  	FALSE - This function must be called again.  More time is needed to 
  			send an ARP response.
  ***************************************************************************/
BOOL ARPProcess(void)
{
	ARP_PACKET packet;
	static NODE_INFO Target;
	static enum
	{
	    SM_ARP_IDLE = 0,
	    SM_ARP_REPLY
	} smARP = SM_ARP_IDLE;

    switch(smARP)
    {
	    case SM_ARP_IDLE:
			// Obtain the incoming ARP packet
		    MACGetArray((BYTE*)&packet, sizeof(packet));		
		    MACDiscardRx();
		    SwapARPPacket(&packet);
		
			// Validate the ARP packet
		    if ( packet.HardwareType != HW_ETHERNET     ||
		         packet.MACAddrLen != sizeof(MAC_ADDR)  ||
		         packet.ProtocolLen != sizeof(IP_ADDR) )
		    {
		         return TRUE;
		    }
		
			// Handle incoming ARP responses
#ifdef STACK_CLIENT_MODE
			if(packet.Operation == ARP_OPERATION_RESP)
			{
				Cache.MACAddr = packet.SenderMACAddr;
				Cache.IPAddr = packet.SenderIPAddr;
				return TRUE;
			}
#endif

			// Handle incoming ARP requests for our MAC address
			if(packet.Operation == ARP_OPERATION_REQ)
			{
				if(packet.TargetIPAddr.Val != AppConfig.MyIPAddr.Val)
				{
					return TRUE;
				}
				Target.IPAddr = packet.SenderIPAddr;
				Target.MACAddr = packet.SenderMACAddr;

				smARP = SM_ARP_REPLY;
			}
			// Do not break.  If we get down here, we need to send a reply.	

	    case SM_ARP_REPLY:
	        packet.Operation		= ARP_OPERATION_RESP;
    	    packet.TargetMACAddr	= Target.MACAddr;
    	    packet.TargetIPAddr		= Target.IPAddr;

			// Send an ARP response to a previously received request
			if(!ARPPut(&packet))
			{
	           return FALSE;
			}

			// Begin listening for ARP requests again
			smARP = SM_ARP_IDLE;
	        break;
	}

    return TRUE;
}



/*****************************************************************************
  Function:
	void ARPResolve(IP_ADDR* IPAddr)

  Summary:
	Transmits an ARP request to resolve an IP address.
	
  Description:
  	This function transmits and ARP request to determine the hardware
  	address of a given IP address.

  Precondition:
	ARP packet is ready in the MAC buffer.

  Parameters:
	IPAddr - The IP address to be resolved

  Returns:
  	None

  Remarks:
  	This function is only required when the stack is a client, and therefore
  	is only enabled when STACK_CLIENT_MODE is enabled.
  ***************************************************************************/
#ifdef STACK_CLIENT_MODE
void ARPResolve(IP_ADDR* IPAddr)
{
    ARP_PACKET packet;

	packet.Operation            = ARP_OPERATION_REQ;
	packet.TargetMACAddr.v[0]   = 0xff;
	packet.TargetMACAddr.v[1]   = 0xff;
	packet.TargetMACAddr.v[2]   = 0xff;
	packet.TargetMACAddr.v[3]   = 0xff;
	packet.TargetMACAddr.v[4]   = 0xff;
	packet.TargetMACAddr.v[5]   = 0xff;


    // ARP query either the IP address directly (on our subnet), or do an ARP query for our Gateway if off of our subnet
	packet.TargetIPAddr			= ((AppConfig.MyIPAddr.Val ^ IPAddr->Val) & AppConfig.MyMask.Val) ? AppConfig.MyGateway : *IPAddr;

    ARPPut(&packet);
}
#endif



/*****************************************************************************
  Function:
	BOOL ARPIsResolved(IP_ADDR* IPAddr, MAC_ADDR* MACAddr)

  Summary:
	Determines if an ARP request has been resolved yet.
	
  Description:
  	This function checks if an ARP request has been resolved yet, and if
  	so, stores the resolved MAC address in the pointer provided.

  Precondition:
	ARP packet is ready in the MAC buffer.

  Parameters:
	IPAddr - The IP address to be resolved
	MACAddr - A buffer to store the corresponding MAC address

  Return Values:
  	TRUE - The IP address has been resolved and MAC address indicates the
  			response.
  	FALSE -	The IP address is not resolved.

  Remarks:
  	This function is only required when the stack is a client, and therefore
  	is only enabled when STACK_CLIENT_MODE is enabled.
  ***************************************************************************/
#ifdef STACK_CLIENT_MODE
BOOL ARPIsResolved(IP_ADDR* IPAddr, MAC_ADDR* MACAddr)
{
    if((Cache.IPAddr.Val == IPAddr->Val) || 
	  ((Cache.IPAddr.Val == AppConfig.MyGateway.Val) && ((AppConfig.MyIPAddr.Val ^ IPAddr->Val) & AppConfig.MyMask.Val)))
    {
        *MACAddr = Cache.MACAddr;
        return TRUE;
    }
    return FALSE;
}
#endif



/*****************************************************************************
  Function:
	static void SwapARPPacket(ARP_PACKET* p)

  Description:
  	Swaps endian-ness of header information in an ARP packet.

  Precondition:
	None

  Parameters:
	p - The ARP packet to be swapped

  Returns:
  	None
  ***************************************************************************/
static void SwapARPPacket(ARP_PACKET* p)
{
    p->HardwareType     = swaps(p->HardwareType);
    p->Protocol         = swaps(p->Protocol);
    p->Operation        = swaps(p->Operation);
}
