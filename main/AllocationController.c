#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "AllocationController.h"
#include <string.h>
#include "DeBug.h"
#include "ConfigManager.h"
#include "CTimer.h"
#include "ChargingStationManager.h"
#include "GPIOManager.h"
#include "CEvent.h"

static int16_t LOAD_BALANCE_CAL = 0;//负载平衡标志
static bool lastModeDualCharging = false; // 记录上一次是否为双桩充电模式

static uint16_t MeterCurrVlaue = 0;     //0942的电流值
static uint16_t InflowCurrent = 0;   //家庭最大流入电流

// 先插枪优先逻辑：记录第一个变为Charging的桩
static char priorityStationMac[20] = {0};  // 优先充电桩MAC地址

// 为两个充电桩维护稳定电流跟踪器
static StableCurrentTracker stableTrackers[CHAEGING_STATION_MAX_NUM] = {0};

// IMeter interface pointer (injected via meter_init)
static const IMeter_t *s_meter = NULL;

/********************************************************
*@Function name:IsStationCharging
*@Description:检查充电桩是否正在充电
*@input param:station 充电桩对象
*@Return:true=正在充电, false=未充电
********************************************************************************/
static bool IsStationCharging(ChargingStation *station)
{
    if (station == NULL)
    {
        return false;
    }

    if(station->enumStatus == Charging || station->enumStatus == SuspendEvse)
    {
        return true;
    }
    return false;
}

/********************************************************
*@Function name:UpdateStableCurrentTracker
*@Description:更新充电桩的稳定电流跟踪器
*@input param:tracker 稳定电流跟踪器
*@input param:currentA 当前实际电流值（A）
*@input param:limitA 下发的限制电流值（A）
*@Return:
********************************************************************************/
static void UpdateStableCurrentTracker(StableCurrentTracker *tracker, uint16_t currentA, uint16_t limitA)
{
    if (tracker == NULL)
    {
        return;
    }

    // 如果电流为0，重置跟踪器（充电桩未充电或暂停）
    if (currentA == 0 || limitA == 0)
    {
        memset(tracker, 0, sizeof(StableCurrentTracker));
        return;
    }

    // 计算实际电流和下发电流的差值
    int16_t currentDiff = (int16_t)limitA - (int16_t)currentA;
    if (currentDiff < 0) currentDiff = -currentDiff;

    // 如果差值很小（<=2A），说明充电桩在正常响应调整，标记为不稳定
    if (currentDiff <= STABLE_CURRENT_TOLERANCE)
    {
        // 充电桩正在响应我们的调整，重置跟踪器
        memset(tracker, 0, sizeof(StableCurrentTracker));
        tracker->isStable = false;
        dPrint(DEBUG, "实际电流(%d A)接近下发电流(%d A)，差值=%d A，充电桩正在响应调整\n",
               currentA, limitA, currentDiff);
        return;
    }

    // 差值大于2A，说明受到BMS限制，开始判断稳定性
    dPrint(DEBUG, "实际电流(%d A)与下发电流(%d A)差值较大=%d A，开始稳定性判断\n",
           currentA, limitA, currentDiff);

    // 记录当前电流到历史数组
    tracker->historyCurrent[tracker->historyIndex] = currentA;
    tracker->historyIndex = (tracker->historyIndex + 1) % STABLE_CURRENT_SAMPLE_COUNT;

    if (tracker->sampleCount < STABLE_CURRENT_SAMPLE_COUNT)
    {
        tracker->sampleCount++;
    }

    // 只有采集到足够的样本才能判断是否稳定
    if (tracker->sampleCount < STABLE_CURRENT_SAMPLE_COUNT)
    {
        tracker->isStable = false;
        dPrint(DEBUG, "采样进度: %d/%d\n", tracker->sampleCount, STABLE_CURRENT_SAMPLE_COUNT);
        return;
    }

    // 检查最近3次电流是否在误差范围内
    uint16_t minCurrent = tracker->historyCurrent[0];
    uint16_t maxCurrent = tracker->historyCurrent[0];
    uint32_t sum = tracker->historyCurrent[0];

    for (int i = 1; i < STABLE_CURRENT_SAMPLE_COUNT; i++)
    {
        uint16_t curr = tracker->historyCurrent[i];
        sum += curr;
        if (curr < minCurrent) minCurrent = curr;
        if (curr > maxCurrent) maxCurrent = curr;
    }

    // 判断是否稳定：最大值和最小值的差不超过容差的2倍（±2A范围即4A）
    if ((maxCurrent - minCurrent) <= (STABLE_CURRENT_TOLERANCE * 2))
    {
        tracker->isStable = true;
        tracker->stableCurrent = sum / STABLE_CURRENT_SAMPLE_COUNT; // 取平均值作为稳定电流
        dPrint(INFO, "检测到稳定电流: %d A (范围: %d-%d A, 下发值: %d A, 差值: %d A)\n",
               tracker->stableCurrent, minCurrent, maxCurrent, limitA, currentDiff);
    }
    else
    {
        tracker->isStable = false;
        dPrint(DEBUG, "电流未稳定: 范围 %d-%d A (波动: %d A)\n",
               minCurrent, maxCurrent, maxCurrent - minCurrent);
    }
}

/********************************************************
*@Function name:ResetStableCurrentTracker
*@Description:重置充电桩的稳定电流跟踪器
*@input param:trackerIndex 跟踪器索引
*@Return:
********************************************************************************/
static void ResetStableCurrentTracker(uint8_t trackerIndex)
{
    if (trackerIndex >= CHAEGING_STATION_MAX_NUM)
    {
        return;
    }
    memset(&stableTrackers[trackerIndex], 0, sizeof(StableCurrentTracker));
}

void meter_init(const IMeter_t *meter)
{
    s_meter = meter;
}

