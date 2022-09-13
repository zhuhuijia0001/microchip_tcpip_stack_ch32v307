/*********************************************************************
 *
 *	Domain Name System (DNS) Client
 *  Module for Microchip TCP/IP Stack
 *	 -Provides hostname to IP address translation
 *	 -Reference: RFC 1035
 *
 *********************************************************************
 * FileName:        DNS.c
 * Dependencies:    UDP, ARP, Tick
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
 * Author               Date		Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Howard Schlunder     7/31/06		Original
 * Howard Schlunder		10/09/06	Added DNSBeginUsage(), DNSEndUsage() 
 *									module ownership semaphore
 ********************************************************************/
#define __DNS_C

#include "TCPIP Stack/TCPIP.h"

#if defined(STACK_USE_DNS)

/****************************************************************************
  Section:
	Constants and Global Variables
  ***************************************************************************/

#define DNS_PORT		53u					// Default port for DNS resolutions
#define DNS_TIMEOUT		(TICK_SECOND*1)		// Elapsed time after which a DNS resolution is considered to have timed out

static UDP_SOCKET MySocket = INVALID_UDP_SOCKET;	// UDP socket to use for DNS queries
static BYTE *DNSHostName;							// Host name in RAM to look up
static ROM BYTE *DNSHostNameROM;					// Host name in ROM to look up
static BYTE RecordType;								// Record type being queried
static NODE_INFO ResolvedInfo;						// Node information about the resolved node

// Semaphore flags for the DNS module
static union
{
	BYTE Val;
	struct
	{
		unsigned char DNSInUse 		: 1;	// Indicates the DNS module is in use
		unsigned char AddressValid	: 1;	// Indicates that the address resolution is valid and complete
		unsigned char filler 		: 6;
	} bits;
} Flags = {0x00};

// State machine for a DNS query
static enum
{
	DNS_ARP_START_RESOLVE = 0,	// First attempt to resolve the DNS server via ARP
	DNS_ARP_RESOLVE,			// Waiting for response to first ARP attempt
	DNS_ARP_START_RESOLVE2,		// Second attempt to resolve the DNS server via ARP
	DNS_ARP_RESOLVE2,			// Waiting for response to second ARP attempt
	DNS_ARP_START_RESOLVE3,		// Final attempt to resolve the DNS server via ARP
	DNS_ARP_RESOLVE3,			// Waiting for response to final ARP attempt
	DNS_ARP_FAIL,				// Could not ARP the DNS server
	DNS_OPEN_SOCKET,			// Opens socket to the DNS server
	DNS_QUERY,					// First attempt to query the DNS server
	DNS_GET_RESULT,				// Waiting for response to first DNS query
	DNS_QUERY2,					// Second attempt to query the DNS server
	DNS_GET_RESULT2,			// Waiting for response to second DNS query
	DNS_QUERY3,					// Final attempt to query the DNS server
	DNS_GET_RESULT3,			// Waiting for response to final DNS query
	DNS_QUERY_FAIL,				// DNS query has failed
	DNS_DONE					// DNS query is finished
} smDNS = DNS_DONE;

// Structure for the DNS header
typedef struct
{
	WORD_VAL TransactionID;
	WORD_VAL Flags;
	WORD_VAL Questions;
	WORD_VAL Answers;
	WORD_VAL AuthoritativeRecords;
	WORD_VAL AdditionalRecords;
} DNS_HEADER;

typedef struct __attribute__((aligned(2), packed))
{
	WORD_VAL	ResponseName;
	WORD_VAL	ResponseType;
	WORD_VAL	ResponseClass;
	DWORD_VAL	ResponseTTL;
	WORD_VAL	ResponseLen;
} DNS_ANSWER_HEADER;


/****************************************************************************
  Section:
	Function Prototypes
  ***************************************************************************/

static void DNSPutString(BYTE* String);
static void DNSGetString(BYTE* String);

#if defined(__18CXX)
	static void DNSPutROMString(ROM BYTE* String);
#else
	// Non-ROM alias for C30/C32
	#define DNSPutROMString(a)	DNSPutString((BYTE*)a)
