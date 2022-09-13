/*********************************************************************
 *
 *           Helper Functions for Microchip TCP/IP Stack
 *
 *********************************************************************
 * FileName:		Helpers.C
 * Dependencies:	None
 * Processor:       PIC18, PIC24F, PIC24H, dsPIC30F, dsPIC33F, PIC32
 * Compiler:        Microchip C32 v1.00 or higher
 *					Microchip C30 v3.01 or higher
 *					Microchip C18 v3.13 or higher
 *					HI-TECH PICC-18 STD 9.50PL3 or higher
 * Company:			Microchip Technology, Inc.
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
 * Nilesh Rajbharti     5/17/01 Original        (Rev 1.0)
 * Nilesh Rajbharti     2/9/02  Cleanup
 * Nilesh Rajbharti     6/25/02 Rewritten CalcIPChecksum() to avoid
 *                              multi-byte shift operation.
 * Howard Schlunder		2/9/05  Added hexatob(), btohexa_high(), and
 *							    btohexa_low()
 * Howard Schlunder    10/10/06 Optimized swapl()
 * Elliott Wood		   11/20/07	Added leftRotateDWORD()
 ********************************************************************/
#define __HELPERS_C

#include "TCPIP Stack/TCPIP.h"

#include "ch32v30x.h"

/*****************************************************************************
  Function:
	DWORD GenerateRandomDWORD(void)

  Summary:
	Generates a random DWORD.

  Description:
	This function generates a random 32-bit integer.  It collects
	randomness by comparing the A/D converter's internal R/C oscillator
	clock with our main system clock.  By passing collected entropy to the
	C rand()/srand() functions, the output is normalized to meet statistical
	randomness tests.

  Precondition:
	None

  Parameters:
	None

  Returns:
  	Random 32-bit number.
  	
  Side Effects:
	This function uses the A/D converter (and so you must disable 
	interrupts if you use the A/D converted in your ISR).  The C rand()
	function will be reseeded, and Timer0 (PIC18) and Timer1 (PIC24, 
	dsPIC, and PIC32) will be used.  TMR#H:TMR#L will have a new value.
	Note that this is the same timer used by the Tick module.
	
  Remarks:
	This function times out after 1 second of attempting to generate the 
	random DWORD.  In such a case, the output may not be truly random.  
	Typically, this function executes in around 500,000 instruction cycles.
	
	The intent of this function is to produce statistically random and
	cryptographically secure random number.  Whether or not this is true on
	all (or any) devices/voltages/temperatures is not tested.
  ***************************************************************************/
DWORD GenerateRandomDWORD(void)
{
     while(1)
     {
         if(RNG_GetFlagStatus(RNG_FLAG_DRDY) == SET)
         {
             break;
         }
         if(RNG_GetFlagStatus(RNG_FLAG_CECS) == SET)
         {
             RNG_ClearFlag(RNG_FLAG_CECS);
             //Delay_Us(100);
         }
         if(RNG_GetFlagStatus(RNG_FLAG_SECS) == SET)
         {
             RNG_ClearFlag(RNG_FLAG_SECS);
             RNG_Cmd(DISABLE);
             //Delay_Us(100);
             RNG_Cmd(ENABLE);
             //Delay_Us(100);
         }
     }

     DWORD rand = RNG_GetRandomNumber();
     TRACE("rand:0x%08X\n", rand);

     return rand;
}

#if defined(STACK_USE_HTTP_SERVER)
/*****************************************************************************
  Function:
	void UnencodeURL(BYTE* URL)

  Summary:
	Decodes a URL-encoded string.

  Description:
	This function is deprecated except for use with HTTP Classic.  It
	attempts to decode a URL encoded string, converting all hex escape
	sequences into a literal byte.  However, it is inefficient over long
	strings and does not handle URL-encoded data strings ('&' and '=').

  Precondition:
	None

  Parameters:
	URL - the null-terminated string to decode

  Returns:
  	None
  ***************************************************************************/
