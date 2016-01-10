#ifndef _Z_PCM_H_
#define _Z_PCM_H_

#define PCM_SOUND_ID_CLICK 0
#define PCM_SOUND_ID_FAILED 1
#define PCM_SOUND_ID_MAX (PCM_SOUND_ID_FAILED)

extern int alsaSetup();
extern int alsaPlaySound(int snd_id);

#endif
