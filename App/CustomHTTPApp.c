/*********************************************************************
 *
 *  Application to Demo HTTP2 Server
 *  Support for HTTP2 module in Microchip TCP/IP Stack
 *	 -Implements the application 
 *	 -Reference: RFC 1002
 *
 *********************************************************************
 * FileName:        CustomHTTPApp.c
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
 * Author               Date    Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Elliott Wood     	6/18/07	Original
 ********************************************************************/
#define __CUSTOMHTTPAPP_C

#include "TCPIP Stack/TCPIP.h"

#include "AppConfig.h"

#if defined(STACK_USE_HTTP2_SERVER)

/****************************************************************************
  Section:
	Function Prototypes and Memory Globalizers
  ***************************************************************************/
#if defined(HTTP_USE_POST)
	#if defined(USE_LCD)
		static HTTP_IO_RESULT HTTPPostLCD(void);
	#endif
	#if defined(STACK_USE_HTTP_MD5_DEMO)
		#if !defined(STACK_USE_MD5)
			#error The HTTP_MD5_DEMO requires STACK_USE_MD5
		#endif
		static HTTP_IO_RESULT HTTPPostMD5(void);
	#endif
	#if defined(STACK_USE_HTTP_APP_RECONFIG)
		extern APP_CONFIG AppConfig;
		static HTTP_IO_RESULT HTTPPostConfig(void);
	#endif
	#if defined(STACK_USE_HTTP_EMAIL_DEMO)
		#if !defined(STACK_USE_SMTP_CLIENT)
			#error The HTTP_EMAIL_DEMO requires STACK_USE_SMTP_CLIENT
		#endif
		static HTTP_IO_RESULT HTTPPostEmail(void);
	#endif
	#if defined(STACK_USE_DYNAMICDNS_CLIENT)
		static HTTP_IO_RESULT HTTPPostDDNSConfig(void);
	#endif
#endif

// RAM allocated for DDNS parameters
#if defined(STACK_USE_DYNAMICDNS_CLIENT)
	static BYTE DDNSData[100];
#endif


// Sticky status message variable.
// This is used to indicated whether or not the previous POST operation was 
// successful.  The application uses these to store status messages when a 
// POST operation redirects.  This lets the application provide status messages
// after a redirect, when connection instance data has already been lost.
static BOOL lastSuccess = FALSE;

// Stick status message variable.  See lastSuccess for details.
static BOOL lastFailure = FALSE;

/****************************************************************************
  Section:
	Authorization Handlers
  ***************************************************************************/
  
/*****************************************************************************
  Function:
	BYTE HTTPNeedsAuth(BYTE* cFile)
	
  Internal:
  	See documentation in the TCP/IP Stack API or HTTP2.h for details.
  ***************************************************************************/
#if defined(HTTP_USE_AUTHENTICATION)
BYTE HTTPNeedsAuth(BYTE* cFile)
{
	// If the filename begins with the folder "protect", then require auth
	if(memcmppgm2ram(cFile, (ROM void*)"index.htm", 9) == 0)
		return 0x00;		// Authentication will be needed later

	#if defined(HTTP_MPFS_UPLOAD_REQUIRES_AUTH)
	if(memcmppgm2ram(cFile, (ROM void*)HTTP_MPFS_UPLOAD, strlenpgm(HTTP_MPFS_UPLOAD)) == 0)
		return 0x00;
	#endif

	// You can match additional strings here to password protect other files.
	// You could switch this and exclude files from authentication.
	// You could also always return 0x00 to require auth for all files.
	// You can return different values (0x00 to 0x79) to track "realms" for below.

	return 0x80;			// No authentication required
}
#endif

/*****************************************************************************
  Function:
	BYTE HTTPCheckAuth(BYTE* cUser, BYTE* cPass)
	
  Internal:
  	See documentation in the TCP/IP Stack API or HTTP2.h for details.
  ***************************************************************************/
#if defined(HTTP_USE_AUTHENTICATION)
BYTE HTTPCheckAuth(BYTE* cUser, BYTE* cPass)
{
	if(strcmppgm2ram((char *)cUser,(ROM char *)"admin") == 0
		&& strcmppgm2ram((char *)cPass, (ROM char *)"admin") == 0)
		return 0x80;		// We accept this combination
	
	// You can add additional user/pass combos here.
	// If you return specific "realm" values above, you can base this 
	//   decision on what specific file or folder is being accessed.
	// You could return different values (0x80 to 0xff) to indicate 
	//   various users or groups, and base future processing decisions
	//   in HTTPExecuteGet/Post or HTTPPrint callbacks on this value.
	
	return 0x00;			// Provided user/pass is invalid
}
#endif

/****************************************************************************
  Section:
	GET Form Handlers
  ***************************************************************************/
  
/*****************************************************************************
  Function:
	HTTP_IO_RESULT HTTPExecuteGet(void)
	
  Internal:
  	See documentation in the TCP/IP Stack API or HTTP2.h for details.
  ***************************************************************************/
HTTP_IO_RESULT HTTPExecuteGet(void)
{
	BYTE *ptr;
	BYTE filename[20];
	
	// Load the file name
	// Make sure BYTE filename[] above is large enough for your longest name
	MPFSGetFilename(curHTTP.file, filename, 20);
	
	// If its the forms.htm page
	if(!memcmppgm2ram(filename, "index.htm", 9))
	{
		// Seek out each of the four LED strings, and if it exists set the LED states
		ptr = HTTPGetROMArg(curHTTP.data, (ROM BYTE *)"led7");
		if(ptr)
		{
		    //LED7_IO = (*ptr == '1');
		}

		ptr = HTTPGetROMArg(curHTTP.data, (ROM BYTE *)"led6");
		if(ptr)
		{
		    //LED6_IO = (*ptr == '1');
		}

		ptr = HTTPGetROMArg(curHTTP.data, (ROM BYTE *)"led5");
		if(ptr)
		{
		    //LED5_IO = (*ptr == '1');
		}

		ptr = HTTPGetROMArg(curHTTP.data, (ROM BYTE *)"led4");
		if(ptr)
		{
		    //LED4_IO = (*ptr == '1');
		}

		ptr = HTTPGetROMArg(curHTTP.data, (ROM BYTE *)"led3");
		if(ptr)
		{
		    //LED3_IO = (*ptr == '1');
		}

		ptr = HTTPGetROMArg(curHTTP.data, (ROM BYTE *)"led2");
		if(ptr)
		{
		    //LED2_IO = (*ptr == '1');
		}

		ptr = HTTPGetROMArg(curHTTP.data, (ROM BYTE *)"led1");
		if(ptr)
		{
		    //LED1_IO = (*ptr == '1');
		}

		ptr = HTTPGetROMArg(curHTTP.data, (ROM BYTE *)"led0");
		if(ptr)
		{
		    //LED0_IO = (*ptr == '1');
		}

		ptr = HTTPGetROMArg(curHTTP.data, (ROM BYTE *)"dev0");
		if(ptr)
		{
		    //RELAY1_IO = (*ptr == '1');
		}

		ptr = HTTPGetROMArg(curHTTP.data, (ROM BYTE *)"dev1");
		if(ptr)
		{
		    //RELAY2_IO = (*ptr == '1');
		}
	}
	
	// If it's the LED updater file
	else if(!memcmppgm2ram(filename, "cookies.htm", 11))
	{
		// This is very simple.  The names and values we want are already in
		// the data array.  We just set the hasArgs value to indicate how many
		// name/value pairs we want stored as cookies.
		// To add the second cookie, just increment this value.
		// remember to also add a dynamic variable callback to control the printout.
		curHTTP.hasArgs = 0x01;
	}
	return HTTP_IO_DONE;
}


/****************************************************************************
  Section:
	POST Form Handlers
  ***************************************************************************/
#if defined(HTTP_USE_POST)

/*****************************************************************************
  Function:
	HTTP_IO_RESULT HTTPExecutePost(void)
	
  Internal:
  	See documentation in the TCP/IP Stack API or HTTP2.h for details.
  ***************************************************************************/