void UnencodeURL(BYTE* URL)
{
	BYTE *Right, *Copy;
	WORD_VAL Number;

	while((Right = (BYTE*)strchr((char*)URL, '%')))
	{
		// Make sure the string is long enough
		if(Right[1] == '\0')
			break;
		if(Right[2] == '\0')
			break;

		// Update the string in place
		Number.v[0] = Right[2];
		Number.v[1] = Right[1];
		*Right++ = hexatob(Number);
		URL = Right;

		// Remove two blank spots by shifting all remaining characters right two
		Copy = Right + 2;
		while((*Right++ = *Copy++));
	}
}		    
#endif


/*****************************************************************************
  Function:
	BOOL StringToIPAddress(BYTE* str, IP_ADDR* IPAddress)

  Summary:
	Converts a string to an IP address

  Description:
	This function parses a dotted-quad decimal IP address string into an 
	IP_ADDR struct.  The output result is big-endian.
	
  Precondition:
	None

  Parameters:
	str - Pointer to a dotted-quad IP address string
	IPAddress - Pointer to IP_ADDR in which to store the result

  Return Values:
  	TRUE - an IP address was successfully decoded
  	FALSE - no IP address could be found, or the format was incorrect
  ***************************************************************************/
BOOL StringToIPAddress(BYTE* str, IP_ADDR* IPAddress)
{
	DWORD_VAL dwVal;
	BYTE i, charLen, currentOctet;

	charLen = 0;
	currentOctet = 0;
	dwVal.Val = 0;
	while((i = *str++))
	{
		if(currentOctet > 3u)
			break;

		i -= '0';
		

		// Validate the character is a numerical digit or dot, depending on location
		if(charLen == 0u)
		{
			if(i > 9u)
				return FALSE;
		}
		else if(charLen == 3u)
		{
			if(i != (BYTE)('.' - '0'))
				return FALSE;

			if(dwVal.Val > 0x00020505ul)
				return FALSE;

			IPAddress->v[currentOctet++] = dwVal.v[2]*((BYTE)100) + dwVal.v[1]*((BYTE)10) + dwVal.v[0];
			charLen = 0;
			dwVal.Val = 0;
			continue;
		}
		else
		{
			if(i == (BYTE)('.' - '0'))
			{
				if(dwVal.Val > 0x00020505ul)
					return FALSE;

				IPAddress->v[currentOctet++] = dwVal.v[2]*((BYTE)100) + dwVal.v[1]*((BYTE)10) + dwVal.v[0];
				charLen = 0;
				dwVal.Val = 0;
				continue;
			}
			if(i > 9u)
				return FALSE;
		}

		charLen++;
		dwVal.Val <<= 8;
		dwVal.v[0] = i;
	}

	// Make sure the very last character is a valid termination character 
	// (i.e., not more hostname, which could be legal and not an IP 
	// address as in "10.5.13.233.picsaregood.com"
	if(i != 0u && i != '/' && i != '\r' && i != '\n' && i != ' ' && i != '\t')
		return FALSE;

	// Verify and convert the last octet and return the result
	if(dwVal.Val > 0x00020505ul)
		return FALSE;

	IPAddress->v[3] = dwVal.v[2]*((BYTE)100) + dwVal.v[1]*((BYTE)10) + dwVal.v[0];

	return TRUE;
}

/*****************************************************************************
  Function:
	BOOL ROMStringToIPAddress(ROM BYTE* str, IP_ADDR* IPAddress)

  Summary:
	Converts a string to an IP address

  Description:
	This function parses a dotted-quad decimal IP address string into an 
	IP_ADDR struct.  The output result is big-endian.
	
  Precondition:
	None

  Parameters:
	str - Pointer to a dotted-quad IP address string
	IPAddress - Pointer to IP_ADDR in which to store the result

  Return Values:
  	TRUE - an IP address was successfully decoded
  	FALSE - no IP address could be found, or the format was incorrect
  
  Remarks:
	This function is aliased to StringToIPAddress on non-PIC18 platforms.
  ***************************************************************************/
