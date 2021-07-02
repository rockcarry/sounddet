#ifndef __SOUNDDET_H__
#define __SOUNDDET_H__

#include <stdint.h>

#define SOUNDDET_TYPE_SOUND  (1 << 0)
#define SOUNDDET_TYPE_VOICE  (1 << 1)
#define SOUNDDET_TYPE_BBCRY  (1 << 2)

void*    sounddet_init(int samprate, int type);
void     sounddet_free(void *ctxt);
uint32_t sounddet_run (void *ctxt, int16_t *pcm, int n);

#endif

