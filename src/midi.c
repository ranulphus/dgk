/* midi.c - General MIDI messages to wherever they play (dgk/midi.h). The GLOS ports, the MPU-401 and the host
 * log and file are Echoes of the Void's midiout.c (2026-11-26), moved here; the FM chip is GLOS's sdk/fm on DOS
 * and GLOS's FM driver on ymfm elsewhere (midi_ymfm.cpp). */
#include "dgk/midi.h"
#include "dgk/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct msg { uint32_t at; uint8_t status, d1, d2; } msg;

enum { B_NONE, B_GLOS, B_FMSYNTH, B_MPU, B_YMFM };

static int kind = DGK_MIDI_OFF, backend = B_NONE;
static const char *name = "none";
static uint32_t hash = 2166136261u, count, last_now;
static int volume = 100;
static uint8_t cc7[16];                 /* each channel's volume as sent (GM's 100 until then) */

static int msg_len(uint8_t status)      /* the data bytes after the status */
{
    uint8_t hi = status & 0xF0;
    return hi == 0xC0 || hi == 0xD0 ? 1 : 2;
}

static void hash_msg(uint32_t at, uint8_t s, uint8_t d1, uint8_t d2)
{
    const uint8_t b[7] = { (uint8_t)at, (uint8_t)(at >> 8), (uint8_t)(at >> 16), (uint8_t)(at >> 24), s, d1, d2 };
    int i;
    for (i = 0; i < 7; i++)
        hash = (hash ^ b[i]) * 16777619u;
    count++;
}

/* ---- Tests: the log (DGK_MIDI_LOG) and a standard MIDI file at exit (DGK_MIDI_MID) --------------------------- */

static FILE *mlog;
static int mlog_used;                   /* (opened again after a close: added to, not emptied) */
static msg *file_msgs;
static uint32_t file_n, file_cap;
static const char *file_path;

static void put32(FILE *f, uint32_t v)
{
    fputc((int)(v >> 24), f), fputc((int)(v >> 16) & 255, f), fputc((int)(v >> 8) & 255, f), fputc((int)v & 255, f);
}

static void put_var(FILE *f, uint32_t v)        /* a variable-length quantity */
{
    uint8_t b[5];
    int n = 0;
    b[n++] = (uint8_t)(v & 0x7F);
    while (v >>= 7)
        b[n++] = (uint8_t)(0x80 | (v & 0x7F));
    while (n--)
        fputc(b[n], f);
}

static void write_file(void)
{
    FILE *f;
    long len_at, end;
    uint32_t i, last;
    if (!file_path || !file_msgs || !(f = fopen(file_path, "wb")))
        return;
    last = file_msgs[0].at;                     /* (the file starts at the first message) */
    /* Type 0, 1000 ticks a quarter note at 120 bpm: a tick is half a millisecond. */
    fwrite("MThd", 1, 4, f);
    put32(f, 6);
    fputc(0, f), fputc(0, f), fputc(0, f), fputc(1, f), fputc(1000 >> 8, f), fputc(1000 & 255, f);
    fwrite("MTrk", 1, 4, f);
    len_at = ftell(f);
    put32(f, 0);
    put_var(f, 0), fputc(0xFF, f), fputc(0x51, f), fputc(3, f), fputc(0x07, f), fputc(0xA1, f), fputc(0x20, f);
    for (i = 0; i < file_n; i++) {
        const msg *m = &file_msgs[i];
        uint32_t at = (int32_t)(m->at - last) < 0 ? last : m->at;
        put_var(f, (at - last) * 2);
        last = at;
        fputc(m->status, f);
        fputc(m->d1, f);
        if (msg_len(m->status) == 2)
            fputc(m->d2, f);
    }
    put_var(f, 0), fputc(0xFF, f), fputc(0x2F, f), fputc(0, f);
    end = ftell(f);
    fseek(f, len_at, SEEK_SET);
    put32(f, (uint32_t)(end - len_at - 4));
    fclose(f);
    dgk_log("DGK-MIDI file %s: %u messages", file_path, (unsigned)file_n);
}

static void file_add(uint32_t at, uint8_t s, uint8_t d1, uint8_t d2)
{
    if (!file_path)
        return;
    if (file_n == file_cap) {
        uint32_t cap = file_cap ? file_cap * 2 : 4096;
        msg *m = cap <= 1u << 20 ? realloc(file_msgs, cap * sizeof *m) : NULL;
        if (!m)
            return;
        file_msgs = m;
        file_cap = cap;
    }
    file_msgs[file_n].at = at;
    file_msgs[file_n].status = s, file_msgs[file_n].d1 = d1, file_msgs[file_n].d2 = d2;
    file_n++;
}

