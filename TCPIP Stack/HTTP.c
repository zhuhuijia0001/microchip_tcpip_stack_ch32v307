/*********************************************************************
 *
 *  HyperText Transfer Protocol (HTTP) Server
 *  Module for Microchip TCP/IP Stack
 *   -Serves dynamic pages to web browsers such as Microsoft Internet 
 *    Explorer, Mozilla Firefox, etc.
 *	 -Reference: RFC 2068
 *
 **********************************************************************
 * FileName:        HTTP.c
 * Dependencies:    TCP, MPFS, HTTPGetVar() callback, HTTPExecCmd() callback
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
 * Nilesh Rajbharti     8/14/01     Original
 * Nilesh Rajbharti     9/12/01     Released (Rev. 1.0)
 * Nilesh Rajbharti     2/9/02      Cleanup
 * Nilesh Rajbharti     5/22/02     Rev 2.0 (See version.log for detail)
 * Nilesh Rajbharti     7/9/02      Rev 2.1 (Fixed HTTPParse bug)
 * Howard Schlunder		2/9/05		Fixed variable substitution 
 *									parsing (uses hex now)
 ********************************************************************/
#define __HTTP_C

#include "TCPIP Stack/TCPIP.h"
#include "ctype.h"

#if defined(STACK_USE_HTTP_SERVER)


// Each dynamic variable within a CGI file should be preceeded with this character.
#define HTTP_VAR_ESC_CHAR       '%'
#define HTTP_DYNAMIC_FILE_TYPE  (HTTP_CGI)

// HTTP File Types
#define HTTP_TXT        (0u)
#define HTTP_HTML       (1u)
#define HTTP_CGI        (2u)
#define HTTP_XML        (3u)
#define HTTP_GIF        (4u)
#define HTTP_PNG        (5u)
#define HTTP_JPG        (6u)
#define HTTP_JAVA       (7u)
#define HTTP_WAV        (8u)
#define HTTP_UNKNOWN    (9u)


#define FILE_EXT_LEN    (3u)
typedef struct _FILE_TYPES
{
    BYTE fileExt[FILE_EXT_LEN+1];
} FILE_TYPES;

// Each entry in this structure must be in UPPER case.
// Order of these entries must match with those defined by "HTTP File Types" defines.
static ROM FILE_TYPES httpFiles[] =
{
    { "TXT" },          // HTTP_TXT
    { "HTM" },          // HTTP_HTML
    { "CGI" },          // HTTP_CGI
    { "XML" },          // HTTP_XML
    { "GIF" },          // HTTP_GIF
    { "PNG" },          // HTTP_PNG
    { "JPG" },          // HTTP_JPG
    { "CLA" },          // HTTP_JAVA
    { "WAV" },          // HTTP_WAV
	{ ""    }			// HTTP_UNKNOWN
};
#define TOTAL_FILE_TYPES        ( sizeof(httpFiles)/sizeof(httpFiles[0]) )


typedef struct _HTTP_CONTENT
{
    ROM BYTE typeString[20];
} HTTP_CONTENT;

// Content entry order must match with those "HTTP File Types" define's.
static ROM HTTP_CONTENT httpContents[] =
{
    { "text/plain" },            // HTTP_TXT
    { "text/html" },             // HTTP_HTML
    { "text/html" },             // HTTP_CGI
    { "text/xml" },              // HTTP_XML
    { "image/gif" },             // HTTP_GIF
    { "image/png" },             // HTTP_PNG
    { "image/jpeg" },            // HTTP_JPG
    { "application/java-vm" },   // HTTP_JAVA
    { "audio/x-wave" },          // HTTP_WAV
	{ "" }						 // HTTP_UNKNOWN
};
#define TOTAL_HTTP_CONTENTS     ( sizeof(httpContents)/sizeof(httpConetents[0]) )

// HTTP FSM states for each connection.
typedef enum _SM_HTTP
{
    SM_HTTP_IDLE = 0u,
    SM_HTTP_GET,
    SM_HTTP_NOT_FOUND,
    SM_HTTP_GET_READ,
    SM_HTTP_GET_PASS,
    SM_HTTP_GET_DLE,
    SM_HTTP_GET_HANDLE,
    SM_HTTP_GET_HANDLE_NEXT,
    SM_HTTP_GET_VAR,
    SM_HTTP_HEADER,
    SM_HTTP_DISCARD
} SM_HTTP;