HTTP_IO_RESULT HTTPExecutePost(void)
{
	// Resolve which function to use and pass along
	BYTE filename[20];
	
	// Load the file name
	// Make sure BYTE filename[] above is large enough for your longest name
	MPFSGetFilename(curHTTP.file, filename, sizeof(filename));
	
#if defined(USE_LCD)
	if(!memcmppgm2ram(filename, "index.htm", 9))
	{
		 b_NewMessage = 1;
		return HTTPPostLCD();
	}
#endif

#if defined(STACK_USE_HTTP_MD5_DEMO)
	if(!memcmppgm2ram(filename, "upload.htm", 10))
		return HTTPPostMD5();
#endif

#if defined(STACK_USE_HTTP_APP_RECONFIG)
	if(!memcmppgm2ram(filename, "protect/config.htm", 18))
		return HTTPPostConfig();
#endif

#if defined(STACK_USE_SMTP_CLIENT)
	if(!strcmppgm2ram((char*)filename, (ROM char*)"email/index.htm"))
		return HTTPPostEmail();
#endif
	
#if defined(STACK_USE_DYNAMICDNS_CLIENT)
	if(!strcmppgm2ram((char*)filename, "dyndns/index.htm"))
		return HTTPPostDDNSConfig();
#endif

	return HTTP_IO_DONE;
}

/*****************************************************************************
  Function:
	static HTTP_IO_RESULT HTTPPostLCD(void)

  Summary:
	Processes the LCD form on forms.htm

  Description:
	Locates the 'lcd' parameter and uses it to update the text displayed
	on the board's LCD display.
	
	This function has four states.  The first reads a name from the data
	string returned as part of the POST request.  If a name cannot
	be found, it returns, asking for more data.  Otherwise, if the name 
	is expected, it reads the associated value and writes it to the LCD.  
	If the name is not expected, the value is discarded and the next name 
	parameter is read.
	
	In the case where the expected string is never found, this function 
	will eventually return HTTP_IO_NEED_DATA when no data is left.  In that
	case, the HTTP2 server will automatically trap the error and issue an
	Internal Server Error to the browser.

  Precondition:
	None

  Parameters:
	None

  Return Values:
  	HTTP_IO_DONE - the parameter has been found and saved
  	HTTP_IO_WAITING - the function is pausing to continue later
  	HTTP_IO_NEED_DATA - data needed by this function has not yet arrived
  ***************************************************************************/
#if defined(USE_LCD)
static HTTP_IO_RESULT HTTPPostLCD(void)
{
	BYTE* cDest;
	
	#define SM_POST_LCD_READ_NAME		(0u)
	#define SM_POST_LCD_READ_VALUE		(1u)
	
	switch(curHTTP.smPost)
	{
		// Find the name
		case SM_POST_LCD_READ_NAME:
		
			// Read a name
			if(HTTPReadPostName(curHTTP.data, HTTP_MAX_DATA_LEN) == HTTP_READ_INCOMPLETE)
				return HTTP_IO_NEED_DATA;

			curHTTP.smPost = SM_POST_LCD_READ_VALUE;
			// No break...continue reading value
		
		// Found the value, so store the LCD and return
		case SM_POST_LCD_READ_VALUE:
					
			// If value is expected, read it to data buffer,
			// otherwise ignore it (by reading to NULL)	
			if(!strcmppgm2ram((char*)curHTTP.data, (ROM char*)"lcd"))
				cDest = curHTTP.data;
			else
				cDest = NULL;
			
			// Read a value string
			if(HTTPReadPostValue(cDest, HTTP_MAX_DATA_LEN) == HTTP_READ_INCOMPLETE)
				return HTTP_IO_NEED_DATA;
			
			// If this was an unexpected value, look for a new name
			if(!cDest)
			{
				curHTTP.smPost = SM_POST_LCD_READ_NAME;
				break;
			}
			
			// Copy up to 32 characters to the LCD
			if(strlen((char*)cDest) < 32u)
			{
				memset(LCDText, ' ', 32);
				strcpy((char*)LCDText, (char*)cDest);
			}
			else
			{
				memcpy(LCDText, (void *)cDest, 32);
			}
			LCDUpdate();
			
			// This is the only expected value, so callback is done
			strcpypgm2ram((char*)curHTTP.data, (ROM void*)"/index.htm");
			curHTTP.httpStatus = HTTP_REDIRECT;
			return HTTP_IO_DONE;
	}
	
	// Default assumes that we're returning for state machine convenience.
	// Function will be called again later.
	return HTTP_IO_WAITING;
}
#endif

/*****************************************************************************
  Function:
	static HTTP_IO_RESULT HTTPPostConfig(void)

  Summary:
	Processes the configuration form on config/index.htm

  Description:
	Accepts configuration parameters from the form, saves them to a
	temporary location in RAM, then eventually saves the data to EEPROM or
	external Flash.
	
	When complete, this function redirects to config/reboot.htm, which will
	then issue an AJAX call to reboot the device.

	This function creates a shadow copy of the AppConfig structure in 
	the upper portion of curHTTP.data, and then overwrites incoming data 
	there as it arrives.  For each name/value pair, the name is first 
	read to curHTTP.data[0:7].  Next, the value is read to 
	curHTTP.data[8:31].  The name is matched against an expected list, 
	and then the value is parsed and stored in the appropriate location 
	of the shadow copy of AppConfig.  Once all data has been read, the new
	AppConfig is saved back to EEPROM and the browser is redirected to 
	reboot.htm.  That file includes an AJAX call to reboot.cgi, which 
	performs the actual reboot of the machine.
	
	If an IP address cannot be parsed, the browser redirects to
	config_error.htm and does not save anything so as to prevent errors.

  Precondition:
	None

  Parameters:
	None

  Return Values:
  	HTTP_IO_DONE - all parameters have been processed
  	HTTP_IO_WAITING - the function is pausing to continue later
  	HTTP_IO_NEED_DATA - data needed by this function has not yet arrived
  ***************************************************************************/
