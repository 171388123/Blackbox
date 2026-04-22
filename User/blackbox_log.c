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

static uint8_t BlackBox_IsRecordErased(const BlackBoxRecord_t *Record)
{
    const uint8_t *p = (const uint8_t *)Record;
    uint32_t i;

    for (i = 0; i < BLACKBOX_RECORD_SIZE; i++)
    {
        if (p[i] != 0xFF)
        {
            return 0;
        }
    }

    return 1;
}

static uint8_t BlackBox_IsRecordEqual(const BlackBoxRecord_t *A, const BlackBoxRecord_t *B)
{
    const uint8_t *pA = (const uint8_t *)A;
    const uint8_t *pB = (const uint8_t *)B;
    uint32_t i;

    for (i = 0; i < BLACKBOX_RECORD_SIZE; i++)
    {
        if (pA[i] != pB[i])
        {
            return 0;
        }
    }

    return 1;
}

static void BlackBox_ClearRecord(BlackBoxRecord_t *Record)
{
    uint8_t *p = (uint8_t *)Record;
    uint32_t i;

    for (i = 0; i < BLACKBOX_RECORD_SIZE; i++)
    {
        p[i] = 0;
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
        W25Qxx_ReadData(addr, (uint8_t *)&Record, BLACKBOX_RECORD_SIZE);

        if (Record.Magic == BLACKBOX_LOG_MAGIC)
        {
            BlackBox_RecordCount++;
            BlackBox_NextSeq = Record.Seq + 1;
            BlackBox_NextWriteAddr = addr + BLACKBOX_RECORD_SIZE;
        }
        else if (BlackBox_IsRecordErased(&Record))
        {
            /* 遇到真正空白区域，说明日志到这里结束 */
            BlackBox_NextWriteAddr = addr;
            return;
        }
        else
        {
            /*
             * 既不是有效记录，也不是空白区：
             * 说明这个位置是脏数据/异常数据。
             * 对当前这个小项目，直接格式化整个日志区，保证后续行为确定。
             */
            BlackBoxLog_Format();
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

    /* 写之前先确认当前位置真的是空白区 */
    W25Qxx_ReadData(BlackBox_NextWriteAddr, (uint8_t *)&VerifyRecord, BLACKBOX_RECORD_SIZE);
    if (!BlackBox_IsRecordErased(&VerifyRecord))
    {
        /*
         * 理论上 Init 后 NextWriteAddr 应该指向空白区。
         * 如果这里不是空白区，说明日志区状态已经不可信。
         * 对当前项目直接格式化，避免继续往脏区域写。
         */
        BlackBoxLog_Format();
    }

    BlackBox_ClearRecord(&Record);

    Record.Magic = BLACKBOX_LOG_MAGIC;
    Record.Seq = BlackBox_NextSeq;
    Record.TickMs = TickMs;
    Record.Event = Event;
    Record.Pitch_x10 = Pitch_x10;
    Record.Roll_x10 = Roll_x10;
    Record.AD = AD;
    Record.GD = GD;
    Record.Time = *Time;

    W25Qxx_PageProgram(BlackBox_NextWriteAddr, (uint8_t *)&Record, BLACKBOX_RECORD_SIZE);

    /* 写完立刻回读校验，防止“假保存成功” */
    W25Qxx_ReadData(BlackBox_NextWriteAddr, (uint8_t *)&VerifyRecord, BLACKBOX_RECORD_SIZE);
    if (!BlackBox_IsRecordEqual(&Record, &VerifyRecord))
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