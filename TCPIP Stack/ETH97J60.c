/*********************************************************************
 *
 *	Medium Access Control (MAC) Layer for Microchip PIC18F97J60 family
 *  Module for Microchip TCP/IP Stack
 *	 -Provides access to PIC18F97J60 family Ethernet controller
 *	 -Reference: PIC18F97J60 Family data sheet, IEEE 802.3 Standard
 *
 *********************************************************************
 * FileName:        ETH97J60.c
 * Dependencies:    ETH97J60.h
 *					MAC.h
 *					string.h
 *                  StackTsk.h
 *                  Helpers.h
 *					Delay.h
 * Processor:       PIC18F97J60 family
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
 * Author               Date   	 Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Rawin Rojvanit       07/26/05 Stuff
 * Howard Schlunder     11/17/05 Ported to PIC18F97J60
 * Howard Schlunder		06/16/06 Synchronized with ENC28J60 code
 * Howard Schlunder		05/21/07 Fixed a TX lockup problem
********************************************************************/
#define __ETH97J60_C

#include "TCPIP Stack/TCPIP.h"

// Make sure that this hardware profile has a PIC18F97J60 family device in it
#if (defined(__18F97J60) || defined(__18F96J65) || defined(__18F96J60) || defined(__18F87J60) || defined(__18F86J65) || defined(__18F86J60) || defined(__18F67J60) || defined(__18F66J65) || defined(__18F66J60) || \
	  defined(_18F97J60) ||  defined(_18F96J65) ||  defined(_18F96J60) ||  defined(_18F87J60) ||  defined(_18F86J65) ||  defined(_18F86J60) ||  defined(_18F67J60) ||  defined(_18F66J65) ||  defined(_18F66J60)) \
	&& !defined(ENC_CS_TRIS)


/** D E F I N I T I O N S ****************************************************/
// Since the Ethernet PHY doesn't support auto-negotiation, full-duplex mode is 
// not compatible with most switches/routers.  If a dedicated network is used 
// where the duplex of the remote node can be manually configured, you may 
// change this configuration.  Otherwise, half duplex should always be used.
#define HALF_DUPLEX
//#define FULL_DUPLEX

// Pseudo Functions
#define LOW(a) 					(a & 0xFF)
#define HIGH(a) 				((a>>8) & 0xFF)

#define ETHER_IP	(0x00u)
#define ETHER_ARP	(0x06u)

// A header appended at the start of all RX frames by the hardware
typedef struct _ENC_PREAMBLE
{
    WORD			NextPacketPointer;
    RXSTATUS		StatusVector;

    MAC_ADDR        DestMACAddr;
    MAC_ADDR        SourceMACAddr;
    WORD_VAL        Type;
} ENC_PREAMBLE;


// Internal MAC level variables and flags.
static WORD_VAL NextPacketLocation;
static WORD_VAL CurrentPacketLocation;
static BOOL WasDiscarded;
static WORD wTXWatchdog;


/******************************************************************************
 * Function:        void MACInit(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        MACInit enables the Ethernet module, waits for the 
 *                  to become ready, and programs all registers for future 
 *                  TX/RX operations.
 *
 * Note:            This function blocks for at least 1ms, waiting for the 
 *                  hardware to stabilize.
 *****************************************************************************/
void MACInit(void)
{
	BYTE i;
	

    TRISAbits.TRISA0 = 0;   // Set LED0 as output
    TRISAbits.TRISA1 = 0;   // Set LED1 as output
    ECON2bits.ETHEN = 1;    // Enable Ethernet!

	// Wait for PHYRDY to become set.
    while(!ESTATbits.PHYRDY);
	
	// Configure the receive buffer boundary pointers 
	// and the buffer write protect pointer (receive buffer read pointer)
	WasDiscarded = TRUE;
	NextPacketLocation.Val = RXSTART;
	ERXST = RXSTART;
	ERXRDPTL = LOW(RXSTOP);	// Write low byte first
	ERXRDPTH = HIGH(RXSTOP);// Write high byte last
	ERXND = RXSTOP;
	ETXST = TXSTART;

	// Write a permanant per packet control byte of 0x00
	EWRPT = TXSTART;
	MACPut(0x00);

	// Configure Receive Filters 
	// (No need to reconfigure - Unicast OR Broadcast with CRC checking is 
	// acceptable)
	//ERXFCON = ERXFCON_CRCEN;     // Promiscious mode

	// Configure the MAC
	// Enable the receive portion of the MAC
    MACON1 = MACON1_TXPAUS | MACON1_RXPAUS | MACON1_MARXEN; Nop();

	// Pad packets to 60 bytes, add CRC, and check Type/Length field.
#if defined(FULL_DUPLEX)
	MACON3 = MACON3_PADCFG0 | MACON3_TXCRCEN | MACON3_FRMLNEN | MACON3_FULDPX; Nop();
	MABBIPG = 0x15; Nop();
#else
	MACON3 = MACON3_PADCFG0 | MACON3_TXCRCEN | MACON3_FRMLNEN; Nop();
	MABBIPG = 0x12; Nop();
#endif

    // Allow infinite deferals if the medium is continuously busy 
    // (do not time out a transmission if the half duplex medium is 
    // completely saturated with other people's data)
    MACON4 = MACON4_DEFER; Nop();

	// Set non-back-to-back inter-packet gap to 9.6us.  The back-to-back 
	// inter-packet gap (MABBIPG) is set by MACSetDuplex() which is called 
	// later.
    MAIPGL = 0x12; Nop();
    MAIPGH = 0x0C; Nop();

	// Set the maximum packet size which the controller will accept
    MAMXFLL = LOW(6+6+2+1500+4); Nop();
    MAMXFLH = HIGH(6+6+2+1500+4); Nop();
	
    // Initialize physical MAC address registers
	MAADR1 = AppConfig.MyMACAddr.v[0]; Nop();
	MAADR2 = AppConfig.MyMACAddr.v[1]; Nop();
	MAADR3 = AppConfig.MyMACAddr.v[2]; Nop();
	MAADR4 = AppConfig.MyMACAddr.v[3]; Nop();
	MAADR5 = AppConfig.MyMACAddr.v[4]; Nop();
	MAADR6 = AppConfig.MyMACAddr.v[5]; Nop();

	// Disable half duplex loopback in PHY and set RXAPDIS bit as per errata 
	WritePHYReg(PHCON2, PHCON2_HDLDIS | PHCON2_RXAPDIS);

	// Configure LEDA to display LINK status, LEDB to display TX/RX activity
	SetLEDConfig(0x3472);
	
	// Set the PHY into the proper duplex state
#if defined(FULL_DUPLEX)
	WritePHYReg(PHCON1, PHCON1_PDPXMD);
#else
	WritePHYReg(PHCON1, 0x0000);
#endif

	// Enable packet reception
    ECON1bits.RXEN = 1;
}//end MACInit


