#include "TCPIP Stack/TCPIP.h"
#include "AppConfig.h"

#include "Parameters.h"

APP_CONFIG AppConfig;

#define FLASH_START_ADDRESS   0
#define FLASH_SECTOR_SIZE     4096
#define FLASH_SECTOR_COUNT    8
#define FLASH_END_ADDRESS     (FLASH_START_ADDRESS + FLASH_SECTOR_SIZE * FLASH_SECTOR_COUNT)

#define INVALID_PARAMATER_START    (DWORD)-1

static DWORD curParameterStart = INVALID_PARAMATER_START;

static BOOL CheckAppConfig(const APP_CONFIG *appConfig)
{
    BYTE sum;
    WORD size;
    BYTE *p = (BYTE*)appConfig;
    WORD i;

    if (appConfig->header != EEPROM_HEADER)
    {
        return FALSE;
    }

    size = appConfig->size;
    if (size != sizeof(*appConfig) - sizeof(appConfig->header) - sizeof(appConfig->size) - sizeof(appConfig->sum))
    {
        return FALSE;
    }

    sum = 0;

    p += sizeof(appConfig->header) + sizeof(appConfig->size);
    for (i = 0; i < size; i++)
    {
        sum += *p++;
    }

    if (sum != appConfig->sum)
    {
        return FALSE;
    }

    return TRUE;
}

BOOL LoadParameters(void)
{
    int validStorageCount = 0;

    DWORD firstValidAddr = INVALID_PARAMATER_START, secondValidAddr = INVALID_PARAMATER_START;

    curParameterStart = INVALID_PARAMATER_START;

    for (int i = 0; i < FLASH_SECTOR_COUNT; i++)
    {
        SPIFlashReadArray(FLASH_START_ADDRESS + FLASH_SECTOR_SIZE * i, (BYTE *)&AppConfig, sizeof(AppConfig));

        if (CheckAppConfig(&AppConfig))
        {
            if (validStorageCount == 0)
            {
                firstValidAddr = FLASH_START_ADDRESS + FLASH_SECTOR_SIZE * i;
            }
            else if (validStorageCount == 1)
            {
                secondValidAddr = FLASH_START_ADDRESS + FLASH_SECTOR_SIZE * i;
            }

            validStorageCount++;

            if (validStorageCount == 2)
            {
                break;
            }
        }
    }

    if (validStorageCount == 2)
    {
        if (firstValidAddr + FLASH_SECTOR_SIZE == secondValidAddr)
        {
            //erase sector
            SPIFlashBeginWrite(firstValidAddr);
            SPIFlashWrite(0xFF);

            curParameterStart = secondValidAddr;

            SPIFlashReadArray(curParameterStart, (BYTE *)&AppConfig, sizeof(AppConfig));

            return TRUE;
        }
        else if (firstValidAddr == FLASH_START_ADDRESS && secondValidAddr + FLASH_SECTOR_SIZE == FLASH_END_ADDRESS)
        {
            //erase sector
            SPIFlashBeginWrite(secondValidAddr);
            SPIFlashWrite(0xFF);

            curParameterStart = firstValidAddr;

            SPIFlashReadArray(curParameterStart, (BYTE *)&AppConfig, sizeof(AppConfig));

            return TRUE;
        }
    }
    else if (validStorageCount == 1)
    {
        curParameterStart = firstValidAddr;

        SPIFlashReadArray(curParameterStart, (BYTE *)&AppConfig, sizeof(AppConfig));

        return TRUE;
    }

    return FALSE;
}

