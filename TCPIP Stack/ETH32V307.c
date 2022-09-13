
#include "TCPIP Stack/TCPIP.h"

#include "ch32v30x.h"

#include "trace.h"

#define PHY_ADDRESS 0x01

#define ETH_TIMEOUT_SWRESET                (10U)

#define ETH_RXBUFNB        5
#define ETH_TXBUFNB        4

__attribute__ ((aligned(4))) ETH_DMADESCTypeDef DMARxDscrTab[ETH_RXBUFNB];/* rx dscriptor */
__attribute__ ((aligned(4))) ETH_DMADESCTypeDef DMATxDscrTab[ETH_TXBUFNB];/* tx descriptor */

__attribute__ ((aligned(4))) static uint8_t Rx_Buff[ETH_RXBUFNB][ETH_MAX_PACKET_SIZE];/* rx fifo */
__attribute__ ((aligned(4))) static uint8_t Tx_Buff[ETH_TXBUFNB][ETH_MAX_PACKET_SIZE];/* tx fifo */

extern ETH_DMADESCTypeDef  *DMATxDescToSet;
extern ETH_DMADESCTypeDef  *DMARxDescToGet;

static BOOL WasDiscarded;

static WORD rxPtr;

static WORD txPtr;
static WORD txCount;

static BOOL rxPtrToRxBuffer;

static BOOL dataTransceiving;

#define  ETH_DMARxDesc_FrameLengthShift           16

#define ETHER_IP	(0x00u)
#define ETHER_ARP	(0x06u)