/**
 * @brief Emergency: suspend all charging stations (meter data INVALID)
 */
static void emergency_suspend_all_stations(void)
{
    char jsonData[256] = {0};
    int stationCount = 0;
    ChargingStation *stations = SelectAllChargeStation(&stationCount);

    if (stations == NULL || stationCount <= 0) {
        return;
    }

    dPrint(WARN, "Meter data INVALID - emergency suspend all stations\n");
    snprintf(jsonData, sizeof(jsonData),
             "{\"content\":\"Meter data invalid, emergency suspend\",\"value\":\"0\"}");
    PublishEvent(EVENT_AUTO_CONTROL_MONITOR, jsonData, strlen(jsonData));

    for (int i = 0; i < stationCount; i++) {
        if (IsStationCharging(&stations[i])) {
            stations[i].limitCurrent = 0;
            PublishEvent(EVENT_AUTO_SET_LIMIT_CUUR, (char *)&stations[i], sizeof(ChargingStation));
            PublishEvent(EVENT_AUTO_SUSPEND, (char *)&stations[i], sizeof(ChargingStation));
        }
    }
}

/**
 * @brief Emergency: reduce all charging stations to EV_MIN_CURRENT (meter data STALE)
 */
static void emergency_reduce_to_min_current(void)
{
    char jsonData[256] = {0};
    int stationCount = 0;
    ChargingStation *stations = SelectAllChargeStation(&stationCount);

    if (stations == NULL || stationCount <= 0) {
        return;
    }

    dPrint(WARN, "Meter data STALE - reducing all stations to min current (%dA)\n", EV_MIN_CURRENT);
    snprintf(jsonData, sizeof(jsonData),
             "{\"content\":\"Meter data stale, limit to min current\",\"value\":\"%d\"}", EV_MIN_CURRENT);
    PublishEvent(EVENT_AUTO_CONTROL_MONITOR, jsonData, strlen(jsonData));

    for (int i = 0; i < stationCount; i++) {
        if (IsStationCharging(&stations[i])) {
            stations[i].limitCurrent = EV_MIN_CURRENT * 100;
            if (stations[i].enumStatus == SuspendEvse) {
                PublishEvent(EVENT_AUTO_START, (char *)&stations[i], sizeof(ChargingStation));
            }
            PublishEvent(EVENT_AUTO_SET_LIMIT_CUUR, (char *)&stations[i], sizeof(ChargingStation));
        }
    }
}

esp_err_t AutoControlInit(void)
{
    //启动控制线程
    xTaskCreate(&AutoControl_task, "AutoControl_task", 4096, NULL, 5, NULL);
    return ESP_OK;
}

void AutoControl_task(void *pvParameter)
{
    AddTimer(TIMER_ID_LOADBALANCE, 1000, LoadBalance_TimerFunc);
    while(1)
    {
        if(LOAD_BALANCE_CAL == 1)
        {
            LOAD_BALANCE_CAL = 0;

            // === Meter data freshness gate ===
            if (s_meter != NULL)
            {
                MeterDataFreshness_t freshness = meter_check_freshness(s_meter);
                if (freshness == METER_DATA_INVALID)
                {
                    emergency_suspend_all_stations();
                    goto task_sleep;
                }
                if (freshness == METER_DATA_STALE)
                {
                    emergency_reduce_to_min_current();
                    goto task_sleep;
                }
            }

            // === Normal load balance logic ===
            InflowCurrent = GPIOManager_GetInletCurrent();
            MeterCurrVlaue = (s_meter != NULL) ?
                (s_meter->get_current() + 9) / 10 : 0;
            int stationCount = 0;
            ChargingStation *stations = SelectAllChargeStation(&stationCount);

            if(stations != NULL && stationCount > 0)
            {
                dPrint(DEBUG, "检测到%d个充电桩，开始负载均衡处理\n", stationCount);
                ProcessAllStations(stations, stationCount);
            }
            else
            {
                dPrint(DEBUG, "当前没有充电桩在线\n");
            }
        }
task_sleep:
        vTaskDelay(pdMS_TO_TICKS(100));  // 每次延时1ms
        taskYIELD();  // 主动让出CPU，给低优先级任务（包括IDLE）运行机会

    }

}

void LoadBalance_TimerFunc(TIMER_ID timerId, char *, int)
{
    LOAD_BALANCE_CAL = 1;
    AddTimer(TIMER_ID_LOADBALANCE, 1300, LoadBalance_TimerFunc);
}

