/*********************************************************************
 *
 *               Microchip File System
 *
 *********************************************************************
 * FileName:        MPFS.h
 * Dependencies:    StackTsk.H
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
 * Howard Schlunder		5/31/06		Changed EEPROM addressing to 32 bits
********************************************************************/
#ifndef __MPFS_H
#define __MPFS_H

#if defined(MPFS_USE_EEPROM) || defined(MPFS_USE_SPI_FLASH)
    typedef DWORD MPFS;									// For default 32 bit addressing (use /ll option with MPFS.exe)
    #define MPFS_INVALID				(0xffffffffu)	// For default 32 bit addressing (use /ll option with MPFS.exe)
//    typedef WORD MPFS;								// For old short 16 bit addressing (use /b option with MPFS.exe)
//    #define MPFS_INVALID				(0xffff)		// For old short 16 bit addressing (use /b option with MPFS.exe)
#else
	#if defined(__C30__)
		typedef DWORD MPFS;
	    #define MPFS_INVALID                (MPFS)(0xffffffffu)
	#else
		typedef ROM BYTE* MPFS;
	    #define MPFS_INVALID                (MPFS)(0xffffffu)
	#endif
#endif

#define MPFS_NOT_AVAILABLE              (0x0u)

#if defined(MPFS_USE_EEPROM)
	#if defined(USE_EEPROM_25LC1024)
		#define MPFS_WRITE_PAGE_SIZE		(256u)
	#else
		#define MPFS_WRITE_PAGE_SIZE		(64u)
	#endif
#endif



BOOL MPFSInit(void);
MPFS MPFSOpen(BYTE* name);
MPFS MPFSOpenROM(ROM BYTE* name);
BYTE MPFSGet(void);
void MPFSClose(void);
MPFS MPFSFormat(void);
BOOL MPFSPut(BYTE b);
MPFS MPFSPutEnd(void);
MPFS MPFSSeek(MPFS offset);
BOOL MPFSGetLong(DWORD *ul);

#if !defined(__MPFS_C)
	extern BYTE mpfsOpenCount;
    extern MPFS _currentHandle;
    extern BYTE _currentCount;
#endif


/*********************************************************************
 * Function:        BOOL MPFSGetBegin(MPFS handle)
 *
 * PreCondition:    MPFSOpen() != MPFS_INVALID &&
 *
 * Input:           handle      - handle of file that is to be read
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
    BOOL MPFSGetBegin(MPFS handle);
#else
    #define MPFSGetBegin(handle)    (_currentHandle = handle)
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
    MPFS MPFSGetEnd(void);
#else
    #define MPFSGetEnd()        _currentHandle
#endif


/*********************************************************************
 * Macro:           BOOL MPFSIsEOF(void)
 *
 * PreCondition:    MPFSGetBegin() must be called.
 *
 * Input:           None
 *
 * Output:          TRUE if current file read has reached end of file.
 *                  FALSE if otherwise.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 ********************************************************************/
#define MPFSIsEOF()     (_currentHandle == MPFS_INVALID)


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
    BOOL MPFSPutBegin(MPFS handle);
#else
    #define MPFSPutBegin(handle)        (_currentHandle = handle)
#endif

/*********************************************************************
 * Function:        MPFS MPFSTell(void)
 *
 * PreCondition:    MPFSOpen() is already called.
 *
 * Input:           None
 *
 * Output:          current MPFS file pointer
 *
 * Side Effects:    None.
 *
 * Overview:        None
 *
 * Note:            None.
 ********************************************************************/
#define MPFSTell()      (_currentHandle)


/*********************************************************************
 * Macro:           BYTE MPFSInUse(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          No. of file currently open.
 *                  If == 0, MPFS is not in use.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 ********************************************************************/
#define MPFSIsInUse()       (mpfsOpenCount)

#endif
