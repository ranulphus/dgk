/* dgk/midi.h - music as General MIDI channel messages, played wherever the game runs:
 *
 *   a GLOS app (on GLOS's desktop)  GLOS's MIDI ports: the card's FM chip played by GLOS's driver with its
 *                                   General MIDI instruments (Freedoom's GENMIDI), else the MPU-401; each
 *                                   message timed on GLOS's clock
 *   plain DOS, or full screen under GLOS (a session: GLOS hands it the chips)
 *                                   the FM chip at 388h through GLOS's FM synthesiser (sdk/fm: the same driver
 *                                   and instruments, so the same sound), or an MPU-401 in UART mode at the port
 *                                   BLASTER's P names
 *   Linux and headless              the same driver and instruments on an emulated OPL3 (ymfm), in dgk's mix
 *
 * DGK_MIDI_AUTO tries the FM chip first: a Sound Blaster 16's MPU-401 answers with nothing behind it. Nothing
 * found: DGK_MIDI_OFF, and every call below does nothing. On DOS no port is touched on a guess: the FM chip is
 * looked for only when BLASTER is set (every Sound Blaster has one at 388h; GLOS sets BLASTER in its
 * sessions), an MPU-401 only at the port BLASTER's P names. A game with its sound off (-nosound) opens
 * DGK_MIDI_OFF, so nothing at all is touched.
 *
 * Times are the caller's milliseconds, on any clock that counts up (compared by difference: it may wrap). Send a
 * little ahead (a frame or more; GLOS's queues hold 512 messages) and call dgk_midi_service(now) every frame:
 * what is due plays. A channel's volume (CC7) is scaled by dgk_midi_volume as it is sent. On the way out (exit,
 * Ctrl-C, a fault) every note is ended and the chip left silent.
 *
 * Tests: every message sent is counted and hashed (FNV-1a over its time and bytes, as the caller sent it).
 * DGK_MIDI_LOG=FILE logs each one ("ms status d1 d2", hex bytes); DGK_MIDI_MID=FILE writes them at exit as a
 * standard MIDI file (type 0, a tick half a millisecond), to listen to elsewhere. */
#ifndef DGK_MIDI_H
#define DGK_MIDI_H

#include "dgk/base.h"

enum { DGK_MIDI_OFF, DGK_MIDI_AUTO, DGK_MIDI_FM, DGK_MIDI_MPU };

int dgk_midi_open(int want);            /* DGK_MIDI_*: what plays (FM, MPU), or DGK_MIDI_OFF */
const char *dgk_midi_name(void);        /* what plays, for the log and the options ("an OPL3", "GLOS's FM" ...) */
void dgk_midi_send(uint32_t at_ms, uint8_t status, uint8_t d1, uint8_t d2);   /* a channel message, 80h-EFh */
void dgk_midi_service(uint32_t now_ms); /* every frame: what is due plays */
void dgk_midi_volume(int percent);      /* 0-100 (100: as sent), from now */
void dgk_midi_panic(void);              /* what waits dropped, every note ended */
void dgk_midi_close(void);              /* silent; DGK_MIDI_OFF until opened again */
uint32_t dgk_midi_hash(void);           /* every message sent so far */
uint32_t dgk_midi_count(void);

#endif