void MACInit(void)
{
    UINT32 timeout;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_ETH_MAC | RCC_AHBPeriph_ETH_MAC_Tx | RCC_AHBPeriph_ETH_MAC_Rx, ENABLE);

    /* Enable internal 10BASE-T PHY*/
    EXTEN->EXTEN_CTR |= EXTEN_ETH_10M_EN;

    /* Reset ETHERNET on AHB Bus */
    ETH_DeInit();

    /* Software reset */
    ETH_SoftwareReset();

    WasDiscarded = TRUE;

    rxPtr = 0;

    txPtr = 0;
    txCount = 0;

    rxPtrToRxBuffer = TRUE;

    dataTransceiving = FALSE;

    timeout = ETH_TIMEOUT_SWRESET;

    /* Wait for software reset */
    while ((ETH->DMABMR & ETH_DMABMR_SR) != (uint32_t)RESET)
    {
        /* Check for the Timeout */
        if (--timeout == 0)
        {
            TRACE("Error:Eth soft-reset timeout!\nPlease check RGMII TX & RX clock line.\n");

            return;
        }

        DelayMs(10);
    }

    ETH_InitTypeDef ETH_InitStructure;

    /* ETHERNET Configuration ------------------------------------------------------*/
    /* Call ETH_StructInit if you don't like to configure all ETH_InitStructure parameter */
    ETH_StructInit(&ETH_InitStructure);
    /* Fill ETH_InitStructure parametrs */
    /*------------------------   MAC   -----------------------------------*/
    ETH_InitStructure.ETH_Mode = ETH_Mode_FullDuplex;
    ETH_InitStructure.ETH_Speed = ETH_Speed_10M;
    ETH_InitStructure.ETH_AutoNegotiation = ETH_AutoNegotiation_Enable  ;
    ETH_InitStructure.ETH_LoopbackMode = ETH_LoopbackMode_Disable;
    ETH_InitStructure.ETH_RetryTransmission = ETH_RetryTransmission_Disable;
    ETH_InitStructure.ETH_AutomaticPadCRCStrip = ETH_AutomaticPadCRCStrip_Disable;
    ETH_InitStructure.ETH_ReceiveAll = ETH_ReceiveAll_Disable;
    ETH_InitStructure.ETH_SourceAddrFilter = ETH_SourceAddrFilter_Disable;
    ETH_InitStructure.ETH_DestinationAddrFilter = ETH_DestinationAddrFilter_Normal;
    ETH_InitStructure.ETH_BroadcastFramesReception = ETH_BroadcastFramesReception_Enable;
    ETH_InitStructure.ETH_PassControlFrames = ETH_PassControlFrames_BlockAll;
    ETH_InitStructure.ETH_PromiscuousMode = ETH_PromiscuousMode_Disable;
    ETH_InitStructure.ETH_MulticastFramesFilter = ETH_MulticastFramesFilter_Perfect;
    ETH_InitStructure.ETH_UnicastFramesFilter = ETH_UnicastFramesFilter_Perfect;
    ETH_InitStructure.ETH_ChecksumOffload = ETH_ChecksumOffload_Disable;
    /*------------------------   DMA   -----------------------------------*/
    /* When we use the Checksum offload feature, we need to enable the Store and Forward mode:
    the store and forward guarantee that a whole frame is stored in the FIFO, so the MAC can insert/verify the checksum,
    if the checksum is OK the DMA can handle the frame otherwise the frame is dropped */
    ETH_InitStructure.ETH_DropTCPIPChecksumErrorFrame = ETH_DropTCPIPChecksumErrorFrame_Enable;
    ETH_InitStructure.ETH_ReceiveStoreForward = ETH_ReceiveStoreForward_Enable;
    ETH_InitStructure.ETH_TransmitStoreForward = ETH_TransmitStoreForward_Enable;
    ETH_InitStructure.ETH_ForwardErrorFrames = ETH_ForwardErrorFrames_Enable;
    ETH_InitStructure.ETH_ForwardUndersizedGoodFrames = ETH_ForwardUndersizedGoodFrames_Enable;
    ETH_InitStructure.ETH_SecondFrameOperate = ETH_SecondFrameOperate_Disable;
    ETH_InitStructure.ETH_AddressAlignedBeats = ETH_AddressAlignedBeats_Enable;
    ETH_InitStructure.ETH_FixedBurst = ETH_FixedBurst_Enable;
    ETH_InitStructure.ETH_RxDMABurstLength = ETH_RxDMABurstLength_32Beat;
    ETH_InitStructure.ETH_TxDMABurstLength = ETH_TxDMABurstLength_32Beat;
    ETH_InitStructure.ETH_DMAArbitration = ETH_DMAArbitration_RoundRobin_RxTx_2_1;

    /* Configure Ethernet */
    UINT32 tmpreg = 0;
    UINT16 RegValue = 0;

    /*---------------------- 物理层配置 -------------------*/
    /* 置SMI接口时钟 ，置为主频的42分频  */
    tmpreg = ETH->MACMIIAR;
    tmpreg &= MACMIIAR_CR_MASK;
    tmpreg |= (UINT32)ETH_MACMIIAR_CR_Div42;
    ETH->MACMIIAR = (UINT32)tmpreg;

    /* 复位物理层 */
    ETH_WritePHYRegister(PHY_ADDRESS, PHY_BCR, PHY_Reset);/* 复位物理层  */

    timeout = 1000;

    do
    {
        /* Check for the Timeout */
        if(--timeout == 0)
        {
            TRACE("Error:Wait phy software timeout!\nPlease check PHY/MID.\nProgram has been blocked!\n");

            return;
        }

        DelayMs(1);

        RegValue = ETH_ReadPHYRegister(PHY_ADDRESS, PHY_BCR);
    } while (RegValue & PHY_Reset);

#if 0
    /* 等待物理层与对端建立LINK */
    timeout = 10000;

    do
    {
        /* Check for the Timeout */
        if(--timeout == 0)
        {
            TRACE("Error:Wait phy linking timeout!\nPlease check MID.\nProgram has been blocked!\n");

            return;
        }

        DelayMs(1);

        RegValue = ETH_ReadPHYRegister(PHY_ADDRESS, PHY_BSR);

    } while ((RegValue & PHY_Linked_Status) != PHY_Linked_Status);

    /* 等待物理层完成自动协商 */
    timeout = 10000;

    do
    {
        /* Check for the Timeout */
        if(--timeout == 0)
        {
            TRACE("Error:Wait phy auto-negotiation complete timeout!\nPlease check MID.\nProgram has been blocked!\n");

            return;
        }

        DelayMs(1);

        RegValue = ETH_ReadPHYRegister(PHY_ADDRESS, PHY_BSR);
    } while ((RegValue & PHY_AutoNego_Complete) != PHY_AutoNego_Complete);

    RegValue = ETH_ReadPHYRegister( PHY_ADDRESS, 0x10 );
    TRACE("PHY_SR value:%04x.\n", RegValue);

    if( RegValue & (1<<2) )
    {
        ETH_InitStructure.ETH_Mode = ETH_Mode_FullDuplex;
        TRACE("Full Duplex.\n");
    }
    else
    {
        ETH_InitStructure.ETH_Mode = ETH_Mode_HalfDuplex;
        TRACE("Half Duplex.\n");
    }
    ETH_InitStructure.ETH_Speed = ETH_Speed_10M;
    if (RegValue & (1<<3))
    {
        TRACE("Loopback_10M \n");
    }