/******************************************************************************
 * Function:        BOOL MACIsLinked(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          TRUE: If the PHY reports that a link partner is present 
 *						  and the link has been up continuously since the last 
 *						  call to MACIsLinked()
 *					FALSE: If the PHY reports no link partner, or the link went 
 *						   down momentarily since the last call to MACIsLinked()
 *
 * Side Effects:    None
 *
 * Overview:        Returns the PHSTAT1.LLSTAT bit.
 *
 * Note:            None
 *****************************************************************************/
BOOL MACIsLinked(void)
{
	// LLSTAT is a latching low link status bit.  Therefore, if the link 
	// goes down and comes back up before a higher level stack program calls
	// MACIsLinked(), MACIsLinked() will still return FALSE.  The next 
	// call to MACIsLinked() will return TRUE (unless the link goes down 
	// again).
	return ReadPHYReg(PHSTAT1).PHSTAT1bits.LLSTAT;
}


/******************************************************************************
 * Function:        BOOL MACIsTxReady(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          TRUE: If no Ethernet transmission is in progress
 *					FALSE: If a previous transmission was started, and it has 
 *						   not completed yet.  While FALSE, the data in the 
 *						   transmit buffer and the TXST/TXND pointers must not
 *						   be changed.
 *
 * Side Effects:    None
 *
 * Overview:        Returns the ECON1.TXRTS bit
 *
 * Note:            None
 *****************************************************************************/
BOOL MACIsTxReady(void)
{
	if(!ECON1bits.TXRTS)
		return TRUE;

	// Retry transmission if the current packet seems to be not completing
	// Wait 3ms before triggering the retry.
	if((WORD)TickGet() - wTXWatchdog >= (3ull*TICK_SECOND/1000ull))
	{
		ECON1bits.TXRTS = 0;
		MACFlush();
	}
	
	return FALSE;
}


/******************************************************************************
 * Function:        void MACDiscardRx(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        Marks the last received packet (obtained using 
 *					MACGetHeader())as being processed and frees the buffer 
 *					memory associated with it
 *
 * Note:            Is is safe to call this function multiple times between 
 *					MACGetHeader() calls.  Extra packets won't be thrown away 
 *					until MACGetHeader() makes it available.
 *****************************************************************************/
void MACDiscardRx(void)
{
	WORD_VAL NewRXRDLocation;

	// Make sure the current packet was not already discarded
	if(WasDiscarded)
		return;
	WasDiscarded = TRUE;

	// Decrement the next packet pointer before writing it into 
	// the ERXRDPT registers.  This is a silicon errata workaround.
	// RX buffer wrapping must be taken into account if the 
	// NextPacketLocation is precisely RXSTART.
	NewRXRDLocation.Val = NextPacketLocation.Val - 1;
#if RXSTART == 0
	if(NewRXRDLocation.Val > RXSTOP)
#else
	if(NewRXRDLocation.Val < RXSTART || NewRXRDLocation.Val > RXSTOP)
#endif
	{
		NewRXRDLocation.Val = RXSTOP;
	}

	// Decrement the RX packet counter register, EPKTCNT
    ECON2bits.PKTDEC = 1;

	// Move the receive read pointer to unwrite-protect the memory used by the 
	// last packet.  The writing order is important: set the low byte first, 
	// high byte last.
    ERXRDPTL = NewRXRDLocation.v[0];
	ERXRDPTH = NewRXRDLocation.v[1];
	
	// The PKTIF flag should automatically be cleared by hardware, but 
	// early beta silicon requires that you manually clear it.  This should be 
	// unneeded for production A0 silicon and later.
	EIRbits.PKTIF = 0;
}


