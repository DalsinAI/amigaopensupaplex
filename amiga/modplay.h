/* A small ProTracker replayer. The AmiCradle Team. GPL 3. */
#ifndef modplay_h
#define modplay_h
#include <stdint.h>
int modLoad(const char *path, int rate);  /* 0 on success */
void modFree(void);
void modStart(void);
void modStop(void);    /* pause */
void modResume(void);  /* carry on from where modStop left it */
/* Add len samples of the tune to out; volume 0-10. */
void modMix(int16_t *out, unsigned len, int volume);
#endif
