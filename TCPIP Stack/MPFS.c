/*********************************************************************
 *
 *	Microchip File System (MPFS) File Access API
 *  Module for Microchip TCP/IP Stack
 *	 -Provides single API for accessing web pages and other files 
 *    from internal program memory or an external serial EEPROM memory
 *	 -Reference: AN833
 *
 *********************************************************************
 * FileName:        MPFS.c
 * Dependencies:    SPIEEPROM
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
 * Nilesh Rajbharti     8/14/01     Original (Rev. 1.0)
 * Nilesh Rajbharti     2/9/02      Cleanup
 * Nilesh Rajbharti     5/22/02     Rev 2.0 (See version.log for detail)
 * Howard Schlunder     3/31/05		Changed MPFS_ENTRY and mpfs_Flags for C30
********************************************************************/
#define __MPFS_C

#include "TCPIP Stack/TCPIP.h"

#if defined(STACK_USE_MPFS)

// This file system supports short file names i.e. 8 + 3.
#define MAX_FILE_NAME_LEN   (12u)

#define MPFS_DATA          (0x00u)
#define MPFS_DELETED       (0x01u)
#define MPFS_DLE           (0x03u)
#define MPFS_ETX           (0x04u)

/*
 * MPFS Structure:
 *
 * MPFS_Start:
 *      <MPFS_DATA><Address1><FileName1>
 *      <MPFS_DATA><Address2><FileName2>
 *      ...
 *      <MPFS_ETX><Addressn><FileNamen>
 * Address1:
 *      <Data1>[<Data2>...<Datan>]<MPFS_ETX><MPFS_INVALID>
 *      ...
 *
 * Note: If File data contains either MPFS_DLE or MPFS_ETX
 *       extra MPFS_DLE is stuffed before that byte.
 */
#if defined(MPFS_USE_EEPROM) || defined(MPFS_USE_SPI_FLASH)
	typedef struct  _MPFS_ENTRY
	{
	    BYTE Flag __attribute__((__packed__));
	    MPFS Address __attribute__((__packed__));
	    BYTE Name[MAX_FILE_NAME_LEN] __attribute__((__packed__));
	} MPFS_ENTRY;
#else	//Use program memory
	typedef struct  _MPFS_ENTRY
	{
	    BYTE Flag;
	    MPFS Address;
	    BYTE Name[MAX_FILE_NAME_LEN];
	} MPFS_ENTRY;
	#if defined(__C30__)
		static DWORD ReadProgramMemory(DWORD address);
	#endif
#endif

static union
{
    struct
    {
        unsigned char bNotAvailable : 1;
    } bits;
    BYTE Val;
} mpfsFlags;

BYTE mpfsOpenCount;

#if !defined(MPFS_USE_EEPROM) && !defined(MPFS_USE_SPI_FLASH)
	// An address where MPFS data starts in program memory.
    extern ROM MPFS_ENTRY MPFS_Start[];
#else
	#define MPFS_Start     MPFS_RESERVE_BLOCK
#endif

MPFS _currentHandle;
MPFS _currentFile;
WORD _currentCount;


/*********************************************************************
 * Function:        BOOL MPFSInit(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          TRUE, if MPFS Storage access is initialized and
 *                          MPFS is ready to be used.
 *                  FALSE otherwise
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 ********************************************************************/
BOOL MPFSInit(void)
{
    mpfsOpenCount = 0;
    mpfsFlags.Val = 0;

#if defined(MPFS_USE_EEPROM)
    // Initialize the EEPROM access routines.
    XEEInit();
#elif defined(MPFS_USE_SPI_FLASH)
	// Initialize the SPI Flash access routines.
	SPIFlashInit();
#endif

    return TRUE;
}


/*********************************************************************
 * Function:        MPFS MPFSOpen(BYTE* file)
 *
 * PreCondition:    None
 *
 * Input:           file        - NULL terminated file name.
 *
 * Output:          A handle if file is found
 *                  MPFS_INVALID if file is not found.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 ********************************************************************/
