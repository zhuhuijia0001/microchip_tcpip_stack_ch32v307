/*********************************************************************
 *
 *  Main Application Entry Point and TCP/IP Stack Demo
 *  Module for Microchip TCP/IP Stack
 *   -Demonstrates how to call and use the Microchip TCP/IP stack
 *	 -Reference: Microchip TCP/IP Stack Help (TCPIP Stack Help.chm)
 *
 *********************************************************************
 * FileName:        MainDemo.c
 * Dependencies:    TCPIP.h
 * Processor:       PIC18, PIC24F, PIC24H, dsPIC30F, dsPIC33F, PIC32
 * Compiler:        Microchip C32 v1.11b or higher
 *					Microchip C30 v3.24 or higher
 *					Microchip C18 v3.36 or higher
 * Company:         Microchip Technology, Inc.
 *
 * Software License Agreement
 *
 * Copyright (C) 2002-2010 Microchip Technology Inc.  All rights
 * reserved.
 *
 * Microchip licenses to you the right to use, modify, copy, and
 * distribute:
 * (i)  the Software when embedded on a Microchip microcontroller or
 *      digital signal controller product ("Device") which is
 *      integrated into Licensee's product; or
 * (ii) ONLY the Software driver source files ENC28J60.c, ENC28J60.h,
 *		ENCX24J600.c and ENCX24J600.h ported to a non-Microchip device
 *		used in conjunction with a Microchip ethernet controller for
 *		the sole purpose of interfacing with the ethernet controller.
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
 * Author              Date         Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Nilesh Rajbharti		4/19/01		Original (Rev. 1.0)
 * Nilesh Rajbharti		2/09/02		Cleanup
 * Nilesh Rajbharti		5/22/02		Rev 2.0 (See version.log for detail)
 * Nilesh Rajbharti		7/9/02		Rev 2.1 (See version.log for detail)
 * Nilesh Rajbharti		4/7/03		Rev 2.11.01 (See version log for detail)
 * Howard Schlunder		10/1/04		Beta Rev 0.9 (See version log for detail)
 * Howard Schlunder		10/8/04		Beta Rev 0.9.1 Announce support added
 * Howard Schlunder		11/29/04	Beta Rev 0.9.2 (See version log for detail)
 * Howard Schlunder		2/10/05		Rev 2.5.0
 * Howard Schlunder		1/5/06		Rev 3.00
 * Howard Schlunder		1/18/06		Rev 3.01 ENC28J60 fixes to TCP,
 *									UDP and ENC28J60 files
 * Howard Schlunder		3/01/06		Rev. 3.16 including 16-bit micro support
 * Howard Schlunder		4/12/06		Rev. 3.50 added LCD for Explorer 16
 * Howard Schlunder		6/19/06		Rev. 3.60 finished dsPIC30F support, added PICDEM.net 2 support
 * Howard Schlunder		8/02/06		Rev. 3.75 added beta DNS, NBNS, and HTTP client (GenericTCPClient.c) services
 * Howard Schlunder		12/28/06	Rev. 4.00RC added SMTP, Telnet, substantially modified TCP layer
 * Howard Schlunder		04/09/07	Rev. 4.02 added TCPPerformanceTest, UDPPerformanceTest, Reboot and fixed some bugs
 * Howard Schlunder		xx/xx/07	Rev. 4.03
 * HSchlunder & EWood	08/27/07	Rev. 4.11
 * HSchlunder & EWood	10/08/07	Rev. 4.13
 * HSchlunder & EWood	11/06/07	Rev. 4.16
 * HSchlunder & EWood	11/08/07	Rev. 4.17
 * HSchlunder & EWood	11/12/07	Rev. 4.18
 * HSchlunder & EWood	02/11/08	Rev. 4.19
 * HSchlunder & EWood   04/26/08    Rev. 4.50 Moved most code to other files for clarity
 * KHesky               07/07/08    Added ZG2100-specific support
 * HSchlunder & EWood   07/24/08    Rev. 4.51
 * Howard Schlunder		11/10/08    Rev. 4.55
 * Howard Schlunder		04/14/09    Rev. 5.00
 * Howard Schlunder		07/10/09    Rev. 5.10
 * Howard Schlunder		11/18/09    Rev. 5.20
 * Howard Schlunder		04/28/10    Rev. 5.25
 * Howard Schlunder		10/19/10	Rev. 5.31
 ********************************************************************/

/*
 * This macro uniquely defines this file as the main entry point.
 * There should only be one such definition in the entire project,
 * and this file must define the AppConfig variable as described below.
 */
#define THIS_IS_STACK_APPLICATION

// Include all headers for any enabled TCPIP Stack functions
#include "TCPIP Stack/TCPIP.h"

// Include functions specific to this stack application
#include "MainDemo.h"

// Declare AppConfig structure and some other supporting stack variables
BYTE AN0String[8];
BYTE myDHCPBindCount = 0xFF;
#if !defined(STACK_USE_DHCP_CLIENT)
#define DHCPBindCount	(1)
#endif

