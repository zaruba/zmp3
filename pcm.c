#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <alsa/asoundlib.h>

#include <sys/stat.h>
#include <unistd.h>

#include "pcm.h"

char *load_pcm(char *file, int *sz);

snd_pcm_t *PCM_handle;
char *SFX_data[PCM_SOUND_ID_MAX+1];
int SFX_sz[PCM_SOUND_ID_MAX+1];

int alsaSetup() {
	unsigned int rate = 16000;
	int err;
	snd_pcm_hw_params_t *hwparams;
 
	if ((err=snd_pcm_open(&PCM_handle,"plughw:0,0",SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
		fprintf(stderr, "ERROR: ALSA: Can't use sound: %s\n", snd_strerror(err));
		return err;
	}
 
	snd_pcm_hw_params_alloca(&hwparams);

	snd_pcm_hw_params_any(PCM_handle, hwparams);
	snd_pcm_hw_params_set_access(PCM_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(PCM_handle, hwparams, SND_PCM_FORMAT_S16_LE);
	snd_pcm_hw_params_set_channels(PCM_handle, hwparams, 1);
	snd_pcm_hw_params_set_rate_near(PCM_handle, hwparams, &rate, NULL);

	if ((err = snd_pcm_hw_params(PCM_handle, hwparams)) < 0) {
		fprintf(stderr, "ERROR: ALSA: Can't set parameters for sound: %s\n", snd_strerror(err));
		return err;
	}

	SFX_data[PCM_SOUND_ID_FAILED] = load_pcm("sounds/failed.wav", &SFX_sz[PCM_SOUND_ID_FAILED]);
	SFX_data[PCM_SOUND_ID_CLICK] = load_pcm("sounds/click.wav", &SFX_sz[PCM_SOUND_ID_CLICK]);

	return 1;
}
 
 
char *load_pcm (char *file, int *len) {
    size_t rv;
    struct stat info;
    FILE *fp;
    char *data;
 
	fp = fopen(file,"rb");
	if (!fp) {
		fprintf(stderr, "ERROR: ALSA: load_pcm() -- Couldn't open %s: ", file);
		perror("fopen() ");
		return NULL;
	}
 
	stat(file,&info);
	data = malloc(info.st_size);
	if ((rv=fread(data,1,info.st_size,fp)) != info.st_size) 
		fprintf(stderr, "ERROR: ALSA: load_pcm -- Read short on %s: %d/%d\n",file,(int)rv,(int)info.st_size);
	fclose(fp);
	*len = info.st_size;
	return data;
}
 
 
int alsaPlaySound(int snd_id) {
	int rem, i;
	char *dp;

	if (snd_id < 0 || snd_id > PCM_SOUND_ID_MAX) {
		fprintf(stderr, "ERROR: ALSA: playsound: incorrect sound ID\n");
		return 0;
	}

	dp = SFX_data[snd_id];
 
// if (!Config.sound) 
//		pthread_exit(0);
 
   // pthread_mutex_lock(&SoundLock);

    for (i=0; i < SFX_sz[snd_id];i+=256) {
        rem = (SFX_sz[snd_id]-i)/2;

        if (rem > 128) 
		rem = 128;

        if (snd_pcm_writei(PCM_handle ,dp, 128) ==-EPIPE) 
		snd_pcm_prepare(PCM_handle);
        dp+=256;
    }
	printf("ALSA: sound played\n");
//    pthread_mutex_unlock(&SoundLock);
//    pthread_exit(0);

	return 0;
}