/* ---- The backends ---------------------------------------------------------------------------------------------- */

#ifdef DGK_DOS
#include <glos/fmsynth.h>
#include <glos/glos.h>
#include <pc.h>
#include <signal.h>

/* A GLOS app: GLOS's MIDI ports. The messages of a frame go in one glos_midi_send, their times moved onto GLOS's
 * clock: the caller's follows its frames, GLOS's is real time, so the offset between them is followed slowly (a
 * sixteenth a frame) to keep the notes' spacing, and taken afresh when they part by more than a quarter second. */
static glos_handle port;
static struct glos_midi_msg out[GLOS_MIDI_QUEUE];
static unsigned nout;
static int32_t glos_off;
static int have_off;

static int port_open(unsigned which)
{
    port = glos_midi_out(which);
    if (!port)
        dgk_log("DGK-MIDI GLOS's %s port: none (error %d)", which == GLOS_MIDI_PORT_FM ? "FM" : "MPU-401",
                glos_error());
    return port != 0;
}

static void port_service(uint32_t now)
{
    unsigned i, taken = 0, queued = 0, g = glos_ticks();
    int32_t off = (int32_t)(g - now);
    if (!have_off || off - glos_off > 250 || glos_off - off > 250)
        glos_off = off, have_off = 1;
    else
        glos_off += (off - glos_off) / 16;
    for (i = 0; i < nout; i++) {
        uint32_t t = out[i].time + (uint32_t)glos_off;
        out[i].time = (int32_t)(t - g) <= 0 ? 0 : t;
    }
    if (nout && glos_midi_send(port, out, nout, &taken, &queued) != 0)
        dgk_log("DGK-MIDI send: %u of %u taken (%u waiting)", taken, nout, queued);
    nout = 0;
}

/* Plain DOS or a GLOS session: an MPU-401 in UART mode, the bytes written as they fall due. */
static unsigned mpu = 0x330;
static msg queue[1024];
static unsigned qhead, qtail;           /* waiting, in the order sent */

static int mpu_wait(int bit, int set)   /* the status port's bit (40h: ready for data when clear; 80h: data) */
{
    long n;
    for (n = 0; n < 100000; n++)
        if (((inportb((unsigned short)(mpu + 1)) & bit) != 0) == set)
            return 1;
    return 0;
}

static int mpu_open(void)
{
    const char *b = getenv("BLASTER"), *p = b ? strchr(b, 'P') : NULL;
    int tries;
    /* Only at a port BLASTER names: a guess at 330h reset a PC with something else there (Echoes, the P3,
     * 2026-11-26). */
    if (!p)
        return 0;
    mpu = (unsigned)strtoul(p + 1, NULL, 16);
    for (tries = 0; tries < 2; tries++) {   /* a reset (acknowledged with FEh), then UART mode */
        if (!mpu_wait(0x40, 0))
            return 0;
        outportb((unsigned short)(mpu + 1), tries ? 0x3F : 0xFF);
        if (!mpu_wait(0x80, 0) || inportb((unsigned short)mpu) != 0xFE)
            if (!tries)
                return 0;
    }
    return 1;
}

static void mpu_byte(uint8_t v)
{
    if (mpu_wait(0x40, 0))
        outportb((unsigned short)mpu, v);
}

static void mpu_message(uint8_t s, uint8_t d1, uint8_t d2)
{
    mpu_byte(s);
    mpu_byte(d1);
    if (msg_len(s) == 2)
        mpu_byte(d2);
}

static void mpu_due(uint32_t now)
{
    while (qhead != qtail && (int32_t)(queue[qhead].at - now) <= 0) {
        const msg *m = &queue[qhead];
        mpu_message(m->status, m->d1, m->d2);
        qhead = (qhead + 1) % 1024;
    }
}

/* Out of the program any way (exit, Ctrl-C, a fault): silent, then on as before (each handler chains to the one
 * it found: DOS-GL's, the C library's). */
static void (*prev_sig[4])(int);
static const int sigs[4] = { SIGINT, SIGILL, SIGSEGV, SIGFPE };
static int hooked;

static void on_signal(int sig)
{
    int i;
    dgk_midi_close();
    for (i = 0; i < 4; i++)
        if (sigs[i] == sig) {
            signal(sig, prev_sig[i] ? prev_sig[i] : SIG_DFL);
            raise(sig);
            return;
        }
}