#endif


/*****************************************************************************
  Function:
	BOOL DNSBeginUsage(void)

  Summary:
	Claims access to the DNS module.
	
  Description:
	This function acts as a semaphore to obtain usage of the DNS module.
	Call this function and ensure that it returns TRUE before calling any
	other DNS APIs.  Call DNSEndUsage when this application no longer 
	needs the DNS module so that other applications may make use of it.

  Precondition:
	Stack is initialized.

  Parameters:
	None

  Return Values:
  	TRUE - No other DNS resolutions are in progress and the calling
  			application has sucessfully taken ownership of the DNS module
  	FALSE - The DNS module is currently in use.  Yield to the stack and 
  			attempt this call again later.
  	
  Remarks:
	Ensure that DNSEndUsage is always called once your application has
	obtained control of the DNS module.  If this is not done, the stack
	will hang for all future applications requiring DNS access.
  ***************************************************************************/
BOOL DNSBeginUsage(void)
{
	if(Flags.bits.DNSInUse)
		return FALSE;

	Flags.bits.DNSInUse = TRUE;
	return TRUE;
}


/*****************************************************************************
  Function:
	BOOL DNSEndUsage(void)

  Summary:
	Releases control of the DNS module.
	
  Description:
	This function acts as a semaphore to obtain usage of the DNS module.
	Call this function when this application no longer needs the DNS 
	module so that other applications may make use of it.

  Precondition:
	DNSBeginUsage returned TRUE on a previous call.

  Parameters:
	None

  Return Values:
  	TRUE - The address to the host name was successfully resolved.
  	FALSE - The DNS failed or the address does not exist.
  	
  Remarks:
	Ensure that DNSEndUsage is always called once your application has
	obtained control of the DNS module.  If this is not done, the stack
	will hang for all future applications requiring DNS access.
  ***************************************************************************/
BOOL DNSEndUsage(void)
{
	if(MySocket != INVALID_UDP_SOCKET)
	{
		UDPClose(MySocket);
		MySocket = INVALID_UDP_SOCKET;
	}
	smDNS = DNS_DONE;
	Flags.bits.DNSInUse = FALSE;

	return Flags.bits.AddressValid;
}


/*****************************************************************************
  Function:
	void DNSResolve(BYTE* Hostname, BYTE Type)

  Summary:
	Begins resolution of an address.
	
  Description:
	This function attempts to resolve a host name to an IP address.  When 
	called, it starts the DNS state machine.  Call DNSIsResolved repeatedly
	to determine if the resolution is complete.
	
	Only one DNS resoultion may be executed at a time.  The Hostname must 
	not be modified in memory until the resolution is complete.

  Precondition:
	DNSBeginUsage returned TRUE on a previous call.

  Parameters:
	Hostname - A pointer to the null terminated string specifiying the
		host for which to resolve an IP.
	RecordType - DNS_TYPE_A or DNS_TYPE_MX depending on what type of
		record resolution is desired.

  Returns:
  	None
  	
  Remarks:
	This function requires access to one UDP socket.  If none are available,
	MAX_UDP_SOCKETS may need to be increased.
  ***************************************************************************/
void DNSResolve(BYTE* Hostname, BYTE Type)
{
	if(StringToIPAddress(Hostname, &ResolvedInfo.IPAddr))
	{
		Flags.bits.AddressValid = TRUE;
		smDNS = DNS_DONE;
	}
	else
	{	
		DNSHostName = Hostname;
		DNSHostNameROM = NULL;
		smDNS = DNS_ARP_START_RESOLVE;
		RecordType = Type;
		Flags.bits.AddressValid = FALSE;
	}
}