/********************************************************
*@Function name:ProcessAllStations
*@Description:统一处理所有充电桩的负载均衡（不区分单桩或双桩）
*@input param:stations 充电桩数组
*@input param:stationCount 充电桩数量
*@Return:
********************************************************************************/
void ProcessAllStations(ChargingStation *stations, int stationCount)
{
    char jsonData[256] = {0};

    if (stations == NULL || stationCount <= 0 || stationCount > CHAEGING_STATION_MAX_NUM)
    {
        return;
    }

    // 先判断并设置优先桩（第一个进入Charging状态的桩）
    if (priorityStationMac[0] == '\0')
    {
        // 还没有优先桩，找第一个Charging状态的桩
        for (int i = 0; i < stationCount; i++)
        {
            if (stations[i].enumStatus == Charging)
            {
                strcpy(priorityStationMac, stations[i].mac);
                dPrint(INFO, "设置优先桩: %s (第一个进入Charging状态)\n", priorityStationMac);
                break;
            }
        }
    }
    else
    {
        // 检查优先桩是否还在充电，如果不在了就清除
        bool priorityStillCharging = false;
        for (int i = 0; i < stationCount; i++)
        {
            if (strcmp(stations[i].mac, priorityStationMac) == 0 && IsStationCharging(&stations[i]))
            {
                priorityStillCharging = true;
                break;
            }
        }
        if (!priorityStillCharging)
        {
            dPrint(INFO, "优先桩 %s 已停止充电，清除优先权\n", priorityStationMac);
            memset(priorityStationMac, 0, sizeof(priorityStationMac));
        }
    }

    // 统计正在充电的充电桩
    int chargingCount = 0;
    int chargingIndexes[CHAEGING_STATION_MAX_NUM] = {-1, -1};
    uint16_t currentChargingCurrents[CHAEGING_STATION_MAX_NUM] = {0};
    int priorityStationIdx = -1; // 优先桩在chargingIndexes中的索引

    for (int i = 0; i < stationCount; i++)
    {
        if (IsStationCharging(&stations[i]))
        {
            chargingIndexes[chargingCount] = i;
            currentChargingCurrents[chargingCount] = RoundCurrent(stations[i].acCurrentL1);

            // 更新稳定电流跟踪器（传入实际电流和上次下发的限制电流）
            uint16_t lastLimitA = stations[i].limitCurrent / 100; // 转换为A
            UpdateStableCurrentTracker(&stableTrackers[i], currentChargingCurrents[chargingCount], lastLimitA);

            // 记录优先桩的索引
            if (priorityStationMac[0] != '\0' && strcmp(stations[i].mac, priorityStationMac) == 0)
            {
                priorityStationIdx = chargingCount;
            }

            chargingCount++;
        }
        else
        {
            // 不在充电的桩，重置其稳定电流跟踪器
            ResetStableCurrentTracker(i);
        }
    }

    if (chargingCount == 0)
    {
        dPrint(DEBUG, "没有充电桩在充电\n");
        return;
    }

    if (priorityStationIdx >= 0)
    {
        dPrint(INFO, "有%d个充电桩正在充电，优先桩: %s\n", chargingCount, priorityStationMac);
    }
    else
    {
        dPrint(INFO, "有%d个充电桩正在充电\n", chargingCount);
    }

    // 计算总的充电桩电流（从电表读数中要减去的）
    uint16_t totalStationCurrent = 0;
    for (int i = 0; i < chargingCount; i++)
    {
        totalStationCurrent += currentChargingCurrents[i];
    }

    // 计算总可用电流
    int16_t totalAvail = InflowCurrent - (MeterCurrVlaue - totalStationCurrent);

    dPrint(INFO, "总可用电流: %d A, 入户电流: %d A, 电表电流: %d A, 充电桩总电流: %d A\n",
           totalAvail, InflowCurrent, MeterCurrVlaue, totalStationCurrent);

    // 检查是否有充电桩电流超标
    for (int i = 0; i < chargingCount; i++)
    {
        int idx = chargingIndexes[i];
        int stationCurrentL = (stations[idx].enumStatus == SuspendEvse) ?
                              currentChargingCurrents[i] :
                              ((stations[idx].acCurrentL1 == 0) ? stations[idx].maxlimitCurrent : currentChargingCurrents[i]);

        if (stationCurrentL > InflowCurrent)
        {
            dPrint(WARN, "充电桩%d电流(%d A)超过入户电流，进入紧急限流\n", idx+1, stationCurrentL);
        }
    }

    // 如果总可用电流为负数或严重超标，紧急限流处理
    if (totalAvail < 0 || totalStationCurrent > InflowCurrent)
    {
        dPrint(WARN, "电流严重超标，紧急平均分配入户电流\n");
        int16_t emergencyCurrent = InflowCurrent / chargingCount;

        snprintf(jsonData, sizeof(jsonData), "{\"content\":\"电流严重超标，紧急限流\",\"value\":\"%d\"}", emergencyCurrent);
        PublishEvent(EVENT_AUTO_CONTROL_MONITOR, jsonData, strlen(jsonData));

        for (int i = 0; i < chargingCount; i++)
        {
            int idx = chargingIndexes[i];
            if (stations[idx].enumStatus == SuspendEvse)
            {
                dPrint(INFO, "充电桩%d处于暂停状态，先发布启动事件\n", idx+1);
                PublishEvent(EVENT_AUTO_START, (char*)&stations[idx], sizeof(ChargingStation));
            }
            stations[idx].limitCurrent = emergencyCurrent * 100;
            dPrint(INFO, "充电桩%d紧急限流: %d A\n", idx+1, emergencyCurrent);
            PublishEvent(EVENT_AUTO_SET_LIMIT_CUUR, (char*)&stations[idx], sizeof(ChargingStation));
        }
        return;
    }

    // 检查充电桩电流是否大于电表电流（数据异常，强制相等）
    if (totalStationCurrent > MeterCurrVlaue)
    {
        dPrint(WARN, "充电桩总电流(%d A)大于电表电流(%d A)，强制使用电表电流值\n",
               totalStationCurrent, MeterCurrVlaue);
        totalStationCurrent = MeterCurrVlaue;
        totalAvail = InflowCurrent;
        dPrint(INFO, "修正后总可用电流: %d A (以入户电流为准)\n", totalAvail);
    }

    // 新的分配逻辑：先给每个桩分配最小电流，剩余再平均分配
    int16_t minRequiredCurrent = EV_MIN_CURRENT * chargingCount;

    if (totalAvail < minRequiredCurrent)
    {
        // 总电流不足以满足所有桩的最小需求
        if (totalAvail >= EV_MIN_CURRENT)
        {
            // 至少够一个桩用，选择优先桩（第一个进入Charging的桩）
            int selectedIdx = (priorityStationIdx >= 0) ? priorityStationIdx : 0; // 优先桩，或默认第一个

            int selectedStationIdx = chargingIndexes[selectedIdx];
            dPrint(WARN, "总电流不足以满足所有桩最小需求，仅启动优先桩: %s\n",
                   stations[selectedStationIdx].mac);
            snprintf(jsonData, sizeof(jsonData), "{\"content\":\"总电流不足，优先先充电的桩\",\"value\":\"%d\"}", totalAvail);
            PublishEvent(EVENT_AUTO_CONTROL_MONITOR, jsonData, strlen(jsonData));

            for (int i = 0; i < chargingCount; i++)
            {
                int idx = chargingIndexes[i];
                if (i == selectedIdx)
                {
                    // 选中的桩获得全部电流
                    int16_t allocCurrent = (totalAvail > stations[idx].maxlimitCurrent) ?
                                           stations[idx].maxlimitCurrent : totalAvail;
                    stations[idx].limitCurrent = allocCurrent * 100;

                    if (stations[idx].enumStatus == SuspendEvse)
                    {
                        dPrint(INFO, "充电桩 %s 处于暂停状态，先发布启动事件\n", stations[idx].mac);
                        PublishEvent(EVENT_AUTO_START, (char*)&stations[idx], sizeof(ChargingStation));
                    }
                    dPrint(INFO, "充电桩 %s 分配全部电流: %d A\n", stations[idx].mac, allocCurrent);
                    PublishEvent(EVENT_AUTO_SET_LIMIT_CUUR, (char*)&stations[idx], sizeof(ChargingStation));
                }
                else
                {
                    // 其他桩暂停
                    stations[idx].limitCurrent = 0;
                    dPrint(WARN, "充电桩 %s 暂停充电\n", stations[idx].mac);
                    PublishEvent(EVENT_AUTO_SET_LIMIT_CUUR, (char*)&stations[idx], sizeof(ChargingStation));
                    PublishEvent(EVENT_AUTO_SUSPEND, (char*)&stations[idx], sizeof(ChargingStation));
                }
            }
        }
        else
        {
            // 所有桩都暂停
            dPrint(WARN, "总电流不足，所有充电桩暂停\n");
            snprintf(jsonData, sizeof(jsonData), "{\"content\":\"总电流不足，所有桩暂停\",\"value\":\"%d\"}", totalAvail);
            PublishEvent(EVENT_AUTO_CONTROL_MONITOR, jsonData, strlen(jsonData));

            for (int i = 0; i < chargingCount; i++)
            {
                int idx = chargingIndexes[i];
                stations[idx].limitCurrent = 0;
                dPrint(WARN, "充电桩%d设置电流为0并暂停\n", idx+1);
                PublishEvent(EVENT_AUTO_SET_LIMIT_CUUR, (char*)&stations[idx], sizeof(ChargingStation));
                PublishEvent(EVENT_AUTO_SUSPEND, (char*)&stations[idx], sizeof(ChargingStation));
            }
        }
        return;
    }

    // 电流充足，执行新的分配逻辑
    // 1. 先给每个桩分配最小电流
    // 2. 剩余电流平均分配
    int16_t remainingCurrent = totalAvail - minRequiredCurrent;
    int16_t extraPerStation = remainingCurrent / chargingCount;
    int16_t baseAllocation = EV_MIN_CURRENT + extraPerStation;

    dPrint(INFO, "分配策略: 最小需求=%d A, 剩余电流=%d A, 每桩额外=%d A, 基础分配=%d A\n",
           minRequiredCurrent, remainingCurrent, extraPerStation, baseAllocation);

    // 考虑稳定电流：检查哪些桩已经达到稳定状态
    int16_t stableCurrentSum = 0;  // 稳定电流总和
    int stableCount = 0;           // 达到稳定的桩数量

    for (int i = 0; i < chargingCount; i++)
    {
        int idx = chargingIndexes[i];
        if (stableTrackers[idx].isStable)
        {
            stableCurrentSum += stableTrackers[idx].stableCurrent;
            stableCount++;
            dPrint(DEBUG, "充电桩%d已达到稳定电流: %d A\n", idx+1, stableTrackers[idx].stableCurrent);
        }
    }

    // 如果有桩达到稳定电流，计算可以让出的电流
    int16_t yieldableCurrent = 0;
    if (stableCount > 0)
    {
        // 计算稳定桩如果按基础分配会获得多少电流
        int16_t stableWouldGet = baseAllocation * stableCount;
        // 实际稳定电流总和
        // 如果稳定电流小于基础分配，可以把差值让给其他桩
        if (stableCurrentSum < stableWouldGet)
        {
            yieldableCurrent = stableWouldGet - stableCurrentSum;
            dPrint(INFO, "稳定桩可让出电流: %d A (稳定总和=%d A, 基础分配总和=%d A)\n",
                   yieldableCurrent, stableCurrentSum, stableWouldGet);
        }
    }

    // 为每个充电桩分配电流
    for (int i = 0; i < chargingCount; i++)
    {
        int idx = chargingIndexes[i];
        uint16_t oldLimitA = stations[idx].limitCurrent / 100;
        uint16_t newLimitA;

        // 无论桩是否稳定，都下发平均分配的电流（这样稳定值取消后可以继续上调）
        newLimitA = baseAllocation;

        // 如果有可让出的电流，分配给未稳定的桩
        if (yieldableCurrent > 0 && !stableTrackers[idx].isStable)
        {
            int unstableCount = chargingCount - stableCount;
            if (unstableCount > 0)
            {
                int16_t bonus = yieldableCurrent / unstableCount;
                newLimitA += bonus;
                dPrint(DEBUG, "充电桩%d获得额外电流: %d A\n", idx+1, bonus);
            }
        }

        // 限制在充电桩最大电流范围内
        if (newLimitA > stations[idx].maxlimitCurrent)
        {
            newLimitA = stations[idx].maxlimitCurrent;
        }

        stations[idx].limitCurrent = newLimitA * 100;

        // 死区判断：避免频繁抖动
        int diff = abs((int)newLimitA - (int)oldLimitA);
        bool isOverload = (MeterCurrVlaue >= InflowCurrent);

        if (!isOverload && diff < MIN_ADJUST_THRESHOLD)
        {
            dPrint(DEBUG, "充电桩%d: 未过载且差异<%dA，跳过调整 (%dA→%dA)\n",
                   idx+1, MIN_ADJUST_THRESHOLD, oldLimitA, newLimitA);
            continue;
        }

        dPrint(INFO, "充电桩%d分配电流: %d A (稳定=%s, 稳定值=%d A)\n",
               idx+1, newLimitA,
               stableTrackers[idx].isStable ? "是" : "否",
               stableTrackers[idx].isStable ? stableTrackers[idx].stableCurrent : 0);

        // 如果充电桩处于暂停状态，先启动
        if (stations[idx].enumStatus == SuspendEvse)
        {
            dPrint(INFO, "充电桩%d处于暂停状态，先发布启动事件\n", idx+1);
            PublishEvent(EVENT_AUTO_START, (char*)&stations[idx], sizeof(ChargingStation));
        }

        PublishEvent(EVENT_AUTO_SET_LIMIT_CUUR, (char*)&stations[idx], sizeof(ChargingStation));
    }

    snprintf(jsonData, sizeof(jsonData), "{\"content\":\"完成%d个桩的负载均衡分配\",\"value\":\"%d\"}",
             chargingCount, baseAllocation);
    PublishEvent(EVENT_AUTO_CONTROL_MONITOR, jsonData, strlen(jsonData));
}