static void hook(void)
{
    int i;
    if (hooked)
        return;
    atexit(dgk_midi_close);
    for (i = 0; i < 4; i++)
        prev_sig[i] = signal(sigs[i], on_signal);
    hooked = 1;
}

#else
/* Linux and headless: GLOS's FM driver (kernel/drv/fm.c with its queue, sdk/fm/fmq.c), declared here as
 * sdk/fm/fmsynth.c declares it, writing an emulated OPL3 (midi_ymfm.cpp). */
typedef void (*fm_write)(unsigned short reg, unsigned char v);
void fm_reset(fm_write wr, int opl3);
void fm_message(unsigned char status, unsigned char d1, unsigned char d2);
void fm_silence(void);
void fm_end(void);
int fmq_put(unsigned at, unsigned char status, unsigned char d1, unsigned char d2, unsigned now);
void fmq_play(unsigned now);
void fmq_clear(void);
int dgk__ymfm_open(void);
void dgk__ymfm_write(unsigned short reg, unsigned char v);
void dgk__ymfm_commit(void);
void dgk__ymfm_close(void);
#endif

/* A message to the backend, at its time (a time gone by: now). */
static void play(uint32_t at, uint8_t s, uint8_t d1, uint8_t d2)
{
    switch (backend) {
#ifdef DGK_DOS
    case B_GLOS:
        if (nout < GLOS_MIDI_QUEUE) {
            struct glos_midi_msg *m = &out[nout++];
            m->time = at;               /* (the caller's clock: port_service moves it onto GLOS's) */
            m->status = s, m->data1 = d1, m->data2 = msg_len(s) == 2 ? d2 : 0, m->pad = 0;
        }
        break;
    case B_FMSYNTH:
        fmsynth_send(at, s, d1, d2);
        break;
    case B_MPU:
        if ((qtail + 1) % 1024 != qhead) {
            queue[qtail].at = at;
            queue[qtail].status = s, queue[qtail].d1 = d1, queue[qtail].d2 = d2;
            qtail = (qtail + 1) % 1024;
        }
        break;
#else
    case B_YMFM:
        fmq_put(at, s, d1, d2, last_now);
        break;
#endif
    default:
        DGK_UNUSED(at), DGK_UNUSED(s), DGK_UNUSED(d1), DGK_UNUSED(d2);
        break;
    }
}

static uint8_t scaled(uint8_t v)
{
    return (uint8_t)((v * volume + 50) / 100);
}

int dgk_midi_open(int want)
{
    const char *f = getenv("DGK_MIDI_MID"), *l = getenv("DGK_MIDI_LOG");
    int c;
    dgk_midi_close();
    if (f && *f && !file_path) {
        file_path = f;
        atexit(write_file);
    }
    if (l && *l && !mlog && (mlog = fopen(l, mlog_used ? "a" : "w")) != NULL)
        setvbuf(mlog, NULL, _IOLBF, 4096);   /* (a line at a time: the program may end without closing it) */
    for (c = 0; c < 16; c++)
        cc7[c] = 100;
    kind = DGK_MIDI_OFF, backend = B_NONE, name = "none";
    if (want == DGK_MIDI_OFF)
        return kind;
#ifdef DGK_DOS
    if (glos_running() && glos_is_app()) {
        /* An app on GLOS's desktop: GLOS's ports (it lets no app reach the chips). */
        if ((want == DGK_MIDI_AUTO || want == DGK_MIDI_FM) && port_open(GLOS_MIDI_PORT_FM))
            kind = DGK_MIDI_FM, name = "GLOS's FM";
        else if ((want == DGK_MIDI_AUTO || want == DGK_MIDI_MPU) && port_open(GLOS_MIDI_PORT_MPU))
            kind = DGK_MIDI_MPU, name = "GLOS's MPU-401";
        if (kind != DGK_MIDI_OFF)
            backend = B_GLOS, nout = 0, have_off = 0;
    } else {
        /* Plain DOS, or full screen under GLOS (a session: GLOS hands it the chips; its MIDI API is apps'). The
         * FM chip is looked for only where BLASTER says there is a Sound Blaster (each has one at 388h; GLOS
         * sets BLASTER in its sessions): no port is touched on a guess. */
        int chip = (want == DGK_MIDI_AUTO || want == DGK_MIDI_FM) && getenv("BLASTER") ? fmsynth_open(0x388) : 0;
        if (chip) {
            kind = DGK_MIDI_FM, backend = B_FMSYNTH;
            name = chip == 3 ? (glos_running() ? "an OPL3 (a GLOS session)" : "an OPL3")
                             : (glos_running() ? "an OPL2 (a GLOS session)" : "an OPL2");
        } else if ((want == DGK_MIDI_AUTO || want == DGK_MIDI_MPU) && mpu_open()) {
            kind = DGK_MIDI_MPU, backend = B_MPU, qhead = qtail = 0;
            name = glos_running() ? "an MPU-401 (a GLOS session)" : "an MPU-401";
        }
    }
    if (kind != DGK_MIDI_OFF)
        hook();
#else
    if ((want == DGK_MIDI_AUTO || want == DGK_MIDI_FM) && dgk__ymfm_open()) {
        fmq_clear();
        fm_reset(dgk__ymfm_write, 1);
        dgk__ymfm_commit();
        kind = DGK_MIDI_FM, backend = B_YMFM, name = "an emulated OPL3 (ymfm)";
    }
#endif
    dgk_log("DGK-MIDI %s", kind == DGK_MIDI_OFF ? "nothing to play music on" : name);
    return kind;
}

