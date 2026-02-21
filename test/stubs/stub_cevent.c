#include "CEvent.h"
#include <string.h>

/* Track last published event for test assertions */
EVENT_TYPE g_last_published_event = -1;
void *g_last_published_data = NULL;

void PublishEvent(EVENT_TYPE event, char *data, int len)
{
    (void)len;
    g_last_published_event = event;
    g_last_published_data  = data;
}

void SubscribeEvent(EVENT_TYPE event, eventFuncProc func)
{
    (void)event;
    (void)func;
}