/*****************************************************************************
  Function:
	void DNSResolveROM(ROM BYTE* Hostname, BYTE Type)

  Summary:
	Begins resolution of an address.
	
  Description:
	This function attempts to resolve a host name to an IP address.  When 
	called, it starts the DNS state machine.  Call DNSIsResolved repeatedly
	to determine if the resolution is complete.
	
	Only one DNS resoultion may be executed at a time.  The Hostname must 
	not be modified in memory until the resolution is complete.

  Precondition:
	DNSBeginUsage returned TRUE on a previous call.

  Parameters:
	Hostname - A pointer to the null terminated string specifiying the
		host for which to resolve an IP.
	RecordType - DNS_TYPE_A or DNS_TYPE_MX depending on what type of
		record resolution is desired.

  Returns:
  	None
  	
  Remarks:
	This function requires access to one UDP socket.  If none are available,
	MAX_UDP_SOCKETS may need to be increased.
	
	This function is aliased to DNSResolve on non-PIC18 platforms.
  ***************************************************************************/
#if defined(__18CXX)
void DNSResolveROM(ROM BYTE* Hostname, BYTE Type)
{
	if(ROMStringToIPAddress(Hostname, &ResolvedInfo.IPAddr))
	{
		Flags.bits.AddressValid = TRUE;
		smDNS = DNS_DONE;
	}
	else
	{	
		DNSHostName = NULL;
		DNSHostNameROM = Hostname;
		smDNS = DNS_ARP_START_RESOLVE;
		RecordType = Type;
		Flags.bits.AddressValid = FALSE;
	}
}
#endif


/*****************************************************************************
  Function:
	BOOL DNSIsResolved(IP_ADDR* HostIP)

  Summary:
	Determines if the DNS resolution is complete and provides the IP.
	
  Description:
	Call this function to determine if the DNS resolution of an address has
	been completed.  If so, the resolved address will be provided in HostIP.

  Precondition:
	DNSResolve or DNSResolveROM has been called.

  Parameters:
	HostIP - A pointer to an IP_ADDR structure in which to store the 
		resolved IP address once resolution is complete.

  Return Values:
  	TRUE - The DNS client has obtained an IP, or the DNS process
  		has timed out.  HostIP will be 0.0.0.0 on timeout.
  	FALSE - The resolution process is still in progress.
  ***************************************************************************/
