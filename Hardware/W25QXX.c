#include "W25Qxx.h"
#include "MySPI.h"

#define W25QXX_WRITE_ENABLE       0x06
#define W25QXX_READ_STATUS_REG1   0x05
#define W25QXX_PAGE_PROGRAM       0x02
#define W25QXX_READ_DATA          0x03
#define W25QXX_SECTOR_ERASE_4K    0x20
#define W25QXX_JEDEC_ID           0x9F

static uint8_t W25Qxx_ReadStatusReg1(void)
{
    uint8_t Status;

    MySPI_Start();
    MySPI_SwapByte(W25QXX_READ_STATUS_REG1);
    Status = MySPI_SwapByte(0xFF);
    MySPI_Stop();

    return Status;
}

static void W25Qxx_WriteEnable(void)
{
    MySPI_Start();
    MySPI_SwapByte(W25QXX_WRITE_ENABLE);
    MySPI_Stop();
}

static void W25Qxx_WaitBusy(void)
{
    while ((W25Qxx_ReadStatusReg1() & 0x01) == 0x01);
}

void W25Qxx_Init(void)
{
    MySPI_Init();
}

uint32_t W25Qxx_ReadID(void)
{
    uint32_t ID = 0;

    MySPI_Start();
    MySPI_SwapByte(W25QXX_JEDEC_ID);
    ID |= ((uint32_t)MySPI_SwapByte(0xFF) << 16);
    ID |= ((uint32_t)MySPI_SwapByte(0xFF) << 8);
    ID |= ((uint32_t)MySPI_SwapByte(0xFF));
    MySPI_Stop();

    return ID;
}

void W25Qxx_ReadData(uint32_t Address, uint8_t *Buffer, uint16_t Count)
{
    uint16_t i;

    MySPI_Start();
    MySPI_SwapByte(W25QXX_READ_DATA);
    MySPI_SwapByte(Address >> 16);
    MySPI_SwapByte(Address >> 8);
    MySPI_SwapByte(Address);

    for (i = 0; i < Count; i++)
    {
        Buffer[i] = MySPI_SwapByte(0xFF);
    }

    MySPI_Stop();
}

void W25Qxx_PageProgram(uint32_t Address, uint8_t *Buffer, uint16_t Count)
{
    uint16_t i;

    /* 注意：单次页编程不要跨页，Count最好 <= 256 */
    W25Qxx_WriteEnable();

    MySPI_Start();
    MySPI_SwapByte(W25QXX_PAGE_PROGRAM);
    MySPI_SwapByte(Address >> 16);
    MySPI_SwapByte(Address >> 8);
    MySPI_SwapByte(Address);

    for (i = 0; i < Count; i++)
    {
        MySPI_SwapByte(Buffer[i]);
    }

    MySPI_Stop();

    W25Qxx_WaitBusy();
}

void W25Qxx_SectorErase(uint32_t Address)
{
    W25Qxx_WriteEnable();

    MySPI_Start();
    MySPI_SwapByte(W25QXX_SECTOR_ERASE_4K);
    MySPI_SwapByte(Address >> 16);
    MySPI_SwapByte(Address >> 8);
    MySPI_SwapByte(Address);
    MySPI_Stop();

    W25Qxx_WaitBusy();
}