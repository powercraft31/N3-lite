/*
 * Catch-all stubs for rarely-used dependencies of AllocationController.c
 * that are not covered by dedicated stub files.
 */

/* DeBug.h - sch_printf (used by dPrint macro) */
#include <stdarg.h>
void sch_printf(const int level, const char *proName, const char *func,
                const int line, const char *format, ...)
{
    (void)level; (void)proName; (void)func; (void)line; (void)format;
}

/* CTimer.h - AddTimer (called by AutoControlInit / LoadBalance_TimerFunc) */
#include "CTimer.h"

int AddTimer(TIMER_ID timerId, int iTmrLen, TimerFunc timerFunc)
{
    (void)timerId; (void)iTmrLen; (void)timerFunc;
    return 0;
}

int DelTimer(TIMER_ID timerId)
{
    (void)timerId;
    return 0;
}
