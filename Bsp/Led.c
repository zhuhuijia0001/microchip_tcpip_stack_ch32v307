#include "TCPIP Stack/TCPIP.h"

#include "ch32v30x.h"

//link pin
#define LINK_PORT_RCC      RCC_APB2Periph_GPIOA
#define LINK_PORT          GPIOA
#define LINK_PIN           GPIO_Pin_9

#define LINK_ON            do { LINK_PORT->BSHR = LINK_PIN; } while (0)
#define LINK_OFF           do { LINK_PORT->BCR = LINK_PIN; } while (0)

//data pin
#define DATA_PORT_RCC      RCC_APB2Periph_GPIOA
#define DATA_PORT          GPIOA
#define DATA_PIN           GPIO_Pin_8

#define DATA_ON            do { DATA_PORT->BSHR = DATA_PIN; } while (0)
#define DATA_OFF           do { DATA_PORT->BCR = DATA_PIN; } while (0)

void EthLedInit(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(LINK_PORT_RCC | DATA_PORT_RCC, ENABLE);

    GPIO_InitStructure.GPIO_Pin = LINK_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LINK_PORT, &GPIO_InitStructure);

    LINK_OFF;

    GPIO_InitStructure.GPIO_Pin = DATA_PIN;
    GPIO_Init(DATA_PORT, &GPIO_InitStructure);

    DATA_OFF;
}

#if defined(STACK_USE_PHY_LED)

void EthLedLinkOn(BOOL on)
{
    if (on)
    {
        LINK_ON;
    }
    else
    {
        LINK_OFF;
    }
}

void EthLedDataOn(BOOL on)
{
    if (on)
    {
        DATA_ON;
    }
    else
    {
        DATA_OFF;
    }
}

#endif