#if defined(__18CXX)
BOOL ROMStringToIPAddress(ROM BYTE* str, IP_ADDR* IPAddress)
{
	DWORD_VAL dwVal;
	BYTE i, charLen, currentOctet;

	charLen = 0;
	currentOctet = 0;
	dwVal.Val = 0;
	while(i = *str++)
	{
		if(currentOctet > 3u)
			break;

		i -= '0';
		

		// Validate the character is a numerical digit or dot, depending on location
		if(charLen == 0u)
		{
			if(i > 9u)
				return FALSE;
		}
		else if(charLen == 3u)
		{
			if(i != (BYTE)('.' - '0'))
				return FALSE;

			if(dwVal.Val > 0x00020505ul)
				return FALSE;

			IPAddress->v[currentOctet++] = dwVal.v[2]*((BYTE)100) + dwVal.v[1]*((BYTE)10) + dwVal.v[0];
			charLen = 0;
			dwVal.Val = 0;
			continue;
		}
		else
		{
			if(i == (BYTE)('.' - '0'))
			{
				if(dwVal.Val > 0x00020505ul)
					return FALSE;

				IPAddress->v[currentOctet++] = dwVal.v[2]*((BYTE)100) + dwVal.v[1]*((BYTE)10) + dwVal.v[0];
				charLen = 0;
				dwVal.Val = 0;
				continue;
			}
			if(i > 9u)
				return FALSE;
		}

		charLen++;
		dwVal.Val <<= 8;
		dwVal.v[0] = i;
	}

	// Make sure the very last character is a valid termination character 
	// (i.e., not more hostname, which could be legal and not an IP 
	// address as in "10.5.13.233.picsaregood.com"
	if(i != 0u && i != '/' && i != '\r' && i != '\n' && i != ' ' && i != '\t')
		return FALSE;

	// Verify and convert the last octet and return the result
	if(dwVal.Val > 0x00020505ul)
		return FALSE;

	IPAddress->v[3] = dwVal.v[2]*((BYTE)100) + dwVal.v[1]*((BYTE)10) + dwVal.v[0];

	return TRUE;
}
#endif



/*****************************************************************************
  Function:
	WORD Base64Decode(BYTE* cSourceData, WORD wSourceLen, 
						BYTE* cDestData, WORD wDestLen)

  Description:
	Decodes a Base-64 array to its literal representation.
	
  Precondition:
	None

  Parameters:
	cSourceData - Pointer to a string of Base-64 encoded data
	wSourceLen	- Length of the Base-64 source data
	cDestData	- Pointer to write the decoded data
	wSourceLen	- Maximum length that can be written to cDestData

  Returns:
  	Number of decoded bytes written to cDestData.
  
  Remarks:
	This function is binary safe and will ignore invalid characters (CR, LF, 
	etc).  If cSourceData is equal to cDestData, the data will be converted
	in-place.  If cSourceData is not equal to cDestData, but the regions 
	overlap, the behavior is undefined.
	
	Decoded data is always at least 1/4 smaller than the source data.
  ***************************************************************************/