#if defined(STACK_USE_HTTP_APP_RECONFIG)
static HTTP_IO_RESULT HTTPPostConfig(void)
{
	APP_CONFIG *app;
	BYTE *ptr;
	WORD len;
	
	#define SM_CFG_START		(0u)
	#define SM_CFG_READ_NAME	(1u)
	#define SM_CFG_READ_VALUE	(2u)
	#define SM_CFG_SUCCESS		(3u)
	#define SM_CFG_FAILURE		(4u)
	
	// Set app config pointer to use data array
	app = (void*)(&curHTTP.data[32]);
	
	switch(curHTTP.smPost)
	{
		case SM_CFG_START:
			// Use current config as defaults
			memcpy((void*)app, (void*)&AppConfig, sizeof(AppConfig));
			app->Flags.bIsDHCPEnabled = 0;
			
			// Move to next state, but no break
			curHTTP.smPost = SM_CFG_READ_NAME;
			
		case SM_CFG_READ_NAME:
			// If all parameters have been read, end
			if(curHTTP.byteCount == 0)
			{
				curHTTP.smPost = SM_CFG_SUCCESS;
				break;
			}
		
			// Read a name
			if(HTTPReadPostName(curHTTP.data, 8) == HTTP_READ_INCOMPLETE)
				return HTTP_IO_NEED_DATA;
				
			// Move to reading a value, but no break
			curHTTP.smPost = SM_CFG_READ_VALUE;
			
		case SM_CFG_READ_VALUE:
			// Read a value
			if(HTTPReadPostValue(curHTTP.data + 8, 24) == HTTP_READ_INCOMPLETE)
				return HTTP_IO_NEED_DATA;
				
			// Default action after this is to read the name, unless there's an error
			curHTTP.smPost = SM_CFG_READ_NAME;
				
			// Parse the value that was read
			if(!strcmppgm2ram((char*)curHTTP.data, (ROM char*)"ip"))
			{// Read new static IP Address
				if(StringToIPAddress(curHTTP.data+8, &(app->MyIPAddr)))
					memcpy((void*)&(app->DefaultIPAddr), (void*)&(app->MyIPAddr), sizeof(IP_ADDR));
				else
					curHTTP.smPost = SM_CFG_FAILURE;
			}
			else if(!strcmppgm2ram((char*)curHTTP.data, (ROM char*)"gw"))
			{// Read new gateway address
				if(!StringToIPAddress(curHTTP.data+8, &(app->MyGateway)))
					curHTTP.smPost = SM_CFG_FAILURE;
			}
			else if(!strcmppgm2ram((char*)curHTTP.data, (ROM char*)"subnet"))
			{// Read new static subnet
				if(StringToIPAddress(curHTTP.data+8, &(app->MyMask)))
					memcpy((void*)&(app->DefaultMask), (void*)&(app->MyMask), sizeof(IP_ADDR));
				else
					curHTTP.smPost = SM_CFG_FAILURE;
			}
			else if(!strcmppgm2ram((char*)curHTTP.data, (ROM char*)"dns1"))
			{// Read new primary DNS server
				if(!StringToIPAddress(curHTTP.data+8, &(app->PrimaryDNSServer)))
					curHTTP.smPost = SM_CFG_FAILURE;
			}
			else if(!strcmppgm2ram((char*)curHTTP.data, (ROM char*)"dns2"))
			{// Read new secondary DNS server
				if(!StringToIPAddress(curHTTP.data+8, &(app->SecondaryDNSServer)))
					curHTTP.smPost = SM_CFG_FAILURE;
			}
			else if(!strcmppgm2ram((char*)curHTTP.data, (ROM char*)"mac"))
			{// Read new MAC address
				WORD_VAL w;
				BYTE i;
	
				ptr = curHTTP.data+8;
	
				for(i = 0; i < 12; i++)
				{// Read the MAC address
					
					// Skip non-hex bytes
					while( *ptr != 0x00 && !(*ptr >= '0' && *ptr <= '9') && !(*ptr >= 'A' && *ptr <= 'F') && !(*ptr >= 'a' && *ptr <= 'f') )
						ptr++;
	
					// MAC string is over, so zeroize the rest
					if(*ptr == 0x00)
					{
						for(; i < 12; i++)
							curHTTP.data[i] = '0';
						break;
					}
					
					// Save the MAC byte
					curHTTP.data[i] = *ptr++;
				}
				
				// Read MAC Address, one byte at a time
				for(i = 0; i < 6; i++)
				{				
					w.v[1] = curHTTP.data[i*2];
					w.v[0] = curHTTP.data[i*2+1];
					app->MyMACAddr.v[i] = hexatob(w);
				}
			}
			else if(!strcmppgm2ram((char*)curHTTP.data, (ROM char*)"host"))
			{// Read new hostname
				for(len = strlen((char*)&curHTTP.data[8]); len < 15; len++)
					curHTTP.data[8+len] = ' ';
				memcpy((void*)app->NetBIOSName, (void*)curHTTP.data+8, 15);
				strupr((char*)app->NetBIOSName);
			}
			else if(!strcmppgm2ram((char*)curHTTP.data, (ROM char*)"dhcpen"))
			{// Read new DHCP Enabled flag
				if(curHTTP.data[8] == '1')
					app->Flags.bIsDHCPEnabled = 1;
				else
					app->Flags.bIsDHCPEnabled = 0;
			}

			break;
			
		case SM_CFG_SUCCESS:
			// Save new settings and force a reboot
			
			// If DCHP, then disallow editing of DefaultIP and DefaultMask
			if(app->Flags.bIsDHCPEnabled)
			{
				// If DHCP is enabled, then reset the default IP and mask
				app->DefaultIPAddr.v[0] = MY_DEFAULT_IP_ADDR_BYTE1;
				app->DefaultIPAddr.v[1] = MY_DEFAULT_IP_ADDR_BYTE2;
				app->DefaultIPAddr.v[2] = MY_DEFAULT_IP_ADDR_BYTE3;
				app->DefaultIPAddr.v[3] = MY_DEFAULT_IP_ADDR_BYTE4;
				app->DefaultMask.v[0] = MY_DEFAULT_MASK_BYTE1;
				app->DefaultMask.v[1] = MY_DEFAULT_MASK_BYTE2;
				app->DefaultMask.v[2] = MY_DEFAULT_MASK_BYTE3;
				app->DefaultMask.v[3] = MY_DEFAULT_MASK_BYTE4;
			}
			ptr = (BYTE*)app;
			#if defined(MPFS_USE_EEPROM)
		    XEEBeginWrite(0x0000);
		    XEEWrite(0x60);
		    for (len = 0; len < sizeof(AppConfig); len++ )
		        XEEWrite(*ptr++);
		    XEEEndWrite();
	        while(XEEIsBusy());
	        #elif defined(MPFS_USE_SPI_FLASH)
	        SPIFlashBeginWrite(0x00);
	        SPIFlashWrite(0x60);
	        SPIFlashWriteArray(ptr, sizeof(AppConfig));
	        #endif
			
			// Set the board to reboot to the new address
			strcpypgm2ram((char*)curHTTP.data, (ROM char*)"/protect/reboot.htm?");
			memcpy((void*)(curHTTP.data+20), (void*)app->NetBIOSName, 16);
			ptr = curHTTP.data;
			while(*ptr != ' ' && *ptr != '\0')
				ptr++;
			*ptr = '\0';
			curHTTP.httpStatus = HTTP_REDIRECT;	
			
			return HTTP_IO_DONE;
			
		case SM_CFG_FAILURE:
			// An error occurred parsing the IP, so don't save 
			// anything to prevent network errors.
			
			strcpypgm2ram((char*)curHTTP.data, (ROM char*)"/protect/config_error.htm");
			curHTTP.httpStatus = HTTP_REDIRECT;		

			return HTTP_IO_DONE;			
	}

	return HTTP_IO_WAITING;		// Assume we're waiting to process more data
}
#endif	// #if defined(STACK_USE_HTTP_APP_RECONFIG)

/*****************************************************************************
  Function:
	static HTTP_IO_RESULT HTTPPostMD5(void)

  Summary:
	Processes the file upload form on upload.htm

  Description:
	This function demonstrates the processing of file uploads.  First, the
	function locates the file data, skipping over any headers that arrive.
	Second, it reads the file 64 bytes at a time and hashes that data.  Once
	all data has been received, the function calculates the MD5 sum and
	stores it in curHTTP.data.

	After the headers, the first line from the form will be the MIME 
	separator.  Following that is more headers about the file, which we 
	discard.  After another CRLFCRLF, the file data begins, and we read 
	it 16 bytes at a time and add that to the MD5 calculation.  The reading
	terminates when the separator string is encountered again on its own 
	line.  Notice that the actual file data is trashed in this process, 
	allowing us to accept files of arbitrary size, not limited by RAM.  
	Also notice that the data buffer is used as an arbitrary storage array 
	for the result.  The ~uploadedmd5~ callback reads this data later to 
	send back to the client.
	
  Precondition:
	None

  Parameters:
	None

  Return Values:
	HTTP_IO_DONE - all parameters have been processed
	HTTP_IO_WAITING - the function is pausing to continue later
	HTTP_IO_NEED_DATA - data needed by this function has not yet arrived
  ***************************************************************************/