MPFS MPFSOpen(BYTE* file)
{
    MPFS_ENTRY entry;
    MPFS FAT;
    BYTE fileNameLen;

    if( mpfsFlags.bits.bNotAvailable )
        return MPFS_NOT_AVAILABLE;

#if defined(MPFS_USE_EEPROM) || defined(MPFS_USE_SPI_FLASH)
    FAT = MPFS_Start;
#else
    FAT = (MPFS)MPFS_Start;
#endif

    // If string is empty, do not attempt to find it in FAT.
    if ( *file == '\0' )
        return MPFS_INVALID;

    file = (BYTE*)strupr((char*)file);

    while(1)
    {
        // Bring current FAT entry into RAM.
		#if defined(MPFS_USE_EEPROM)
        	XEEReadArray(FAT, (unsigned char*)&entry, sizeof(entry));
        #elif defined(MPFS_USE_SPI_FLASH)
        	SPIFlashReadArray(FAT, (BYTE*)&entry, sizeof(entry));
		#else
			#if defined(__C30__)
	        	memcpypgm2ram(&entry, (ROM void*)(WORD)FAT, sizeof(entry));
			#else
	        	memcpypgm2ram(&entry, (ROM void*)FAT, sizeof(entry));
			#endif
		#endif

        // Make sure that it is a valid entry.
        if (entry.Flag == MPFS_DATA)
        {
            // Does the file name match ?
            fileNameLen = strlen((char*)file);
            if ( fileNameLen > MAX_FILE_NAME_LEN )
                fileNameLen = MAX_FILE_NAME_LEN;

            if( memcmp((void*)file, (void*)entry.Name, fileNameLen) == 0 )
            {
                _currentFile = (MPFS)entry.Address;
                mpfsOpenCount++;
                return entry.Address;
            }

            // File does not match.  Try next entry...
            FAT += sizeof(entry);
        }
        else if ( entry.Flag == MPFS_ETX )
        {
            if ( entry.Address != (MPFS)MPFS_INVALID )
                FAT = (MPFS)entry.Address;
            else
                break;
        }
	    else
	        return (MPFS)MPFS_INVALID;
    }
    return (MPFS)MPFS_INVALID;
}

MPFS MPFSOpenROM(ROM BYTE* file) 
{
	BYTE nameRAM[MAX_FILE_NAME_LEN+1];
	
	memcpypgm2ram(nameRAM, (ROM void*)file, strlenpgm((ROM char*)file));
	nameRAM[strlenpgm((ROM char*)file)] = '\0';
	
	return MPFSOpen(nameRAM);
}	

/*********************************************************************
 * Function:        void MPFSClose(void)
 *
 * PreCondition:    None
 *
 * Input:           handle      - File handle to be closed
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 ********************************************************************/
void MPFSClose(void)
{
    _currentCount = 0;
    mpfsFlags.bits.bNotAvailable = FALSE;
    if ( mpfsOpenCount )
        mpfsOpenCount--;
}


/*********************************************************************
 * Function:        BOOL MPFSGetBegin(MPFS hFile)
 *
 * PreCondition:    MPFSOpen() != MPFS_INVALID &&
 *
 * Input:           hFile      - handle of file that is to be read
 *
 * Output:          TRUE if successful
 *                  !TRUE otherwise
 *
 * Side Effects:    None
 *
 * Overview:        Prepares MPFS storage media for subsequent reads.
 *
 * Note:            None
 ********************************************************************/
#if defined(MPFS_USE_EEPROM)
BOOL MPFSGetBegin(MPFS hFile)
{
    _currentHandle = hFile;
    return (XEEBeginRead(hFile) == XEE_SUCCESS);
}
#endif

/*********************************************************************
 * Function:        BYTE MPFSGet(void)
 *
 * PreCondition:    MPFSOpen() != MPFS_INVALID &&
 *                  MPFSGetBegin() == TRUE
 *
 * Input:           None
 *
 * Output:          Data byte from current address.
 *
 * Side Effects:    None
 *
 * Overview:        Reads a byte from current address.
 *
 * Note:            Caller must call MPFSIsEOF() to check for end of
 *                  file condition
 ********************************************************************/
