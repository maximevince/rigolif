#include "ds1000zlib.h"
#include <stdint.h>

extern uint32_t _sbss;      /* start address of .bss section. */
extern uint32_t _ebss;      /* end address of .bss section. */
extern uint32_t _sidata;    /* start address of .data */
extern uint32_t _sdata;     /* start address of .data */
extern uint32_t _edata;     /* end address of .data */
int main(void);

int test = 30;
int i = 2;

int PluginMain(void)
{
    for (i=0; i < test; i++) {
        if (LED_IsOn(1<<i)) {
            LED_Off(1<<i);
            Sleep(20);
            LED_On(1<<i);
        }
        else{
            LED_On(1<<i);
            Sleep(20);
            LED_Off(1<<i);
        }
    }
    PLG_FreeAndRet();
    return(0);
}

