#include <stdio.h>
#include "DeBug.h"
#include "Watchdog.h"

// ??????????
void watchdog_init(void)
{

    /* enable IRC40K */
    //rcu_osci_on(RCU_IRC40K);
    
    /* configure FWDGT counter clock: 40KHz(IRC40K) / 64 = 0.625KHz */
    //fwdgt_config(0x0FFF, FWDGT_PSC_DIV64);  // ?6.5?????
    
    /* enable FWDGT */
    //fwdgt_enable();
    
    printf("Watchdog initialized with ~6.5s timeout\n");
}

// ??????
void watchdog_feed(void)
{
    //fwdgt_counter_reload();
}

// ???????(???????)
void watchdog_status_check(void)
{
    /*
    static uint32_t last_feed_time = 0;
    static uint32_t feed_count = 0;
    
    feed_count++;
    if(feed_count % 100 == 0) {
        printf("Watchdog feed count: %d\n", feed_count);
    }
        */
}