#if defined(STACK_USE_BASE64_DECODE)
WORD Base64Decode(BYTE* cSourceData, WORD wSourceLen, BYTE* cDestData, WORD wDestLen)
{
	BYTE i;
	BYTE vByteNumber;
	BOOL bPad;
	WORD wBytesOutput;

	vByteNumber = 0;
	wBytesOutput = 0;

	// Loop over all provided bytes
	while(wSourceLen--)
	{
		// Fetch a Base64 byte and decode it to the original 6 bits
		i = *cSourceData++;
		bPad = (i == '=');
		if(i >= 'A' && i <= 'Z')	// Regular data
			i -= 'A' - 0;
		else if(i >= 'a' && i <= 'z')
			i -= 'a' - 26;
		else if(i >= '0' && i <= '9')
			i -= '0' - 52;
		else if(i == '+' || i == '-')
			i = 62;
		else if(i == '/' || i == '_')
			i = 63;
		else 						// Skip all padding (=) and non-Base64 characters
			continue;


		// Write the 6 bits to the correct destination location(s)
		if(vByteNumber == 0u)
		{
			if(bPad)				// Padding here would be illegal, treat it as a non-Base64 chacter and just skip over it
				continue;
			vByteNumber++;
			if(wBytesOutput >= wDestLen)
				break;
			wBytesOutput++;
			*cDestData = i << 2;
		}
		else if(vByteNumber == 1u)
		{
			vByteNumber++;
			*cDestData++ |= i >> 4;
			if(wBytesOutput >= wDestLen)
				break;
			if(bPad)
				continue;
			wBytesOutput++;
			*cDestData = i << 4;
		}
		else if(vByteNumber == 2u)
		{
			vByteNumber++;
			*cDestData++ |= i >> 2;
			if(wBytesOutput >= wDestLen)
				break;
			if(bPad)
				continue;
			wBytesOutput++;
			*cDestData = i << 6;
		}
		else if(vByteNumber == 3u)
		{
			vByteNumber = 0;
			*cDestData++ |= i;
		}
	}

	return wBytesOutput;
}
#endif	// #if defined(STACK_USE_BASE64_DECODE)


/*****************************************************************************
  Function:
	WORD Base64Encode(BYTE* cSourceData, WORD wSourceLen,
						BYTE* cDestData, WORD wDestLen)

  Description:
	Encodes a binary array to Base-64.
	
  Precondition:
	None

  Parameters:
	cSourceData - Pointer to a string of binary data
	wSourceLen	- Length of the binary source data
	cDestData	- Pointer to write the Base-64 encoded data
	wSourceLen	- Maximum length that can be written to cDestData

  Returns:
  	Number of encoded bytes written to cDestData.  This will always be
  	a multiple of 4.
  
  Remarks:
	Encoding cannot be performed in-place.  If cSourceData overlaps with 
	cDestData, the behavior is undefined.
	
	Encoded data is always at least 1/3 larger than the source data.  It may
	be 1 or 2 bytes larger than that.
  ***************************************************************************/
#if defined(STACK_USE_BASE64_ENCODE) || defined(STACK_USE_SMTP_CLIENT) || defined(STACK_USE_DYNAMICDNS_CLIENT)
WORD Base64Encode(BYTE* cSourceData, WORD wSourceLen, BYTE* cDestData, WORD wDestLen)
{
	BYTE i, j;
	BYTE vOutput[4];
	WORD wOutputLen;

	wOutputLen = 0;
	while(wDestLen >= 4u)
	{
		// Start out treating the output as all padding
		vOutput[0] = 0xFF;
		vOutput[1] = 0xFF;
		vOutput[2] = 0xFF;
		vOutput[3] = 0xFF;

		// Get 3 input octets and split them into 4 output hextets (6-bits each) 
		if(wSourceLen == 0u)
			break;
		i = *cSourceData++;
		wSourceLen--;
		vOutput[0] = (i & 0xFC)>>2;
		vOutput[1] = (i & 0x03)<<4;
		if(wSourceLen)
		{
			i = *cSourceData++;
			wSourceLen--;
			vOutput[1] |= (i & 0xF0)>>4;
			vOutput[2] = (i & 0x0F)<<2;
			if(wSourceLen)
			{
				i = *cSourceData++;
				wSourceLen--;
				vOutput[2] |= (i & 0xC0)>>6;
				vOutput[3] = i & 0x3F;
			}
		}
	
		// Convert hextets into Base 64 alphabet and store result
		for(i = 0; i < 4u; i++)
		{
			j = vOutput[i];

			if(j <= 25u)
				j += 'A' - 0;
			else if(j <= 51u)
				j += 'a' - 26;
			else if(j <= 61u)
				j += '0' - 52;
			else if(j == 62u)
				j = '+';
			else if(j == 63u)
				j = '/';
			else				// Padding
				j = '=';

			*cDestData++ = j;
		}

		// Update counters
		wDestLen -= 4;
		wOutputLen += 4;
	}

	return wOutputLen;
}
#endif // #if defined(STACK_USE_BASE64_ENCODE) || defined(STACK_USE_SMTP) || defined(STACK_USE_DYNAMICDNS_CLIENT)