/******************************************************************************
 * Function:        WORD MACGetFreeRxSize(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          A WORD estimate of how much RX buffer space is free at 
 *					the present time.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 *****************************************************************************/
WORD MACGetFreeRxSize(void)
{
	WORD_VAL ReadPT, WritePT;

	// Read the Ethernet hardware buffer write pointer.  Because packets can be 
	// received at any time, it can change between reading the low and high 
	// bytes.  A loop is necessary to make certain a proper low/high byte pair
	// is read.
	do {
		// Save EPKTCNT in a temporary location
		ReadPT.v[0] = EPKTCNT;
	
		WritePT.Val = ERXWRPT;	
	} while(EPKTCNT != ReadPT.v[0]);
	
	// Determine where the write protection pointer is
	ReadPT.Val = ERXRDPT;

	
	// Calculate the difference between the pointers, taking care to account 
	// for buffer wrapping conditions
	if(WritePT.Val > ReadPT.Val)
	{
		return (RXSTOP - RXSTART) - (WritePT.Val - ReadPT.Val);
	}
	else if(WritePT.Val == ReadPT.Val)
	{
		return RXSIZE - 1;
	}
	else
    {
		return ReadPT.Val - WritePT.Val - 1;
	}
}

/******************************************************************************
 * Function:        BOOL MACGetHeader(MAC_ADDR *remote, BYTE* type)
 *
 * PreCondition:    None
 *
 * Input:           *remote: Location to store the Source MAC address of the 
 *							 received frame.
 *					*type: Location of a BYTE to store the constant 
 *						   MAC_UNKNOWN, ETHER_IP, or ETHER_ARP, representing 
 *						   the contents of the Ethernet type field.
 *
 * Output:          TRUE: If a packet was waiting in the RX buffer.  The 
 *						  remote, and type values are updated.
 *					FALSE: If a packet was not pending.  remote and type are 
 *						   not changed.
 *
 * Side Effects:    Last packet is discarded if MACDiscardRx() hasn't already
 *					been called.
 *
 * Overview:        None
 *
 * Note:            None
 *****************************************************************************/
BOOL MACGetHeader(MAC_ADDR *remote, BYTE* type)
{
	ENC_PREAMBLE header;

	// Test if at least one packet has been received and is waiting
    if(EPKTCNT == 0u)
    {
        return FALSE;
    }

	// Make absolutely certain that any previous packet was discarded
	if(WasDiscarded == FALSE)
	{
		MACDiscardRx();
		return FALSE;
	}
	// Save the location of this packet
	CurrentPacketLocation.Val = NextPacketLocation.Val;

	// Set the read pointer to the beginning of the next unprocessed packet
    ERDPT = CurrentPacketLocation.Val;

	// Obtain the MAC header from the Ethernet buffer
	MACGetArray((BYTE*)&header, sizeof(header));

	// The EtherType field, like most items transmitted on the Ethernet medium
	// are in big endian.
	header.Type.Val = swaps(header.Type.Val);

	// Do a sanity check.  There might be a bug in code someplace if this 
	// Reset() ever happens.  Check for potential errors in array/pointer writing code.
	if(header.NextPacketPointer > RXSTOP || ((BYTE_VAL*)(&header.NextPacketPointer))->bits.b0 ||
	   header.StatusVector.bits.Zero ||
	   header.StatusVector.bits.CRCError ||
	   header.StatusVector.bits.ByteCount > 1518u ||
	   !header.StatusVector.bits.ReceiveOk)
	{
		Reset();
	}

	// Save the location where the hardware will write the next packet to
	NextPacketLocation.Val = header.NextPacketPointer;

	// Return the Ethernet frame's Source MAC address field to the caller
	// This parameter is useful for replying to requests without requiring an 
	// ARP cycle.
    memcpy((void*)remote->v, (void*)header.SourceMACAddr.v, sizeof(*remote));

	// Return a simplified version of the EtherType field to the caller
    *type = MAC_UNKNOWN;
    if( (header.Type.v[1] == 0x08u) && 
    	((header.Type.v[0] == ETHER_IP) || (header.Type.v[0] == ETHER_ARP)) )
    {
    	*type = header.Type.v[0];
    }

    // Mark this packet as discardable
    WasDiscarded = FALSE;	
	return TRUE;
}


/******************************************************************************
 * Function:        void MACPutHeader(MAC_ADDR *remote, BYTE type, WORD dataLen)
 *
 * PreCondition:    MACIsTxReady() must return TRUE.
 *
 * Input:           *remote: Pointer to memory which contains the destination
 * 							 MAC address (6 bytes)
 *					type: The constant ETHER_ARP or ETHER_IP, defining which 
 *						  value to write into the Ethernet header's type field.
 *					dataLen: Length of the Ethernet data payload
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            Because of the dataLen parameter, it is probably 
 *					advantagous to call this function immediately before 
 *					transmitting a packet rather than initially when the 
 *					packet is first created.  The order in which the packet
 *					is constructed (header first or data first) is not 
 *					important.
 *****************************************************************************/
