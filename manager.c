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
#include "pcm.h"
#include "led.h"
#include "mcp3008reader.h"

#define ADC_CHANNEL 0 // 0 to 7, 8 channels on the MCP3008

int cur_mpd_player_status = MPD_STATE_UNKNOWN;
int cur_mpd_volume = -1;
int cur_mpd_random = -1;

int cur_mpd_status_queue_len = 0;
int cur_mpd_stats_number_of_songs = 0;

time_t ts_started;

int flag_exit = 0;
int flag_start = 0;
int flag_skip1 = 0;
int flag_skip2 = 0;
int flag_stop = 0;

int flag_warmingup = MGR_MODULES_COUNT;

int volume_update();
struct mpd_connection *conn = NULL;

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
int mgr_isExitStatus( void ) {
	return (flag_exit > 0 ? 1 : 0);
}

void mgr_setExitStatus( void ) {
	flag_exit++;
	return;
}

int mgr_mpd_isPlaying( void ) {
	return (cur_mpd_player_status == MPD_STATE_PLAY ? 1 : 0);
}

void mgr_setSkipFile( void ) {
	ts_started = time(NULL);

	flag_skip1++;
	led_setMode( LED_MODE_FLASH );
	return;
}

void mgr_setSkipFolder( void ) {
	ts_started = time(NULL);

	flag_skip2++;
	led_setMode( LED_MODE_DOUBLEFLASH );

	return;
}

int mgr_isStopPlay( void ) {
	return (flag_stop ? 1 : 0);
}
void mgr_setStopPlay( void ) {
	flag_stop++;
	flag_start = 0;
	led_setMode( LED_MODE_FLASH );

	return;
}

int mgr_isStartPlay( void ) {
	return (flag_start ? 1 : 0);
}
void mgr_setStartPlay( void ) {
	ts_started = time(NULL);
	flag_start++;
	flag_stop = 0;
	led_setMode( LED_MODE_FLASH );

	return;
}

int mgr_isWarmingUp( void ) {
	return (flag_warmingup > 0 ? 1 : 0);
}

void mgr_unsetWarmingUp( void ) {
	if (flag_warmingup > 0)
		flag_warmingup--;
	return;
}

void mgr_resetWarmingUpStatus( void ) {
	flag_warmingup++;
	return;
}

int mgr_handle_mpd_error() {
	if (conn != NULL) {
		fprintf(stderr, "MPD ERROR: %s\n", mpd_connection_get_error_message(conn));
        	mpd_connection_free(conn);
		conn = NULL;
	} else
		fprintf(stderr, "MPD ERROR: unknown error (mgr_handle_mpd_error)\n");

        return EXIT_FAILURE;
}

/////////////////////////////////////////////////////////////////////////////////
// VOLUME
/////////////////////////////////////////////////////////////////////////////////
int volume_next_step(int target) {
	if (cur_mpd_volume < 0 || cur_mpd_volume > 100)
		return -1;

	if (cur_mpd_volume < target) {
		// GO UP
		if (target-cur_mpd_volume > 20)
			return cur_mpd_volume + 20;

		if (target-cur_mpd_volume > 5)
			return cur_mpd_volume + 5;

		return cur_mpd_volume+1;
	}

	if (cur_mpd_volume > target) {
		// GO DOWN slowly
		if (cur_mpd_volume-target > 95)
			return cur_mpd_volume - 1;

		if (cur_mpd_volume-target > 70)
			return cur_mpd_volume - 5;

		if (cur_mpd_volume-target > 50)
			return cur_mpd_volume - 15;

		return target;
	}

	return -1;
}

int volume_update() {
	int targetVolume, setVolumeTo = -1;

        if (DEBUG > 1)
                fprintf (stderr, "DEBUG: [mgr-thread] update volume\n");

	if (flag_start || flag_stop || flag_warmingup || cur_mpd_player_status == MPD_STATE_PLAY) {

		// fetch user volume
		ADC_updateVolume();
		targetVolume = ADC_getVolume();
		ADC_ResetFlags();

		if (targetVolume < 0) {
			fprintf(stderr, "ERROR: ADC WRAPPER: incorrect ADC volume received: %d\n", targetVolume);
			return 0;
		}

		if (flag_stop) 
			targetVolume = 0;

		setVolumeTo = volume_next_step(targetVolume);

	} else {
		if (cur_mpd_volume > 0)
			setVolumeTo = 0;
	}
	

	if (conn && setVolumeTo != -1) {
		if (!mpd_run_set_volume(conn, setVolumeTo)) {
			printf("DEBUG: ADC: set volume failed\n");
			return 0;
		}

		cur_mpd_volume = setVolumeTo;

		if (DEBUG)
			fprintf(stderr, "DEBUG: ADC WRAPPER: set volume to %d (target %d)\n", setVolumeTo, targetVolume);
	}


	return 1;
}