/*****************************************************************************
  Function:
	void uitoa(WORD Value, BYTE* Buffer)

  Summary:
	Converts an unsigned integer to a decimal string.
	
  Description:
	Converts a 16-bit unsigned integer to a null-terminated decimal string.
	
  Precondition:
	None

  Parameters:
	Value	- The number to be converted
	Buffer	- Pointer in which to store the converted string

  Returns:
  	None
  ***************************************************************************/
void uitoa(WORD Value, BYTE* Buffer)
{
	BYTE i;
	WORD Digit;
	WORD Divisor;
	BOOL Printed = FALSE;

	if(Value)
	{
		for(i = 0, Divisor = 10000; i < 5u; i++)
		{
			Digit = Value/Divisor;
			if(Digit || Printed)
			{
				*Buffer++ = '0' + Digit;
				Value -= Digit*Divisor;
				Printed = TRUE;
			}
			Divisor /= 10;
		}
	}
	else
	{
		*Buffer++ = '0';
	}

	*Buffer = '\0';
}			    

/*****************************************************************************
  Function:
	void ultoa(DWORD Value, BYTE* Buffer)

  Summary:
	Converts an unsigned integer to a decimal string.
	
  Description:
	Converts a 32-bit unsigned integer to a null-terminated decimal string.
	
  Precondition:
	None

  Parameters:
	Value	- The number to be converted
	Buffer	- Pointer in which to store the converted string

  Returns:
  	None
  ***************************************************************************/
#if !defined(__18CXX) || defined(HI_TECH_C)
void ultoa(DWORD Value, BYTE* Buffer)
{
	BYTE i;
	DWORD Digit;
	DWORD Divisor;
	BOOL Printed = FALSE;

	if(Value)
	{
		for(i = 0, Divisor = 1000000000; i < 10; i++)
		{
			Digit = Value/Divisor;
			if(Digit || Printed)
			{
				*Buffer++ = '0' + Digit;
				Value -= Digit*Divisor;
				Printed = TRUE;
			}
			Divisor /= 10;
		}
	}
	else
	{
		*Buffer++ = '0';
	}

	*Buffer = '\0';
}
#endif

/*****************************************************************************
  Function:
	BYTE hexatob(WORD_VAL AsciiChars)

  Summary:
	Converts a hex string to a single byte.
	
  Description:
	Converts a two-character ASCII hex string to a single packed byte.
	
  Precondition:
	None

  Parameters:
	AsciiChars - WORD_VAL where .v[0] is the ASCII value for the lower nibble
					and .v[1] is the ASCII value for the upper nibble.  Each
					must range from '0'-'9', 'A'-'F', or 'a'-'f'.

  Returns:
  	Resulting packed byte 0x00 - 0xFF.
  ***************************************************************************/
BYTE hexatob(WORD_VAL AsciiChars)
{
	// Convert lowercase to uppercase
	if(AsciiChars.v[1] > 'F')
		AsciiChars.v[1] -= 'a'-'A';
	if(AsciiChars.v[0] > 'F')
		AsciiChars.v[0] -= 'a'-'A';

	// Convert 0-9, A-F to 0x0-0xF
	if(AsciiChars.v[1] > '9')
		AsciiChars.v[1] -= 'A' - 10;
	else
		AsciiChars.v[1] -= '0';

	if(AsciiChars.v[0] > '9')
		AsciiChars.v[0] -= 'A' - 10;
	else
		AsciiChars.v[0] -= '0';

	// Concatenate
	return (AsciiChars.v[1]<<4) |  AsciiChars.v[0];
}