/********************************************************
*@Function name:ProcessSingleStation
*@Description:处理单个充电桩的负载均衡
*@input param:station 充电桩对象
*@Return:
********************************************************************************/
void ProcessSingleStation(ChargingStation *station)
{
    char jsonData[256] = {0};
    // 检查充电桩是否正在充电
    if (IsStationCharging(station))
    {
        dPrint(INFO, "充电桩正在充电，进行负载均衡控制\n");
        int16_t Avail = 0;
        //uint16_t Avail = InflowCurrent - (MeterCurrVlaue - RoundCurrent(station->acCurrentL1));
        //先获取充电桩1和充电桩2的电流值
        int station1CurrentL1 = 0;
        //如果是暂停状态就按照正常获取到的电流值，否则要把0转换为maxlimitCurrent
        if(station->enumStatus == SuspendEvse)
        {
            station1CurrentL1 = RoundCurrent(station->acCurrentL1);
        }
        else
        {
            station1CurrentL1 = (station->acCurrentL1 == 0)?station->maxlimitCurrent:RoundCurrent(station->acCurrentL1);
        }
        Avail = InflowCurrent - (MeterCurrVlaue - RoundCurrent(station->acCurrentL1));
        dPrint(INFO, "当前设备可用电流: %d A\n, 入户最大电流: %d A\n, 电表电流: %d A\n, 充电桩电流: %d A\n, 充电桩计算后电流: %d A\n", Avail, InflowCurrent, MeterCurrVlaue, station->acCurrentL1, RoundCurrent(station->acCurrentL1));

        //如果其中一个充电桩的电流比入户电流还要大,说明严重超标，或者计算出来的是负数
        if(station1CurrentL1 > InflowCurrent || Avail < 0)
        {
            dPrint(INFO,"进入严重超标逻辑\n");
            // 如果充电桩处于暂停状态，需要先发布启动事件
            if (station->enumStatus == SuspendEvse)
            {
                 dPrint(INFO, "充电桩处于暂停状态，先发布启动事件\n");
                PublishEvent(EVENT_AUTO_START, (char*)station, sizeof(ChargingStation));
            }
            station->limitCurrent = InflowCurrent*100;
            dPrint(INFO, "设置限制电流: %d A\n", station->limitCurrent/100);
            PublishEvent(EVENT_AUTO_SET_LIMIT_CUUR,(char *)station, sizeof(ChargingStation));//发布限制电流事件
            return;
        }
 
        if(RoundCurrent(station->acCurrentL1) > MeterCurrVlaue)
        {
            //充电桩的电流>电表电流,说明电表数据是错误的，不用处理
            dPrint(DERROR,"充电桩电流比电表数值还大，不正确的电表值，不用处理\n");
            return;
        }

        // 保存当前限制电流（用于死区判断）
        uint16_t oldLimitA = station->limitCurrent / 100;

        // 限制在合理范围内
        if (Avail < EV_MIN_CURRENT)
        {
            station->limitCurrent = 0;
            dPrint(WARN, "可用电流不足,设置为0 A\n");
            //发布自动化监控控制过程调试消息
            snprintf(jsonData,sizeof(jsonData),"%s%s%s%s%s",
				"{\"content\":\"","可用电流不足,设置为0A",
				"\",\"value\":\"","0",
				"\"}");
            PublishEvent(EVENT_AUTO_CONTROL_MONITOR,jsonData,strlen(jsonData));
            // 先发送设置电流为0的事件，再发送暂停事件
            PublishEvent(EVENT_AUTO_SET_LIMIT_CUUR, (char*)station, sizeof(ChargingStation));
            PublishEvent(EVENT_AUTO_SUSPEND, (char*)station, sizeof(ChargingStation));
        }
        else if (Avail > station->maxlimitCurrent)
        {
            station->limitCurrent = station->maxlimitCurrent * 100;
            dPrint(INFO, "限制电流不超过充电桩最大限制电流: %d A\n", Avail);
            //发布自动化监控控制过程调试消息
            snprintf(jsonData,sizeof(jsonData),"%s%s%s%d%s",
				"{\"content\":\"","可限制电流不超过充电桩最大限制电流Avail",
				"\",\"value\":\"",Avail,
				"\"}");
            PublishEvent(EVENT_AUTO_CONTROL_MONITOR,jsonData,strlen(jsonData));
        }
        else
        {
            station->limitCurrent = Avail * 100;
        }

        // ========== 死区判断：避免频繁抖动 ==========
        uint16_t newLimitA = station->limitCurrent / 100;       // 新计算的限制电流（A）
        bool isOverload = (MeterCurrVlaue >= InflowCurrent);     // 是否过载
        int diff = abs((int)newLimitA - (int)oldLimitA);        // 电流差异

        // 如果未过载 且 差异小于阈值，则不调整
        if (!isOverload && diff < MIN_ADJUST_THRESHOLD)
        {
            dPrint(DEBUG, "未过载且差异小于%dA，跳过调整 (当前:%dA, 计算:%dA, 差异:%dA)\n",
                   MIN_ADJUST_THRESHOLD, oldLimitA, newLimitA, diff);
            return;
        }

        dPrint(INFO, "执行调整: %s, 差异:%dA, %dA → %dA\n",
               isOverload ? "过载" : "正常", diff, oldLimitA, newLimitA);

        // 如果充电桩处于暂停状态，需要先发布启动事件
        if (station->enumStatus == SuspendEvse)
        {
            dPrint(INFO, "充电桩处于暂停状态，先发布启动事件\n");
            PublishEvent(EVENT_AUTO_START, (char*)station, sizeof(ChargingStation));
            //发布自动化监控控制过程调试消息
            snprintf(jsonData,sizeof(jsonData),"%s%s%s%s%s",
				"{\"content\":\"","充电桩处于暂停状态，先发布启动事件",
				"\",\"value\":\"","0",
				"\"}");
            PublishEvent(EVENT_AUTO_CONTROL_MONITOR,jsonData,strlen(jsonData));    
        }
        dPrint(INFO, "设置限制电流: %d A\n", station->limitCurrent);
        PublishEvent(EVENT_AUTO_SET_LIMIT_CUUR, (char*)station, sizeof(ChargingStation));//发布限制电流事件
        //发布自动化监控控制过程调试消息
        snprintf(jsonData,sizeof(jsonData),"%s%s%s%d%s",
				"{\"content\":\"","设置限制电流:",
				"\",\"value\":\"",station->limitCurrent,
				"\"}");
         PublishEvent(EVENT_AUTO_CONTROL_MONITOR,jsonData,strlen(jsonData));    
    }
    else
    {
        dPrint(DEBUG, "充电桩状态: %s, 不满足充电或负载均衡条件\n", station->EVStatus);
    }
}

