/* dgk/mix.h - a small integer mixer: mono 16-bit voices, each with its own
 * volume and pitch (16.16), looping or one-shot, mixed into the output
 * rate. The same input gives the same samples on every platform (tests
 * compare them). The game calls these from its own thread; the audio
 * callback mixes under the platform's lock (dgk_mix_lock). */
#ifndef DGK_MIX_H
#define DGK_MIX_H

#include "dgk/base.h"

#define DGK_MIX_VOICES 12
#define DGK_MIX_LOOP 1

typedef struct dgk_sound {
    const int16_t *pcm;
    uint32_t frames, rate;
} dgk_sound;

void dgk_mix_init(uint32_t rate);
uint32_t dgk_mix_rate(void);                     /* the output's frames a second */
int  dgk_mix_play(const dgk_sound *s, int vol_q8, uint32_t pitch_q16, int flags);  /* voice, or -1; volume
                                                 changes ramp over 3 ms */
void dgk_mix_set(int voice, int vol_q8, uint32_t pitch_q16);
void dgk_mix_master(int vol_q8);                 /* everything's volume: 256 = unity */
void dgk_mix_stop(int voice);                    /* fades out over 3 ms, then frees it */
void dgk_mix_render(int16_t *out, int frames);   /* the audio callback's work */
/* A source of its own (a synthesiser: an FM chip's emulation): each render
 * calls it for `frames` output frames, to add its samples to acc (32-bit,
 * 16-bit scale, before the master volume), under the audio lock. NULL: none.
 * One at a time: off DOS, dgk/midi.h's FM chip is it. */
typedef void (*dgk_mix_stream_fn)(int32_t *acc, int frames, void *user);
void dgk_mix_stream(dgk_mix_stream_fn fn, void *user);
void dgk_mix_lock(void);                         /* held around voice changes */
void dgk_mix_unlock(void);

#endif
