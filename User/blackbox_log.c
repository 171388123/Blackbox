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

#define W25QXX_PAGE_SIZE           256

static uint32_t BlackBox_NextWriteAddr = BLACKBOX_LOG_BASE_ADDR;
static uint32_t BlackBox_RecordCount = 0;
static uint32_t BlackBox_NextSeq = 0;

static void BlackBox_ClearRecord(BlackBoxRecord_t *Record)
{
    uint32_t i;
    uint8_t *p;

    if (Record == 0)
    {
        return;
    }

    p = (uint8_t *)Record;
    for (i = 0; i < BLACKBOX_RECORD_SIZE; i++)
    {
        p[i] = 0;
    }
}

static uint8_t BlackBox_IsRecordSame(const BlackBoxRecord_t *A, const BlackBoxRecord_t *B)
{
    if (A == 0 || B == 0)
    {
        return 0;
    }

    if (A->Magic != B->Magic) return 0;
    if (A->Seq != B->Seq) return 0;
    if (A->TickMs != B->TickMs) return 0;
    if (A->Event != B->Event) return 0;
    if (A->Pitch_x10 != B->Pitch_x10) return 0;
    if (A->Roll_x10 != B->Roll_x10) return 0;
    if (A->AD != B->AD) return 0;
    if (A->GD != B->GD) return 0;

    if (A->Time.Year != B->Time.Year) return 0;
    if (A->Time.Month != B->Time.Month) return 0;
    if (A->Time.Day != B->Time.Day) return 0;
    if (A->Time.Hour != B->Time.Hour) return 0;
    if (A->Time.Minute != B->Time.Minute) return 0;
    if (A->Time.Second != B->Time.Second) return 0;

    return 1;
}

static void BlackBox_WriteBytes(uint32_t Addr, uint8_t *Data, uint16_t Length)
{
    uint16_t pageRemain;
    uint16_t chunk;

    while (Length > 0)
    {
        pageRemain = W25QXX_PAGE_SIZE - (Addr % W25QXX_PAGE_SIZE);
        chunk = (Length < pageRemain) ? Length : pageRemain;

        W25Qxx_PageProgram(Addr, Data, chunk);

        Addr += chunk;
        Data += chunk;
        Length -= chunk;
    }
}

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
        BlackBox_ClearRecord(&Record);
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

uint8_t BlackBoxLog_Append(BlackBoxTime_t *Time,
                           uint32_t TickMs,
                           uint32_t Event,
                           int32_t Pitch_x10,
                           int32_t Roll_x10,
                           uint32_t AD,
                           uint32_t GD)
{
    BlackBoxRecord_t Record;
    BlackBoxRecord_t VerifyRecord;

    if (Time == 0)
    {
        return 0;
    }

    if (BlackBox_NextWriteAddr >= BLACKBOX_LOG_END_ADDR)
    {
        BlackBoxLog_Format();
    }

    BlackBox_ClearRecord(&Record);
    BlackBox_ClearRecord(&VerifyRecord);

    Record.Magic = BLACKBOX_LOG_MAGIC;
    Record.Seq = BlackBox_NextSeq;
    Record.TickMs = TickMs;
    Record.Event = Event;
    Record.Pitch_x10 = Pitch_x10;
    Record.Roll_x10 = Roll_x10;
    Record.AD = AD;
    Record.GD = GD;
    Record.Time = *Time;

    /* 关键修复：分段写，避免 40 字节记录跨 256 字节页边界 */
    BlackBox_WriteBytes(BlackBox_NextWriteAddr, (uint8_t *)&Record, BLACKBOX_RECORD_SIZE);

    /* 写后回读校验 */
    W25Qxx_ReadData(BlackBox_NextWriteAddr, (uint8_t *)&VerifyRecord, BLACKBOX_RECORD_SIZE);
    if (!BlackBox_IsRecordSame(&Record, &VerifyRecord))
    {
        return 0;
    }

    BlackBox_NextWriteAddr += BLACKBOX_RECORD_SIZE;
    BlackBox_RecordCount++;
    BlackBox_NextSeq++;

    return 1;
}

uint8_t BlackBoxLog_ReadByIndex(uint32_t Index, BlackBoxRecord_t *Record)
{
    uint32_t addr;

    if (Record == 0)
    {
        return 0;
    }

    if (Index >= BlackBox_RecordCount)
    {
        return 0;
    }

    addr = BLACKBOX_LOG_BASE_ADDR + Index * BLACKBOX_RECORD_SIZE;
    BlackBox_ClearRecord(Record);
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