BOOL DNSIsResolved(IP_ADDR* HostIP)
{
	static TICK			StartTime;
	static WORD_VAL		SentTransactionID;
	BYTE 				i;
	WORD_VAL			w;
	DNS_HEADER			DNSHeader;
	DNS_ANSWER_HEADER	DNSAnswerHeader;

	switch(smDNS)
	{
		case DNS_ARP_START_RESOLVE:
		case DNS_ARP_START_RESOLVE2:
		case DNS_ARP_START_RESOLVE3:
			ARPResolve(&AppConfig.PrimaryDNSServer);
			StartTime = TickGet();
			smDNS++;
			break;

		case DNS_ARP_RESOLVE:
		case DNS_ARP_RESOLVE2:
		case DNS_ARP_RESOLVE3:
			if(!ARPIsResolved(&AppConfig.PrimaryDNSServer, &ResolvedInfo.MACAddr))
			{
				if(TickGet() - StartTime > DNS_TIMEOUT)
				{
					smDNS++;
				}
				break;
			}
			ResolvedInfo.IPAddr.Val = AppConfig.PrimaryDNSServer.Val;
			smDNS = DNS_OPEN_SOCKET;
			// No break: DNS_OPEN_SOCKET is the correct next state
		
		case DNS_OPEN_SOCKET:
			MySocket = UDPOpen(0, &ResolvedInfo, DNS_PORT);
			if(MySocket == INVALID_UDP_SOCKET)
				break;

			smDNS = DNS_QUERY;
			// No need to break, we can immediately start resolution
			
		case DNS_QUERY:
		case DNS_QUERY2:
		case DNS_QUERY3:
			if(!UDPIsPutReady(MySocket))
				break;
			
			// Put DNS query here
			SentTransactionID.Val++;
			UDPPut(SentTransactionID.v[1]);// User chosen transaction ID
			UDPPut(SentTransactionID.v[0]);
			UDPPut(0x01);		// Standard query with recursion
			UDPPut(0x00);	
			UDPPut(0x00);		// 0x0001 questions
			UDPPut(0x01);
			UDPPut(0x00);		// 0x0000 answers
			UDPPut(0x00);
			UDPPut(0x00);		// 0x0000 name server resource records
			UDPPut(0x00);
			UDPPut(0x00);		// 0x0000 additional records
			UDPPut(0x00);

			// Put hostname string to resolve
			if(DNSHostName)
				DNSPutString(DNSHostName);
			else
				DNSPutROMString(DNSHostNameROM);

			UDPPut(0x00);		// Type: DNS_TYPE_A A (host address) or DNS_TYPE_MX for mail exchange
			UDPPut(RecordType);
			UDPPut(0x00);		// Class: IN (Internet)
			UDPPut(0x01);

			UDPFlush();
			StartTime = TickGet();
			smDNS++;
			break;

		case DNS_GET_RESULT:
		case DNS_GET_RESULT2:
		case DNS_GET_RESULT3:
			if(!UDPIsGetReady(MySocket))
			{
				if(TickGet() - StartTime > DNS_TIMEOUT)
				{
					smDNS++;
				}
				break;
			}

			// Retrieve the DNS header and de-big-endian it
			UDPGet(&DNSHeader.TransactionID.v[1]);
			UDPGet(&DNSHeader.TransactionID.v[0]);

			// Throw this packet away if it isn't in response to our last query
			if(DNSHeader.TransactionID.Val != SentTransactionID.Val)
			{
				UDPDiscard();
				break;
			}

			UDPGet(&DNSHeader.Flags.v[1]);
			UDPGet(&DNSHeader.Flags.v[0]);
			UDPGet(&DNSHeader.Questions.v[1]);
			UDPGet(&DNSHeader.Questions.v[0]);
			UDPGet(&DNSHeader.Answers.v[1]);
			UDPGet(&DNSHeader.Answers.v[0]);
			UDPGet(&DNSHeader.AuthoritativeRecords.v[1]);
			UDPGet(&DNSHeader.AuthoritativeRecords.v[0]);
			UDPGet(&DNSHeader.AdditionalRecords.v[1]);
			UDPGet(&DNSHeader.AdditionalRecords.v[0]);

			// Remove all questions
			while(DNSHeader.Questions.Val--)
			{
				DNSGetString(NULL);
				UDPGet(&w.v[1]);		// Question type
				UDPGet(&w.v[0]);
				UDPGet(&w.v[1]);		// Question class
				UDPGet(&w.v[0]);
			}
			
			// Scan through answers
			while(DNSHeader.Answers.Val--)
			{
				UDPGet(&DNSAnswerHeader.ResponseName.v[1]);		// Response name
				UDPGet(&DNSAnswerHeader.ResponseName.v[0]);
				UDPGet(&DNSAnswerHeader.ResponseType.v[1]);		// Response type
				UDPGet(&DNSAnswerHeader.ResponseType.v[0]);
				UDPGet(&DNSAnswerHeader.ResponseClass.v[1]);	// Response class
				UDPGet(&DNSAnswerHeader.ResponseClass.v[0]);
				UDPGet(&DNSAnswerHeader.ResponseTTL.v[3]);		// Time to live
				UDPGet(&DNSAnswerHeader.ResponseTTL.v[2]);
				UDPGet(&DNSAnswerHeader.ResponseTTL.v[1]);
				UDPGet(&DNSAnswerHeader.ResponseTTL.v[0]);
				UDPGet(&DNSAnswerHeader.ResponseLen.v[1]);		// Response length
				UDPGet(&DNSAnswerHeader.ResponseLen.v[0]);

				// Make sure that this is a 4 byte IP address, response type A or MX, class 1
				// Check if this is Type A or MX
				if( DNSAnswerHeader.ResponseType.Val	== 0x0001u &&
					DNSAnswerHeader.ResponseClass.Val	== 0x0001u && // Internet class
					DNSAnswerHeader.ResponseLen.Val		== 0x0004u)
				{
					Flags.bits.AddressValid = TRUE;
					UDPGet(&ResolvedInfo.IPAddr.v[0]);
					UDPGet(&ResolvedInfo.IPAddr.v[1]);
					UDPGet(&ResolvedInfo.IPAddr.v[2]);
					UDPGet(&ResolvedInfo.IPAddr.v[3]);
					goto DoneSearchingRecords;
				}
				else
				{
					while(DNSAnswerHeader.ResponseLen.Val--)
					{
						UDPGet(&i);
					}
				}
			}

			// Remove all Authoritative Records
			while(DNSHeader.AuthoritativeRecords.Val--)
			{
				UDPGet(&DNSAnswerHeader.ResponseName.v[1]);		// Response name
				UDPGet(&DNSAnswerHeader.ResponseName.v[0]);
				UDPGet(&DNSAnswerHeader.ResponseType.v[1]);		// Response type
				UDPGet(&DNSAnswerHeader.ResponseType.v[0]);
				UDPGet(&DNSAnswerHeader.ResponseClass.v[1]);	// Response class
				UDPGet(&DNSAnswerHeader.ResponseClass.v[0]);
				UDPGet(&DNSAnswerHeader.ResponseTTL.v[3]);		// Time to live
				UDPGet(&DNSAnswerHeader.ResponseTTL.v[2]);
				UDPGet(&DNSAnswerHeader.ResponseTTL.v[1]);
				UDPGet(&DNSAnswerHeader.ResponseTTL.v[0]);
				UDPGet(&DNSAnswerHeader.ResponseLen.v[1]);		// Response length
				UDPGet(&DNSAnswerHeader.ResponseLen.v[0]);

				// Make sure that this is a 4 byte IP address, response type A or MX, class 1
				// Check if this is Type A
				if( DNSAnswerHeader.ResponseType.Val	== 0x0001u &&
					DNSAnswerHeader.ResponseClass.Val	== 0x0001u && // Internet class
					DNSAnswerHeader.ResponseLen.Val		== 0x0004u)
				{
					Flags.bits.AddressValid = TRUE;
					UDPGet(&ResolvedInfo.IPAddr.v[0]);
					UDPGet(&ResolvedInfo.IPAddr.v[1]);
					UDPGet(&ResolvedInfo.IPAddr.v[2]);
					UDPGet(&ResolvedInfo.IPAddr.v[3]);
					goto DoneSearchingRecords;
				}
				else
				{
					while(DNSAnswerHeader.ResponseLen.Val--)
					{
						UDPGet(&i);
					}
				}
			}

			// Remove all Additional Records
			while(DNSHeader.AdditionalRecords.Val--)
			{
				UDPGet(&DNSAnswerHeader.ResponseName.v[1]);		// Response name
				UDPGet(&DNSAnswerHeader.ResponseName.v[0]);
				UDPGet(&DNSAnswerHeader.ResponseType.v[1]);		// Response type
				UDPGet(&DNSAnswerHeader.ResponseType.v[0]);
				UDPGet(&DNSAnswerHeader.ResponseClass.v[1]);	// Response class
				UDPGet(&DNSAnswerHeader.ResponseClass.v[0]);
				UDPGet(&DNSAnswerHeader.ResponseTTL.v[3]);		// Time to live
				UDPGet(&DNSAnswerHeader.ResponseTTL.v[2]);
				UDPGet(&DNSAnswerHeader.ResponseTTL.v[1]);
				UDPGet(&DNSAnswerHeader.ResponseTTL.v[0]);
				UDPGet(&DNSAnswerHeader.ResponseLen.v[1]);		// Response length
				UDPGet(&DNSAnswerHeader.ResponseLen.v[0]);

				// Make sure that this is a 4 byte IP address, response type A or MX, class 1
				// Check if this is Type A
				if( DNSAnswerHeader.ResponseType.Val	== 0x0001u &&
					DNSAnswerHeader.ResponseClass.Val	== 0x0001u && // Internet class
					DNSAnswerHeader.ResponseLen.Val		== 0x0004u)
				{
					Flags.bits.AddressValid = TRUE;
					UDPGet(&ResolvedInfo.IPAddr.v[0]);
					UDPGet(&ResolvedInfo.IPAddr.v[1]);
					UDPGet(&ResolvedInfo.IPAddr.v[2]);
					UDPGet(&ResolvedInfo.IPAddr.v[3]);
					goto DoneSearchingRecords;
				}
				else
				{
					while(DNSAnswerHeader.ResponseLen.Val--)
					{
						UDPGet(&i);
					}
				}
			}

DoneSearchingRecords:

			UDPDiscard();
			UDPClose(MySocket);
			MySocket = INVALID_UDP_SOCKET;
			smDNS = DNS_DONE;
			// No break, DNS_DONE is the correct step

		case DNS_DONE:
			HostIP->Val = ResolvedInfo.IPAddr.Val;
			return TRUE;

		case DNS_ARP_FAIL:
		case DNS_QUERY_FAIL:
			// Return an invalid IP address 0.0.0.0 if we can't finish ARP or DNS query step
			HostIP->Val = 0x00000000;
			return TRUE;
	}
	
	return FALSE;
}