void MACPutHeader(MAC_ADDR *remote, BYTE type, WORD dataLen)
{
	// Set the write pointer to the beginning of the transmit buffer
	EWRPT = TXSTART + 1;

	// Calculate where to put the TXND pointer
    dataLen += (WORD)sizeof(ETHER_HEADER) + TXSTART;

	// Write the TXND pointer into the registers, given the dataLen given
	ETXND = dataLen;

	// Set the per-packet control byte and write the Ethernet destination 
	// address
    MACPutArray((BYTE*)remote, sizeof(*remote));

	// Write our MAC address in the Ethernet source field
	MACPutArray((BYTE*)&AppConfig.MyMACAddr, sizeof(AppConfig.MyMACAddr));

	// Write the appropriate Ethernet Type WORD for the protocol being used
    MACPut(0x08);
    MACPut((type == MAC_IP) ? ETHER_IP : ETHER_ARP);
}

/******************************************************************************
 * Function:        void MACFlush(void)
 *
 * PreCondition:    A packet has been created by calling MACPut() and 
 *					MACPutHeader().
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        MACFlush causes the current TX packet to be sent out on 
 *					the Ethernet medium.  The hardware MAC will take control
 *					and handle CRC generation, collision retransmission and 
 *					other details.
 *
 * Note:			After transmission completes (MACIsTxReady() returns TRUE), 
 *					the packet can be modified and transmitted again by calling 
 *					MACFlush() again.  Until MACPutHeader() or MACPut() is 
 *					called (in the TX data area), the data in the TX buffer 
 *					will not be corrupted.
 *****************************************************************************/
void MACFlush(void)
{
	// Reset the Ethernet TX logic.  This is an errata workaround to 
	// prevent the TXRTS bit from getting stuck set indefinitely, causing the 
	// stack to lock up under certain bad conditions.
	ECON1bits.TXRST = 1;
	ECON1bits.TXRST = 0;	

	// Wait at least 1.6us after TX Reset before setting TXRTS.
	// If you don't wait long enough, the TX logic won't be finished resetting.
	{volatile BYTE i = 8; while(i--);}	
	EIRbits.TXERIF = 0;

	// Start the transmission
	// After transmission completes (MACIsTxReady() returns TRUE), the packet 
	// can be modified and transmitted again by calling MACFlush() again.
	// Until MACPutHeader() is called, the data in the TX buffer will not be 
	// corrupted.
    ECON1bits.TXRTS = 1;
	wTXWatchdog = TickGet();
}


/******************************************************************************
 * Function:        void MACSetReadPtrInRx(WORD offset)
 *
 * PreCondition:    A packet has been obtained by calling MACGetHeader() and 
 *					getting a TRUE result.
 *
 * Input:           offset: WORD specifying how many bytes beyond the Ethernet 
 *							header's type field to relocate the SPI read 
 *							pointer.
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        SPI read pointer are updated.  All calls to 
 *					MACGet() and MACGetArray() will use these new values.
 *
 * Note:			RXSTOP must be statically defined as being > RXSTART for 
 *					this function to work correctly.  In other words, do not 
 *					define an RX buffer which spans the 0x1FFF->0x0000 memory
 *					boundary.
 *****************************************************************************/
void MACSetReadPtrInRx(WORD offset)
{
	WORD_VAL ReadPT;

	// Determine the address of the beginning of the entire packet
	// and adjust the address to the desired location
	ReadPT.Val = CurrentPacketLocation.Val + sizeof(ENC_PREAMBLE) + offset;
	
	// Since the receive buffer is circular, adjust if a wraparound is needed
	if(ReadPT.Val > RXSTOP)
		ReadPT.Val -= RXSIZE;
	
	// Set the read pointer to the new calculated value
	ERDPTL = ReadPT.v[0];
	ERDPTH = ReadPT.v[1];
}


/******************************************************************************
 * Function:        WORD MACSetWritePtr(WORD Address)
 *
 * PreCondition:    None
 *
 * Input:           Address: Address to seek to
 *
 * Output:          WORD: Old EWRPT location
 *
 * Side Effects:    None
 *
 * Overview:        SPI write pointer is updated.  All calls to 
 *					MACPut() and MACPutArray() will use this new value.
 *
 * Note:			None
 *****************************************************************************/
WORD MACSetWritePtr(WORD address)
{
	WORD oldVal;

	oldVal = EWRPT;
	EWRPT = address;
	return oldVal;
}

/******************************************************************************
 * Function:        WORD MACSetReadPtr(WORD Address)
 *
 * PreCondition:    None
 *
 * Input:           Address: Address to seek to
 *
 * Output:          WORD: Old ERDPT value
 *
 * Side Effects:    None
 *
 * Overview:        SPI write pointer is updated.  All calls to 
 *					MACPut() and MACPutArray() will use this new value.
 *
 * Note:			None
 *****************************************************************************/
WORD MACSetReadPtr(WORD address)
{
	WORD oldVal;

	oldVal = ERDPT;
	ERDPT = address;
	return oldVal;
}


/******************************************************************************
 * Function:        WORD MACCalcRxChecksum(WORD offset, WORD len)
 *
 * PreCondition:    None
 *
 * Input:           offset	- Number of bytes beyond the beginning of the 
 *							Ethernet data (first byte after the type field) 
 *							where the checksum should begin
 *					len		- Total number of bytes to include in the checksum
 *
 * Output:          16-bit checksum as defined by RFC 793.
 *
 * Side Effects:    None
 *
 * Overview:        This function performs a checksum calculation in the MAC
 *                  buffer itself
 *
 * Note:            None
 *****************************************************************************/