/*
 * returns: 0 - error; 1 - ok 
 */
int mqr_update_playlist() {
	struct mpd_entity *entity;
	const struct mpd_directory *dir;
	// const struct mpd_song *song;
	const char *path;

	char *toplevel_dirs[256];
	int array_sz = 0;
	int i;

	// Clear current playlist (QUEUE)
	if (!mpd_run_clear(conn)) {
		fprintf(stderr, "ERROR: MPD: Unable to clear the current playlist\n");
	} else
		if (DEBUG)
			fprintf(stderr, "DEBUG: [mgr-thread] mpd: playlist cleared\n");

	// list items & add to the playlist
	mpd_send_list_all(conn, NULL);
	while ((entity = mpd_recv_entity(conn)) != NULL) {
		path = NULL;

		switch (mpd_entity_get_type(entity)) {
			case MPD_ENTITY_TYPE_UNKNOWN:
				break;
			case MPD_ENTITY_TYPE_SONG:
				// ignore top level items
				// song = mpd_entity_get_song(entity);
				// path = mpd_song_get_uri(song);
				// if (strchr(path, '/') == NULL) {
				//	printf("FOUND SON %s\n", path);
				// }
				break;
			case MPD_ENTITY_TYPE_DIRECTORY:
				// check if has subfolders
				dir = mpd_entity_get_directory(entity);
				path = mpd_directory_get_path(dir);
				if (strchr(path, '/') != NULL)
					path = NULL;
				break;
			case MPD_ENTITY_TYPE_PLAYLIST:
				break;
				
		}

		if (path != NULL && array_sz < 255) {
			toplevel_dirs[ array_sz++ ] = strdup( path );
		}

		mpd_entity_free(entity);
	}
	mpd_response_finish(conn);

	for (i = 0; i < array_sz; i++) {
		if (!mpd_run_add(conn, toplevel_dirs[i])) {
			fprintf(stderr, "ERROR: MPD: Unable to add item to the file playlist: %s\n", toplevel_dirs[i]);
			continue;
		}

		if (DEBUG)
			fprintf(stderr, "DEBUG: [mgr-thread]: mpd: entry added to the playlist: %s\n", toplevel_dirs[i]);

		free( toplevel_dirs[i] );
	}

	return 1;
}

/*
 * returns: 0 - error; 1 - ok 
 */
int mgr_mpd_connect( void ) {
	if (conn != NULL) {
		if (DEBUG)
			fprintf(stderr, "DEBUG: [mgr-thread] mpd: close open connection\n");

        	mpd_connection_free(conn);
		conn = NULL;
	}

	conn = mpd_connection_new(NULL, 0, 30000);

	if (conn == NULL || mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS) {
                fprintf(stderr, "ERROR: MPD: Unable to connect to MPD\n");

		return 0;
	}

	// SETUP mode: repeat:yes, random:yes
	if (!mpd_run_repeat(conn, true) || !mpd_run_random(conn, true)) {
                fprintf(stderr, "ERROR: MPD: Unable to change mode for the created MPD connection\n");
		mgr_handle_mpd_error();

		return 0;
	}

	if (DEBUG)
		fprintf(stderr, "DEBUG: [mgr-thread] mpd: new MPD connection was created\n");

	if (DEBUG)
		fprintf(stderr, "DEBUG: [mgr-thread] mpd: database update started\n");

	// Send update command
	if (!mpd_run_update(conn, NULL)) { // ALL MUSIC in the folder
		fprintf(stderr, "ERROR: MPD: Unable to update MPD database\n");
		mgr_handle_mpd_error();
		return 0;
	}

	return 1;
}

/*
 * Returns: 0 - error; 1 - ok 
 */