#endif

    ETH_InitStructure.ETH_Mode = ETH_Mode_FullDuplex;
    ETH_InitStructure.ETH_Speed = ETH_Speed_10M;

    /*------------------------ MAC寄存器配置  ----------------------- --------------------*/
    /* MACCCR在RGMII接口模式下具有调整RGMII接口时序的域，请注意 */
    tmpreg = ETH->MACCR;
    tmpreg &= MACCR_CLEAR_MASK;
    tmpreg |= (uint32_t)(ETH_InitStructure.ETH_Watchdog |
                    ETH_InitStructure.ETH_Jabber |
                    ETH_InitStructure.ETH_InterFrameGap |
                    ETH_InitStructure.ETH_CarrierSense |
                    ETH_InitStructure.ETH_Speed |
                    ETH_InitStructure.ETH_ReceiveOwn |
                    ETH_InitStructure.ETH_LoopbackMode |
                    ETH_InitStructure.ETH_Mode |
                    ETH_InitStructure.ETH_ChecksumOffload |
                    ETH_InitStructure.ETH_RetryTransmission |
                    ETH_InitStructure.ETH_AutomaticPadCRCStrip |
                    ETH_InitStructure.ETH_BackOffLimit |
                    ETH_InitStructure.ETH_DeferralCheck);
    /* 写MAC控制寄存器 */
    ETH->MACCR = (uint32_t)tmpreg;

    ETH->MACCR|=ETH_Internal_Pull_Up_Res_Enable;/* 启用内部上拉  */

    ETH->MACFFR = (uint32_t)(ETH_InitStructure.ETH_ReceiveAll |
                            ETH_InitStructure.ETH_SourceAddrFilter |
                            ETH_InitStructure.ETH_PassControlFrames |
                            ETH_InitStructure.ETH_BroadcastFramesReception |
                            ETH_InitStructure.ETH_DestinationAddrFilter |
                            ETH_InitStructure.ETH_PromiscuousMode |
                            ETH_InitStructure.ETH_MulticastFramesFilter |
                            ETH_InitStructure.ETH_UnicastFramesFilter);
    /*--------------- ETHERNET MACHTHR and MACHTLR Configuration ---------------*/
    /* Write to ETHERNET MACHTHR */
    ETH->MACHTHR = (uint32_t)ETH_InitStructure.ETH_HashTableHigh;
    /* Write to ETHERNET MACHTLR */
    ETH->MACHTLR = (uint32_t)ETH_InitStructure.ETH_HashTableLow;
    /*----------------------- ETHERNET MACFCR Configuration --------------------*/
    /* Get the ETHERNET MACFCR value */
    tmpreg = ETH->MACFCR;
    /* Clear xx bits */
    tmpreg &= MACFCR_CLEAR_MASK;

    tmpreg |= (uint32_t)((ETH_InitStructure.ETH_PauseTime << 16) |
                     ETH_InitStructure.ETH_ZeroQuantaPause |
                     ETH_InitStructure.ETH_PauseLowThreshold |
                     ETH_InitStructure.ETH_UnicastPauseFrameDetect |
                     ETH_InitStructure.ETH_ReceiveFlowControl |
                     ETH_InitStructure.ETH_TransmitFlowControl);
    ETH->MACFCR = (uint32_t)tmpreg;

    ETH->MACVLANTR = (uint32_t)(ETH_InitStructure.ETH_VLANTagComparison |
                               ETH_InitStructure.ETH_VLANTagIdentifier);

    tmpreg = ETH->DMAOMR;
    /* Clear xx bits */
    tmpreg &= DMAOMR_CLEAR_MASK;

    tmpreg |= (uint32_t)(ETH_InitStructure.ETH_DropTCPIPChecksumErrorFrame |
                    ETH_InitStructure.ETH_ReceiveStoreForward |
                    ETH_InitStructure.ETH_FlushReceivedFrame |
                    ETH_InitStructure.ETH_TransmitStoreForward |
                    ETH_InitStructure.ETH_TransmitThresholdControl |
                    ETH_InitStructure.ETH_ForwardErrorFrames |
                    ETH_InitStructure.ETH_ForwardUndersizedGoodFrames |
                    ETH_InitStructure.ETH_ReceiveThresholdControl |
                    ETH_InitStructure.ETH_SecondFrameOperate);
    ETH->DMAOMR = (uint32_t)tmpreg;

    ETH->DMABMR = (uint32_t)(ETH_InitStructure.ETH_AddressAlignedBeats |
                            ETH_InitStructure.ETH_FixedBurst |
                            ETH_InitStructure.ETH_RxDMABurstLength | /* !! if 4xPBL is selected for Tx or Rx it is applied for the other */
                            ETH_InitStructure.ETH_TxDMABurstLength |
                           (ETH_InitStructure.ETH_DescriptorSkipLength << 2) |
                            ETH_InitStructure.ETH_DMAArbitration |
                            ETH_DMABMR_USP);
    
    /* initialize MAC address in ethernet MAC */ 
    ETH_MACAddressConfig(ETH_MAC_Address0, AppConfig.MyMACAddr.v);

    ETH_DMATxDescChainInit(DMATxDscrTab, &Tx_Buff[0][0], ETH_TXBUFNB);
    ETH_DMARxDescChainInit(DMARxDscrTab, &Rx_Buff[0][0], ETH_RXBUFNB);

    /* Disable the Ethernet Rx Interrupt */
    ETH_DMAITConfig(ETH_DMA_IT_NIS | ETH_DMA_IT_R | ETH_DMA_IT_T | ETH_DMA_IT_PHYLINK, DISABLE);

    ETH_Start();

    TRACE("eth init ok\n");
}