WORD MACCalcRxChecksum(WORD offset, WORD len)
{
	WORD temp;
	WORD RDSave;

	// Add the offset requested by firmware plus the Ethernet header
	temp = CurrentPacketLocation.Val + sizeof(ENC_PREAMBLE) + offset;
	if(temp > RXSTOP)		// Adjust value if a wrap is needed
	{
		temp -= RXSIZE;
	}

	RDSave = ERDPT;
	ERDPT = temp;
	temp = CalcIPBufferChecksum(len);
	ERDPT = RDSave;

	return temp;
}


/******************************************************************************
 * Function:        WORD CalcIPBufferChecksum(WORD len)
 *
 * PreCondition:    Read buffer pointer set to starting of checksum data
 *
 * Input:           len: Total number of bytes to calculate the checksum over. 
 *						 The first byte included in the checksum is the byte 
 *						 pointed to by ERDPT, which is updated by calls to 
 *						 MACGet(), MACSetRxBuffer(), MACSetTxBuffer(), etc.
 *
 * Output:          16-bit checksum as defined by RFC 793
 *
 * Side Effects:    None
 *
 * Overview:        This function performs a checksum calculation in the MAC
 *                  buffer itself.  The MAC has a hardware DMA module 
 *					which can calculate the checksum faster than software, so 
 *					this function replaces the CaclIPBufferChecksum() function 
 *					defined in the helpers.c file.  Through the use of 
 *					preprocessor defines, this replacement is automatic.
 *
 * Note:            This function works either in the RX buffer area or the TX
 *					buffer area.  No validation is done on the len parameter.
 *****************************************************************************/
/*
WORD CalcIPBufferChecksum(WORD len)
{
	WORD_VAL temp;

	// Take care of special cases which the DMA cannot be used for
	if(len == 0u)
	{
		return 0xFFFF;
	}
	else if(len == 1u)
	{
		return ~((WORD)MACGet());
	}
		

	// Set the DMA starting address to the RAM read pointer value
    temp.Val = ERDPT;
    EDMAST = temp.Val;
	
	// See if we are calculating a checksum within the RX buffer (where 
	// wrapping rules apply) or TX/unused area (where wrapping rules are 
	// not applied)
#if RXSTART == 0
	if(temp.Val <= RXSTOP)
#else
	if(temp.Val >= RXSTART && temp.Val <= RXSTOP)
#endif
	{
		// Calculate the DMA ending address given the starting address and len 
		// parameter.  The DMA will follow the receive buffer wrapping boundary.
		temp.Val += len-1;
		if(temp.Val > RXSTOP)
		{
			temp.Val -= RXSIZE;
		}
	}
	else
	{
		temp.Val += len-1;
	}	

	// Write the DMA end address
    EDMAND = temp.Val;
	
	// Begin the DMA checksum calculation and wait until it is finished
    ECON1bits.CSUMEN = 1;
    ECON1bits.DMAST = 1;
    while(ECON1bits.DMAST);

	// Return the resulting good stuff
	return (((WORD)EDMACSL)<<8) | EDMACSH;
}
*/

/******************************************************************************
 * Function:        WORD CalcIPBufferChecksum(WORD len)
 *
 * PreCondition:    Read buffer pointer set to starting of checksum data
 *
 * Input:           len: Total number of bytes to calculate the checksum over. 
 *						 The first byte included in the checksum is the byte 
 *						 pointed to by ERDPT, which is updated by calls to 
 *						 MACSetReadPtr(), MACGet(), MACGetArray(), 
 *						 MACGetHeader(), etc.
 *
 * Output:          16-bit checksum as defined by RFC 793
 *
 * Side Effects:    None
 *
 * Overview:        This function performs a checksum calculation in the MAC
 *                  buffer itself
 *
 * Note:            This function works either in the RX buffer area or the TX
 *					buffer area.  No validation is done on the len parameter.
 *****************************************************************************/
WORD CalcIPBufferChecksum(WORD len)
{
	WORD Start;
	DWORD_VAL Checksum = {0x00000000ul};
	WORD ChunkLen;
	BYTE DataBuffer[20];	// Must be an even size
	WORD *DataPtr;

	// Save the read pointer starting address
	Start = ERDPT;

	while(len)
	{
		// Obtain a chunk of data (less SPI overhead compared 
		// to requesting one byte at a time)
		ChunkLen = len > sizeof(DataBuffer) ? sizeof(DataBuffer) : len;
		MACGetArray(DataBuffer, ChunkLen);

		len -= ChunkLen;

		// Take care of a last odd numbered data byte
		if(((WORD_VAL*)&ChunkLen)->bits.b0)
		{
			DataBuffer[ChunkLen] = 0x00;
			ChunkLen++;
		}

		// Calculate the checksum over this chunk
		DataPtr = (WORD*)&DataBuffer[0];
		while(ChunkLen)
		{
			Checksum.Val += *DataPtr++;
			ChunkLen -= 2;
		}
	}

	// Restore old read pointer location
	ERDPT = Start;
	
	// Do an end-around carry (one's complement arrithmatic)
	Checksum.Val = (DWORD)Checksum.w[0] + (DWORD)Checksum.w[1];

	// Do another end-around carry in case if the prior add 
	// caused a carry out
	Checksum.w[0] += Checksum.w[1];

	// Return the resulting checksum
	return ~Checksum.w[0];
}

