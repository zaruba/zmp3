#ifndef _Z_LED_H_
#define _Z_LED_H_

//#define GPIO_LED        0 // GPIO 17
#define GPIO_LED        11  // GPIO 7
#define GPIO_LED2       6   // GPIO 25

#define LED_MODE_FLASH 0x0001
#define LED_MODE_DOUBLEFLASH 0x0010
#define LED_MODE_LONGFLASH 0x0100

extern void led_setMode( int led_mode );
extern void led_writeMode( int onoff );

extern void* threadLed( void *thread_arg );

#endif