#if defined(STACK_USE_HTTP_MD5_DEMO)
static HTTP_IO_RESULT HTTPPostMD5(void)
{
	WORD lenA, lenB;
	static HASH_SUM md5;			// Assume only one simultaneous MD5
	
	#define SM_MD5_READ_SEPARATOR	(0u)
	#define SM_MD5_SKIP_TO_DATA		(1u)
	#define SM_MD5_READ_DATA		(2u)
	#define SM_MD5_POST_COMPLETE	(3u)
	
	// Don't care about curHTTP.data at this point, so use that for buffer
	switch(curHTTP.smPost)
	{
		// Just started, so try to find the separator string
		case SM_MD5_READ_SEPARATOR:
			// Reset the MD5 calculation
			MD5Initialize(&md5);
			
			// See if a CRLF is in the buffer
			lenA = TCPFindROMArray(sktHTTP, (ROM BYTE*)"\r\n", 2, 0, FALSE);
			if(lenA == 0xffff)
			{//if not, ask for more data
				return HTTP_IO_NEED_DATA;
			}
		
			// If so, figure out where the last byte of data is
			// Data ends at CRLFseparator--CRLF, so 6+len bytes
			curHTTP.byteCount -= lenA + 6;
			
			// Read past the CRLF
			curHTTP.byteCount -= TCPGetArray(sktHTTP, NULL, lenA+2);
			
			// Save the next state (skip to CRLFCRLF)
			curHTTP.smPost = SM_MD5_SKIP_TO_DATA;
			
			// No break...continue reading the headers if possible
				
		// Skip the headers
		case SM_MD5_SKIP_TO_DATA:
			// Look for the CRLFCRLF
			lenA = TCPFindROMArray(sktHTTP, (ROM BYTE*)"\r\n\r\n", 4, 0, FALSE);
	
			if(lenA != 0xffff)
			{// Found it, so remove all data up to and including
				lenA = TCPGetArray(sktHTTP, NULL, lenA+4);
				curHTTP.byteCount -= lenA;
				curHTTP.smPost = SM_MD5_READ_DATA;
			}
			else
			{// Otherwise, remove as much as possible
				lenA = TCPGetArray(sktHTTP, NULL, TCPIsGetReady(sktHTTP) - 4);
				curHTTP.byteCount -= lenA;
			
				// Return the need more data flag
				return HTTP_IO_NEED_DATA;
			}
			
			// No break if we found the header terminator
			
		// Read and hash file data
		case SM_MD5_READ_DATA:
			// Find out how many bytes are available to be read
			lenA = TCPIsGetReady(sktHTTP);
			if(lenA > curHTTP.byteCount)
				lenA = curHTTP.byteCount;
	
			while(lenA > 0)
			{// Add up to 64 bytes at a time to the sum
				lenB = TCPGetArray(sktHTTP, curHTTP.data, (lenA < 64)?lenA:64);			
				curHTTP.byteCount -= lenB;
				lenA -= lenB;
				MD5AddData(&md5, curHTTP.data, lenB);
			}
					
			// If we've read all the data
			if(curHTTP.byteCount == 0)
			{// Calculate and copy result to curHTTP.data for printout
				curHTTP.smPost = SM_MD5_POST_COMPLETE;
				MD5Calculate(&md5, curHTTP.data);
				return HTTP_IO_DONE;
			}
				
			// Ask for more data
			return HTTP_IO_NEED_DATA;
	}
	
	return HTTP_IO_DONE;
}
#endif // #if defined(STACK_USE_HTTP_MD5_DEMO)

/*****************************************************************************
  Function:
	static HTTP_IO_RESULT HTTPPostEmail(void)

  Summary:
	Processes the e-mail form on email/index.htm

  Description:
	This function sends an e-mail message using the SMTP client and 
	optionally encrypts the connection to the SMTP server using SSL.  It
	demonstrates the use of the SMTP client, waiting for asynchronous
	processes in an HTTP callback, and how to send e-mail attachments using
	the stack.

	Messages with attachments are sent using multipart/mixed MIME encoding,
	which has three sections.  The first has no headers, and is only to be
	displayed by old clients that cannot interpret the MIME format.  (The 
	overwhelming majority of these clients have been obseleted, but the
	so-called "ignored" section is still used.)  The second has a few 
	headers to indicate that it is the main body of the message in plain-
	text encoding.  The third section has headers indicating an attached 
	file, along with its name and type.  All sections are separated by a
	boundary string, which cannot appear anywhere else in the message.
	
  Precondition:
	None

  Parameters:
	None

  Return Values:
	HTTP_IO_DONE - the message has been sent
	HTTP_IO_WAITING - the function is waiting for the SMTP process to complete
	HTTP_IO_NEED_DATA - data needed by this function has not yet arrived
  ***************************************************************************/