BOOL MACIsLinked(void)
{
    if ((ETH_ReadPHYRegister(PHY_ADDRESS, PHY_BSR) & PHY_Linked_Status) == PHY_Linked_Status)
    {
        return TRUE;
    }

    return FALSE;
}

BOOL MACIsTxReady(void)
{
    if ((DMATxDescToSet->Status & ETH_DMATxDesc_OWN) == (UINT32)RESET
           && (ETH_ReadPHYRegister(PHY_ADDRESS, PHY_BSR) & PHY_Linked_Status) == PHY_Linked_Status)
    {
        return TRUE;
    }

    return FALSE;
}

void MACDiscardRx(void)
{
    // Make sure the current packet was not already discarded
	if(WasDiscarded)
		return;
	WasDiscarded = TRUE;

	DMARxDescToGet->Status = ETH_DMARxDesc_OWN;

    DMARxDescToGet = (ETH_DMADESCTypeDef*) (DMARxDescToGet->Buffer2NextDescAddr);

    if ((ETH->DMASR & ETH_DMASR_RBUS) != (u32)RESET)
    {
        /* Clear RBUS ETHERNET DMA flag */
        ETH->DMASR = ETH_DMASR_RBUS;
        /* Resume DMA reception */
        ETH->DMARPDR = 0;
    }
}

WORD MACGetFreeRxSize(void)
{
   /* WORD size = 0;

    ETH_DMADESCTypeDef *DMARxDesc = DMARxDescToGet;

    if ((DMARxDesc->Status & ETH_DMARxDesc_OWN) != (uint32_t)RESET
            || (DMARxDesc->Status & ETH_DMARxDesc_LS) == (uint32_t)RESET)
    {
        TRACE("status:0x%08X\n", DMARxDesc->Status);
        TRACE("size:%d\n", size);

        return size;
    }

    WORD len = ((DMARxDesc->Status & ETH_DMARxDesc_FL) >> ETH_DMARxDesc_FrameLengthShift) - 4;
    if (ETH_MAX_PACKET_SIZE >= len)
    {
        size = ETH_MAX_PACKET_SIZE - len;
    }

    return size;*/
    return 1460;
}

