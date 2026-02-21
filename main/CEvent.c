#include "CEvent.h"
#include <string.h>
#define MAX_SUPPORT_EVENT 30
CEVENT_STRUCT sg_event_struct[MAX_SUPPORT_EVENT] = {0};

int sg_count = 0;

void SubscribeEvent(EVENT_TYPE event,eventFuncProc func)
{
	if(sg_count >= MAX_SUPPORT_EVENT)
	{
			return;
	}
	sg_event_struct[sg_count].event = event;
	sg_event_struct[sg_count].func = func;
	sg_count++;
}

void PublishEvent(EVENT_TYPE event,char* data, int len)
{
	for(int i = 0;i<sg_count;i++)
	{
			if(event == sg_event_struct[i].event)
			{
				sg_event_struct[i].func(event,data,len);
			}
	}
}