#if defined(STACK_USE_SMTP_CLIENT)
static HTTP_IO_RESULT HTTPPostEmail(void)
{
	static BYTE *ptrData;
	static BYTE *szPort;
	#if defined(STACK_USE_SSL_CLIENT)
	static BYTE *szUseSSL;
	#endif
	WORD len, rem;
	BYTE cName[8];

	#define SM_EMAIL_CLAIM_MODULE				(0u)
	#define SM_EMAIL_READ_PARAM_NAME			(1u)
	#define SM_EMAIL_READ_PARAM_VALUE			(2u)
	#define SM_EMAIL_PUT_IGNORED				(3u)
	#define SM_EMAIL_PUT_BODY					(4u)
	#define SM_EMAIL_PUT_ATTACHMENT_HEADER		(5u)
	#define SM_EMAIL_PUT_ATTACHMENT_DATA_BTNS	(6u)
	#define SM_EMAIL_PUT_ATTACHMENT_DATA_LEDS	(7u)
	#define SM_EMAIL_PUT_ATTACHMENT_DATA_POT	(8u)
	#define SM_EMAIL_PUT_TERMINATOR				(9u)
	#define SM_EMAIL_FINISHING					(10u)
	
	#define EMAIL_SPACE_REMAINING				(HTTP_MAX_DATA_LEN - (ptrData - curHTTP.data))
	
	switch(curHTTP.smPost)
	{
		case SM_EMAIL_CLAIM_MODULE:
			// Try to claim module
			if(SMTPBeginUsage())
			{// Module was claimed, so set up static parameters
				SMTPClient.Subject.szROM = (ROM BYTE*)"Microchip TCP/IP Stack Status Update";
				SMTPClient.ROMPointers.Subject = 1;
				SMTPClient.From.szROM = (ROM BYTE*)"\"SMTP Service\" <mchpboard@picsaregood.com>";
				SMTPClient.ROMPointers.From = 1;
				
				// The following two lines indicate to the receiving client that 
				// this message has an attachment.  The boundary field *must not*
				// be included anywhere in the content of the message.  In real 
				// applications it is typically a long random string.
				SMTPClient.OtherHeaders.szROM = (ROM BYTE*)"MIME-version: 1.0\r\nContent-type: multipart/mixed; boundary=\"frontier\"\r\n";
				SMTPClient.ROMPointers.OtherHeaders = 1;
				
				// Move our state machine forward
				ptrData = curHTTP.data;
				szPort = NULL;
				curHTTP.smPost = SM_EMAIL_READ_PARAM_NAME;
			}
			return HTTP_IO_WAITING;			
			
		case SM_EMAIL_READ_PARAM_NAME:
			// Search for a parameter name in POST data
			if(HTTPReadPostName(cName, sizeof(cName)) == HTTP_READ_INCOMPLETE)
				return HTTP_IO_NEED_DATA;
			
			// Try to match the name value
			if(!strcmppgm2ram((char*)cName, (ROM char*)"server"))
			{// Read the server name
				SMTPClient.Server.szRAM = ptrData;
				curHTTP.smPost = SM_EMAIL_READ_PARAM_VALUE;
			}
			else if(!strcmppgm2ram((char*)cName, (ROM char*)"port"))
			{// Read the server port
				szPort = ptrData;
				curHTTP.smPost = SM_EMAIL_READ_PARAM_VALUE;
			}
			#if defined(STACK_USE_SSL_CLIENT)
			else if(!strcmppgm2ram((char*)cName, (ROM char*)"ssl"))
			{// Read the server port
				szUseSSL = ptrData;
				curHTTP.smPost = SM_EMAIL_READ_PARAM_VALUE;
			}
			#endif
			else if(!strcmppgm2ram((char*)cName, (ROM char*)"user"))
			{// Read the user name
				SMTPClient.Username.szRAM = ptrData;
				curHTTP.smPost = SM_EMAIL_READ_PARAM_VALUE;
			}
			else if(!strcmppgm2ram((char*)cName, (ROM char*)"pass"))
			{// Read the password
				SMTPClient.Password.szRAM = ptrData;
				curHTTP.smPost = SM_EMAIL_READ_PARAM_VALUE;
			}
			else if(!strcmppgm2ram((char*)cName, (ROM char*)"to"))
			{// Read the To string
				SMTPClient.To.szRAM = ptrData;
				curHTTP.smPost = SM_EMAIL_READ_PARAM_VALUE;
			}
			else if(!strcmppgm2ram((char*)cName, (ROM char*)"msg"))
			{// Done with headers, move on to the message
				// Delete paramters that are just null strings (no data from user) or illegal (ex: password without username)
				if(SMTPClient.Server.szRAM)
					if(*SMTPClient.Server.szRAM == 0x00)
						SMTPClient.Server.szRAM = NULL;
				if(SMTPClient.Username.szRAM)
					if(*SMTPClient.Username.szRAM == 0x00)
						SMTPClient.Username.szRAM = NULL;
				if(SMTPClient.Password.szRAM)
					if((*SMTPClient.Password.szRAM == 0x00) || (SMTPClient.Username.szRAM == NULL))
						SMTPClient.Password.szRAM = NULL;
				
				// Decode server port string if it exists
				if(szPort)
					if(*szPort)
						SMTPClient.ServerPort = (WORD)atol((char*)szPort);

				// Determine if SSL should be used
				#if defined(STACK_USE_SSL_CLIENT)
				if(szUseSSL)
					if(*szUseSSL == '1')
						SMTPClient.UseSSL = TRUE;
				#endif
				
				// Start sending the message
				SMTPSendMail();
				curHTTP.smPost = SM_EMAIL_PUT_IGNORED;
				return HTTP_IO_WAITING;
			}
			else
			{// Don't know what we're receiving
				curHTTP.smPost = SM_EMAIL_READ_PARAM_VALUE;
			}
			
			// No break...continue to try reading the value
		
		case SM_EMAIL_READ_PARAM_VALUE:
			// Search for a parameter value in POST data
			if(HTTPReadPostValue(ptrData, EMAIL_SPACE_REMAINING) == HTTP_READ_INCOMPLETE)
				return HTTP_IO_NEED_DATA;
				
			// Move past the data that was just read
			ptrData += strlen((char*)ptrData);
			if(ptrData < curHTTP.data + HTTP_MAX_DATA_LEN - 1)
				ptrData += 1;
			
			// Try reading the next parameter
			curHTTP.smPost = SM_EMAIL_READ_PARAM_NAME;
			return HTTP_IO_WAITING;
			
		case SM_EMAIL_PUT_IGNORED:
			// This section puts a message that is ignored by compatible clients.
			// This text will not display unless the receiving client is obselete 
			// and does not understand the MIME structure.
			// The "--frontier" indicates the start of a section, then any
			// needed MIME headers follow, then two CRLF pairs, and then
			// the actual content (which will be the body text in the next state).
			
			// Check to see if a failure occured
			if(!SMTPIsBusy())
			{
				curHTTP.smPost = SM_EMAIL_FINISHING;
				return HTTP_IO_WAITING;
			}
		
			// See if we're ready to write data
			if(SMTPIsPutReady() < 90u)
				return HTTP_IO_WAITING;
				
			// Write the ignored text				
			SMTPPutROMString((ROM BYTE*)"This is a multi-part message in MIME format.\r\n");
			SMTPPutROMString((ROM BYTE*)"--frontier\r\nContent-type: text/plain\r\n\r\n");
			SMTPFlush();
			
			// Move to the next state
			curHTTP.smPost = SM_EMAIL_PUT_BODY;
			
		case SM_EMAIL_PUT_BODY:
			// Write as much body text as is available from the TCP buffer
			// return HTTP_IO_NEED_DATA or HTTP_IO_WAITING
			// On completion, => PUT_ATTACHMENT_HEADER and continue
			
			// Check to see if a failure occurred
			if(!SMTPIsBusy())
			{
				curHTTP.smPost = SM_EMAIL_FINISHING;
				return HTTP_IO_WAITING;
			}
			
			// Loop as long as data remains to be read
			while(curHTTP.byteCount)
			{
				// See if space is available to write
				len = SMTPIsPutReady();
				if(len == 0)
					return HTTP_IO_WAITING;
				
				// See if data is ready to be read
				rem = TCPIsGetReady(sktHTTP);
				if(rem == 0)
					return HTTP_IO_NEED_DATA;
				
				// Only write as much as we can handle
				if(len > rem)
					len = rem;
				if(len > HTTP_MAX_DATA_LEN - 2)
					len = HTTP_MAX_DATA_LEN - 2;
				
				// Read the data from HTTP POST buffer and send it to SMTP
				curHTTP.byteCount -= TCPGetArray(sktHTTP, curHTTP.data, len);
				curHTTP.data[len] = '\0';
				HTTPURLDecode(curHTTP.data);
				SMTPPutString(curHTTP.data);
				SMTPFlush();
			}
			
			// We're done with the POST data, so continue
			curHTTP.smPost = SM_EMAIL_PUT_ATTACHMENT_HEADER;
						
		case SM_EMAIL_PUT_ATTACHMENT_HEADER:
			// This section writes the attachment to the message.
			// This portion generally will not display in the reader, but
			// will be downloadable to the local machine.  Use caution
			// when selecting the content-type and file name, as certain
			// types and extensions are blocked by virus filters.

			// The same structure as the message body is used.
			// Any attachment must not include high-bit ASCII characters or
			// binary data.  If binary data is to be sent, the data should
			// be encoded using Base64 and a MIME header should be added:
			// Content-transfer-encoding: base64
			
			// Check to see if a failure occurred
			if(!SMTPIsBusy())
			{
				curHTTP.smPost = SM_EMAIL_FINISHING;
				return HTTP_IO_WAITING;
			}
			
			// See if we're ready to write data
			if(SMTPIsPutReady() < 100u)
				return HTTP_IO_WAITING;
			
			// Write the attachment header
			SMTPPutROMString((ROM BYTE*)"\r\n--frontier\r\nContent-type: text/csv\r\nContent-Disposition: attachment; filename=\"status.csv\"\r\n\r\n");
			SMTPFlush();
			
			// Move to the next state
			curHTTP.smPost = SM_EMAIL_PUT_ATTACHMENT_DATA_BTNS;
			
		case SM_EMAIL_PUT_ATTACHMENT_DATA_BTNS:
			// The following states output the system status as a CSV file.
			
			// Check to see if a failure occurred
			if(!SMTPIsBusy())
			{
				curHTTP.smPost = SM_EMAIL_FINISHING;
				return HTTP_IO_WAITING;
			}
			
			// See if we're ready to write data
			if(SMTPIsPutReady() < 36u)
				return HTTP_IO_WAITING;
				
			// Write the header and button strings
			SMTPPutROMString((ROM BYTE*)"SYSTEM STATUS\r\n");
			SMTPPutROMString((ROM BYTE*)"Buttons:,");
			SMTPPut(BUTTON0_IO + '0');
			SMTPPut(',');
			SMTPPut(BUTTON1_IO + '0');
			SMTPPut(',');
			SMTPPut(BUTTON2_IO + '0');
			SMTPPut(',');
			SMTPPut(BUTTON3_IO + '0');
			SMTPPut('\r');
			SMTPPut('\n');
			SMTPFlush();
			
			// Move to the next state
			curHTTP.smPost = SM_EMAIL_PUT_ATTACHMENT_DATA_LEDS;

		case SM_EMAIL_PUT_ATTACHMENT_DATA_LEDS:
			// Check to see if a failure occurred
			if(!SMTPIsBusy())
			{
				curHTTP.smPost = SM_EMAIL_FINISHING;
				return HTTP_IO_WAITING;
			}
			
			// See if we're ready to write data
			if(SMTPIsPutReady() < 30u)
				return HTTP_IO_WAITING;
				
			// Write the header and button strings
			SMTPPutROMString((ROM BYTE*)"LEDs:,");
			SMTPPut(LED0_IO + '0');
			SMTPPut(',');
			SMTPPut(LED1_IO + '0');
			SMTPPut(',');
			SMTPPut(LED2_IO + '0');
			SMTPPut(',');
			SMTPPut(LED3_IO + '0');
			SMTPPut(',');
			SMTPPut(LED4_IO + '0');
			SMTPPut(',');
			SMTPPut(LED5_IO + '0');
			SMTPPut(',');
			SMTPPut(LED6_IO + '0');
			SMTPPut(',');
			SMTPPut(LED7_IO + '0');
			SMTPPut('\r');
			SMTPPut('\n');
			SMTPFlush();

			// Move to the next state
			curHTTP.smPost = SM_EMAIL_PUT_ATTACHMENT_DATA_POT;

		case SM_EMAIL_PUT_ATTACHMENT_DATA_POT:
			// Check to see if a failure occurred
			if(!SMTPIsBusy())
			{
				curHTTP.smPost = SM_EMAIL_FINISHING;
				return HTTP_IO_WAITING;
			}
			
			// See if we're ready to write data
			if(SMTPIsPutReady() < 16u)
				return HTTP_IO_WAITING;

			// Do the A/D conversion
			#if defined(__18CXX)
			    // Wait until A/D conversion is done
			    ADCON0bits.GO = 1;
			    while(ADCON0bits.GO);
			    // Convert 10-bit value into ASCII string
			    len = (WORD)ADRES;
			    uitoa(len, (BYTE*)&curHTTP.data[1]);
			#else
				len = (WORD)ADC1BUF0;
			    uitoa(len, (BYTE*)&curHTTP.data[1]);
			#endif

			// Write the header and button strings
			SMTPPutROMString((ROM BYTE*)"Pot:,");
			SMTPPutString(&curHTTP.data[1]);
			SMTPPut('\r');
			SMTPPut('\n');
			SMTPFlush();
			
			// Move to the next state
			curHTTP.smPost = SM_EMAIL_PUT_TERMINATOR;
			
		case SM_EMAIL_PUT_TERMINATOR:
			// This section finishes the message
			// This consists of two dashes, the boundary, and two more dashes
			// on a single line, followed by a CRLF pair to terminate the message.

			// Check to see if a failure occured
			if(!SMTPIsBusy())
			{
				curHTTP.smPost = SM_EMAIL_FINISHING;
				return HTTP_IO_WAITING;
			}
		
			// See if we're ready to write data
			if(SMTPIsPutReady() < 16u)
				return HTTP_IO_WAITING;
				
			// Write the ignored text				
			SMTPPutROMString((ROM BYTE*)"--frontier--\r\n");
			SMTPPutDone();
			SMTPFlush();
			
			// Move to the next state
			curHTTP.smPost = SM_EMAIL_FINISHING;
		
		case SM_EMAIL_FINISHING:
			// Wait for status
			if(!SMTPIsBusy())
			{
				// Release the module and check success
				// Redirect the user based on the result
				if(SMTPEndUsage() == SMTP_SUCCESS)
					lastSuccess = TRUE;
				else
					lastFailure = TRUE;
									
				// Redirect to the page
				strcpypgm2ram((char*)curHTTP.data, (ROM void*)"/email/index.htm");
				curHTTP.httpStatus = HTTP_REDIRECT;
				return HTTP_IO_DONE;
			}
			
			return HTTP_IO_WAITING;
	}
	
	return HTTP_IO_DONE;
}
#endif	// #if defined(STACK_USE_SMTP_CLIENT)