BYTE MPFSGet(void)
{
    BYTE t;
    
	#if defined(MPFS_USE_EEPROM)
		t = XEERead();
		_currentHandle++;
	#elif defined(MPFS_USE_SPI_FLASH)
		SPIFlashReadArray(_currentHandle++, (BYTE*)&t, 1);
	#else
		#if defined(__C30__)
			{
				DWORD_VAL i;
	
				// The uppermost byte, ((DWORD_VAL*)&_currentHandle)->v[3]), is the byte lane to read from.
				// 16 bit PICs have a 24 bit wide Flash program word.  Bytes 0-2 are the actual address, but 
				// odd addresses aren't implemented.
				i.Val = ReadProgramMemory(_currentHandle & 0x00FFFFFF);
				t = i.v[((DWORD_VAL*)&_currentHandle)->v[3]++];
				if(((DWORD_VAL*)&_currentHandle)->v[3] >= 3)
				{
					_currentHandle = (_currentHandle + 2) & 0x00FFFFFF;
				}
			}
		#else
	    	t = (BYTE)*_currentHandle;
		    _currentHandle++;
		#endif
	#endif

    if(t == MPFS_DLE)
    {
		#if defined(MPFS_USE_EEPROM)
		    t = XEERead();
		    _currentHandle++;
		#elif defined(MPFS_USE_SPI_FLASH)
			SPIFlashReadArray(_currentHandle++, (BYTE*)&t, 1);
		#else
			#if defined(__C30__)
				{
					DWORD_VAL i;
		
				// The uppermost byte, ((DWORD_VAL*)&_currentHandle)->v[3]), is the byte lane to read from.
				// 16 bit PICs have a 24 bit wide Flash program word.  Bytes 0-2 are the actual address, but 
				// odd addresses aren't implemented.
				i.Val = ReadProgramMemory(_currentHandle & 0x00FFFFFF);
				t = i.v[((DWORD_VAL*)&_currentHandle)->v[3]++];
				if(((DWORD_VAL*)&_currentHandle)->v[3] >= 3)
				{
					_currentHandle = (_currentHandle + 2) & 0x00FFFFFF;
				}
				}
			#else
		    	t = (BYTE)*_currentHandle;
			    _currentHandle++;
			#endif
		#endif
    }
    else if(t == MPFS_ETX)
    {
        _currentHandle = MPFS_INVALID;
    }

    return t;
}


#if defined(__C30__) && !defined(MPFS_USE_EEPROM)
/*********************************************************************
 * Function:        static DWORD ReadProgramMemory(DWORD address)
 *
 * PreCondition:    None
 *
 * Input:           Program memory address to read from.  Should be 
 *					an even number.
 *
 * Output:          Program word at the specified address.  For the 
 *					PIC24, dsPIC, etc. which have a 24 bit program 
 *					word size, the upper byte is 0x00.
 *
 * Side Effects:    None
 *
 * Overview:        Modifies and restores TBLPAG.  Make sure that if 
 *					using interrupts and the PSV feature of the CPU 
 *					in an ISR that the TBLPAG register is preloaded 
 *					with the correct value (rather than assuming 
 *					TBLPAG is always pointing to the .const section.
 *
 * Note:            None
 ********************************************************************/
static DWORD ReadProgramMemory(DWORD address) 
{  
	DWORD dwResult;
	WORD wTBLPAGSave;

	wTBLPAGSave = TBLPAG;
	TBLPAG = ((WORD*)&address)[1];
	((WORD*)&dwResult)[1] = __builtin_tblrdh((WORD)address);
	((WORD*)&dwResult)[0] = __builtin_tblrdl((WORD)address);
	TBLPAG = wTBLPAGSave;

	return dwResult;
}
#endif


/*********************************************************************
 * Function:        MPFS MPFSGetEnd(void)
 *
 * PreCondition:    MPFSOpen() != MPFS_INVALID &&
 *                  MPFSGetBegin() = TRUE
 *
 * Input:           None
 *
 * Output:          Current mpfs handle.
 *
 * Side Effects:    None
 *
 * Overview:        Ends on-going read cycle.
 *                  MPFS handle that is returned must be used
 *                  for subsequent begin gets..
 *
 * Note:            None
 ********************************************************************/
#if defined(MPFS_USE_EEPROM)
MPFS MPFSGetEnd(void)
{
    XEEEndRead();
    return _currentHandle;
}
#endif


/*********************************************************************
 * Function:        MPFS MPFSFormat(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          A valid MPFS handle that can be used for MPFSPut
 *
 * Side Effects:    None
 *
 * Overview:        Prepares MPFS image to get re-written
 *                  Declares MPFS as in use.
 *
 * Note:            MPFS will be unaccessible until MPFSClose is
 *                  called.
 ********************************************************************/
MPFS MPFSFormat(void)
{
    mpfsFlags.bits.bNotAvailable = TRUE;
	#if defined(MPFS_USE_SPI_FLASH)
		SPIFlashBeginWrite(MPFS_RESERVE_BLOCK);
	#endif
    return (MPFS)MPFS_RESERVE_BLOCK;
}