void SaveParameters(void)
{
    WORD i;
    BYTE *p;
    BYTE sum;

    WORD size;

    AppConfig.header = EEPROM_HEADER;
    AppConfig.size = sizeof(AppConfig) - sizeof(AppConfig.header) - sizeof(AppConfig.size) - sizeof(AppConfig.sum);

    size = AppConfig.size;

    p = (BYTE *)&AppConfig;
    p += sizeof(AppConfig.header) + sizeof(AppConfig.size);

    sum = 0;
    for (i = 0; i < size; i++)
    {
        sum += *p++;
    }

    AppConfig.sum = sum;

    //write
    p = (BYTE *)&AppConfig;

    if (curParameterStart == INVALID_PARAMATER_START)
    {
        SPIFlashBeginWrite(FLASH_START_ADDRESS);
        SPIFlashWriteArray(p, sizeof(AppConfig));

        curParameterStart = FLASH_START_ADDRESS;
    }
    else
    {
        DWORD nextParameterStart = curParameterStart + FLASH_SECTOR_SIZE;
        if (nextParameterStart == FLASH_END_ADDRESS)
            nextParameterStart = FLASH_START_ADDRESS;

        SPIFlashBeginWrite(nextParameterStart);
        SPIFlashWriteArray(p, sizeof(AppConfig));

        //erase sector
        SPIFlashBeginWrite(curParameterStart);
        SPIFlashWrite(0xFF);

        curParameterStart = nextParameterStart;
    }
}

void ClearParameters(void)
{
    //erase sector
    SPIFlashBeginWrite(curParameterStart);
    SPIFlashWrite(0xFF);

    curParameterStart = INVALID_PARAMATER_START;
}

void LoadDefaultParameters(void)
{
    BYTE i;

    AppConfig.Flags.bIsDHCPEnabled = FALSE;
    AppConfig.Flags.bInConfigMode = TRUE;

    /* set MAC hardware address */
    UINT8 *mac = (UINT8 *)(0x1FFFF7E8 + 5);
    for (i = 0; i < sizeof(AppConfig.MyMACAddr); i++)
    {
        AppConfig.MyMACAddr.v[i] =  *mac--;
    }

    TRACE("mac:");
    TRACE_ARRAY_DELIMIT(AppConfig.MyMACAddr.v, 6, ':');


    AppConfig.MyIPAddr.Val = MY_DEFAULT_IP_ADDR_BYTE1 | MY_DEFAULT_IP_ADDR_BYTE2<<8ul | MY_DEFAULT_IP_ADDR_BYTE3<<16ul | MY_DEFAULT_IP_ADDR_BYTE4<<24ul;
    AppConfig.DefaultIPAddr.Val = AppConfig.MyIPAddr.Val;
    AppConfig.MyMask.Val = MY_DEFAULT_MASK_BYTE1 | MY_DEFAULT_MASK_BYTE2<<8ul | MY_DEFAULT_MASK_BYTE3<<16ul | MY_DEFAULT_MASK_BYTE4<<24ul;
    AppConfig.DefaultMask.Val = AppConfig.MyMask.Val;
    AppConfig.MyGateway.Val = MY_DEFAULT_GATE_BYTE1 | MY_DEFAULT_GATE_BYTE2<<8ul | MY_DEFAULT_GATE_BYTE3<<16ul | MY_DEFAULT_GATE_BYTE4<<24ul;
    AppConfig.PrimaryDNSServer.Val = MY_DEFAULT_PRIMARY_DNS_BYTE1 | MY_DEFAULT_PRIMARY_DNS_BYTE2<<8ul  | MY_DEFAULT_PRIMARY_DNS_BYTE3<<16ul  | MY_DEFAULT_PRIMARY_DNS_BYTE4<<24ul;
    AppConfig.SecondaryDNSServer.Val = MY_DEFAULT_SECONDARY_DNS_BYTE1 | MY_DEFAULT_SECONDARY_DNS_BYTE2<<8ul  | MY_DEFAULT_SECONDARY_DNS_BYTE3<<16ul  | MY_DEFAULT_SECONDARY_DNS_BYTE4<<24ul;

    // Load the default NetBIOS Host Name
    memcpypgm2ram(AppConfig.NetBIOSName, (ROM void*)MY_DEFAULT_HOST_NAME, 16);

    //user name
    memset(AppConfig.UserName, 0, USERNAME_LEN);
    strcpypgm2ram(AppConfig.UserName, DEFAULT_USERNAME);

    //password
    memset(AppConfig.Password, 0, PASSWORD_LEN);
    strcpypgm2ram(AppConfig.Password, DEFAULT_PASSWORD);
}