/******************************************************************************
 * Function:        void MACMemCopyAsync(WORD destAddr, WORD sourceAddr, WORD len)
 *
 * PreCondition:    None
 *
 * Input:           destAddr:	Destination address in the Ethernet memory to 
 *								copy to.  If the MSb is set, the current EWRPT 
 *								value will be used instead.
 *					sourceAddr:	Source address to read from.  If the MSb is 
 *								set, the current ERDPT value will be used 
 *								instead.
 *					len:		Number of bytes to copy
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        Bytes are asynchrnously transfered within the buffer.  Call 
 *					MACIsMemCopyDone() to see when the transfer is complete.  
 *
 * Note:            If a prior transfer is already in progress prior to 
 *					calling this function, this function will block until it 
 *					can start this transfer.
 *****************************************************************************/
void MACMemCopyAsync(WORD destAddr, WORD sourceAddr, WORD len)
{
	WORD_VAL ReadSave, WriteSave;
	BOOL UpdateWritePointer = FALSE;
	BOOL UpdateReadPointer = FALSE;

	if(((WORD_VAL*)&destAddr)->bits.b15)
	{
		UpdateWritePointer = TRUE;
		destAddr = EWRPT;
	}
	if(((WORD_VAL*)&sourceAddr)->bits.b15)
	{
		UpdateReadPointer = TRUE;
		sourceAddr = ERDPT;
	}

	// Handle special conditions where len == 0 or len == 1
	// The DMA module is not capable of handling those corner cases
	if(len <= 1u)
	{
		ReadSave.Val = ERDPT;
		WriteSave.Val = EWRPT;
		ERDPT = sourceAddr;
		EWRPT = destAddr;
		while(len--)
			MACPut(MACGet());
		if(!UpdateReadPointer)
		{
			ERDPT = ReadSave.Val;
		}
		if(!UpdateWritePointer)
		{
			EWRPT = WriteSave.Val;
		}
	}
	else
	{
		if(UpdateWritePointer)
		{
			WriteSave.Val = destAddr + len;
			EWRPT = WriteSave.Val;
		}
		len += sourceAddr - 1;
		while(ECON1bits.DMAST);
		EDMAST = sourceAddr;
		EDMADST = destAddr;
		if((sourceAddr <= RXSTOP) && (len > RXSTOP)) //&& (sourceAddr >= RXSTART))
			len -= RXSIZE;
		EDMAND = len;
		ECON1bits.CSUMEN = 0;
		ECON1bits.DMAST = 1;
		while(ECON1bits.DMAST);			// DMA requires that you must not access EDATA while DMA active

		if(UpdateReadPointer)
		{
			len++;
			if((sourceAddr <= RXSTOP) && (len > RXSTOP)) //&& (sourceAddr >= RXSTART))
				len -= RXSIZE;
			ERDPT = len;
		}
	}
}

/*
void MACMemCopyAsync(WORD destAddr, WORD sourceAddr, WORD len)
{
	WORD_VAL ReadSave, WriteSave;
	BOOL UpdateWritePointer = FALSE;
	BOOL UpdateReadPointer = FALSE;

	if(((WORD_VAL*)&destAddr)->bits.b15)
	{
		UpdateWritePointer = TRUE;
		destAddr = EWRPT;
	}
	if(((WORD_VAL*)&sourceAddr)->bits.b15)
	{
		UpdateReadPointer = TRUE;
		sourceAddr = ERDPT;
	}

	ReadSave.Val = ERDPT;
	WriteSave.Val = EWRPT;
	ERDPT = sourceAddr;
	EWRPT = destAddr;
	while(len--)
	{
		MACPut(MACGet());
	}

	if(!UpdateReadPointer)
	{
		ERDPT = ReadSave.Val;
	}
	if(!UpdateWritePointer)
	{
		EWRPT = WriteSave.Val;
	}
}
*/

BOOL MACIsMemCopyDone(void)
{
	return !ECON1bits.DMAST;
}

/******************************************************************************
 * Function:        BYTE MACGet()
 *
 * PreCondition:    ERDPT must point to the place to read from.
 *
 * Input:           None
 *
 * Output:          Byte read from the Ethernet's buffer RAM
 *
 * Side Effects:    None
 *
 * Overview:        MACGet returns the byte pointed to by ERDPT and 
 *					increments ERDPT so MACGet() can be called again.  The 
 *					increment will follow the receive buffer wrapping boundary.
 *
 * Note:            For better performance, implement this function as a macro:
 *					#define MACGet()	(EDATA)
 *****************************************************************************/
BYTE MACGet()
{
    return EDATA;
}//end MACGet


/******************************************************************************
 * Function:        WORD MACGetArray(BYTE *val, WORD len)
 *
 * PreCondition:    ERDPT must point to the place to read from.
 *
 * Input:           *val: Pointer to storage location
 *					len:  Number of bytes to read from the data buffer.
 *
 * Output:          Byte(s) of data read from the data buffer.
 *
 * Side Effects:    None
 *
 * Overview:        Reads several sequential bytes from the data buffer 
 *					and places them into local memory.  ERDPT is incremented 
 *					after each byte, following the same rules as MACGet().
 *
 * Note:            None
 *****************************************************************************/