/****************************************************************************
  Function:
    HTTP_IO_RESULT HTTPPostDDNSConfig(void)
    
  Summary:
    Parsing and collecting http data received from http form.

  Description:
    This routine will be excuted every time the Dynamic DNS Client
    configuration form is submitted.  The http data is received 
    as a string of the variables seperated by '&' characters in the TCP RX
    buffer.  This data is parsed to read the required configuration values, 
    and those values are populated to the global array (DDNSData) reserved 
    for this purpose.  As the data is read, DDNSPointers is also populated
    so that the dynamic DNS client can execute with the new parameters.
    
  Precondition:
     curHTTP is loaded.

  Parameters:
    None.

  Return Values:
    HTTP_IO_DONE 		-  Finished with procedure
    HTTP_IO_NEED_DATA	-  More data needed to continue, call again later
    HTTP_IO_WAITING 	-  Waiting for asynchronous process to complete, 
    						call again later
  ***************************************************************************/
#if defined(STACK_USE_DYNAMICDNS_CLIENT)
static HTTP_IO_RESULT HTTPPostDDNSConfig(void)
{
	static BYTE *ptrDDNS;

	#define SM_DDNS_START			(0u)
	#define SM_DDNS_READ_NAME		(1u)
	#define SM_DDNS_READ_VALUE		(2u)
	#define SM_DDNS_READ_SERVICE	(3u)
	#define SM_DDNS_DONE			(4u)

	#define DDNS_SPACE_REMAINING				(sizeof(DDNSData) - (ptrDDNS - DDNSData))

	switch(curHTTP.smPost)
	{
		// Sets defaults for the system
		case SM_DDNS_START:
			ptrDDNS = DDNSData;
			DDNSSetService(0);
			DDNSClient.Host.szROM = NULL;
			DDNSClient.Username.szROM = NULL;
			DDNSClient.Password.szROM = NULL;
			DDNSClient.ROMPointers.Host = 0;
			DDNSClient.ROMPointers.Username = 0;
			DDNSClient.ROMPointers.Password = 0;
			curHTTP.smPost++;
			
		// Searches out names and handles them as they arrive
		case SM_DDNS_READ_NAME:
			// If all parameters have been read, end
			if(curHTTP.byteCount == 0)
			{
				curHTTP.smPost = SM_DDNS_DONE;
				break;
			}
		
			// Read a name
			if(HTTPReadPostName(curHTTP.data, HTTP_MAX_DATA_LEN) == HTTP_READ_INCOMPLETE)
				return HTTP_IO_NEED_DATA;
			
			if(!strcmppgm2ram((char *)curHTTP.data, (ROM char*)"service"))
			{
				// Reading the service (numeric)
				curHTTP.smPost = SM_DDNS_READ_SERVICE;
				break;
			}
			else if(!strcmppgm2ram((char *)curHTTP.data, (ROM char*)"user"))
				DDNSClient.Username.szRAM = ptrDDNS;
			else if(!strcmppgm2ram((char *)curHTTP.data, (ROM char*)"pass"))
				DDNSClient.Password.szRAM = ptrDDNS;
			else if(!strcmppgm2ram((char *)curHTTP.data, (ROM char*)"host"))
				DDNSClient.Host.szRAM = ptrDDNS;
			
			// Move to reading the value for user/pass/host
			curHTTP.smPost++;
			
		// Reads in values and assigns them to the DDNS RAM
		case SM_DDNS_READ_VALUE:
			// Read a name
			if(HTTPReadPostValue(ptrDDNS, DDNS_SPACE_REMAINING) == HTTP_READ_INCOMPLETE)
				return HTTP_IO_NEED_DATA;
				
			// Move past the data that was just read
			ptrDDNS += strlen((char*)ptrDDNS);
			if(ptrDDNS < DDNSData + sizeof(DDNSData) - 1)
				ptrDDNS += 1;			
			
			// Return to reading names
			curHTTP.smPost = SM_DDNS_READ_NAME;
			break;
		
		// Reads in a service ID
		case SM_DDNS_READ_SERVICE:
			// Read the integer id
			if(HTTPReadPostValue(curHTTP.data, HTTP_MAX_DATA_LEN) == HTTP_READ_INCOMPLETE)
				return HTTP_IO_NEED_DATA;
			
			// Convert to a service ID
			DDNSSetService((BYTE)atol((char*)curHTTP.data));

			// Return to reading names
			curHTTP.smPost = SM_DDNS_READ_NAME;
			break;
			
		// Sets up the DDNS client for an update
		case SM_DDNS_DONE:
			// Since user name and password changed, force an update immediately
			DDNSForceUpdate();
			
			// Redirect to prevent POST errors
			lastSuccess = TRUE;
			strcpypgm2ram((char*)curHTTP.data, (ROM void*)"/dyndns/index.htm");
			curHTTP.httpStatus = HTTP_REDIRECT;
			return HTTP_IO_DONE;				
	}
	
	return HTTP_IO_WAITING;		// Assume we're waiting to process more data
}
#endif	// #if defined(STACK_USE_DYNAMICDNS_CLIENT)

#endif //(use_post)


/****************************************************************************
  Section:
	Dynamic Variable Callback Functions
  ***************************************************************************/

/*****************************************************************************
  Function:
	void HTTPPrint_varname(void)
	
  Internal:
  	See documentation in the TCP/IP Stack API or HTTP2.h for details.
  ***************************************************************************/

void HTTPPrint_builddate(void)
{
	curHTTP.callbackPos = 0x01;
	if(TCPIsPutReady(sktHTTP) < strlenpgm((ROM char*)__DATE__" "__TIME__))
		return;
	
	curHTTP.callbackPos = 0x00;
	TCPPutROMString(sktHTTP, (ROM void*)__DATE__" "__TIME__);
}

void HTTPPrint_version(void)
{
	TCPPutROMString(sktHTTP, (ROM void*)VERSION);
}




ROM BYTE HTML_UP_ARROW[] = "up";
ROM BYTE HTML_DOWN_ARROW[] = "dn";

void HTTPPrint_btn(WORD num)
{
	// Determine which button
	switch(num)
	{
		case 0:
			//num = BUTTON0_IO;
		    num = 1;
			break;
		case 1:
			//num = BUTTON1_IO;
		    num = 1;
			break;
		case 2:
			//num = BUTTON2_IO;
		    num = 1;
			break;
		case 3:
			//num = BUTTON3_IO;
		    num = 1;
			break;
		default:
			num = 0;
	}

	// Print the output
	TCPPutROMString(sktHTTP, (num?HTML_UP_ARROW:HTML_DOWN_ARROW));
	return;
}
	
