#include "Serial.h"

#define SERIAL_RX_BUFFER_SIZE 64

static volatile uint8_t Serial_RxBuffer[SERIAL_RX_BUFFER_SIZE];
static volatile uint16_t Serial_RxHead = 0;
static volatile uint16_t Serial_RxTail = 0;
static volatile uint16_t Serial_RxOverflowCount = 0;

static uint16_t Serial_NextIndex(uint16_t Index)
{
    Index++;
    if (Index >= SERIAL_RX_BUFFER_SIZE)
    {
        Index = 0;
    }
    return Index;
}

void Serial_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_InitTypeDef USART_InitStructure;
    USART_InitStructure.USART_BaudRate = 9600;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_Init(USART1, &USART_InitStructure);

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_Init(&NVIC_InitStructure);

    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

    USART_Cmd(USART1, ENABLE);
}

void Serial_SendByte(uint8_t Byte)
{
    USART_SendData(USART1, Byte);
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
        ;
}

void Serial_SendString(const char *String)
{
    while (*String != '\0')
    {
        Serial_SendByte(*String);
        String++;
    }
}

void Serial_SendNumber(uint32_t Number)
{
    char Buffer[11];
    uint8_t i = 0;
    uint8_t j;

    if (Number == 0)
    {
        Serial_SendByte('0');
        return;
    }

    while (Number > 0)
    {
        Buffer[i++] = Number % 10 + '0';
        Number /= 10;
    }

    for (j = 0; j < i; j++)
    {
        Serial_SendByte(Buffer[i - 1 - j]);
    }
}

uint8_t Serial_GetRxFlag(void)
{
    return (Serial_RxHead != Serial_RxTail);
}

uint8_t Serial_GetRxData(void)
{
    uint8_t Data = 0;

    if (Serial_RxHead != Serial_RxTail)
    {
        Data = Serial_RxBuffer[Serial_RxTail];
        Serial_RxTail = Serial_NextIndex(Serial_RxTail);
    }

    return Data;
}

uint16_t Serial_GetRxOverflowCount(void)
{
    return Serial_RxOverflowCount;
}

void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) == SET)
    {
        uint8_t Data;
        uint16_t NextHead;

        Data = (uint8_t)USART_ReceiveData(USART1);
        NextHead = Serial_NextIndex(Serial_RxHead);

        if (NextHead != Serial_RxTail)
        {
            Serial_RxBuffer[Serial_RxHead] = Data;
            Serial_RxHead = NextHead;
        }
        else
        {
            Serial_RxOverflowCount++;
        }
    }
}