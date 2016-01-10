#ifndef _Z_BTN_H_
#define _Z_BTN_H_

#define GPIO_BTN_PLAY   5 // GPIO 24
#define GPIO_BTN_STOP   4 // GPIO 23
#define GPIO_VOLUME2    3 // GPIO 22

extern void* threadPlayBtn( void *thread_arg );

#endif
