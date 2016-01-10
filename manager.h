#ifndef _Z_MGR_H_
#define _Z_MGR_H_

#ifndef DEBUG
//#define DEBUG 0
#define DEBUG 1
//#define DEBUG 2
#endif

#define MGR_MODULES_COUNT 3

extern int mgr_isExitStatus( void );
extern void mgr_setExitStatus( void );

extern int mgr_mpd_isPlaying( void );

extern int  mgr_isStartPlay( void );
extern void mgr_setStartPlay( void );
extern int  mgr_isStopPlay( void );
extern void mgr_setStopPlay( void );

extern void mgr_setSkipFile( void );
extern void mgr_setSkipFolder( void );

extern int mgr_isWarmingUp( void );
extern void mgr_resetWarmingUpStatus( void );
extern void mgr_unsetWarmingUp( void );

extern void* threadManager(void *thread_arg);

#endif