void HTTPPrint_led(WORD num)
{
	// Determine which LED
	switch(num)
	{
		case 0:
			//num = LED0_IO;
			break;
		case 1:
			//num = LED1_IO;
			break;
		case 2:
			//num = LED2_IO;
			break;
		case 3:
			//num = LED3_IO;
			break;
		case 4:
			//num = LED4_IO;
			break;
		case 5:
			//num = LED5_IO;
			break;
		case 6:
			//num = LED6_IO;
			break;
		case 7:
			//num = LED7_IO;
			break;

		default:
			num = 0;
	}

	// Print the output
	TCPPut(sktHTTP, (num?'1':'0'));
	return;
}

void HTTPPrint_ledSelected(WORD num, WORD state)
{
	// Determine which LED to check
	switch(num)
	{
		case 0:
			//num = LED0_IO;
			break;
		case 1:
			//num = LED1_IO;
			break;
		case 2:
			//num = LED2_IO;
			break;
		case 3:
			//num = LED3_IO;
			break;
		case 4:
			//num = LED4_IO;
			break;
		case 5:
			//num = LED5_IO;
			break;
		case 6:
			//num = LED6_IO;
			break;
		case 7:
			//num = LED7_IO;
			break;

		default:
			num = 0;
	}
	
	// Print output if TRUE and ON or if FALSE and OFF
	if((state && num) || (!state && !num))
		TCPPutROMString(sktHTTP, (ROM BYTE*)"SELECTED");
	return;
}

void HTTPPrint_devSelected(WORD num, WORD state)
{
	// Determine which LED to check
	switch(num)
	{
		case 0:
			//num = RELAY1_IO;
			break;
		case 1:
			//num = RELAY2_IO;
			break;
		default:
			num = 0;
	}
	
	// Print output if TRUE and ON or if FALSE and OFF
	if((state && num) || (!state && !num))
		TCPPutROMString(sktHTTP, (ROM BYTE*)"SELECTED");
	return;
}


void HTTPPrint_TP(void)
{
	char TPString[8];
///	TP = 0b1111110010010000;
	int TP_temp;
	TP_temp = 0;
	if(TP_temp & 0xf800l)
	{
		TP_temp = ~TP_temp + 1;
		TP_temp = TP_temp >> 4;
		TPString[0] = '-';
    	uitoa(TP_temp, &TPString[1]);
	}
	else
	{
		TP_temp = TP_temp >> 4;
    	uitoa(TP_temp, TPString);
	}
   	TCPPutArray(sktHTTP,(void *)TPString, strlen((char*)TPString));
	return;
}
void HTTPPrint_YR(void)
{
	char YRString[8];
	int YR_temp;
	BYTE TEMP;
	TEMP = 1;
	YR_temp = TEMP;
	if(YR_temp < 10)
	{
		YRString[0] = '0';
   		uitoa(YR_temp, &YRString[1]);
	}
	else
	{
		uitoa(YR_temp, YRString);
	}
   	TCPPutArray(sktHTTP,(void *)YRString, strlen((char*)YRString));
	return;
}

void HTTPPrint_MH(void)
{
	char MHString[8];
	int MH_temp;
	BYTE TEMP;
	TEMP = 1;
	MH_temp = TEMP;
   	uitoa(MH_temp, MHString);
   	TCPPutArray(sktHTTP,(void *)MHString, strlen((char*)MHString));
	return;
}
void HTTPPrint_DA(void)
{
	char DAString[8];
	int DA_temp;
	BYTE TEMP;
	TEMP = 1;
	DA_temp = TEMP;
   	uitoa(DA_temp, DAString);
   	TCPPutArray(sktHTTP,(void *)DAString, strlen((char*)DAString));
	return;
}
void HTTPPrint_HR(void)
{
	char HRString[8];
	int HR_temp;
	BYTE TEMP;
	TEMP = 1;
	HR_temp = TEMP;
   	uitoa(HR_temp, HRString);
   	TCPPutArray(sktHTTP,(void *)HRString, strlen((char*)HRString));
	return;
}

void HTTPPrint_ME(void)
{
	char MEString[8];
	int ME_temp;
	BYTE TEMP;
	TEMP = 1;
	ME_temp = TEMP;
   	uitoa(ME_temp, MEString);
   	TCPPutArray(sktHTTP,(void *)MEString, strlen((char*)MEString));
	return;
}
void HTTPPrint_SD(void)
{
	char SDString[8];
	int SD_temp;
	BYTE TEMP;
	TEMP = 1;
	SD_temp = TEMP;
   	uitoa(SD_temp, SDString);
   	TCPPutArray(sktHTTP,(void *)SDString, strlen((char*)SDString));
	return;
}



















//************************************************
void HTTPPrint_pot(void)
{    
	BYTE AN0String[8];
	WORD ADval;

#if defined(__18CXX)
    // Wait until A/D conversion is done
    ADCON0bits.GO = 1;
    while(ADCON0bits.GO);

    // Convert 10-bit value into ASCII string
    ADval = (WORD)ADRES;
    //ADval *= (WORD)10;
    //ADval /= (WORD)102;
    uitoa(ADval, AN0String);
#else
	ADval = 111;
	//ADval *= (WORD)10;
	//ADval /= (WORD)102;
    uitoa(ADval, (BYTE*)AN0String);
#endif

   	TCPPutString(sktHTTP, AN0String);
}

void HTTPPrint_lcdtext(void)
{
	WORD len;

	// Determine how many bytes we can write
	len = TCPIsPutReady(sktHTTP);
	
	#if defined(USE_LCD)
	// If just starting, set callbackPos
	if(curHTTP.callbackPos == 0)
		curHTTP.callbackPos = 32;
	
	// Write a byte at a time while we still can
	// It may take up to 12 bytes to write a character
	// (spaces and newlines are longer)
	while(len > 12 && curHTTP.callbackPos)
	{
		// After 16 bytes write a newline
		if(curHTTP.callbackPos == 16)
			len -= TCPPutROMArray(sktHTTP, (ROM BYTE*)"<br />", 6);

		if(LCDText[32-curHTTP.callbackPos] == ' ' || LCDText[32-curHTTP.callbackPos] == '\0')
			len -= TCPPutROMArray(sktHTTP, (ROM BYTE*)"&nbsp;", 6);
		else
			len -= TCPPut(sktHTTP, LCDText[32-curHTTP.callbackPos]);

		curHTTP.callbackPos--;
	}
	#else
	len = len;
	TCPPutROMString(sktHTTP, (ROM BYTE*)"No LCD Present");
	#endif

	return;
}

void HTTPPrint_hellomsg(void)
{
	BYTE *ptr;
	
	ptr = HTTPGetROMArg(curHTTP.data, (ROM BYTE*)"name");
	
	// We omit checking for space because this is the only data being written
	if(ptr != NULL)
	{
		TCPPutROMString(sktHTTP, (ROM BYTE*)"Hello, ");
		TCPPutString(sktHTTP, ptr);
	}

	return;
}

void HTTPPrint_cookiename(void)
{
	BYTE *ptr;
	
	ptr = HTTPGetROMArg(curHTTP.data, (ROM BYTE*)"name");
	
	if(ptr)
		TCPPutString(sktHTTP, ptr);
	else
		TCPPutROMString(sktHTTP, (ROM BYTE*)"not set");
	
	return;
}

void HTTPPrint_uploadedmd5(void)
{
	BYTE i;

	// Set a flag to indicate not finished
	curHTTP.callbackPos = 1;
	
	// Make sure there's enough output space
	if(TCPIsPutReady(sktHTTP) < 32u + 37u + 5u)
		return;

	// Check for flag set in HTTPPostMD5
#if defined(STACK_USE_HTTP_MD5_DEMO)
	if(curHTTP.smPost != SM_MD5_POST_COMPLETE)
#endif
	{// No file uploaded, so just return
		TCPPutROMString(sktHTTP, (ROM BYTE*)"<b>Upload a File</b>");
		curHTTP.callbackPos = 0;
		return;
	}
	
	TCPPutROMString(sktHTTP, (ROM BYTE*)"<b>Uploaded File's MD5 was:</b><br />");
	
	// Write a byte of the md5 sum at a time
	for(i = 0; i < 16u; i++)
	{
		TCPPut(sktHTTP, btohexa_high(curHTTP.data[i]));
		TCPPut(sktHTTP, btohexa_low(curHTTP.data[i]));
		if((i & 0x03) == 3u)
			TCPPut(sktHTTP, ' ');
	}
	
	curHTTP.callbackPos = 0x00;
	return;
}