/********************************************************
*@Function name:ProcessTwoStations
*@Description:处理两个充电桩的负载均衡
*@input param:stations 充电桩数组
*@input param:stationCount 充电桩数量
*@Return:
********************************************************************************/
void ProcessTwoStations(ChargingStation *stations, int stationCount)
{
    char jsonData[256] = {0};
    // 检查两个桩的充电状态
    bool station1Charging = IsStationCharging(&stations[0]);
    bool station2Charging = IsStationCharging(&stations[1]);

    if (station1Charging && station2Charging)
    {
        // 两个桩都在充电，需要平均分配可用电流
        dPrint(INFO, "两个充电桩都在充电，平均分配电流\n");
        //发布自动化监控控制过程调试消息
        snprintf(jsonData,sizeof(jsonData),"%s%s%s%s%s",
				"{\"content\":\"","两个充电桩都在充电，平均分配电流",
				"\",\"value\":\"","0",
				"\"}");
        PublishEvent(EVENT_AUTO_CONTROL_MONITOR,jsonData,strlen(jsonData));    

        // 计算总可用电流
        int16_t totalAvail = InflowCurrent - (MeterCurrVlaue - RoundCurrent(stations[0].acCurrentL1) - RoundCurrent(stations[1].acCurrentL1));
        // 平均分配电流
        int16_t avgCurrent = totalAvail / 2;
        dPrint(INFO, "当前设备可用电流: %d A\n, 入户最大电流: %d A\n, 电表电流: %d A\n, 充电桩1电流: %d A\n, 充电桩2电流: %d A\n, 平均分配后的电流: %d A\n", totalAvail, InflowCurrent, MeterCurrVlaue, RoundCurrent(stations[0].acCurrentL1), RoundCurrent(stations[1].acCurrentL1), avgCurrent);
        int station1CurrentL = 0;
        int station2CurrentL = 0;
        //如果是暂停状态就用获取到的电流值，否则如果是0，就用最大值32
        if(stations[0].enumStatus == SuspendEvse)
        {
            station1CurrentL = RoundCurrent(stations[0].acCurrentL1);
        }
        else
        {
            station1CurrentL = (stations[0].acCurrentL1 == 0)?stations[0].maxlimitCurrent:RoundCurrent(stations[0].acCurrentL1);
        }

        if(stations[1].enumStatus == SuspendEvse)
        {
            station2CurrentL = RoundCurrent(stations[1].acCurrentL1);
        }
        else
        {
            station2CurrentL = (stations[1].acCurrentL1 == 0)?stations[1].maxlimitCurrent:RoundCurrent(stations[1].acCurrentL1);
        }
        if(station1CurrentL > InflowCurrent 
            || station2CurrentL > InflowCurrent 
            || (station1CurrentL + station2CurrentL) > InflowCurrent
            || totalAvail < 0
          )
        {
            //每一个都平均分配
            for(int i = 0;i<CHAEGING_STATION_MAX_NUM;i++)
            {
                dPrint(INFO,"进入严重超标逻辑\n");
                // 如果充电桩处于暂停状态，需要先发布启动事件
                if (stations[i].enumStatus == SuspendEvse)
                {
                    PublishEvent(EVENT_AUTO_START, (char*)&stations[i], sizeof(ChargingStation));
                }
                stations[i].limitCurrent = (InflowCurrent/CHAEGING_STATION_MAX_NUM)*100;
                dPrint(INFO, "i:%d,num:%d,mac:%s设置限制电流: %d A\n",i,CHAEGING_STATION_MAX_NUM,stations[i].mac,stations[i].limitCurrent/100);
                PublishEvent(EVENT_AUTO_SET_LIMIT_CUUR,(char *)&stations[i], sizeof(ChargingStation));//发布限制电流事件
            }
            
            return;
        }

        if(RoundCurrent(stations[0].acCurrentL1) + RoundCurrent(stations[1].acCurrentL1) > MeterCurrVlaue)
        {
            //充电桩的电流>电表电流,说明电表数据是错误的，不用处理
            dPrint(DERROR,"充电桩电流比电表数值还大，不正确的电表值，不用处理\n");
            return;
        }
        // 判断是否可以平均分配（带滞回机制，避免临界值附近频繁切换）
        // 滞回逻辑：从单桩切换到双桩需要更高的阈值，从双桩切换到单桩使用原阈值
        int16_t threshold = EV_MIN_CURRENT;
        if (!lastModeDualCharging)
        {
            // 上一次是单桩充电模式，要切换到双桩需要额外余量
            threshold = EV_MIN_CURRENT + MODE_SWITCH_HYSTERESIS;
        }

        if (avgCurrent >= threshold)
        {
            // 平均电流足够，两个桩都可以充电
            lastModeDualCharging = true; // 更新模式状态
            dPrint(INFO, "平均电流充足(%d A >= %d A)，两个充电桩平均分配\n", avgCurrent, threshold);
            snprintf(jsonData,sizeof(jsonData),"%s%s%s%d%s",
                    "{\"content\":\"","两个充电桩平均分配电流",
                    "\",\"value\":\"",avgCurrent,
                    "\"}");
            PublishEvent(EVENT_AUTO_CONTROL_MONITOR,jsonData,strlen(jsonData));

            // 循环处理两个充电桩
            for (int i = 0; i < 2; i++)
            {
                // 保存旧电流值
                uint16_t oldLimitA = stations[i].limitCurrent / 100;

                // 计算新电流值
                if (avgCurrent > stations[i].maxlimitCurrent)
                {
                    stations[i].limitCurrent = stations[i].maxlimitCurrent * 100;
                }
                else
                {
                    stations[i].limitCurrent = avgCurrent * 100;
                }

                uint16_t newLimitA = stations[i].limitCurrent / 100;
                int diff = abs((int)newLimitA - (int)oldLimitA);
                bool isOverload = (MeterCurrVlaue > InflowCurrent);

                // 死区判断：未过载且差异小于阈值，跳过调整
                if (!isOverload && diff < MIN_ADJUST_THRESHOLD)
                {
                    dPrint(DEBUG, "充电桩%d: 未过载且差异<%dA，跳过 (%dA→%dA)\n",
                           i + 1, MIN_ADJUST_THRESHOLD, oldLimitA, newLimitA);
                    continue;
                }

                dPrint(INFO, "充电桩%d分配电流: %s, 差异:%dA, %dA → %dA\n",
                       i + 1, isOverload ? "过载" : "正常", diff, oldLimitA, newLimitA);

                // 如果充电桩处于暂停状态，需要先发布启动事件
                if (stations[i].enumStatus == SuspendEvse)
                {
                    dPrint(INFO, "充电桩%d处于暂停状态，先发布启动事件\n", i + 1);
                    PublishEvent(EVENT_AUTO_START, (char*)&stations[i], sizeof(ChargingStation));
                }
                PublishEvent(EVENT_AUTO_SET_LIMIT_CUUR, (char*)&stations[i], sizeof(ChargingStation));
            }
        }
        else if (totalAvail >= EV_MIN_CURRENT)
        {
            // 平均不够，但总电流足够一台使用，固定选择第一个充电桩充电，另一台暂停
            lastModeDualCharging = false; // 更新模式状态为单桩充电
            uint8_t selectedStation = 0;  // 固定选择第一个充电桩

            dPrint(WARN, "平均电流不足(%d A < %d A)，但总电流充足(%d A)，选择充电桩%d充电\n",
                   avgCurrent, threshold, totalAvail, selectedStation + 1);
            snprintf(jsonData,sizeof(jsonData),"%s%s%d%s%s%d%s",
                    "{\"content\":\"","平均电流不足，选择充电桩",selectedStation + 1,"充电",
                    "\",\"value\":\"",totalAvail,
                    "\"}");
            PublishEvent(EVENT_AUTO_CONTROL_MONITOR,jsonData,strlen(jsonData));

            for (int i = 0; i < 2; i++)
            {
                // 保存旧电流值
                uint16_t oldLimitA = stations[i].limitCurrent / 100;

                if (i == selectedStation)
                {
                    // 被选中的充电桩使用全部电流
                    if (totalAvail > stations[i].maxlimitCurrent)
                    {
                        stations[i].limitCurrent = stations[i].maxlimitCurrent * 100;
                    }
                    else
                    {
                        stations[i].limitCurrent = totalAvail * 100;
                    }

                    uint16_t newLimitA = stations[i].limitCurrent / 100;
                    int diff = abs((int)newLimitA - (int)oldLimitA);
                    bool isOverload = (MeterCurrVlaue > InflowCurrent);

                    // 死区判断
                    if (!isOverload && diff < MIN_ADJUST_THRESHOLD)
                    {
                        dPrint(DEBUG, "充电桩%d: 未过载且差异<%dA，跳过 (%dA→%dA)\n",
                               i + 1, MIN_ADJUST_THRESHOLD, oldLimitA, newLimitA);
                        continue;
                    }

                    dPrint(INFO, "充电桩%d分配全部电流: %s, 差异:%dA, %dA → %dA\n",
                           i + 1, isOverload ? "过载" : "正常", diff, oldLimitA, newLimitA);

                    // 如果充电桩处于暂停状态，需要先发布启动事件
                    if (stations[i].enumStatus == SuspendEvse)
                    {
                        PublishEvent(EVENT_AUTO_START, (char*)&stations[i], sizeof(ChargingStation));
                    }
                    PublishEvent(EVENT_AUTO_SET_LIMIT_CUUR, (char*)&stations[i], sizeof(ChargingStation));
                }
                else
                {
                    // 未被选中的充电桩暂停充电
                    stations[i].limitCurrent = 0;
                    dPrint(WARN, "充电桩%d暂停充电\n", i + 1);
                    // 先发送设置电流为0的事件，再发送暂停事件
                    PublishEvent(EVENT_AUTO_SET_LIMIT_CUUR, (char*)&stations[i], sizeof(ChargingStation));
                    PublishEvent(EVENT_AUTO_SUSPEND, (char*)&stations[i], sizeof(ChargingStation));
                }
            }
        }
        else
        {
            // 总电流也不足，两个桩都暂停
            lastModeDualCharging = false; // 更新模式状态
            dPrint(WARN, "总电流不足(%d A < %d A)，两个充电桩都暂停\n", totalAvail, EV_MIN_CURRENT);
            snprintf(jsonData,sizeof(jsonData),"%s%s%s%d%s",
                    "{\"content\":\"","总电流不足，两个充电桩都暂停",
                    "\",\"value\":\"",totalAvail,
                    "\"}");
            PublishEvent(EVENT_AUTO_CONTROL_MONITOR,jsonData,strlen(jsonData));

            // 循环处理两个充电桩
            for (int i = 0; i < 2; i++)
            {
                stations[i].limitCurrent = 0;
                dPrint(WARN, "充电桩%d暂停充电\n", i + 1);
                // 先发送设置电流为0的事件，再发送暂停事件
                PublishEvent(EVENT_AUTO_SET_LIMIT_CUUR, (char*)&stations[i], sizeof(ChargingStation));
                PublishEvent(EVENT_AUTO_SUSPEND, (char*)&stations[i], sizeof(ChargingStation));
            }
        }
    }
    else if (station1Charging)
    {
        // 只有第一个桩在充电
        lastModeDualCharging = false; // 更新模式状态
        dPrint(INFO, "只有充电桩1在充电，按单桩处理\n");
        ProcessSingleStation(&stations[0]);
        snprintf(jsonData,sizeof(jsonData),"%s%s%s%s%s",
                        "{\"content\":\"","只有充电桩1在充电，按单桩处理:",
                        "\",\"value\":\"","0",
                        "\"}");
        PublishEvent(EVENT_AUTO_CONTROL_MONITOR,jsonData,strlen(jsonData));
    }
    else if (station2Charging)
    {
        // 只有第二个桩在充电
        lastModeDualCharging = false; // 更新模式状态
        dPrint(INFO, "只有充电桩2在充电，按单桩处理\n");
        ProcessSingleStation(&stations[1]);
        snprintf(jsonData,sizeof(jsonData),"%s%s%s%s%s",
                        "{\"content\":\"","只有充电桩2在充电，按单桩处理:",
                        "\",\"value\":\"","0",
                        "\"}");
        PublishEvent(EVENT_AUTO_CONTROL_MONITOR,jsonData,strlen(jsonData));
    }
    else
    {
        lastModeDualCharging = false; // 更新模式状态
        dPrint(DEBUG, "两个充电桩都未在充电，无需负载均衡\n");
        snprintf(jsonData,sizeof(jsonData),"%s%s%s%s%s",
                        "{\"content\":\"","两个充电桩都未在充电，无需负载均衡:",
                        "\",\"value\":\"","0",
                        "\"}");
        PublishEvent(EVENT_AUTO_CONTROL_MONITOR,jsonData,strlen(jsonData));    
    }
}