WORD MACGetArray(BYTE *val, WORD len)
{
    WORD w;
	volatile BYTE i;
    
    w = len;
	if(val)
	{
	    while(w--)
	    {
	        *val++ = EDATA;
	    }
	}
	else
	{
		while(w--)
		{
			i = EDATA;
		}
	}

	return len;
}//end MACGetArray


/******************************************************************************
 * Function:        void MACPut(BYTE val)
 *
 * PreCondition:    EWRPT must point to the location to begin writing.
 *
 * Input:           Byte to write into the Ethernet buffer memory
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        Writes to the EDATA register, which will indirectly 
*					increment EWRPTH:EWRPTL.
 *
 * Note:            None
 *****************************************************************************/
void MACPut(BYTE val)
{
	// Note:  Due to a PIC18F97J60 bug, you must use the MOVFF instruction to 
	// write to EDATA or else the read pointer (ERDPT) will indvertently 
	// increment.a
	PRODL = val;
	#if defined(HI_TECH_C)
		asm("movff	_PRODL, _EDATA");
	#else
		_asm movff	PRODL, EDATA _endasm
	#endif
}//end MACPut


/******************************************************************************
 * Function:        void MACPutArray(BYTE *val, WORD len)
 *
 * PreCondition:    EWRPT must point to the location to begin writing.
 *
 * Input:           *val: Pointer to source of bytes to copy.
 *					len:  Number of bytes to write to the data buffer.
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        MACPutArray writes several sequential bytes to the 
 *					Ethernet buffer RAM.  It performs faster than multiple MACPut()
 *					calls.  EWRPT is incremented by len.
 *
 * Note:            None
 *****************************************************************************/
void MACPutArray(BYTE *val, WORD len)
{
    while(len--)
	{
		// Note:  Due to a PIC18F97J60 bug, you must use the MOVFF instruction to 
		// write to EDATA or else the read pointer (ERDPT) will inadvertently 
		// increment.
		PRODL = *val++;
		#if defined(HI_TECH_C)
			asm("movff	_PRODL, _EDATA");
		#else
			_asm movff	PRODL, EDATA _endasm
		#endif
	}
}//end MACPutArray

void MACPutROMArray(ROM BYTE *val, WORD len)
{
    while(len--)
	{
		// Note:  Due to a PIC18F97J60 bug, you must use the MOVFF instruction to 
		// write to EDATA or else the read pointer (ERDPT) will inadvertently 
		// increment.
		PRODL = *val++;
		#if defined(HI_TECH_C)
			asm("movff	_PRODL, _EDATA");
		#else
			_asm movff	PRODL, EDATA _endasm
		#endif
	}
}//end MACPutROMArray


/******************************************************************************
 * Function:        ReadPHYReg
 *
 * PreCondition:    Ethernet module must be enabled (ECON1.ETHEN = 1).
 *
 * Input:           Address of the PHY register to read from.
 *
 * Output:          16 bits of data read from the PHY register.
 *
 * Side Effects:    None
 *
 * Overview:        ReadPHYReg performs an MII read operation.  While in 
 *					progress, it simply polls the MII BUSY bit wasting time 
 *					(10.24us).
 *
 * Note:            None
 *****************************************************************************/
PHYREG ReadPHYReg(BYTE Register)
{
	PHYREG Result;

	// Set the right address and start the register read operation
    MIREGADR = Register; Nop();
    MICMD = MICMD_MIIRD; Nop();

	// Loop to wait until the PHY register has been read through the MII
	// This requires 10.24us
    while(MISTATbits.BUSY);

	// Stop reading
    MICMD = 0x00; Nop();
	
	// Obtain results and return
    Result.VAL.v[0] = MIRDL;
    Nop();
    Result.VAL.v[1] = MIRDH;

	return Result;
}//end ReadPHYReg


/******************************************************************************
 * Function:        WritePHYReg
 *
 * PreCondition:    Ethernet module must be enabled (ECON1.ETHEN = 1).
 *
 * Input:           Address of the PHY register to write to.
 *					16 bits of data to write to PHY register.
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        WritePHYReg performs an MII write operation.  While in 
 *					progress, it simply polls the MII BUSY bit wasting time 
 *					(10.24us).
 *
 * Note:            None
 *****************************************************************************/
void WritePHYReg(BYTE Register, WORD Data)
{
	BYTE GIESave;

	// Write the register address
	MIREGADR = Register;

	// Write the data through the MIIM interface
	// Order is important: write low byte first, high byte last
	//
	// Due to a silicon problem, you cannot access any register with LSb address 
	// bits of 0x16 between your write to MIWRL and MIWRH or else the value in 
	// MIWRL will be corrupted.  This inline assembly prevents this by copying 
	// the value to PRODH:PRODL first, which is at fixed locations of 
	// 0xFF4:0xFF3.  These addresses have LSb address bits of 0x14 and 0x13.
	// Interrupts must be disabled to prevent arbitrary ISR code from accessing
	// memory with LSb bits of 0x16 and corrupting the MIWRL value.
	PRODL = ((WORD_VAL*)&Data)->v[0];
	PRODH = ((WORD_VAL*)&Data)->v[1];
	GIESave = INTCON & 0xC0;		// Save GIEH and GIEL bits
	INTCON &= 0x3F;					// Clear INTCONbits.GIEH and INTCONbits.GIEL
	#if defined(HI_TECH_C)
		asm("movff	_PRODL, _MIWRL");
		asm("nop");
		asm("movff	_PRODH, _MIWRH");
	#else
		_asm
		movff	PRODL, MIWRL
		nop
		movff	PRODH, MIWRH
		_endasm
	#endif
	INTCON |= GIESave;				// Restore GIEH and GIEL value

	// Wait until the PHY register has been written
	// This operation requires 10.24us
    while(MISTATbits.BUSY);
}//end WritePHYReg