/*****************************************************************************
  Function:
	static void DNSPutString(BYTE* String)

  Summary:
	Writes a string to the DNS socket.
	
  Description:
	This function writes a string to the DNS socket, ensuring that it is
	properly formatted.

  Precondition:
	UDP socket is obtained and ready for writing.

  Parameters:
	String - the string to write to the UDP socket.

  Returns:
  	None
  ***************************************************************************/
static void DNSPutString(BYTE* String)
{
	BYTE *RightPtr;
	BYTE i;
	BYTE Len;

	RightPtr = String;

	while(1)
	{
		do
		{
			i = *RightPtr++;
		} while((i != 0x00u) && (i != '.') && (i != '/') && (i != ',') && (i != '>'));
	
		// Put the length and data
		// Also, skip over the '.' in the input string
		Len = (BYTE)(RightPtr-String-1);
		UDPPut(Len);
		String += UDPPutArray(String, Len) + 1;

		if(i == 0x00u || i == '/' || i == ',' || i == '>')
			break;
	}
	
	// Put the string null terminator character
	UDPPut(0x00);
}

/*****************************************************************************
  Function:
	static void DNSPutROMString(ROM BYTE* String)

  Summary:
	Writes a ROM string to the DNS socket.
	
  Description:
	This function writes a string to the DNS socket, ensuring that it is
	properly formatted.

  Precondition:
	UDP socket is obtained and ready for writing.

  Parameters:
	String - the string to write to the UDP socket.

  Returns:
  	None
  
  Remarks:
  	This function is aliased to DNSPutString on non-PIC18 platforms.
  ***************************************************************************/
