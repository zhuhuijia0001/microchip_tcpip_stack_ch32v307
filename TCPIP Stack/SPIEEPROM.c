/*********************************************************************
 *
 *               Data SPI EEPROM Access Routines
 *
 *********************************************************************
 * FileName:        SPIEEPROM.c
 * Dependencies:    None
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
 * Nilesh Rajbharti     5/20/02     Original (Rev. 1.0)
 * Howard Schlunder		9/01/04		Rewritten for SPI EEPROMs
 * Howard Schlunder		8/10/06		Modified to control SPI module 
 *									frequency whenever EEPROM accessed 
 *									to allow bus sharing with different 
 *									frequencies.
********************************************************************/
#define __SPIEEPROM_C

#include "TCPIP Stack/TCPIP.h"

#include "ch32v30x.h"

#if defined(MPFS_USE_EEPROM) && (defined(STACK_USE_MPFS) || defined(STACK_USE_MPFS2))

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

static void DoWrite(void);

#define EEPROM_SECTOR_SIZE              (4096)
#define EEPROM_SECTOR_SIZE_MASK         (EEPROM_SECTOR_SIZE - 1)
#define EEPROM_PAGE_SIZE                (256)
#define EEPROM_PAGE_SIZE_MASK           (EEPROM_PAGE_SIZE - 1)

static DWORD EEPROMWriteSectorAddress;
static DWORD EEPROMWritePageAddress;

static BYTE EEPROMWriteBuffer[EEPROM_SECTOR_SIZE];
static BYTE *EEPROMWriteBufferPtr;

#define EEPROM_READ_BUFFER_SIZE     (64)

static DWORD EEPROMReadAddress;
static BYTE EEPROMReadBuffer[EEPROM_READ_BUFFER_SIZE];
static BYTE *EEPROMReadBufferPtr;

//cs pin
#define CS_PORT        GPIOA
#define CS_PIN         GPIO_Pin_4

#define SPI            SPI1

#define SET_CS         do { CS_PORT->BSHR = CS_PIN; } while (0)
#define CLR_CS         do { CS_PORT->BCR = CS_PIN; } while (0)

static BYTE SPIWriteReadData(BYTE dat)
{
    while ((SPI->STATR & SPI_I2S_FLAG_TXE) == RESET);
    SPI->DATAR = dat;
    while ((SPI->STATR & SPI_I2S_FLAG_RXNE) == RESET);

    return SPI->DATAR;
}

/*********************************************************************
 * Function:        void XEEInit(unsigned char speed)
 *
 * PreCondition:    None
 *
 * Input:           speed - not used (included for compatibility only)
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        Initialize SPI module to communicate to serial
 *                  EEPROM.
 *
 * Note:            Code sets SPI clock to Fosc/16.  
 ********************************************************************/

void XEEInit(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    SPI_InitTypeDef  SPI_InitStructure;

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
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI, &SPI_InitStructure);

    SPI_Cmd(SPI, ENABLE);
}


/*********************************************************************
 * Function:        XEE_RESULT XEEBeginRead(DWORD address)
 *
 * PreCondition:    XEEInit() is already called.
 *
 * Input:           address - Address at which read is to be performed.
 *
 * Output:          XEE_SUCCESS if successful
 *                  other value if failed.
 *
 * Side Effects:    None
 *
 * Overview:        Sets internal address counter to given address.
 *                  Puts EEPROM in sequential read mode.
 *
 * Note:            This function does not release I2C bus.
 *                  User must call XEEEndRead() when read is not longer
 *                  needed; I2C bus will released after XEEEndRead()
 *                  is called.
 ********************************************************************/
XEE_RESULT XEEBeginRead(DWORD address)
{
	// Save the address and emptry the contents of our local buffer
	EEPROMReadAddress = address;
	EEPROMReadBufferPtr = EEPROMReadBuffer + EEPROM_READ_BUFFER_SIZE;
	return XEE_SUCCESS;
}


/*********************************************************************
 * Function:        BYTE XEERead(void)
 *
 * PreCondition:    XEEInit() && XEEBeginRead() are already called.
 *
 * Input:           None
 *
 * Output:          BYTE that was read
 *
 * Side Effects:    None
 *
 * Overview:        Reads next byte from EEPROM; internal address
 *                  is incremented by one.
 *
 * Note:            None
 ********************************************************************/