/*********************************************************************
 * Function:        BOOL MPFSPutBegin(MPFS handle)
 *
 * PreCondition:    MPFSInit() and MPFSFormat() are already called.
 *
 * Input:           handle  - handle to where put to begin
 *
 * Output:          TRUE if successful
 *                  !TRUE otherwise
 *
 * Side Effects:    None
 *
 * Overview:        Prepares MPFS image to get re-written
 *
 * Note:            MPFS will be unaccessible until MPFSClose is
 *                  called.
 ********************************************************************/
#if defined(MPFS_USE_EEPROM)
BOOL MPFSPutBegin(MPFS handle)
{
    //_currentCount = 0;
    _currentHandle = handle;
    _currentCount = (BYTE)handle;
    _currentCount &= (MPFS_WRITE_PAGE_SIZE-1);
    return (XEEBeginWrite(handle) == XEE_SUCCESS);
}
#endif


/*********************************************************************
 * Function:        BOOL MPFSPut(BYTE b)
 *
 * PreCondition:    MPFSFormat() or MPFSCreate() must be called
 *                  MPFSPutBegin() is already called.
 *
 * Input:           b       - data to write.
 *
 * Output:          TRUE if successfull
 *                  !TRUE if failed.
 *
 * Side Effects:    Original MPFS handle is no longer valid.
 *                  Updated MPFS handle must be obtained by calling
 *                  MPFSPutEnd().
 *
 * Overview:        None
 *
 * Note:            Actual write may not get started until internal
 *                  write page is full.  To ensure that previously
 *                  data gets written, caller must call MPFSPutEnd()
 *                  after last call to MPFSPut().
 ********************************************************************/
BOOL MPFSPut(BYTE b)
{
#if defined(MPFS_USE_EEPROM)
    if ( XEEWrite(b) )
        return FALSE;

    _currentCount++;
    _currentHandle++;
    if ( _currentCount >= MPFS_WRITE_PAGE_SIZE )
    {
        MPFSPutEnd();
        XEEBeginWrite(_currentHandle);
    }
#elif defined(MPFS_USE_SPI_FLASH)
	SPIFlashWrite(b);
#endif
    return TRUE;
}

/*********************************************************************
 * Function:        MPFS MPFSPutEnd(void)
 *
 * PreCondition:    MPFSPutBegin() is already called.
 *
 * Input:           None
 *
 * Output:          Up-to-date MPFS handle
 *
 * Side Effects:    Original MPFS handle is no longer valid.
 *                  Updated MPFS handle must be obtained by calling
 *                  MPFSPutEnd().
 *
 * Overview:        None
 *
 * Note:            Actual write may not get started until internal
 *                  write page is full.  To ensure that previously
 *                  data gets written, caller must call MPFSPutEnd()
 *                  after last call to MPFSPut().
 ********************************************************************/
MPFS MPFSPutEnd(void)
{
#if defined(MPFS_USE_EEPROM)
    _currentCount = 0;
    XEEEndWrite();
    while(XEEIsBusy());
#endif

    return _currentHandle;
}

/*********************************************************************
 * Function:        MPFS MPFSSeek(MPFS offset)
 *
 * PreCondition:    MPFSGetBegin() is already called.
 *
 * Input:           offset      - Offset from current pointer
 *
 * Output:          New MPFS handle located to given offset
 *
 * Side Effects:    None.
 *
 * Overview:        None
 *
 * Note:            None.
 ********************************************************************/
MPFS MPFSSeek(MPFS offset)
{
    MPFS i;

    MPFSGetBegin(_currentFile);

    i = (MPFS)0;
    while(i++ != offset)
        MPFSGet();

    MPFSGetEnd();

    return _currentHandle;
}


/*********************************************************************
 * Function:        BOOL MPFSGetLong(DWORD *ul)
 *
 * PreCondition:    MPFSOpen() and MPFSBeginGet()
 *
 * Input:           ul: pointer to an DWORD to read
 *
 * Output:          TRUE on success
 *					FALSE on EOF
 * 
 * Side Effects:    None
 *
 * Overview:        Reads an DWORD value from an MPFS file
 *
 * Note:            None
 ********************************************************************/
BOOL MPFSGetLong(DWORD *ul)
{
	BYTE* b = (BYTE*)ul;
	*(b) = MPFSGet();	
	if(MPFSIsEOF())
		return FALSE;
	*(b+1) = MPFSGet();	
	*(b+2) = MPFSGet();	
	*(b+3) = MPFSGet();	

	return TRUE;
}

#endif //#if defined(STACK_USE_MPFS)