BOOL MACGetHeader(MAC_ADDR *remote, BYTE* type)
{
    ETHER_HEADER header;

    if((DMARxDescToGet->Status & ETH_DMARxDesc_OWN) == (uint32_t)RESET
        && (DMARxDescToGet->Status & ETH_DMARxDesc_LS) != (uint32_t)RESET)
    {
        // Make absolutely certain that any previous packet was discarded
        if(WasDiscarded == FALSE)
        {
            //TRACE("discard mac\n");
            MACDiscardRx();
            return FALSE;
        }

        if ((DMARxDescToGet->Status & ETH_DMARxDesc_ES) != (uint32_t)RESET)
        {
            TRACE("frame err\n");

            MACDiscardRx();

            return FALSE;
        }

        //for test
#if 0
        UINT16 len = ((DMARxDescToGet->Status & ETH_DMARxDesc_FL) >> ETH_DMARxDesc_FrameLengthShift) - 4;
        TRACE("mac rx len:%d\n", len);
        UINT8 *data =(UINT8 *)DMARxDescToGet->Buffer1Addr;
        TRACE_ARRAY(data, len);
#endif
        //end for test

        if (rxPtrToRxBuffer)
        {
            rxPtr = 0;
        }

        // Obtain the MAC header from the Ethernet buffer
        MACGetArray((BYTE*)&header, sizeof(header));

        // The EtherType field, like most items transmitted on the Ethernet medium
        // are in big endian.
        header.Type.Val = swaps(header.Type.Val);

        // Return the Ethernet frame's Source MAC address field to the caller
        // This parameter is useful for replying to requests without requiring an
        // ARP cycle.
        memcpy((void*)remote->v, (void*)header.SourceMACAddr.v, sizeof(*remote));

        // Return a simplified version of the EtherType field to the caller
        *type = MAC_UNKNOWN;
        if( (header.Type.v[1] == 0x08u) &&
            ((header.Type.v[0] == ETHER_IP) || (header.Type.v[0] == ETHER_ARP)) )
        {
            *type = header.Type.v[0];
        }

        // Mark this packet as discardable
        WasDiscarded = FALSE;

        dataTransceiving = TRUE;

        return TRUE;
    }

    return FALSE;
}


void MACPutHeader(MAC_ADDR *remote, BYTE type, WORD dataLen)
{
    txPtr = 0;

    dataLen += (WORD)sizeof(ETHER_HEADER);

    txCount = dataLen;

	// Set the per-packet control byte and write the Ethernet destination 
	// address
    MACPutArray((BYTE*)remote, sizeof(*remote));

	// Write our MAC address in the Ethernet source field
	MACPutArray((BYTE*)&AppConfig.MyMACAddr, sizeof(AppConfig.MyMACAddr));

	// Write the appropriate Ethernet Type WORD for the protocol being used
    MACPut(0x08);
    MACPut((type == MAC_IP) ? ETHER_IP : ETHER_ARP);
}

void MACFlush(void)
{
    //for test
#if 0
    UINT8 *data =(UINT8 *)DMATxDescToSet->Buffer1Addr;
    TRACE("tx len:%d\n", txCount);
    TRACE_ARRAY(data, txCount);
#endif
    //end for test

    if ((DMATxDescToSet->Status & ETH_DMATxDesc_OWN) == (u32)RESET)
    {
		DMATxDescToSet->ControlBufferSize = (txCount & ETH_DMATxDesc_TBS1);

	    /* Set LAST and FIRST segment */
	    DMATxDescToSet->Status |= ETH_DMATxDesc_LS | ETH_DMATxDesc_FS;

	    DMATxDescToSet->Status |= ETH_DMATxDesc_OWN;

	    /* When Tx Buffer unavailable flag is set: clear it and resume transmission */
	    if ((ETH->DMASR & ETH_DMASR_TBUS) != (u32)RESET)
	    {
	        /* Clear TBUS ETHERNET DMA flag */
	        ETH->DMASR = ETH_DMASR_TBUS;
	        /* Resume DMA transmission*/

	        ETH->DMATPDR = 0;
	    }

	    DMATxDescToSet = (ETH_DMADESCTypeDef*) (DMATxDescToSet->Buffer2NextDescAddr);

	    txPtr = 0;

	    txCount = 0;

	    dataTransceiving = TRUE;
    }
}