extern APP_CONFIG AppConfig;

void HTTPPrintIP(IP_ADDR ip)
{
	BYTE digits[4];
	BYTE i;
	
	for(i = 0; i < 4u; i++)
	{
		if(i)
			TCPPut(sktHTTP, '.');
		uitoa(ip.v[i], digits);
		TCPPutString(sktHTTP, digits);
	}
}

void HTTPPrint_config_hostname(void)
{
	TCPPutString(sktHTTP, AppConfig.NetBIOSName);
	return;
}

void HTTPPrint_config_dhcpchecked(void)
{
	if(AppConfig.Flags.bIsDHCPEnabled)
		TCPPutROMString(sktHTTP, (ROM BYTE*)"checked");
	return;
}

void HTTPPrint_config_ip(void)
{
	HTTPPrintIP(AppConfig.MyIPAddr);
	return;
}

void HTTPPrint_config_gw(void)
{
	HTTPPrintIP(AppConfig.MyGateway);
	return;
}

void HTTPPrint_config_subnet(void)
{
	HTTPPrintIP(AppConfig.MyMask);
	return;
}

void HTTPPrint_config_dns1(void)
{
	HTTPPrintIP(AppConfig.PrimaryDNSServer);
	return;
}

void HTTPPrint_config_dns2(void)
{
	HTTPPrintIP(AppConfig.SecondaryDNSServer);
	return;
}

void HTTPPrint_config_mac(void)
{
	BYTE i;
	
	if(TCPIsPutReady(sktHTTP) < 18u)
	{//need 17 bytes to write a MAC
		curHTTP.callbackPos = 0x01;
		return;
	}	
	
	// Write each byte
	for(i = 0; i < 6u; i++)
	{
		if(i)
			TCPPut(sktHTTP, ':');
		TCPPut(sktHTTP, btohexa_high(AppConfig.MyMACAddr.v[i]));
		TCPPut(sktHTTP, btohexa_low(AppConfig.MyMACAddr.v[i]));
	}
	
	// Indicate that we're done
	curHTTP.callbackPos = 0x00;
	return;
}

void HTTPPrint_reboot(void)
{
	// This is not so much a print function, but causes the board to reboot
	// when the configuration is changed.  If called via an AJAX call, this
	// will gracefully reset the board and bring it back online immediately
	Reset();
}

void HTTPPrint_rebootaddr(void)
{// This is the expected address of the board upon rebooting
	TCPPutString(sktHTTP, curHTTP.data);	
}

void HTTPPrint_ddns_user(void)
{
	#if defined(STACK_USE_DYNAMICDNS_CLIENT)
	if(DDNSClient.ROMPointers.Username || !DDNSClient.Username.szRAM)
		return;
	if(curHTTP.callbackPos == 0x00)
		curHTTP.callbackPos = (PTR_BASE)DDNSClient.Username.szRAM;
	curHTTP.callbackPos = (PTR_BASE)TCPPutString(sktHTTP, (BYTE*)(PTR_BASE)curHTTP.callbackPos);
	if(*(BYTE*)(PTR_BASE)curHTTP.callbackPos == '\0')
		curHTTP.callbackPos = 0x00;
	#endif
}

void HTTPPrint_ddns_pass(void)
{
	#if defined(STACK_USE_DYNAMICDNS_CLIENT)
	if(DDNSClient.ROMPointers.Password || !DDNSClient.Password.szRAM)
		return;
	if(curHTTP.callbackPos == 0x00)
		curHTTP.callbackPos = (PTR_BASE)DDNSClient.Password.szRAM;
	curHTTP.callbackPos = (PTR_BASE)TCPPutString(sktHTTP, (BYTE*)(PTR_BASE)curHTTP.callbackPos);
	if(*(BYTE*)(PTR_BASE)curHTTP.callbackPos == '\0')
		curHTTP.callbackPos = 0x00;
	#endif
}

void HTTPPrint_ddns_host(void)
{
	#if defined(STACK_USE_DYNAMICDNS_CLIENT)
	if(DDNSClient.ROMPointers.Host || !DDNSClient.Host.szRAM)
		return;
	if(curHTTP.callbackPos == 0x00)
		curHTTP.callbackPos = (PTR_BASE)DDNSClient.Host.szRAM;
	curHTTP.callbackPos = (PTR_BASE)TCPPutString(sktHTTP, (BYTE*)(PTR_BASE)curHTTP.callbackPos);
	if(*(BYTE*)(PTR_BASE)curHTTP.callbackPos == '\0')
		curHTTP.callbackPos = 0x00;
	#endif
}

extern ROM char *ddnsServiceHosts[];
void HTTPPrint_ddns_service(WORD i)
{
	#if defined(STACK_USE_DYNAMICDNS_CLIENT)
	if(!DDNSClient.ROMPointers.UpdateServer || !DDNSClient.UpdateServer.szROM)
		return;
	if((ROM char*)DDNSClient.UpdateServer.szROM == ddnsServiceHosts[i])
		TCPPutROMString(sktHTTP, (ROM BYTE*)"selected");
	#endif
}
		

void HTTPPrint_ddns_status(void)
{
	#if defined(STACK_USE_DYNAMICDNS_CLIENT)
	BYTE s;
	s = DDNSGetLastStatus();
	if(s == DDNS_STATUS_GOOD || s == DDNS_STATUS_UNCHANGED || s == DDNS_STATUS_NOCHG)
		TCPPutROMString(sktHTTP, (ROM BYTE*)"ok");
	else if(s == DDNS_STATUS_UNKNOWN)
		TCPPutROMString(sktHTTP, (ROM BYTE*)"unk");
	else
		TCPPutROMString(sktHTTP, (ROM BYTE*)"fail");
	#else
	TCPPutROMString(sktHTTP, (ROM BYTE*)"fail");
	#endif
}

void HTTPPrint_ddns_status_msg(void)
{
	if(TCPIsPutReady(sktHTTP) < 75u)
	{
		curHTTP.callbackPos = 0x01;
		return;
	}
	
	#if defined(STACK_USE_DYNAMICDNS_CLIENT)
	switch(DDNSGetLastStatus())
	{
		case DDNS_STATUS_GOOD:
		case DDNS_STATUS_NOCHG:
			TCPPutROMString(sktHTTP, (ROM BYTE*)"The last update was successful.");
			break;
		case DDNS_STATUS_UNCHANGED:
			TCPPutROMString(sktHTTP, (ROM BYTE*)"The IP has not changed since the last update.");
			break;
		case DDNS_STATUS_UPDATE_ERROR:
		case DDNS_STATUS_CHECKIP_ERROR:
			TCPPutROMString(sktHTTP, (ROM BYTE*)"Could not communicate with DDNS server.");
			break;
		case DDNS_STATUS_INVALID:
			TCPPutROMString(sktHTTP, (ROM BYTE*)"The current configuration is not valid.");
			break;
		case DDNS_STATUS_UNKNOWN:
			TCPPutROMString(sktHTTP, (ROM BYTE*)"The Dynamic DNS client is pending an update.");
			break;
		default:
			TCPPutROMString(sktHTTP, (ROM BYTE*)"An error occurred during the update.<br />The DDNS Client is suspended.");
			break;
	}
	#else
	TCPPutROMString(sktHTTP, (ROM BYTE*)"The Dynamic DNS Client is not enabled.");
	#endif
	
	curHTTP.callbackPos = 0x00;
}

void HTTPPrint_status_ok(void)
{
	if(lastSuccess)
		TCPPutROMString(sktHTTP, (ROM BYTE*)"block");
	else
		TCPPutROMString(sktHTTP, (ROM BYTE*)"none");
	lastSuccess = FALSE;
}

void HTTPPrint_status_fail(void)
{
	if(lastFailure)
		TCPPutROMString(sktHTTP, (ROM BYTE*)"block");
	else
		TCPPutROMString(sktHTTP, (ROM BYTE*)"none");
	lastFailure = FALSE;
}

#endif