/*****************************************************************************
  Function:
	BYTE btohexa_high(BYTE b)

  Summary:
	Converts the upper nibble of a binary value to a hexadecimal ASCII byte.

  Description:
	Converts the upper nibble of a binary value to a hexadecimal ASCII byte.
	For example, btohexa_high(0xAE) will return 'A'.

  Precondition:
	None

  Parameters:
	b - the byte to convert

  Returns:
  	The upper hexadecimal ASCII byte '0'-'9' or 'A'-'F'.
  ***************************************************************************/
BYTE btohexa_high(BYTE b)
{
	b >>= 4;
	return (b>0x9u) ? b+'A'-10:b+'0';
}

/*****************************************************************************
  Function:
	BYTE btohexa_high(BYTE b)

  Summary:
	Converts the lower nibble of a binary value to a hexadecimal ASCII byte.

  Description:
	Converts the lower nibble of a binary value to a hexadecimal ASCII byte.
	For example, btohexa_high(0xAE) will return 'E'.

  Precondition:
	None

  Parameters:
	b - the byte to convert

  Returns:
  	The lower hexadecimal ASCII byte '0'-'9' or 'A'-'F'.
  ***************************************************************************/
BYTE btohexa_low(BYTE b)
{
	b &= 0x0F;
	return (b>9u) ? b+'A'-10:b+'0';
}

/*****************************************************************************
  Function:
	signed char stricmppgm2ram(BYTE* a, ROM BYTE* b)

  Summary:
	Case-insensitive comparison of a string in RAM to a string in ROM.

  Description:
	Performs a case-insensitive comparison of a string in RAM to a string
	in ROM.  This function performs identically to strcmppgm2ram, except that
	the comparison is not case-sensitive.

  Precondition:
	None

  Parameters:
	a - Pinter to tring in RAM
	b - Pointer to string in ROM

  Return Values:
  	\-1 - a < b
  	0	- a = b
  	1	- a > b
  ***************************************************************************/
signed char stricmppgm2ram(BYTE* a, ROM BYTE* b)
{
	BYTE cA, cB;
	
	// Load first two characters
	cA = *a;
	cB = *b;
	
	// Loop until one string terminates
	while(cA != '\0' && cB != '\0')
	{
		// Shift case if necessary
		if(cA >= 'a' && cA <= 'z')
			cA -= 'a' - 'A';
		if(cB >= 'a' && cB <= 'z')
			cB -= 'a' - 'A';
			
		// Compare
		if(cA > cB)
			return 1;
		if(cA < cB)
			return -1;
		
		// Characters matched, so continue
		a++;
		b++;
		cA = *a;
		cB = *b;
	}
	
	// See if one string terminated first
	if(cA > cB)
		return 1;
	if(cA < cB)
		return -1;
		
	// Strings match
	return 0;
}

/*****************************************************************************
  Function:
	WORD swaps(WORD v)

  Description:
	Swaps the endian-ness of a WORD.

  Precondition:
	None

  Parameters:
	v - the WORD to swap

  Returns:
	The swapped version of v.
  ***************************************************************************/
WORD swaps(WORD v)
{
	WORD_VAL t;
	BYTE b;

	t.Val   = v;
	b       = t.v[1];
	t.v[1]  = t.v[0];
	t.v[0]  = b;

	return t.Val;
}

/*****************************************************************************
  Function:
	DWORD swapl(DWORD v)

  Description:
	Swaps the endian-ness of a DWORD.

  Precondition:
	None

  Parameters:
	v - the DWORD to swap

  Returns:
	The swapped version of v.
  ***************************************************************************/