#include "Driver.h"
#include "Task.h"

#include "trace.h"

//
// Main application entry point.
//

/****************************************************************************
  Function:
    static void InitializeBoard(void)

  Description:
    This routine initializes the hardware.  It is a generic initialization
    routine for many of the Microchip development boards, using definitions
    in HardwareProfile.h to determine specific initialization.

  Precondition:
    None

  Parameters:
    None - None

  Returns:
    None

  Remarks:
    None
  ***************************************************************************/
static void InitializeBoard(void)
{
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

	trace_init();

	EthLedInit();

	DelayInit();

	RCC_PLL3Cmd(DISABLE);
    RCC_PREDIV2Config(RCC_PREDIV2_Div2);
    RCC_PLL3Config(RCC_PLL3Mul_15);
    RCC_MCOConfig(RCC_MCO_NoClock);
    RCC_PLL3Cmd(ENABLE);

    DelayUs(100);

    while (RCC_GetFlagStatus(RCC_FLAG_PLL3RDY) == RESET);
    printf("PLL3 is ready.\n");

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_RNG, ENABLE);
    RNG_Cmd(ENABLE);

#if defined(MPFS_USE_EEPROM)
    XEEInit();
#elif defined(MPFS_USE_SPI_FLASH)
    SPIFlashInit();
#endif

    //DelayMs(100);

	printf("SystemClk:%d\r\n",SystemCoreClock);
}

/*********************************************************************
 * Function:        void InitAppConfig(void)
 *
 * PreCondition:    MPFSInit() is already called.
 *
 * Input:           None
 *
 * Output:          Write/Read non-volatile config variables.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 ********************************************************************/
static void InitAppConfig(void)
{
#if 1
	if (!LoadParameters())
	{
		LoadDefaultParameters();

		SaveParameters();
	}
#else
	LoadDefaultParameters();
#endif
}

int main(void)
{
	InitVariables();

	// Initialize application specific hardware
	InitializeBoard();

	// Initialize Stack and application related NV variables into AppConfig.
	InitAppConfig();

	// Initialize stack-related hardware components that may be
	// required by the UART configuration routines
	TickInit();
#if defined(STACK_USE_MPFS) || defined(STACK_USE_MPFS2)
	MPFSInit();
#endif

	// Initialize core stack layers (MAC, ARP, TCP, UDP) and
	// application modules (HTTP, SNMP, etc.)
	StackInit();

	// Initialize any application-specific modules or functions/
	// For this demo application, this only includes the
	// UART 2 TCP Bridge
#if defined(STACK_USE_UART2TCP_BRIDGE)
	UART2TCPBridgeInit();
#endif

	// Now that all items are initialized, begin the co-operative
	// multitasking loop.  This infinite loop will continuously
	// execute all stack-related tasks, as well as your own
	// application's functions.  Custom functions should be added
	// at the end of this loop.
	// Note that this is a "co-operative mult-tasking" mechanism
	// where every task performs its tasks (whether all in one shot
	// or part of it) and returns so that other tasks can do their
	// job.
	// If a task needs very long time to do its job, it must be broken
	// down into smaller pieces so that other tasks can have CPU time.
	while(1)
	{
		// This task performs normal stack task including checking
		// for incoming packet, type of packet and calling
		// appropriate stack entity to process it.
		StackTask();

		// This tasks invokes each of the core stack application tasks
		StackApplications();
	
		// Process application specific tasks here.
		// For this demo app, this will include the Generic TCP
		// client and servers, and the SNMP, Ping, and SNMP Trap
		// demos.  Following that, we will process any IO from
		// the inputs on the board itself.
		// Any custom modules or processing you need to do should
		// go here.

		
#if defined(STACK_USE_GENERIC_TCP_CLIENT_EXAMPLE)
		GenericTCPClient();
#endif

#if defined(STACK_USE_GENERIC_TCP_SERVER_EXAMPLE)
		GenericTCPServer();
#endif

#if defined(STACK_USE_SMTP_CLIENT)
		SMTPDemo();
#endif

#if defined(STACK_USE_ICMP_CLIENT)
		PingDemo();
#endif

#if defined(STACK_USE_SNMP_SERVER) && !defined(SNMP_TRAP_DISABLED)
		//User should use one of the following SNMP demo
		// This routine demonstrates V1 or V2 trap formats with one variable binding.
		SNMPTrapDemo();
#endif

#if defined(STACK_USE_BERKELEY_API)
		BerkeleyTCPClientDemo();
		BerkeleyTCPServerDemo();
		BerkeleyUDPClientDemo();
#endif

		static IP_ADDR saveIp = { 0 };
		if (AppConfig.MyIPAddr.Val != saveIp.Val)
		{
		    TRACE("ip:%d.%d.%d.%d\n", AppConfig.MyIPAddr.v[0], AppConfig.MyIPAddr.v[1], AppConfig.MyIPAddr.v[2], AppConfig.MyIPAddr.v[3]);

		    saveIp.Val = AppConfig.MyIPAddr.Val;
		}
	}

	return 0;
}

