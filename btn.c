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
#include "btn.h"

pthread_cond_t fakeCond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t fakeMutex = PTHREAD_MUTEX_INITIALIZER;

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
void btn_risingInterrupt() {
	if (digitalRead(GPIO_BTN_PLAY)==HIGH || digitalRead(GPIO_BTN_STOP)==HIGH) {
		pthread_cond_signal(&fakeCond);
	}
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
// pressed
// wait for double click, 100 ms
// perform action
//
void* threadPlayBtn( void *thread_arg ){
	int bPlay_cnt_clicks, bPlay_cnt_highs, bPlay_cnt_lows;
	int bStop_cnt_clicks, bStop_cnt_highs, bStop_cnt_lows;

	bPlay_cnt_clicks = bPlay_cnt_highs = bPlay_cnt_lows = 0;
	bStop_cnt_clicks = bStop_cnt_highs = bStop_cnt_lows = 0;

	if (DEBUG)
		fprintf (stderr, "DEBUG: [btn-thread] -starting\n");

	// SETUP
	pinMode(GPIO_BTN_PLAY, INPUT);   //make "GPIO 2" input  play button
	pullUpDnControl(GPIO_BTN_PLAY, PUD_DOWN);
	if (wiringPiISR (GPIO_BTN_PLAY, INT_EDGE_RISING, &btn_risingInterrupt) < 0) {
		mgr_setExitStatus();
		fprintf (stderr, "FATAL: HW: Unable to setup ISR: %s\n", strerror (errno));
		return thread_arg;
	}

	pinMode(GPIO_BTN_STOP, INPUT);   //make "GPIO 3" input  stop button
	pullUpDnControl(GPIO_BTN_STOP, PUD_DOWN);
	if (wiringPiISR (GPIO_BTN_STOP, INT_EDGE_RISING, &btn_risingInterrupt) < 0) {
		mgr_setExitStatus();
		fprintf (stderr, "FATAL: HW: Unable to setup ISR: %s\n", strerror (errno));
		return thread_arg;
	}

	mgr_unsetWarmingUp();

	if (DEBUG)
		fprintf (stderr, "DEBUG: [btn-thread] +started\n");

	while (!mgr_isExitStatus()) {

		if (mgr_isWarmingUp()) {
			delay(500);
			continue;
		}

		/////////////////////////////////////////////////////////////////
		// PLAY BUTTON
		/////////////////////////////////////////////////////////////////

		if (digitalRead(GPIO_BTN_PLAY)==HIGH) {
			bPlay_cnt_highs++;
			bPlay_cnt_lows = 0;

		} else {
			if (bPlay_cnt_highs> 2)
				bPlay_cnt_clicks++;

			if (bPlay_cnt_clicks) {
				bPlay_cnt_lows++;

				if (bPlay_cnt_lows > 20) { // bouncetime protection

					if (mgr_isStartPlay() || (mgr_mpd_isPlaying() && !mgr_isStopPlay())) {
						if (bPlay_cnt_clicks == 1) {
							if (DEBUG)
								fprintf(stderr, "DEBUG: HW: <skip> pressed: skip track\n");
							mgr_setSkipFile();
						} else {
							if (DEBUG)
								fprintf(stderr, "DEBUGL: HD: <skip> double pressed: switch player mode\n");
							mgr_setSkipFolder();
						}
					} else {
						if (DEBUG)
							fprintf(stderr, "DEBUG: HW: <skip> pressed: start playing\n");
						mgr_setStartPlay();
					}

					bPlay_cnt_clicks = 0;
					bPlay_cnt_lows = 0;
				}
			}

			bPlay_cnt_highs = 0;
		}

		/////////////////////////////////////////////////////////////////
		// STOP BUTTON

		if (digitalRead(GPIO_BTN_STOP)==HIGH) {
			bStop_cnt_highs++;
			bStop_cnt_lows = 0;

		} else {
			if (bStop_cnt_highs> 2)
				bStop_cnt_clicks++;

			if (bStop_cnt_clicks) {
				bStop_cnt_lows++;

				if (bStop_cnt_lows > 20) { // bouncetime protection
					if (mgr_isStartPlay() || (mgr_mpd_isPlaying() && !mgr_isStopPlay())) {
						printf("DEBUG: HW: <skip> pressed: stop playing\n");
						mgr_setStopPlay();

					} else { // STOPPED OR PAUSED
						printf("DEBUG: HW: <skip> pressed: start playing\n");
						mgr_setStartPlay();
					}

					bStop_cnt_clicks = 0;
					bStop_cnt_lows = 0;
				}
			}

			bStop_cnt_highs = 0;
		}

		////////////////////////////////////////////////////////////////////
		// Wait 10 ms
		if (bStop_cnt_highs || bPlay_cnt_highs || bStop_cnt_lows || bPlay_cnt_lows) {
			delay(10); // bouncetime protection workaround

		} else {
			struct timespec timeToWait;
			memset(&timeToWait, 0, sizeof timeToWait);

			timeToWait.tv_sec = time(0)+5;
			timeToWait.tv_nsec = 0;

			pthread_mutex_lock(&fakeMutex);
			pthread_cond_timedwait(&fakeCond, &fakeMutex, &timeToWait);
			pthread_mutex_unlock(&fakeMutex);
		}

	}

	if (DEBUG)
		fprintf (stderr, "DEBUG: [btn-thread] Thread finished\n");

	return thread_arg;
}