// Supported HTTP Commands
typedef enum _HTTP_COMMAND
{
    HTTP_GET = 0u,
    HTTP_POST,
    HTTP_NOT_SUPPORTED,
    HTTP_INVALID_COMMAND
} HTTP_COMMAND;

// HTTP Connection Info - one for each connection.
typedef struct _HTTP_INFO
{
    TCP_SOCKET socket;
    MPFS file;
    SM_HTTP smHTTP;
    BYTE smHTTPGet;
    WORD VarRef;
    BYTE bProcess;
    BYTE Variable;
    BYTE fileType;
} HTTP_INFO;
typedef BYTE HTTP_HANDLE;


typedef enum
{
    HTTP_NOT_FOUND = 0u,
    HTTP_NOT_AVAILABLE
} HTTP_MESSAGES;

// Following message order must match with that of HTTP_MESSAGES enum.
static ROM BYTE *HTTPMessages[] =
{
	    (ROM BYTE*)"HTTP/1.1 404 Not found\r\n\r\nNot found.\r\n",
	    (ROM BYTE*)"HTTP/1.1 503 \r\n\r\nService Unavailable\r\n"
};

// Standard HTTP messages.
static ROM BYTE HTTP_OK_STRING[] = "HTTP/1.1 200 OK\r\nContent-type: ";
static ROM BYTE HTTP_OK_NO_CACHE_STRING[] = "HTTP/1.1 200 OK\r\nDate: Wed, 05 Apr 2006 02:53:05 GMT\r\nExpires: Wed, 05 Apr 2006 02:52:05 GMT\r\nCache-control: private\r\nContent-type: ";
#define HTTP_OK_STRING_LEN (sizeof(HTTP_OK_STRING)-1)
#define HTTP_OK_NO_CACHE_STRING_LEN (sizeof(HTTP_OK_NO_CACHE_STRING)-1)

static ROM BYTE HTTP_HEADER_END_STRING[] = "\r\n\r\n";
#define HTTP_HEADER_END_STRING_LEN (sizeof(HTTP_HEADER_END_STRING)-1)

// HTTP Command Strings
static ROM BYTE HTTP_GET_STRING[] = "GET";
#define HTTP_GET_STRING_LEN (sizeof(HTTP_GET_STRING)-1)

// Default HTML file.
static ROM BYTE HTTP_DEFAULT_FILE_STRING[] = "INDEX.HTM";
#define HTTP_DEFAULT_FILE_STRING_LEN (sizeof(HTTP_DEFAULT_FILE_STRING)-1)


// Maximum nuber of arguments supported by this HTTP Server.
#define MAX_HTTP_ARGS       (11u)

// Maximum HTML Command String length.
#define MAX_HTML_CMD_LEN    (100u)


static HTTP_INFO HCB[MAX_HTTP_CONNECTIONS];


static void HTTPProcess(HTTP_HANDLE h);
static HTTP_COMMAND HTTPParse(BYTE *string,
                              BYTE** arg,
                              BYTE* argc,
                              BYTE* type);
static BOOL SendFile(HTTP_INFO* ph);




/*********************************************************************
 * Function:        void HTTPInit(void)
 *
 * PreCondition:    TCP must already be initialized.
 *
 * Input:           None
 *
 * Output:          HTTP FSM and connections are initialized
 *
 * Side Effects:    None
 *
 * Overview:        Set all HTTP connections to Listening state.
 *                  Initialize FSM for each connection.
 *
 * Note:            This function is called only one during lifetime
 *                  of the application.
 ********************************************************************/
void HTTPInit(void)
{
    BYTE i;

    for ( i = 0; i <  MAX_HTTP_CONNECTIONS; i++ )
    {
        HCB[i].socket = TCPOpen(0, TCP_OPEN_SERVER, HTTP_PORT, TCP_PURPOSE_HTTP_SERVER);

        HCB[i].smHTTP = SM_HTTP_IDLE;
    }
}



/*********************************************************************
 * Function:        void HTTPServer(void)
 *
 * PreCondition:    HTTPInit() must already be called.
 *
 * Input:           None
 *
 * Output:          Opened HTTP connections are served.
 *
 * Side Effects:    None
 *
 * Overview:        Browse through each connections and let it
 *                  handle its connection.
 *                  If a connection is not finished, do not process
 *                  next connections.  This must be done, all
 *                  connections use some static variables that are
 *                  common.
 *
 * Note:            This function acts as a task (similar to one in
 *                  RTOS).  This function performs its task in
 *                  co-operative manner.  Main application must call
 *                  this function repeatdly to ensure all open
 *                  or new connections are served on time.
 ********************************************************************/
