#include "blackbox_log.h"
#include "W25Qxx.h"

#define BLACKBOX_LOG_MAGIC         0xA55AA55A

/* 日志区域：从 0x010000 开始，连续 4 个扇区，共 16KB */
#define BLACKBOX_LOG_BASE_ADDR     0x010000
#define BLACKBOX_LOG_SECTOR_SIZE   4096
#define BLACKBOX_LOG_SECTOR_COUNT  4
#define BLACKBOX_LOG_END_ADDR      (BLACKBOX_LOG_BASE_ADDR + BLACKBOX_LOG_SECTOR_SIZE * BLACKBOX_LOG_SECTOR_COUNT)

#define BLACKBOX_RECORD_SIZE       (sizeof(BlackBoxRecord_t))
#define BLACKBOX_MAX_RECORDS       ((BLACKBOX_LOG_END_ADDR - BLACKBOX_LOG_BASE_ADDR) / BLACKBOX_RECORD_SIZE)

static uint32_t BlackBox_NextWriteAddr = BLACKBOX_LOG_BASE_ADDR;
static uint32_t BlackBox_RecordCount = 0;
static uint32_t BlackBox_NextSeq = 0;

void BlackBoxLog_Format(void)
{
    uint32_t i;

    for (i = 0; i < BLACKBOX_LOG_SECTOR_COUNT; i++)
    {
        W25Qxx_SectorErase(BLACKBOX_LOG_BASE_ADDR + i * BLACKBOX_LOG_SECTOR_SIZE);
    }

    BlackBox_NextWriteAddr = BLACKBOX_LOG_BASE_ADDR;
    BlackBox_RecordCount = 0;
    BlackBox_NextSeq = 0;
}

void BlackBoxLog_Init(void)
{
    uint32_t addr;
    BlackBoxRecord_t Record;

    BlackBox_NextWriteAddr = BLACKBOX_LOG_BASE_ADDR;
    BlackBox_RecordCount = 0;
    BlackBox_NextSeq = 0;

    for (addr = BLACKBOX_LOG_BASE_ADDR; addr < BLACKBOX_LOG_END_ADDR; addr += BLACKBOX_RECORD_SIZE)
    {
        W25Qxx_ReadData(addr, (uint8_t *)&Record, BLACKBOX_RECORD_SIZE);

        if (Record.Magic == BLACKBOX_LOG_MAGIC)
        {
            BlackBox_RecordCount++;
            BlackBox_NextSeq = Record.Seq + 1;
            BlackBox_NextWriteAddr = addr + BLACKBOX_RECORD_SIZE;
        }
        else
        {
            /* 遇到空白或无效数据，就认为后面还没写 */
            BlackBox_NextWriteAddr = addr;
            return;
        }
    }

    /* 如果整个区域都写满了，简单处理：清空后重头开始 */
    BlackBoxLog_Format();
}

uint32_t BlackBoxLog_GetCount(void)
{
    return BlackBox_RecordCount;
}

uint8_t BlackBoxLog_Append(uint32_t TickMs, uint32_t Event, int32_t Pitch_x10, int32_t Roll_x10, uint32_t AD, uint32_t GD)
{
    BlackBoxRecord_t Record;

    if (BlackBox_NextWriteAddr >= BLACKBOX_LOG_END_ADDR)
    {
        BlackBoxLog_Format();
    }

    Record.Magic = BLACKBOX_LOG_MAGIC;
    Record.Seq = BlackBox_NextSeq;
    Record.TickMs = TickMs;
    Record.Event = Event;
    Record.Pitch_x10 = Pitch_x10;
    Record.Roll_x10 = Roll_x10;
    Record.AD = AD;
    Record.GD = GD;

    W25Qxx_PageProgram(BlackBox_NextWriteAddr, (uint8_t *)&Record, BLACKBOX_RECORD_SIZE);

    BlackBox_NextWriteAddr += BLACKBOX_RECORD_SIZE;
    BlackBox_RecordCount++;
    BlackBox_NextSeq++;

    return 1;
}

uint8_t BlackBoxLog_ReadByIndex(uint32_t Index, BlackBoxRecord_t *Record)
{
    uint32_t addr;

    if (Index >= BlackBox_RecordCount)
    {
        return 0;
    }

    addr = BLACKBOX_LOG_BASE_ADDR + Index * BLACKBOX_RECORD_SIZE;
    W25Qxx_ReadData(addr, (uint8_t *)Record, BLACKBOX_RECORD_SIZE);

    if (Record->Magic != BLACKBOX_LOG_MAGIC)
    {
        return 0;
    }

    return 1;
}

uint8_t BlackBoxLog_ReadLast(BlackBoxRecord_t *Record)
{
    if (BlackBox_RecordCount == 0)
    {
        return 0;
    }

    return BlackBoxLog_ReadByIndex(BlackBox_RecordCount - 1, Record);
}