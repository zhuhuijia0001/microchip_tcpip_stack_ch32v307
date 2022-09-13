/*********************************************************************
 *
 *  SPI Flash Interface Headers
 *	- Tested to be compatible with SST25VF016B
 *	- Expected compatibility with all SST25VFxxxB devices
 *
 *********************************************************************
 * FileName:        SPIFlash.c
 * Dependencies:    None
 * Processor:       PIC18, PIC24F, PIC24H, dsPIC30F, dsPIC33F, PIC32MX
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
 * E. Wood				3/20/08	Original
********************************************************************/
#define __SPIFLASH_C

#include "TCPIP Stack/TCPIP.h"

#include "ch32v30x.h"

#if defined(MPFS_USE_SPI_FLASH) && (defined(STACK_USE_MPFS) || defined(STACK_USE_MPFS2))

// EEPROM SPI opcodes
#define COMMAND_WREN    0x06
#define COMMAND_WRDI    0x04
#define COMMAND_RDSR    0x05
#define COMMAND_WRSR    0x01
#define COMMAND_READ	0x03
#define COMMAND_FREAD   0x0B
#define COMMAND_WRITE	0x02
#define COMMAND_RDID    0x9F
#define COMMAND_SE      0x20
#define COMMAND_BE      0x52
#define COMMAND_CE      0x60

#define STATUS_WIP          0x01
#define STATUS_WEL          0x02
#define STATUS_BP0          0x04
#define STATUS_BP1          0x08
#define STATUS_BP2          0x10
#define STATUS_BP3          0x20
#define STATUS_RES          0x40
#define STATUS_SWRD         0x80

#define DUMMY               0xFF

#define SPI_FLASH_SECTOR_SIZE		(4096ul)
#define SPI_FLASH_SECTOR_MASK		(SPI_FLASH_SECTOR_SIZE - 1)

#define SPI_FLASH_PAGE_SIZE         (256ul)
#define SPI_FLASH_PAGE_SIZE_MASK    (SPI_FLASH_PAGE_SIZE - 1)

//cs pin
#define CS_PORT        GPIOA
#define CS_PIN         GPIO_Pin_4

#define SPI            SPI1

#define SET_CS         do { CS_PORT->BSHR = CS_PIN; } while (0)
#define CLR_CS         do { CS_PORT->BCR = CS_PIN; } while (0)


// Internal pointer to address being written
static DWORD dwWriteAddr;

static BOOL initialized = FALSE;

static void _SendCmd(BYTE cmd);
static void _WaitWhileBusy(void);
static void _EraseSector(DWORD dwAddr);

static BYTE SPIWriteReadData(BYTE dat)
{
    while ((SPI->STATR & SPI_I2S_FLAG_TXE) == RESET);
    SPI->DATAR = dat;
    while ((SPI->STATR & SPI_I2S_FLAG_RXNE) == RESET);

    return SPI->DATAR;
}

/*****************************************************************************
  Function:
	void SPIFlashInit(void)

  Description:
	Initializes SPI Flash module.

  Precondition:
	None

  Parameters:
	None

  Returns:
	None

  Remarks:
	This function is only called once during the lifetime of the application.

  Internal:
	This function sends WRDI to clear any pending write operation, and also
	clears the software write-protect on all memory locations.
  ***************************************************************************/
void SPIFlashInit(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    SPI_InitTypeDef  SPI_InitStructure;

    if (initialized)
        return;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_SPI1, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_SetBits(GPIOA, GPIO_Pin_4);

    SET_CS;

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI, &SPI_InitStructure);

    SPI_Cmd(SPI, ENABLE);

    initialized = TRUE;
}


/*****************************************************************************
  Function:
	void SPIFlashReadArray(DWORD dwAddress, BYTE *vData, WORD wLength)

  Description:
	Reads an array of bytes from the SPI Flash module.

  Precondition:
	SPIFlashInit has been called, and the chip is not busy (should be
	handled elsewhere automatically.)

  Parameters:
	dwAddress - Address from which to read
	vData - Where to store data that has been read
	wLength - Length of data to read

  Returns:
	None
  ***************************************************************************/
