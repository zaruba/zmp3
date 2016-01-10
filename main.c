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

#include "pcm.h"
#include "btn.h"
#include "led.h"
#include "manager.h"
#include "mcp3008reader.h"

time_t ts_started;
pthread_t thManager, thPlayBtn, thLed;

void zmp3_exit( int code ) {
	fprintf(stderr, "INFO: Exiting(%d)\n", code);
	fflush(stderr) ;

	exit( code );
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
void sigintHandler(int sig_num) {
	mgr_resetWarmingUpStatus();
	mgr_setExitStatus();

	signal(SIGINT, sigintHandler);
	signal(SIGTERM, sigintHandler);

	// turn the led off
	led_writeMode( 0 );

	zmp3_exit(0);
}



int main(void) {
	signal(SIGINT, sigintHandler);

	ts_started = 0;

    	if (wiringPiSetup() < 0) {
		fprintf (stderr, "FATAL: Pi: Unable to setup wiringPi: %s\n", strerror (errno));
		zmp3_exit(1);
	}

	if (DEBUG)
		fprintf (stderr, "DEBUG: [main] Starting threads\n");

	// create threads
	pthread_create(&thManager, NULL, threadManager, NULL);
	pthread_create(&thPlayBtn, NULL, threadPlayBtn, NULL);
	pthread_create(&thLed, NULL, threadLed, NULL);

	if (DEBUG)
		fprintf (stderr, "DEBUG: [main] Threads started, joing now\n");

	pthread_join(thManager,NULL);
	pthread_join(thPlayBtn,NULL);
	pthread_join(thLed,NULL);

	if (DEBUG)
		fprintf (stderr, "DEBUG: [main] Threads finished, exiting now\n");

	zmp3_exit(0);
	return 0;
}