int mgr_mpd_fetch_status( struct mpd_connection *conn, int dump_status ) {
	struct mpd_status *status;
	struct mpd_stats *stats;

        if (DEBUG > 1)
                fprintf(stderr, "DEBUG: [mgr-thread] mpd: -status fetching...\n");

	if (conn == NULL) {
                fprintf(stderr, "ERROR: MPD: Unable to retrieve the MPD status: not connected\n");
		return 0;
	}

	if (mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS) {
                fprintf(stderr, "ERROR: MPD: Unable to retrieve MPD status: connection error\n");
		return 0;
	}

	if (DEBUG > 1)
		fprintf(stderr, "DEBUG: [mgr-thread] mpd: run status\n");

	status = mpd_run_status(conn);
	if (!status) {
                fprintf(stderr, "ERROR: MPD: Unable to retrieve MPD status\n");
		return 0;
	}

	cur_mpd_player_status = mpd_status_get_state(status);

	if (DEBUG > 1) {
		fprintf(stderr, "DEBUG: [mgr-thread] mpd: +status received:\n");
	}


	if (DEBUG && dump_status) {
		fprintf(stderr, "DEBUG: [mgr-thread] mpd status: => mode      : %s\n", (cur_mpd_player_status == MPD_STATE_PLAY ? "Play" : "Other"));
		fprintf(stderr, "DEBUG: [mgr-thread] mpd status: => random    : %d\n", mpd_status_get_random(status));
		fprintf(stderr, "DEBUG: [mgr-thread] mpd status: => repeat    : %d\n", mpd_status_get_random(status));
		fprintf(stderr, "DEBUG: [mgr-thread] mpd status: => volume    : %d\n", mpd_status_get_volume(status));
		fprintf(stderr, "DEBUG: [mgr-thread] mpd status: => queue ver : %d\n", mpd_status_get_queue_version(status));
		fprintf(stderr, "DEBUG: [mgr-thread] mpd status: => queue len : %d\n", mpd_status_get_queue_length(status));
	}

	cur_mpd_random = mpd_status_get_random(status) ? 1 : 0;
	cur_mpd_volume = mpd_status_get_volume(status);
	cur_mpd_status_queue_len = mpd_status_get_queue_length(status);

	if (DEBUG > 1)
		fprintf(stderr, "DEBUG: [mgr-thread] mpd: release status\n");

	if (mpd_status_get_error(status) != NULL) {
		fprintf(stderr, "WARNING: MPD: Error Received from MPD: %s\n", mpd_status_get_error(status));
		// TODO - clear error
	}

	mpd_status_free(status);
	mpd_response_finish(conn);

	stats = mpd_run_stats(conn);
	if (stats == NULL) {
                fprintf(stderr, "ERROR: MPD: Unable to retrieve MPD statistics\n");
		return 0;
	}

	cur_mpd_stats_number_of_songs = mpd_stats_get_number_of_songs( stats );
	if (DEBUG && dump_status) {
		fprintf(stderr, "DEBUG: [mgr-thread] mpd stats : => # of songs: %d\n", cur_mpd_stats_number_of_songs);
	}

	mpd_stats_free( stats );

	return 1;
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
void* threadManager(void *thread_arg){
	int i;
	conn = NULL;

        if (DEBUG)
                fprintf (stderr, "DEBUG: [mgr-thread] -starting\n");

	alsaSetup();

        if (ADC_init() < 0) {
                fprintf(stderr, "FATAL: ADC: Unable to setup ADC interface\n");
		flag_exit++;
		return thread_arg;
        }

	for (i = 0; i < 10 && conn == NULL; i++) {
		if (!mgr_mpd_connect()) {
			fprintf(stderr, "ERROR: MPD: Unable to connect, reconnecting...\n");
			delay(1000);
		}
	}

	if (conn == NULL) {
		fprintf(stderr, "FATAL: MPD: Unable to connect to MPD server, exiting...\n");
		flag_exit++;
		return thread_arg;
	}

	if (!mgr_mpd_fetch_status( conn, 1 )) {
		fprintf(stderr, "ERROR: MPD: Unable to fetch MPD status\n");
		mgr_handle_mpd_error();
	}

	// Read ADC & Update MPD
	volume_update();

	// Set volume to 0 if it does not play
	if (conn != NULL && cur_mpd_player_status != MPD_STATE_PLAY) {

		if (DEBUG)
			fprintf (stderr, "DEBUG: [mgr-thread] mpd: starting playing...\n");

		// set value to 0
		if (!mpd_run_set_volume(conn, 0)) {
			fprintf(stderr, "ERROR: MPD: Unable to set volume\n");
			mgr_handle_mpd_error();
		}

		// start player
		if (!mpd_run_play(conn)) {
			fprintf(stderr, "ERROR: MPD: Unable to start playing\n");
			mgr_handle_mpd_error();
		}
	}

	mgr_unsetWarmingUp();

        if (DEBUG)
                fprintf (stderr, "DEBUG: [mgr-thread] +started\n");

	while (!flag_exit) {

		//////////////////////////////////////////////////////////////////
		// Open MPD Connection
		//////////////////////////////////////////////////////////////////
		if (conn == NULL) {
			if (!mgr_mpd_connect()) {
				delay(5000);
				continue;
			}

		}

		//////////////////////////////////////////////////////////////////
		// WARMING?
		//////////////////////////////////////////////////////////////////
		if (mgr_isWarmingUp()) {
			delay(1000);
			continue;
		}

		//////////////////////////////////////////////////////////////////
		// RECEIVE STATUS
		//////////////////////////////////////////////////////////////////
		if (!mgr_mpd_fetch_status( conn, 0 )) {
			mgr_handle_mpd_error();
			continue;
		}

		//////////////////////////////////////////////////////////////////
		// update volume
		//////////////////////////////////////////////////////////////////
		volume_update();

		//////////////////////////////////////////////////////////////////
		// update playlist if needed
		//////////////////////////////////////////////////////////////////
		if (cur_mpd_stats_number_of_songs != cur_mpd_status_queue_len) {
			if (!mqr_update_playlist()) {
				fprintf(stderr, "ERROR: MPD: Unable to update playlist\n");
				mgr_handle_mpd_error();
				return 0;
			}
		}

		//////////////////////////////////////////////////////////////////
		// check player timeout
		//////////////////////////////////////////////////////////////////
		if (cur_mpd_player_status == MPD_STATE_PLAY) {
			if (ts_started == 0)
				ts_started = time(NULL); 

			int timeout = 3600;
			if (ts_started + timeout < time(NULL)) {
				if (!flag_stop) {
					if (DEBUG)
						fprintf(stderr, "DEBUG: MGR: Timeout reached, stop playing\n");
				}
				flag_stop++;
			}
		} else {
			ts_started = 0;
		}

		//////////////////////////////////////////////////////////////////
		// START
		//////////////////////////////////////////////////////////////////
		if (flag_start) {
			alsaPlaySound(PCM_SOUND_ID_CLICK);

			if (!mpd_run_play(conn)) {
				fprintf(stderr, "ERROR: [mgr-thread] mpd: start command failed\n");
				mgr_handle_mpd_error();
				continue;
			}

			if (DEBUG)
				fprintf(stderr, "DEBUG: [mgr-thread] player started\n");

			cur_mpd_player_status = MPD_STATE_PLAY; // for volume control
			flag_start = 0;
		}

		//////////////////////////////////////////////////////////////////
		// STOP 
		if (flag_stop) {
			// alsaPlaySound(PCM_SOUND_ID_CLICK);
			if (cur_mpd_volume == 0) {

				if (DEBUG)
					fprintf(stderr, "DEBUG: [mgr-thread] player stopped\n");

				if (!mpd_run_pause(conn, true)) {
					fprintf(stderr, "ERROR: [mgr-thread] mpd: stop command failed\n");

					mgr_handle_mpd_error();
					continue;
				}

				cur_mpd_player_status = MPD_STATE_PAUSE; // for volume control
				flag_stop = 0;
			}
		}

		//////////////////////////////////////////////////////////////////
		// RANDOM SWTICH MODE (SKIP2)
		if (flag_skip2) {
			alsaPlaySound(PCM_SOUND_ID_CLICK);

			if (!mpd_run_random(conn, (cur_mpd_random > 0 ? false : true) )) {
				fprintf(stderr, "ERROR: [mgr-thread] mpd: random command failed\n");

				mgr_handle_mpd_error();
				continue;
			}

			cur_mpd_random = cur_mpd_random > 0 ? 0 : 1;
		}

		//////////////////////////////////////////////////////////////////
		// SKIP1 (skip file)
		if (flag_skip1 || flag_skip2) {
			if (flag_skip1) {
				alsaPlaySound(PCM_SOUND_ID_CLICK);
			}

			if (!mpd_run_next(conn)) {
				fprintf(stderr, "ERROR: [mgr-thread] mpd: skip command failed\n");

				mgr_handle_mpd_error();
				continue;
			}
		}

		flag_skip1 = flag_skip2 = 0;

		delay(400); //delay 40 ms

	}//end while

        if (DEBUG)
                fprintf (stderr, "DEBUG: [mgr-thread] thread finished\n");
	
	return thread_arg;
}