BYTE XEERead(void)
{
    // Check if no more bytes are left in our local buffer
    if( EEPROMReadBufferPtr == EEPROMReadBuffer + EEPROM_READ_BUFFER_SIZE )
    {
        // Get a new set of bytes
        XEEReadArray(EEPROMReadAddress, EEPROMReadBuffer, EEPROM_READ_BUFFER_SIZE);
        EEPROMReadAddress += EEPROM_READ_BUFFER_SIZE;
        EEPROMReadBufferPtr = EEPROMReadBuffer;
    }

    // Return a byte from our local buffer
    return *EEPROMReadBufferPtr++;
}

/*********************************************************************
 * Function:        XEE_RESULT XEEEndRead(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          XEE_SUCCESS
 *
 * Side Effects:    None
 *
 * Overview:        This function does nothing.
 *
 * Note:            Function is used for backwards compatability with 
 *					I2C EEPROM module.
 ********************************************************************/
XEE_RESULT XEEEndRead(void)
{
    return XEE_SUCCESS;
}


/*********************************************************************
 * Function:        XEE_RESULT XEEReadArray(DWORD address,
 *                                          BYTE *buffer,
 *                                          BYTE length)
 *
 * PreCondition:    XEEInit() is already called.
 *
 * Input:           address     - Address from where array is to be read
 *                  buffer      - Caller supplied buffer to hold the data
 *                  length      - Number of bytes to read.
 *
 * Output:          XEE_SUCCESS
 *
 * Side Effects:    None
 *
 * Overview:        Reads desired number of bytes in sequential mode.
 *                  This function performs all necessary steps
 *                  and releases the bus when finished.
 *
 * Note:            None
 ********************************************************************/
XEE_RESULT XEEReadArray(DWORD address,
                        BYTE *buffer,
                        BYTE length)
{
    CLR_CS;

    // Send COMMAND_READ opcode
    SPIWriteReadData(COMMAND_READ);

    // Send address
    SPIWriteReadData((address >> 16) & 0xff);
    SPIWriteReadData((address >> 8) & 0xff);
    SPIWriteReadData(address & 0xff);

    while(length--)
    {
        BYTE t = SPIWriteReadData(DUMMY);
        if(buffer != NULL)
            *buffer++ = t;
    };

    SET_CS;

	return XEE_SUCCESS;
}

/*********************************************************************
 * Function:        XEE_RESULT XEESetAddr(DWORD address)
 *
 * PreCondition:    XEEInit() is already called.
 *
 * Input:           address     - address to be set for writing
 *
 * Output:          XEE_SUCCESS
 *
 * Side Effects:    None
 *
 * Overview:        Modifies internal address counter of EEPROM.
 *
 * Note:            Unlike XEESetAddr() in xeeprom.c for I2C EEPROM 
 *					memories, this function is used only for writing
 *					to the EEPROM.  Reads must use XEEBeginRead(), 
 *					XEERead(), and XEEEndRead().
 *					This function does not release the SPI bus.
 ********************************************************************/
XEE_RESULT XEEBeginWrite(DWORD address)
{
	EEPROMWriteSectorAddress = address - address & EEPROM_SECTOR_SIZE_MASK;
	EEPROMWritePageAddress = address;

	EEPROMWriteBufferPtr = EEPROMWriteBuffer + (address & EEPROM_SECTOR_SIZE_MASK);

	return XEE_SUCCESS;
}


/*********************************************************************
 * Function:        XEE_RESULT XEEWrite(BYTE val)
 *
 * PreCondition:    XEEInit() && XEEBeginWrite() are already called.
 *
 * Input:           val - Byte to be written
 *
 * Output:          XEE_SUCCESS
 *
 * Side Effects:    None
 *
 * Overview:        Adds a byte to the current page to be writen when
 *					XEEEndWrite() is called.
 *
 * Note:            Page boundary cannot be exceeded or the byte 
 *					to be written will be looped back to the 
 *					beginning of the page.
 ********************************************************************/
XEE_RESULT XEEWrite(BYTE val)
{
	*EEPROMWriteBufferPtr++ = val;
	if(EEPROMWriteBufferPtr == EEPROMWriteBuffer + EEPROM_SECTOR_SIZE)
    {
		DoWrite();
    }

    return XEE_SUCCESS;
}


/*********************************************************************
 * Function:        XEE_RESULT XEEEndWrite(void)
 *
 * PreCondition:    XEEInit() && XEEBeginWrite() are already called.
 *
 * Input:           None
 *
 * Output:          XEE_SUCCESS
 *
 * Side Effects:    None
 *
 * Overview:        Instructs EEPROM to begin write cycle.
 *
 * Note:            Call this function after either page full of bytes
 *                  written or no more bytes are left to load.
 *                  This function initiates the write cycle.
 *                  User must call for XEEIsBusy() to ensure that write
 *                  cycle is finished before calling any other
 *                  routine.
 ********************************************************************/