void HTTPServer(void)
{
    BYTE conn;

    for ( conn = 0;  conn < MAX_HTTP_CONNECTIONS; conn++ )
        HTTPProcess(conn);
}


/*********************************************************************
 * Function:        static BOOL HTTPProcess(HTTP_HANDLE h)
 *
 * PreCondition:    HTTPInit() called.
 *
 * Input:           h   -   Index to the handle which needs to be
 *                          processed.
 *
 * Output:          Connection referred by 'h' is served.
 *
 * Side Effects:    None
 *
 * Overview:
 *
 * Note:            None.
 ********************************************************************/
static void HTTPProcess(HTTP_HANDLE h)
{
    BYTE httpData[MAX_HTML_CMD_LEN+1];
    HTTP_COMMAND httpCommand;
    BOOL lbContinue;
    BYTE *arg[MAX_HTTP_ARGS];
    BYTE argc;
    HTTP_INFO* ph;
	WORD w;

    ph = &HCB[h];


    do
    {
        lbContinue = FALSE;

        // If during handling of HTTP socket, it gets disconnected,
        // forget about previous processing and return to idle state.
        if(!TCPIsConnected(ph->socket))
        {
            ph->smHTTP = SM_HTTP_IDLE;
            break;
        }


        switch(ph->smHTTP)
        {
        case SM_HTTP_IDLE:
			// Search for the CRLF deliminating the end of the first GET/HEAD/POST request
			w = TCPFindROMArray(ph->socket, (ROM BYTE*)"\r\n", 2, 0, FALSE);
            if(w == 0xFFFFu)
			{
				if(TCPGetRxFIFOFree(ph->socket) == 0)
				{
					// Request is too big, we can't support it.
					TCPDisconnect(ph->socket);
				}
				break;
			}

            lbContinue = TRUE;

			if(w > sizeof(httpData)-1)
				w = sizeof(httpData)-1;

			TCPGetArray(ph->socket, httpData, w);
            httpData[w] = '\0';
            TCPDiscard(ph->socket);

            ph->smHTTP = SM_HTTP_NOT_FOUND;
            argc = MAX_HTTP_ARGS;
            httpCommand = HTTPParse(httpData, arg, &argc, &ph->fileType);
            if ( httpCommand == HTTP_GET )
            {
                // If there are any arguments, this must be a remote command.
                // Execute it and then send the file.
                // The file name may be modified by command handler.
                if ( argc > 1u )
                {
                    // Let main application handle this remote command.
                    HTTPExecCmd(&arg[0], argc);

                    // Command handler must have modified arg[0] which now
                    // points to actual file that will be sent as a result of
                    // this remote command.

                    // Assume that Web author will only use CGI or HTML
                    // file for remote command.
                    ph->fileType = HTTP_CGI;
                }

                ph->file = MPFSOpen(arg[0]);
                if ( ph->file == MPFS_INVALID )
                {
                    ph->Variable = HTTP_NOT_FOUND;
                    ph->smHTTP = SM_HTTP_NOT_FOUND;
                }
                else if ( ph->file == MPFS_NOT_AVAILABLE )
                {
                    ph->Variable = HTTP_NOT_AVAILABLE;
                    ph->smHTTP = SM_HTTP_NOT_FOUND;
                }
                else
                {
                    ph->smHTTP = SM_HTTP_HEADER;
                }
            }
            break;

        case SM_HTTP_NOT_FOUND:
            if(TCPIsPutReady(ph->socket) >= strlenpgm((ROM char*)HTTPMessages[ph->Variable]))
            {
				TCPPutROMString(ph->socket, HTTPMessages[ph->Variable]);
				TCPFlush(ph->socket);
				TCPDisconnect(ph->socket);
				ph->smHTTP = SM_HTTP_IDLE;
            }
            break;

        case SM_HTTP_HEADER:
            if ( TCPIsPutReady(ph->socket) )
            {
                lbContinue = TRUE;

                if ( ph->fileType == HTTP_DYNAMIC_FILE_TYPE )
				{
                    ph->bProcess = TRUE;
					TCPPutROMArray(ph->socket, (ROM BYTE*)HTTP_OK_NO_CACHE_STRING, HTTP_OK_NO_CACHE_STRING_LEN);
				}
                else
				{
                    ph->bProcess = FALSE;
					TCPPutROMArray(ph->socket, (ROM BYTE*)HTTP_OK_STRING, HTTP_OK_STRING_LEN);
				}

                TCPPutROMString(ph->socket, httpContents[ph->fileType].typeString);
				TCPPutROMArray(ph->socket, (ROM BYTE*)HTTP_HEADER_END_STRING, HTTP_HEADER_END_STRING_LEN);

                ph->smHTTPGet = SM_HTTP_GET_READ;
                ph->smHTTP = SM_HTTP_GET;
            }
            break;

        case SM_HTTP_GET:
			// Throw away any more data receieved - we aren't going to use it.
			TCPDiscard(ph->socket);

            if(SendFile(ph))
            {
                MPFSClose();
				TCPDisconnect(ph->socket);
                ph->smHTTP = SM_HTTP_IDLE;
            }
            break;

		default:
			break;
        }
    } while( lbContinue );
}