DWORD swapl(DWORD v)
{
	// Swap bytes 0 and 3
	((DWORD_VAL*)&v)->v[0] ^= ((DWORD_VAL*)&v)->v[3];
	((DWORD_VAL*)&v)->v[3] ^= ((DWORD_VAL*)&v)->v[0];
	((DWORD_VAL*)&v)->v[0] ^= ((DWORD_VAL*)&v)->v[3];

	// Swap bytes 1 and 2
	((DWORD_VAL*)&v)->v[1] ^= ((DWORD_VAL*)&v)->v[2];
	((DWORD_VAL*)&v)->v[2] ^= ((DWORD_VAL*)&v)->v[1];
	((DWORD_VAL*)&v)->v[1] ^= ((DWORD_VAL*)&v)->v[2];

	return v;
}


/*****************************************************************************
  Function:
	WORD CalcIPChecksum(BYTE* buffer, WORD count)

  Summary:
	Calculates an IP checksum value.

  Description:
	This function calculates an IP checksum over an array of input data.  The
	checksum is the 16-bit one's complement of one's complement sum of all 
	words in the data (with zero-padding if an odd number of bytes are 
	summed).  This checksum is defined in RFC 793.

  Precondition:
	buffer is WORD aligned (even memory address) on 16- and 32-bit PICs.

  Parameters:
	buffer - pointer to the data to be checksummed
	count  - number of bytes to be checksummed

  Returns:
	The calculated checksum.
	
  Internal:
	This function could be improved to do 32-bit sums on PIC32 platforms.
  ***************************************************************************/
WORD CalcIPChecksum(BYTE* buffer, WORD count)
{
	WORD i;
	WORD *val;
	DWORD_VAL sum = {0x00000000ul};

	i = count >> 1;
	val = (WORD*)buffer;

	// Calculate the sum of all words
	while(i--)
		sum.Val += (DWORD)*val++;

	// Add in the sum of the remaining byte, if present
	if(((WORD_VAL*)&count)->bits.b0)
		sum.Val += (DWORD)*(BYTE*)val;

	// Do an end-around carry (one's complement arrithmatic)
	sum.Val = (DWORD)sum.w[0] + (DWORD)sum.w[1];

	// Do another end-around carry in case if the prior add 
	// caused a carry out
	sum.w[0] += sum.w[1];

	// Return the resulting checksum
	return ~sum.w[0];
}


/*****************************************************************************
  Function:
	WORD CalcIPBufferChecksum(WORD len)

  Summary:
	Calculates an IP checksum in the MAC buffer itself.

  Description:
	This function calculates an IP checksum over an array of input data 
	existing in the MAC buffer.  The checksum is the 16-bit one's complement 
	of one's complement sum of all words in the data (with zero-padding if 
	an odd number of bytes are summed).  This checksum is defined in RFC 793.

  Precondition:
	TCP is initialized and the MAC buffer pointer is set to the start of
	the buffer.

  Parameters:
	len - number of bytes to be checksummed

  Returns:
	The calculated checksum.

  Remarks:
	All Microchip MACs should perform this function in hardware.
  ***************************************************************************/
#if defined(NON_MCHP_MAC)
WORD CalcIPBufferChecksum(WORD len)
{
	DWORD_VAL Checksum = {0x00000000ul};
	WORD ChunkLen;
	BYTE DataBuffer[20];	// Must be an even size
	WORD *DataPtr;

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
	
	// Do an end-around carry (one's complement arrithmatic)
	Checksum.Val = (DWORD)Checksum.w[0] + (DWORD)Checksum.w[1];

	// Do another end-around carry in case if the prior add 
	// caused a carry out
	Checksum.w[0] += Checksum.w[1];

	// Return the resulting checksum
	return ~Checksum.w[0];
}
#endif