XEE_RESULT XEEEndWrite(void)
{
	if( EEPROMWriteBufferPtr != EEPROMWriteBuffer )
    {
		DoWrite();
    }

    return XEE_SUCCESS;
}

static void DoWrite(void)
{
    DWORD BytesToWrite;

    CLR_CS;
    // Set the Write Enable latch
    SPIWriteReadData(COMMAND_WREN);
    SET_CS;

    CLR_CS;
    //erase sector
    SPIWriteReadData(COMMAND_SE);

    SPIWriteReadData((EEPROMWriteSectorAddress >> 16) & 0xff);
    SPIWriteReadData((EEPROMWriteSectorAddress >> 8) & 0xff);
    SPIWriteReadData(EEPROMWriteSectorAddress & 0xff);
    SET_CS;

    while( XEEIsBusy() );

    DWORD uplimit = EEPROMWriteSectorAddress + (EEPROMWriteBufferPtr - EEPROMWriteBuffer);

    EEPROMWriteBufferPtr = EEPROMWriteBuffer + (EEPROMWritePageAddress & EEPROM_SECTOR_SIZE_MASK);

    if ((EEPROMWritePageAddress & EEPROM_SECTOR_SIZE_MASK) > 0)
    {
        BytesToWrite = EEPROM_PAGE_SIZE - (EEPROMWritePageAddress & EEPROM_PAGE_SIZE_MASK);

        CLR_CS;
        // Set the Write Enable latch
        SPIWriteReadData(COMMAND_WREN);
        SET_CS;

        CLR_CS;
        SPIWriteReadData(COMMAND_WRITE);

        // Send address
        SPIWriteReadData((EEPROMWritePageAddress >> 16) & 0xff);
        SPIWriteReadData((EEPROMWritePageAddress >> 8) & 0xff);
        SPIWriteReadData(EEPROMWritePageAddress & 0xff);

        while (BytesToWrite--)
        {
            SPIWriteReadData(*EEPROMWriteBufferPtr++);
        }

        SET_CS;

        while( XEEIsBusy() );

        EEPROMWritePageAddress = EEPROMWritePageAddress + EEPROM_PAGE_SIZE - (EEPROMWritePageAddress & EEPROM_PAGE_SIZE_MASK);
    }

    while (EEPROMWritePageAddress < uplimit)
    {
        CLR_CS;
        // Set the Write Enable latch
        SPIWriteReadData(COMMAND_WREN);
        SET_CS;

        // Send COMMAND_WRITE opcode
        CLR_CS;
        SPIWriteReadData(COMMAND_WRITE);

        // Send address
        SPIWriteReadData((EEPROMWritePageAddress >> 16) & 0xff);
        SPIWriteReadData((EEPROMWritePageAddress >> 8) & 0xff);
        SPIWriteReadData(EEPROMWritePageAddress & 0xff);

        BytesToWrite = EEPROM_PAGE_SIZE;

        while (BytesToWrite--)
        {
            SPIWriteReadData(*EEPROMWriteBufferPtr++);
        }

        SET_CS;

        while( XEEIsBusy() );

        EEPROMWritePageAddress += EEPROM_PAGE_SIZE;
    }

    EEPROMWriteSectorAddress += EEPROM_SECTOR_SIZE;
    EEPROMWriteBufferPtr = EEPROMWriteBuffer;
}

/*********************************************************************
 * Function:        BOOL XEEIsBusy(void)
 *
 * PreCondition:    XEEInit() is already called.
 *
 * Input:           None
 *
 * Output:          FALSE if EEPROM is not busy
 *                  TRUE if EEPROM is busy
 *
 * Side Effects:    None
 *
 * Overview:        Reads the status register
 *
 * Note:            None
 ********************************************************************/
BOOL XEEIsBusy(void)
{
    BYTE_VAL result;

    CLR_CS;

    // Send COMMAND_RDSR - Read Status Register opcode
    SPIWriteReadData(COMMAND_RDSR);

    // Get register contents
    result.Val = SPIWriteReadData(0xFF);

    SET_CS;

    return result.bits.b0;
}


#endif //#if defined(MPFS_USE_EEPROM) && defined(EEPROM_CS_TRIS) && defined(STACK_USE_MPFS)
