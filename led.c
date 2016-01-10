#include <pthread.h>
#include <wiringPi.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <sys/time.h>

#include <mpd/player.h>
#include <mpd/client.h>
#include <mpd/status.h>
#include <mpd/entity.h>
#include <mpd/search.h>
#include <mpd/tag.h>
#include <mpd/message.h>

#include "manager.h"
#include "led.h"

int flag_led_flash = 0;
int flag_led_doubleflash = 0;
int flag_led_longflash = 0;

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
void led_writeMode( int mode ) {
	if (mode)
		digitalWrite(GPIO_LED, 1);
	else
		digitalWrite(GPIO_LED, 0);
	return;
}

void led_setMode(int mode) {
	if (mode == LED_MODE_FLASH) {
		flag_led_flash++;

	} else if (mode == LED_MODE_DOUBLEFLASH) {
		flag_led_doubleflash++;

	} else if (mode == LED_MODE_LONGFLASH) {
		flag_led_longflash++;
	}

	return;
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
void* threadLed( void *thread_arg ) {

        if (DEBUG)
                fprintf (stderr, "DEBUG: [led-thread] -starting\n");

	// Setup LED
	pinMode(GPIO_LED, OUTPUT);  //make "GPIO 0" output
	// Turn it off
	led_writeMode( 0 );

        // request long flash 
        // led_setMode( LED_MODE_LONGFLASH );

	mgr_unsetWarmingUp();

        if (DEBUG)
                fprintf (stderr, "DEBUG: [led-thread] +started\n");

	while (!mgr_isExitStatus()) {
		int tdelay = 100; //delay 10 ms

                if (mgr_isWarmingUp()) {
			led_writeMode( 1 );
			delay(tdelay/2);
			led_writeMode( 0 );
			delay(tdelay/2);

                        continue;
                }

		if (flag_led_flash) {
			led_writeMode( 1 );
			delay(tdelay);
			led_writeMode( 0 );

			flag_led_flash = 0;
		}

		if (flag_led_doubleflash) {

			led_writeMode( 1 );
			delay(tdelay);
			led_writeMode( 0 );

			delay(tdelay/2);

			led_writeMode( 1 );
			delay(tdelay);
			led_writeMode( 0 );

			flag_led_doubleflash = 0;
		}

		if (flag_led_longflash) {
			led_writeMode( 1 );
			delay(tdelay*5);
			led_writeMode( 0 );

			flag_led_longflash = 0;
		}

		delay(100); 
	}

        if (DEBUG)
                fprintf (stderr, "DEBUG: [led-thread] Thread finished\n");

	return thread_arg;
}
