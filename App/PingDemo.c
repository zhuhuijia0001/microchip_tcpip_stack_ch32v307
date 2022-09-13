/*********************************************************************
 *
 *  ICMP Client Demo (Ping)
 *
 *********************************************************************
 * FileName:        PingDemo.c
 * Dependencies:    TCP/IP stack
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
 * Author               Date      Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * E. Wood     			4/26/08	  Moved from MainDemo.c
 ********************************************************************/
#define __PINGDEMO_C

#include "TCPIP Stack/TCPIP.h"
#include "MainDemo.h"

/*****************************************************************************
  Function:
	void PingDemo(void)

  Summary:
	Demonstrates use of the ICMP (Ping) client.
	
  Description:
	This function implements a simple ICMP client.  The function is called
	periodically by the stack, and it checks if BUTTON0 has been pressed.  
	If the button is pressed, the function sends an ICMP Echo Request (Ping)
	to a Microchip web server.  The round trip time is displayed on the UART
	when the response is received.
	
	This function can be used as a model for applications requiring Ping 
	capabilities to check if a host is reachable.

  Precondition:
	TCP is initialized.

  Parameters:
	None

  Returns:
  	None
  ***************************************************************************/
#if defined(STACK_USE_ICMP_CLIENT)
void PingDemo(void)
{
	static enum
	{
		SM_HOME = 0,
		SM_GET_RESPONSE
	} PingState = SM_HOME;
	static TICK Timer;
	LONG ret;
	IP_ADDR RemoteIP;

	switch(PingState)
	{
		case SM_HOME:
			// Send a ping request out if the user pushes BUTTON0 (right-most one)
			//if(BUTTON0_IO == 0u)
			{
				// Don't ping flood: wait at least 1 second between ping requests
				if(TickGet() - Timer > 1ul*TICK_SECOND)
				{
					// Obtain ownership of the ICMP module
					if(ICMPBeginUsage())
					{
						Timer = TickGet();
						PingState = SM_GET_RESPONSE;
	
						// Send the ping request to 63.217.8.154 (ww1.microchip.com)
						/*RemoteIP.v[0] = 63;
						RemoteIP.v[1] = 217;
						RemoteIP.v[2] = 8;
						RemoteIP.v[3] = 154;
						*/
						RemoteIP.v[0] = 192;
                        RemoteIP.v[1] = 168;
                        RemoteIP.v[2] = 0;
                        RemoteIP.v[3] = 1;
						ICMPSendPing(RemoteIP.Val);
					}
				}
			}
			break;

		case SM_GET_RESPONSE:
			// Get the status of the ICMP module
			ret = ICMPGetReply();					
			if(ret == -2)
			{
				// Do nothing: still waiting for echo
				break;
			}
			else if(ret == -1)
			{
				// Request timed out
				#if defined(USE_LCD)
				memcpypgm2ram((void*)&LCDText[16], (ROM void *)"Ping timed out", 15);
				LCDUpdate();
				#endif
				PingState = SM_HOME;
			}
			else
			{
				// Echo received.  Time elapsed is stored in ret (units of TICK).
				#if defined(USE_LCD)
				memcpypgm2ram((void*)&LCDText[16], (ROM void *)"Reply: ", 7);
				uitoa((WORD)TickConvertToMilliseconds((DWORD)ret), &LCDText[16+7]);
				strcatpgm2ram((char*)&LCDText[16+7], (ROM char*)"ms");
				LCDUpdate();
				#endif
				PingState = SM_HOME;

				TRACE("ping ok\n");
			}
			
			// Finished with the ICMP module, release it so other apps can begin using it
			ICMPEndUsage();
			break;
	}
}
#endif	//#if defined(STACK_USE_ICMP_CLIENT)