#if defined(__18CXX)
static void DNSPutROMString(ROM BYTE* String)
{
	ROM BYTE *RightPtr;
	BYTE i;
	BYTE Len;

	RightPtr = String;

	while(1)
	{
		do
		{
			i = *RightPtr++;
		} while((i != 0x00u) && (i != '.') && (i != '/') && (i != ',') && (i != '>'));
	
		// Put the length and data
		// Also, skip over the '.' in the input string
		Len = (BYTE)(RightPtr-String-1);
		UDPPut(Len);
		String += UDPPutROMArray(String, Len) + 1;

		if(i == 0x00u || i == '/' || i == ',' || i == '>')
			break;
	}
	
	// Put the string terminator character
	UDPPut(0x00);
}
#endif

/*****************************************************************************
  Function:
	static void DNSGetString(BYTE* String)

  Summary:
	Reads a string from the DNS socket.
	
  Description:
	This function reads a string from the DNS socket, first by reading the
	length of the string and then the data itself.

  Precondition:
	UDP socket is obtained and ready for reading and String is large enough
	to hold the data (or NULL).

  Parameters:
	String - where to read the string to

  Returns:
  	None
  ***************************************************************************/
static void DNSGetString(BYTE* String)
{
	BYTE i;

	while(1)
	{
		if(!UDPGet(&i))
			return;
		if(i == 0u)
			return;
		i = UDPGetArray(String, i);
		if(String)
			String += i;
		
	}
}


#endif	//#if defined(STACK_USE_DNS)