/*********************************************************
*@Function name:RoundCurrent
*@Description:将0.1A单位的电流值四舍五入到整数安培
*@input param:current 电流值，单位0.1A (如321表示32.1A)
*@Return:四舍五入后的整数安培值
*@Note:
*********************************************************/
uint16_t RoundCurrent(uint16_t current)
{
    return (current + 10) / 100;
}

#ifdef UNIT_TEST
/**
 * @brief Test helper: set static vars that AutoControl_task normally sets
 *        before calling ProcessAllStations().
 * @param inflow  Household inflow current in Amperes (same unit as GPIOManager returns)
 * @param meter_curr_01A  Meter current in 0.1A units (same as s_meter->get_current())
 */
void alloc_ctrl_test_set_vars(uint16_t inflow, uint16_t meter_curr_01A)
{
    /* InflowCurrent and MeterCurrVlaue are static in this file — accessible directly */
    InflowCurrent  = inflow;
    MeterCurrVlaue = (meter_curr_01A + 9) / 10;  /* matches AutoControl_task rounding */
}

/**
 * @brief Test helper: run freshness check + ProcessAllStations in one call,
 *        mimicking AutoControl_task critical path.
 */
void alloc_ctrl_test_run_cycle(ChargingStation *stations, int count)
{
    /* Freshness gate (same as AutoControl_task) */
    if (s_meter != NULL) {
        MeterDataFreshness_t freshness = meter_check_freshness(s_meter);
        if (freshness == METER_DATA_INVALID) {
            emergency_suspend_all_stations();
            return;
        }
        if (freshness == METER_DATA_STALE) {
            emergency_reduce_to_min_current();
            return;
        }
    }
    ProcessAllStations(stations, count);
}
#endif /* UNIT_TEST */