/*********************************************************************
 * Function:        static BOOL SendFile(HTTP_INFO* ph)
 *
 * PreCondition:    None
 *
 * Input:           ph      -   A HTTP connection info.
 *
 * Output:          File reference by this connection is served.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None.
 ********************************************************************/
static BOOL SendFile(HTTP_INFO* ph)
{
    BOOL lbTransmit;
    BYTE c;
	BYTE buffer[8];
	WORD w;
    WORD_VAL HexNumber;

    MPFSGetBegin(ph->file);

	// Check if file is dynamic (.cgi) -- need to look for and 
	// process escape sequences
	if(ph->bProcess)					
	{
		w = TCPIsPutReady(ph->socket);
		while(w)
		{
	        lbTransmit = FALSE;
	
	        if(ph->smHTTPGet != SM_HTTP_GET_VAR)
	        {
	            c = MPFSGet();
	            if(MPFSIsEOF())
	            {
	                MPFSGetEnd();
	                TCPFlush(ph->socket);
	                return TRUE;
	            }
	        }
	
			switch(ph->smHTTPGet)
			{
			case SM_HTTP_GET_READ:
			    if ( c == HTTP_VAR_ESC_CHAR )
			        ph->smHTTPGet = SM_HTTP_GET_DLE;
			    else
			        lbTransmit = TRUE;
			    break;
			
			case SM_HTTP_GET_DLE:
			    if ( c == HTTP_VAR_ESC_CHAR )
			    {
			        lbTransmit = TRUE;
			        ph->smHTTPGet = SM_HTTP_GET_READ;
			    }
			    else
			    {
			        HexNumber.v[1] = c;
			        ph->smHTTPGet = SM_HTTP_GET_HANDLE;
			    }
			    break;
			
			case SM_HTTP_GET_HANDLE:
			    HexNumber.v[0] = c;
			    ph->Variable = hexatob(HexNumber);
			
			    ph->smHTTPGet = SM_HTTP_GET_VAR;
			    ph->VarRef = HTTP_START_OF_VAR;
			    break;
			
			case SM_HTTP_GET_VAR:
			    ph->VarRef = HTTPGetVar(ph->Variable, ph->VarRef, &c);
			    lbTransmit = TRUE;
			    if ( ph->VarRef == HTTP_END_OF_VAR )
			        ph->smHTTPGet = SM_HTTP_GET_READ;
			    break;
			}
			
			if(lbTransmit)
			{
			    TCPPut(ph->socket, c);
				w--;
			}
		}
    }	
	else	// Static page content -- no processing required
	{
		w = TCPIsPutReady(ph->socket);
		while(w >= sizeof(buffer))
		{
			for(c = 0; c < sizeof(buffer); c++)
			{
				buffer[c] = MPFSGet();
				if(MPFSIsEOF())
				{
	                MPFSGetEnd();
					TCPPutArray(ph->socket, buffer, c);
	                TCPFlush(ph->socket);
					return TRUE;
				}
			}
	
			TCPPutArray(ph->socket, buffer, sizeof(buffer));
			w -= sizeof(buffer);
			lbTransmit = TRUE;
		}
		if(lbTransmit)
			TCPFlush(ph->socket);
	}
    ph->file = MPFSGetEnd();

    // We are not done sending a file yet...
    return FALSE;
}