void MACSetReadPtrInRx(WORD offset)
{
    rxPtr = sizeof(ETHER_HEADER) + offset;
}

WORD MACSetWritePtr(WORD address)
{
    WORD oldPtr = txPtr;
	txPtr = address;

	return oldPtr;
}

WORD MACSetReadPtr(WORD address)
{
    WORD oldPtr = rxPtr;
	rxPtr = address;

	return oldPtr;
}

BOOL MACSetReadPtrToRx(BOOL toRx)
{
    BOOL oldToRx = rxPtrToRxBuffer;
    rxPtrToRxBuffer = toRx;

    return oldToRx;
}

WORD MACCalcRxChecksum(WORD offset, WORD len)
{
	WORD temp;

	BOOL ptrToRxSave = rxPtrToRxBuffer;
	rxPtrToRxBuffer = TRUE;

	WORD rxPtrSave = rxPtr;
	rxPtr = sizeof(ETHER_HEADER) + offset;

	temp = CalcIPBufferChecksum(len);
	rxPtr = rxPtrSave;

	rxPtrToRxBuffer = ptrToRxSave;

	return temp;
}

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

void MACMemCopyAsync(DWORD destAddr, DWORD sourceAddr, WORD len)
{
    BOOL UpdateWritePointer = FALSE;
    BOOL UpdateReadPointer = FALSE;

    if(((DWORD_VAL*)&destAddr)->bits.b31)
    {
        UpdateWritePointer = TRUE;

        destAddr = rxPtr;
    }
    if(((DWORD_VAL*)&sourceAddr)->bits.b31)
    {
        UpdateReadPointer = TRUE;

        sourceAddr = txPtr;
    }

    UINT8 *dst =(UINT8 *)DMATxDescToSet->Buffer1Addr;
    UINT8 *src =(UINT8 *)DMARxDescToGet->Buffer1Addr;

    for (WORD i = 0; i < len; i++)
    {
        *(dst + destAddr++) = *(src + sourceAddr++);
        if (UpdateWritePointer)
        {
            txPtr++;
        }
        if (UpdateReadPointer)
        {
            rxPtr++;
        }
    }
}

BOOL MACIsMemCopyDone(void)
{
	return TRUE;
}

BYTE MACGet()
{
    UINT8 *data;
    if (rxPtrToRxBuffer)
    {
        data = (UINT8 *)DMARxDescToGet->Buffer1Addr;
    }
    else
    {
        data = (UINT8 *)DMATxDescToSet->Buffer1Addr;
    }

    return data[rxPtr++];
}//end MACGet

WORD MACGetArray(BYTE *val, WORD len)
{
    UINT8 *data;
    if (rxPtrToRxBuffer)
    {
        data = (UINT8 *)DMARxDescToGet->Buffer1Addr;
    }
    else
    {
        data = (UINT8 *)DMATxDescToSet->Buffer1Addr;
    }

	if(val)
	{
	    memcpy(val, &data[rxPtr], len);
	}

	rxPtr += len;

	return len;
}//end MACGetArray

void MACPut(BYTE val)
{
	UINT8 *data =(UINT8 *)DMATxDescToSet->Buffer1Addr;

    data[txPtr++] = val;
}//end MACPut


/******************************************************************************
 * Function:        void MACPutArray(BYTE *val, WORD len)
 *
 * PreCondition:    EWRPT must point to the location to begin writing.
 *
 * Input:           *val: Pointer to source of bytes to copy.
 *					len:  Number of bytes to write to the data buffer.
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        MACPutArray writes several sequential bytes to the 
 *					Ethernet buffer RAM.  It performs faster than multiple MACPut()
 *					calls.  EWRPT is incremented by len.
 *
 * Note:            None
 *****************************************************************************/
void MACPutArray(BYTE *val, WORD len)
{
    UINT8 *data =(UINT8 *)DMATxDescToSet->Buffer1Addr;

    memcpy(&data[txPtr], val, len);

    txPtr += len;
}//end MACPutArray

BOOL MACIsDataTransceiving(void)
{
    return dataTransceiving;
}

void MACSetDataTransceiving(BOOL transceiving)
{
    dataTransceiving = transceiving;
}