void SPIFlashReadArray(DWORD dwAddress, BYTE *vData, WORD wLength)
{
	// Ignore operations when the destination is NULL or nothing to read
	if(vData == NULL || wLength == 0)
		return;
	
	// Activate chip select
	CLR_CS;

	// Send READ opcode
	SPIWriteReadData(COMMAND_READ);
	
	// Send address
	SPIWriteReadData((dwAddress >> 16) & 0xff);
    SPIWriteReadData((dwAddress >> 8) & 0xff);
    SPIWriteReadData(dwAddress & 0xff);
	
	// Read data
	while(wLength--)
	{
		*vData++ = SPIWriteReadData(DUMMY);
	}
	
	// Deactivate chip select
	SET_CS;
}

/*****************************************************************************
  Function:
	void SPIFlashBeginWrite(DWORD dwAddr)

  Summary:
	Prepares the SPI Flash module for writing.

  Description:
	Prepares the SPI Flash module for writing.  Subsequent calls to 
	SPIFlashWrite or SPIFlashWriteArray will begin at this location and 
	continue sequentially.
	
	SPI Flash

  Precondition:
	SPIFlashInit has been called.

  Parameters:
	dwAddr - Address where the writing will begin

  Returns:
	None

  Remarks:
	Flash parts have large sector sizes, and can only erase entire sectors
	at once.  The SST parts for which this library was written have sectors
	that are 4kB in size.  Your application must ensure that writes begin on 
	a sector boundary so that the SPIFlashWrite functions will erase the 
	sector before attempting to write.  Entire sectors need not be written
	at once, so applications can begin writing to the front of a sector, 
	perform other tasks, then later call SPIFlashBeginWrite and point to an
	address in this sector that has not yet been programmed.  However, care
	must taken to ensure that writes are not attempted on addresses that are
	not in the erased state.  The chip will provide no indication that the
	write has failed, and will silently ignore the command.
  ***************************************************************************/
void SPIFlashBeginWrite(DWORD dwAddr)
{
	dwWriteAddr = dwAddr;
}

/*****************************************************************************
  Function:
	void SPIFlashWrite(BYTE vData)

  Summary:
	Writes a byte to the SPI Flash part.

  Description:
	This function writes a byte to the SPI Flash part.  If the current 
	address pointer indicates the beginning of a 4kB sector, the entire
	sector will first be erased to allow writes to proceed.  If the current
	address pointer indicates elsewhere, it will be assumed that the sector
	has already been erased.  If this is not true, the chip will silently
	ignore the write command.

  Precondition:
	SPIFlashInit and SPIFlashBeginWrite have been called, and the current
	address is either the front of a 4kB sector or has already been erased.

  Parameters:
	vData - The byte to write to the next memory location.

  Returns:
	None

  Remarks:
	See Remarks in SPIFlashBeginWrite for important information about Flash
	memory parts.
  ***************************************************************************/
void SPIFlashWrite(BYTE vData)
{
	// If address is a 4k boundary, erase a sector first
	if((dwWriteAddr & SPI_FLASH_SECTOR_MASK) == 0)
	    _EraseSector(dwWriteAddr);
	
	// Enable writing
	_SendCmd(COMMAND_WREN);
	
	// Activate the chip select
	CLR_CS;
	
	// Issue WRITE command with address
	SPIWriteReadData(COMMAND_WRITE);

	SPIWriteReadData((dwWriteAddr >> 16) & 0xff);
    SPIWriteReadData((dwWriteAddr >> 8) & 0xff);
    SPIWriteReadData(dwWriteAddr & 0xff);
	
	// Write the byte
	SPIWriteReadData(vData);

	dwWriteAddr++;
	
	// Deactivate chip select and wait for write to complete
	SET_CS;
	
	_WaitWhileBusy();
}

/*****************************************************************************
  Function:
	void SPIFlashWriteArray(BYTE* vData, WORD wLen)

  Summary:
	Writes an array of bytes to the SPI Flash part.

  Description:
	This function writes an array of bytes to the SPI Flash part.  When the
	address pointer crosses a 4kB sector boundary (and has more data to write),
	the next sector will automatically be erased.  If the current address 
	pointer indicates an address that is not a sector boundary and is not 
	already erased, the chip will silently ignore the write command until the
	next sector boundary is crossed.
	
  Precondition:
	SPIFlashInit and SPIFlashBeginWrite have been called, and the current
	address is either the front of a 4kB sector or has already been erased.

  Parameters:
	vData - The array to write to the next memory location
	wLen - The length of the data to be written

  Returns:
	None

  Remarks:
	See Remarks in SPIFlashBeginWrite for important information about Flash
	memory parts.
  ***************************************************************************/