/*********************************************************************
 * Function:        static HTTP_COMMAND HTTPParse(BYTE *string,
 *                                              BYTE** arg,
 *                                              BYTE* argc,
 *                                              BYTE* type)
 *
 * PreCondition:    None
 *
 * Input:           string      - HTTP Command String
 *                  arg         - List of string pointer to hold
 *                                HTTP arguments.
 *                  argc        - Pointer to hold total number of
 *                                arguments in this command string/
 *                  type        - Pointer to hold type of file
 *                                received.
 *                              Valid values are:
 *                                  HTTP_TXT
 *                                  HTTP_HTML
 *                                  HTTP_GIF
 *                                  HTTP_CGI
 *                                  HTTP_UNKNOWN
 *
 * Output:          HTTP FSM and connections are initialized
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            This function parses URL that may or may not
 *                  contain arguments.
 *                  e.g. "GET HTTP/1.0 thank.htm?name=MCHP&age=12"
 *                      would be returned as below:
 *                          arg[0] => GET
 *                          arg[1] => thank.htm
 *                          arg[2] => name
 *                          arg[3] => MCHP
 *                          arg[4] => 12
 *                          argc = 5
 *
 *                  This parses does not "de-escape" URL string.
 ********************************************************************/
static HTTP_COMMAND HTTPParse(BYTE *string,
                            BYTE** arg,
                            BYTE* argc,
                            BYTE* type)
{
    BYTE i;
    BYTE smParse;
    HTTP_COMMAND cmd;
    BYTE *ext;
    BYTE c;
    ROM BYTE *fileType;

    enum
    {
        SM_PARSE_IDLE,
        SM_PARSE_ARG,
        SM_PARSE_ARG_FORMAT
    };

    smParse = SM_PARSE_IDLE;
    ext = NULL;
    i = 0;

    // Only "GET" is supported for time being.
    if ( !memcmppgm2ram(string, (ROM void*) HTTP_GET_STRING, HTTP_GET_STRING_LEN) )
    {
        string += (HTTP_GET_STRING_LEN + 1);
        cmd = HTTP_GET;
    }
    else
    {
        return HTTP_NOT_SUPPORTED;
    }

    // Skip white spaces.
    while( *string == ' ' )
        string++;

    c = *string;

    while ( c != ' ' &&  c != '\0' && c != '\r' && c != '\n' )

    {
        // Do not accept any more arguments than we haved designed to.
        if ( i >= *argc )
            break;

        switch(smParse)
        {
        case SM_PARSE_IDLE:
            arg[i] = string;
            c = *string;
            if ( c == '/' || c == '\\' )
                smParse = SM_PARSE_ARG;
            break;

        case SM_PARSE_ARG:
            arg[i++] = string;
            smParse = SM_PARSE_ARG_FORMAT;
            /*
             * Do not break.
             * Parameter may be empty.
             */

        case SM_PARSE_ARG_FORMAT:
            c = *string;
            if ( c == '?' || c == '&' )
            {
                *string = '\0';
                smParse = SM_PARSE_ARG;
            }
            else
            {
                // Recover space characters.
                if ( c == '+' )
                    *string = ' ';

                // Remember where file extension starts.
                else if ( c == '.' && i == 1u )
                {
                    ext = ++string;
                }

                else if ( c == '=' )
                {
                    *string = '\0';
                    smParse = SM_PARSE_ARG;
                }

                // Only interested in file name - not a path.
                else if ( c == '/' || c == '\\' )
                    arg[i-1] = string+1;

            }
            break;
        }
        string++;
        c = *string;
    }
    *string = '\0';

    *type = HTTP_UNKNOWN;
    if ( ext != NULL )
    {
        ext = (BYTE*)strupr((char*)ext);

        fileType = httpFiles[0].fileExt;
        for ( c = 0; c < TOTAL_FILE_TYPES; c++ )
        {
            if ( !memcmppgm2ram((void*)ext, (ROM void*)fileType, FILE_EXT_LEN) )
            {
                *type = c;
                break;
            }
            fileType += sizeof(FILE_TYPES);

        }
    }

    if ( i == 0u )
    {
        memcpypgm2ram(arg[0], (ROM void*)HTTP_DEFAULT_FILE_STRING,
                                     HTTP_DEFAULT_FILE_STRING_LEN);
        arg[0][HTTP_DEFAULT_FILE_STRING_LEN] = '\0';
        *type = HTTP_HTML;
        i++;
    }
    *argc = i;

    return cmd;
}


#endif //#if defined(STACK_USE_HTTP_SERVER)