/*****************************************************************************
  Function:
	char* strupr(char* s)

  Summary:
	Converts a string to uppercase.

  Description:
	This function converts strings to uppercase on platforms that do not
	already have this function defined.  All lower-case characters are
	converted, an characters not included in 'a'-'z' are left as-is.

  Precondition:
	None

  Parameters:
	s - the null-terminated string to be converted.

  Returns:
	Pointer to the initial string.
  ***************************************************************************/
#if !defined(__18CXX) || defined(HI_TECH_C)
char* strupr(char* s)
{
	char c;
	char *t;

	t = s;
	while( (c = *t) )
	{
		if(c >= 'a' && c <= 'z')
		{
			*t -= ('a' - 'A');
		}
		t++;
	}
	return s;
}
#endif

#if defined(__18CXX)
// Make this variable global for the following function.
// Hi-Tech PICC18 cannot access local function variables from inline asm.
DWORD_VAL toRotate; 
#endif

/*****************************************************************************
  Function:
	DWORD leftRotateDWORD(DWORD val, BYTE bits)

  Summary:
	Left-rotates a DWORD.

  Description:
	This function rotates the bits in a 32-bit DWORD left by a specific 
	number of bits.

  Precondition:
	None

  Parameters:
	val		- the DWORD to be rotated
	bits	- the number of bits by which to shift

  Returns:
	Rotated DWORD value.
	
  Remarks:
	This function is only implemented on 8-bit platforms for now.  The 
	8-bit compilers generate excessive code for this function, while C30
	and C32 already generate compact code.  Those compilers are served
	by a macro defined in Helpers.h.
  ***************************************************************************/
#if defined(__18CXX)
DWORD leftRotateDWORD(DWORD val, BYTE bits)
{
	BYTE i, t;
	//DWORD_VAL toRotate;
	toRotate.Val = val;
	
	for(i = bits; i >= 8u; i -= 8)
	{
		t = toRotate.v[3];
		toRotate.v[3] = toRotate.v[2];
		toRotate.v[2] = toRotate.v[1];
		toRotate.v[1] = toRotate.v[0];
		toRotate.v[0] = t;
	}
	
	
	#if defined(HI_TECH_C)
	for(; i != 0; i--)
	{
		asm("movlb (_toRotate)>>8");
		asm("bcf _STATUS,0,C");
		asm("btfsc (_toRotate)&0ffh+3,7,B");
		asm("bsf _STATUS,0,C");
		asm("rlcf (_toRotate)&0ffh+0,F,B");
		asm("rlcf (_toRotate)&0ffh+1,F,B");
		asm("rlcf (_toRotate)&0ffh+2,F,B");
		asm("rlcf (_toRotate)&0ffh+3,F,B");
	}
	#else
	for(; i != 0u; i--)
	{
		_asm
		movlb toRotate
		bcf STATUS,0,0
		btfsc toRotate+3,7,1
		bsf STATUS,0,0
		rlcf toRotate+0,1,1
		rlcf toRotate+1,1,1
		rlcf toRotate+2,1,1
		rlcf toRotate+3,1,1
		_endasm
	}
	#endif
	
	return toRotate.Val;
}
#endif

/*****************************************************************************
  Function:
	void FormatNetBIOSName(BYTE Name[])

  Summary:
	Formats a string to a valid NetBIOS name.

  Description:
	This function formats a string to a valid NetBIOS name.  Names will be
	exactly 16 characters, as defined by the NetBIOS spec.  The 16th 
	character will be a 0x00 byte, while the other 15 will be the 
	provided string, padded with spaces as necessary.

  Precondition:
	None

  Parameters:
	Name - the string to format as a NetBIOS name.  This parameter must have
	  at least 16 bytes allocated.

  Returns:
	None
  ***************************************************************************/
void FormatNetBIOSName(BYTE Name[])
{
	BYTE i;

	Name[15] = '\0';
	strupr((char*)Name);
	i = 0;
	while(i < 15u)
	{
		if(Name[i] == '\0')
		{
			while(i < 15u)
			{
				Name[i++] = ' ';
			}
			break;
		}
		i++;
	}
}
