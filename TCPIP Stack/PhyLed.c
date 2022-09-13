#include "TCPIP Stack/TCPIP.h"

#include "Led.h"

#if defined(STACK_USE_PHY_LED)

#define  LINK_LED_UPDATE_TIMEOUT       (TICK_SECOND / 50)

#define  DATA_LED_UPDATE_TIMEOUT       (TICK_SECOND / 15)

void LinkLedUpdate(void)
{
    static TICK tick = 0;

    if (TickGet() - tick >= LINK_LED_UPDATE_TIMEOUT)
    {
        if (MACIsLinked())
        {
            EthLedLinkOn(TRUE);
        }
        else
        {
            EthLedLinkOn(FALSE);
        }

        tick = TickGet();
    }
}

void DataLedUpdate(void)
{
    static TICK tick = 0;

    static BOOL linkOn = FALSE;

    if (TickGet() - tick >= DATA_LED_UPDATE_TIMEOUT)
    {
        if (linkOn)
        {
            EthLedDataOn(FALSE);

            linkOn = FALSE;
        }
        else
        {
            if (MACIsDataTransceiving())
            {
                EthLedDataOn(TRUE);

                MACSetDataTransceiving(FALSE);

                linkOn = TRUE;
            }
        }
        tick = TickGet();
    }
}

#endif