/******************************************************************************
 * Function:        void MACPowerDown(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        MACPowerDown disables the Ethernet module.
 *					All MAC and PHY registers should not be accessed.
 *
 * Note:            Normally, this function would be called before putting the 
 *					PIC to sleep.  If a packet is being transmitted while this 
 *					function is called, this function will block until it is 
 *					it complete. If anything is being received, it will be 
 *					completed.
 *					
 *					The Ethernet module will continue to draw significant 
 *					power in sleep mode if this function is not called first.
 *****************************************************************************/
void MACPowerDown(void)
{
	// Disable packet reception
	ECON1bits.RXEN = 0;

	// Make sure any last packet which was in-progress when RXEN was cleared 
	// is completed
	while(ESTATbits.RXBUSY);

	// If a packet is being transmitted, wait for it to finish
	while(ECON1bits.TXRTS);
	
	// Disable the Ethernet module
	ECON2bits.ETHEN = 0;
}//end MACPowerDown

/******************************************************************************
 * Function:        void MACPowerUp(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        MACPowerUp returns the Ethernet module back to normal operation
 *					after a previous call to MACPowerDown().  Calling this 
 *					function when already powered up will have no effect.
 *
 * Note:            If a link partner is present, it will take 10s of 
 *					milliseconds before a new link will be established after
 *					waking up.  While not linked, packets which are 
 *					transmitted will most likely be lost.  MACIsLinked() can 
 *					be called to determine if a link is established.
 *****************************************************************************/
void MACPowerUp(void)
{	
	// Power up the Ethernet module
	ECON2bits.ETHEN = 1;

	// Wait for PHY to become ready
	while(!ESTATbits.PHYRDY)

	// Enable packet reception
	ECON1bits.RXEN = 1;
}//end MACPowerUp



/******************************************************************************
 * Function:        void SetRXHashTableEntry(MAC_ADDR DestMACAddr)
 *
 * PreCondition:    SPI bus must be initialized (done in MACInit()).
 *
 * Input:           DestMACAddr: 6 byte group destination MAC address to allow 
 *								 through the Hash Table Filter
 *
 * Output:          Sets the appropriate bit in the EHT* registers to allow 
 *					packets sent to DestMACAddr to be received if the Hash 
 *					Table receive filter is enabled
 *
 * Side Effects:    None
 *
 * Overview:        Calculates a CRC-32 using polynomial 0x4C11DB7 and then, 
 *					using bits 28:23 of the CRC, sets the appropriate bit in 
 *					the EHT* registers
 *
 * Note:            This code is commented out to save code space on systems 
 *					that do not need this function.  Change the "#if 0" line 
 *					to "#if 1" to uncomment it.
 *****************************************************************************/
#if 0
void SetRXHashTableEntry(MAC_ADDR DestMACAddr)
{
	DWORD_VAL CRC = {0xFFFFFFFF};
	BYTE *HTRegister;
	BYTE i, j;

	// Calculate a CRC-32 over the 6 byte MAC address 
	// using polynomial 0x4C11DB7
	for(i = 0; i < sizeof(MAC_ADDR); i++)
	{
		BYTE  crcnext;
	
		// shift in 8 bits
		for(j = 0; j < 8; j++)
		{
			crcnext = 0;
			if(((BYTE_VAL*)&(CRC.v[3]))->bits.b7)
				crcnext = 1;
			crcnext ^= (((BYTE_VAL*)&DestMACAddr.v[i])->bits.b0);
	
			CRC.Val <<= 1;
			if(crcnext)
				CRC.Val ^= 0x4C11DB7;
			// next bit
			DestMACAddr.v[i] >>= 1;
		}
	}
	
	// CRC-32 calculated, now extract bits 28:23
	// Bits 25:23 define where within the Hash Table byte the bit needs to be set
	// Bits 28:26 define which of the 8 Hash Table bytes that bits 25:23 apply to
	i = CRC.v[3] & 0x1F;
	HTRegister = (i >> 2) + &EHT0;
	i = (i << 1) & 0x06;
	((BYTE_VAL*)&i)->bits.b0 = ((BYTE_VAL*)&CRC.v[2])->bits.b7;
	
	// Set the proper bit in the Hash Table
	*HTRegister |= 1<<i;
}
#endif



#endif //#if (defined(__18F97J60) || defined(__18F96J65) || defined(__18F96J60) || defined(__18F87J60) || defined(__18F86J65) || defined(__18F86J60) || defined(__18F67J60) || defined(__18F66J65) || defined(__18F66J60)) || defined(HI_TECH_C)
