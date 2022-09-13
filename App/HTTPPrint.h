/**************************************************************
 * HTTPPrint.h
 * Provides callback headers and resolution for user's custom
 * HTTP Application.
 * 
 * This file is automatically generated by the MPFS Utility
 * ALL MODIFICATIONS WILL BE OVERWRITTEN BY THE MPFS GENERATOR
 **************************************************************/

#ifndef __HTTPPRINT_H
#define __HTTPPRINT_H

#include "TCPIP Stack/TCPIP.h"

#if defined(STACK_USE_HTTP2_SERVER)

extern HTTP_CONN curHTTP;
extern HTTP_STUB httpStubs[MAX_HTTP_CONNECTIONS];
extern BYTE curHTTPID;

void HTTPPrint(DWORD callbackID);
void HTTPPrint_YR(void);
void HTTPPrint_MH(void);
void HTTPPrint_DA(void);
void HTTPPrint_HR(void);
void HTTPPrint_ME(void);
void HTTPPrint_SD(void);
void HTTPPrint_TP(void);
void HTTPPrint_devSelected(WORD,WORD);
void HTTPPrint_ledSelected(WORD,WORD);

void HTTPPrint(DWORD callbackID)
{
	switch(callbackID)
	{
        case 0x00000000:
			HTTPPrint_YR();
			break;
        case 0x00000001:
			HTTPPrint_MH();
			break;
        case 0x00000002:
			HTTPPrint_DA();
			break;
        case 0x00000003:
			HTTPPrint_HR();
			break;
        case 0x00000004:
			HTTPPrint_ME();
			break;
        case 0x00000005:
			HTTPPrint_SD();
			break;
        case 0x00000006:
			HTTPPrint_TP();
			break;
        case 0x00000007:
			HTTPPrint_devSelected(0,TRUE);
			break;
        case 0x00000008:
			HTTPPrint_devSelected(0,FALSE);
			break;
        case 0x00000009:
			HTTPPrint_devSelected(1,TRUE);
			break;
        case 0x0000000a:
			HTTPPrint_devSelected(1,FALSE);
			break;
        case 0x0000000b:
			HTTPPrint_ledSelected(0,TRUE);
			break;
        case 0x0000000c:
			HTTPPrint_ledSelected(0,FALSE);
			break;
        case 0x0000000d:
			HTTPPrint_ledSelected(1,TRUE);
			break;
        case 0x0000000e:
			HTTPPrint_ledSelected(1,FALSE);
			break;
        case 0x0000000f:
			HTTPPrint_ledSelected(2,TRUE);
			break;
        case 0x00000010:
			HTTPPrint_ledSelected(2,FALSE);
			break;
        case 0x00000011:
			HTTPPrint_ledSelected(3,TRUE);
			break;
        case 0x00000012:
			HTTPPrint_ledSelected(3,FALSE);
			break;
        case 0x00000013:
			HTTPPrint_ledSelected(4,TRUE);
			break;
        case 0x00000014:
			HTTPPrint_ledSelected(4,FALSE);
			break;
        case 0x00000015:
			HTTPPrint_ledSelected(5,TRUE);
			break;
        case 0x00000016:
			HTTPPrint_ledSelected(5,FALSE);
			break;
        case 0x00000017:
			HTTPPrint_ledSelected(6,TRUE);
			break;
        case 0x00000018:
			HTTPPrint_ledSelected(6,FALSE);
			break;
        case 0x00000019:
			HTTPPrint_ledSelected(7,TRUE);
			break;
        case 0x0000001a:
			HTTPPrint_ledSelected(7,FALSE);
			break;
		default:
			// Output notification for undefined values
			TCPPutROMArray(sktHTTP, (ROM BYTE*)"!DEF", 4);
	}

	return;
}

void HTTPPrint_(void)
{
	TCPPut(sktHTTP, '~');
	return;
}

#endif

#endif