void SPIFlashWriteArray(BYTE* vData, WORD wLen)
{
	DWORD BytesToWrite;

	while (wLen > 0)
	{
		// If address is a sector boundary
		if((dwWriteAddr & SPI_FLASH_SECTOR_MASK) == 0)
		    _EraseSector(dwWriteAddr);

		BytesToWrite = SPI_FLASH_PAGE_SIZE - (dwWriteAddr & SPI_FLASH_PAGE_SIZE_MASK);
		if (BytesToWrite > wLen)
		{
		    BytesToWrite = wLen;
		}

		// Enable writing
		_SendCmd(COMMAND_WREN);
	
		CLR_CS;
        SPIWriteReadData(COMMAND_WRITE);

        // Send address
        SPIWriteReadData((dwWriteAddr >> 16) & 0xff);
        SPIWriteReadData((dwWriteAddr >> 8) & 0xff);
        SPIWriteReadData(dwWriteAddr & 0xff);

		DWORD count = BytesToWrite;
        while (count--)
        {
            SPIWriteReadData(*vData++);
        }

		wLen -= BytesToWrite;

		dwWriteAddr += BytesToWrite;

		SET_CS;

		// Don't do anything until chip is ready
		_WaitWhileBusy();
	}
}


/*****************************************************************************
  Function:
	void _EraseSector(DWORD dwAddr)

  Summary:
	Erases a 4kB sector.

  Description:
	This function erases a 4kB sector in the Flash part.  It is called
	internally by the SPIFlashWrite functions whenever a write is attempted
	on the first byte in a sector.
	
  Precondition:
	SPIFlashInit has been called.

  Parameters:
	dwAddr - The address of the sector to be erased.

  Returns:
	None

  Remarks:
	See Remarks in SPIFlashBeginWrite for important information about Flash
	memory parts.
  ***************************************************************************/
static void _EraseSector(DWORD dwAddr)
{
	// Enable writing
	_SendCmd(COMMAND_WREN);
	
	// Activate the chip select
	CLR_CS;
	
	// Issue ERASE_4K command with address
	SPIWriteReadData(COMMAND_SE);

	SPIWriteReadData((dwWriteAddr >> 16) & 0xff);
    SPIWriteReadData((dwWriteAddr >> 8) & 0xff);
    SPIWriteReadData(dwWriteAddr & 0xff);
	
	// Deactivate chip select to perform the erase
	SET_CS;
	
	// Wait for erase to complete
	_WaitWhileBusy();
}

/*****************************************************************************
  Function:
	void _SendCmd(BYTE cmd)

  Summary:
	Sends a single-byte command to the SPI Flash part.

  Description:
	This function sends a single-byte command to the SPI Flash part.  It is
	used for commands such as WREN, WRDI, and EWSR that must have the chip
	select activated, then deactivated immediately after the command is 
	transmitted.
	
  Precondition:
	SPIFlashInit has been called.

  Parameters:
	cmd - The single-byte command code to send

  Returns:
	None
  ***************************************************************************/
static void _SendCmd(BYTE cmd)
{
	// Activate chip select
	CLR_CS;
	
	// Send Read Status Register instruction
	SPIWriteReadData(cmd);
	
	// Deactivate chip select
	SET_CS;
}


/*****************************************************************************
  Function:
	void _WaitWhileBusy(void)

  Summary:
	Waits for the SPI Flash part to indicate it is idle.

  Description:
	This function waits for the SPI Flash part to indicate it is idle.  It is
	used in the programming functions to wait for operations to complete.
	
  Precondition:
	SPIFlashInit has been called.

  Parameters:
	None

  Returns:
	None
  ***************************************************************************/
static void _WaitWhileBusy(void)
{
	BYTE_VAL result;

	// Activate chip select
    CLR_CS;

    // Send Read Status Register instruction
    SPIWriteReadData(COMMAND_RDSR);

	do 
	{
		// Poll the BUSY bit
		result.Val = SPIWriteReadData(DUMMY);
	} while (result.bits.b0);

	// Deactivate chip select
	SET_CS;
}


#endif //#if defined(SPIFLASH_CS_TRIS)