const char *dgk_midi_name(void)
{
    return name;
}

void dgk_midi_send(uint32_t at, uint8_t status, uint8_t d1, uint8_t d2)
{
    if (kind == DGK_MIDI_OFF || status < 0x80 || status >= 0xF0)
        return;
    hash_msg(at, status, d1, d2);
    file_add(at, status, d1, d2);
    if (mlog)
        fprintf(mlog, "%lu %02x %02x %02x\n", (unsigned long)at, status, d1, d2);
    if ((status & 0xF0) == 0xB0 && d1 == 7) {
        cc7[status & 15] = d2;
        d2 = scaled(d2);
    }
    play(at, status, d1, d2);
}

void dgk_midi_service(uint32_t now)
{
    last_now = now;
    switch (backend) {
#ifdef DGK_DOS
    case B_GLOS:
        port_service(now);
        break;
    case B_FMSYNTH:
        fmsynth_play_due(now);
        break;
    case B_MPU:
        mpu_due(now);
        break;
#else
    case B_YMFM:
        fmq_play(now);
        dgk__ymfm_commit();
        break;
#endif
    default:
        break;
    }
}

void dgk_midi_volume(int percent)
{
    int c;
    volume = DGK_CLAMP(percent, 0, 100);
    if (kind == DGK_MIDI_OFF)
        return;
    for (c = 0; c < 16; c++)
        play(last_now, (uint8_t)(0xB0 | c), 7, scaled(cc7[c]));
}

void dgk_midi_panic(void)
{
    int c;
    if (file_path && file_n) {          /* (the file: every note ended where it stops) */
        uint32_t at = file_msgs[file_n - 1].at;
        for (c = 0; c < 16; c++)
            file_add(at, (uint8_t)(0xB0 | c), 123, 0);
    }
    switch (backend) {
#ifdef DGK_DOS
    case B_GLOS:
        nout = 0;
        glos_midi_flush(port);
        break;
    case B_FMSYNTH:
        fmsynth_flush();
        break;
    case B_MPU:
        qhead = qtail;
        for (c = 0; c < 16; c++) {
            mpu_message((uint8_t)(0xB0 | c), 123, 0);      /* all notes off */
            mpu_message((uint8_t)(0xB0 | c), 120, 0);      /* all sound off */
        }
        break;
#else
    case B_YMFM:
        fmq_clear();
        fm_silence();
        dgk__ymfm_commit();
        break;
#endif
    default:
        DGK_UNUSED(c);
        break;
    }
}

void dgk_midi_close(void)
{
    if (kind == DGK_MIDI_OFF)
        return;
    dgk_midi_panic();
    switch (backend) {
#ifdef DGK_DOS
    case B_GLOS:
        glos_close(port);
        port = 0;
        break;
    case B_FMSYNTH:
        fmsynth_close();
        break;
#else
    case B_YMFM:
        fmq_clear();
        fm_end();
        dgk__ymfm_commit();
        dgk__ymfm_close();
        break;
#endif
    default:
        break;
    }
    kind = DGK_MIDI_OFF, backend = B_NONE, name = "none";
    if (mlog) {                         /* (on DOS a file's length is written only as it closes) */
        fclose(mlog);
        mlog = NULL;
        mlog_used = 1;
    }
}

uint32_t dgk_midi_hash(void)
{
    return hash;
}

uint32_t dgk_midi_count(void)
{
    return count;
}
