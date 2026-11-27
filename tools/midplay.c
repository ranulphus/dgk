/* midplay.c - a standard MIDI file through dgk/midi.h (make midplay; make midplay-dos):
 *
 *   MIDPLAY FILE.MID ...                 DOS: each file in turn, live on whatever dgk/midi.h finds (an FM chip
 *                                        with GLOS's instruments, its ports, an MPU-401); Esc skips a file
 *   midplay FILE.MID OUT.WAV [SECONDS [CHANNELS]]
 *                                        host: the file through GLOS's driver and instruments and ymfm, into a WAV
 *                                        (mono, 44,100 Hz); SECONDS: at most that long (0: all of it); CHANNELS:
 *                                        only those notes heard (1-16, comma-separated: a part on its own)
 *   midplay -sweep OUT.WAV               host: every program (C3, C4, C5, each 0.7 s) and every drum key (35-81)
 *                                        at velocity 100, and a line each: its level (RMS, peak) and its pitch
 *                                        (the strongest semitone, which should be the note's)
 *
 * The file's tracks are merged and timed by its tempo map; the messages go to dgk/midi.h 100 ms ahead. */
#include "dgk/log.h"
#include "dgk/midi.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ev { uint32_t ms; uint8_t s, d1, d2; } ev;
typedef struct tev { uint32_t tick, order, tempo; uint8_t s, d1, d2; } tev;

static ev *evs;
static int nev;

static uint32_t rd32(const unsigned char *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static int cmp_tev(const void *a, const void *b)
{
    const tev *x = a, *y = b;
    return x->tick != y->tick ? (x->tick < y->tick ? -1 : 1) : (int)x->order - (int)y->order;
}

static uint32_t vlq(const unsigned char *d, uint32_t *p, uint32_t stop)
{
    uint32_t v = 0;
    do
        v = v << 7 | (d[*p] & 0x7F);
    while ((d[(*p)++] & 0x80) && *p < stop);
    return v;
}

/* The file's channel messages, in time order, in milliseconds; the count, or -1. */
static int load(const char *path, uint32_t *end_ms)
{
    FILE *f = fopen(path, "rb");
    unsigned char *d;
    long size;
    tev *t;
    uint32_t at = 14, nt = 0, cap = 1024, end = 0, tempo = 500000, last = 0;
    int k, tracks, division, i;
    double us = 0;
    if (!f)
        return -1;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    d = malloc((size_t)size + 1);
    t = malloc(cap * sizeof *t);
    if (!d || !t || fread(d, 1, (size_t)size, f) != (size_t)size || size < 14 || memcmp(d, "MThd", 4)) {
        fclose(f);
        return -1;
    }
    fclose(f);
    tracks = d[10] << 8 | d[11];
    division = d[12] << 8 | d[13];
    if (division <= 0 || division >= 0x8000)
        return -1;
    for (k = 0; k < tracks && at + 8 <= (uint32_t)size; k++) {
        uint32_t len = rd32(d + at + 4), p = at + 8, stop = at + 8 + len, tick = 0;
        uint8_t run = 0;
        if (memcmp(d + at, "MTrk", 4) || stop > (uint32_t)size)
            return -1;
        while (p < stop) {
            uint8_t s;
            tick += vlq(d, &p, stop);
            s = d[p];
            if (s & 0x80)
                p++;
            else
                s = run;
            if (nt == cap && !(t = realloc(t, (cap *= 2) * sizeof *t)))
                return -1;
            if (s == 0xFF) {
                uint8_t type = d[p++];
                uint32_t ml = vlq(d, &p, stop);
                if (type == 0x51 && ml == 3) {
                    t[nt].tick = tick, t[nt].order = nt, t[nt].s = 0xFF;
                    t[nt].tempo = (uint32_t)d[p] << 16 | (uint32_t)d[p + 1] << 8 | d[p + 2];
                    nt++;
                }
                p += ml;
            } else if (s == 0xF0 || s == 0xF7) {
                p += vlq(d, &p, stop);
            } else {
                int n = (s & 0xF0) == 0xC0 || (s & 0xF0) == 0xD0 ? 1 : 2;
                run = s;
                t[nt].tick = tick, t[nt].order = nt, t[nt].s = s, t[nt].d1 = d[p], t[nt].d2 = n == 2 ? d[p + 1] : 0;
                nt++;
                p += (uint32_t)n;
            }
        }
        if (tick > end)
            end = tick;
        at = stop;
    }
    qsort(t, nt, sizeof *t, cmp_tev);
    evs = malloc((nt + 1) * sizeof *evs);
    nev = 0;
    for (i = 0; i < (int)nt; i++) {
        us += (double)(t[i].tick - last) * tempo / division;
        last = t[i].tick;
        if (t[i].s == 0xFF) {
            tempo = t[i].tempo;
            continue;
        }
        evs[nev].ms = (uint32_t)(us / 1000), evs[nev].s = t[i].s, evs[nev].d1 = t[i].d1, evs[nev].d2 = t[i].d2;
        nev++;
    }
    us += (double)(end - last) * tempo / division;
    *end_ms = (uint32_t)(us / 1000);
    free(t);
    free(d);
    return nev;
}

#ifdef DGK_DOS
#include <conio.h>
#include <time.h>

static uint32_t now_ms(void)
{
    return (uint32_t)(uclock() / (UCLOCKS_PER_SEC / 1000));
}

int main(int argc, char **argv)
{
    int a;
    if (argc < 2) {
        printf("MIDPLAY FILE.MID ...\n");
        return 1;
    }
    if (dgk_midi_open(DGK_MIDI_AUTO) == DGK_MIDI_OFF) {
        printf("MIDPLAY nothing to play on\n");
        return 1;
    }
    printf("MIDPLAY on %s\n", dgk_midi_name());
    for (a = 1; a < argc; a++) {
        uint32_t end, t0, now;
        int pos = 0, c, skip = 0;
        if (load(argv[a], &end) < 0) {
            printf("MIDPLAY %s: not a MIDI file\n", argv[a]);
            continue;
        }
        printf("MIDPLAY playing %s: %d messages, %u.%u s\n", argv[a], nev, (unsigned)(end / 1000),
               (unsigned)(end % 1000 / 100));
        fflush(stdout);
        t0 = now_ms() + 200;
        for (c = 0; c < 16; c++)
            dgk_midi_send(t0, (uint8_t)(0xB0 | c), 121, 0);
        do {
            now = now_ms();
            while (pos < nev && (int32_t)(t0 + evs[pos].ms - (now + 100)) <= 0) {
                dgk_midi_send(t0 + evs[pos].ms, evs[pos].s, evs[pos].d1, evs[pos].d2);
                pos++;
            }
            dgk_midi_service(now);
            if (kbhit() && getch() == 27)
                skip = 1;
        } while (!skip && (int32_t)(now - (t0 + end + 1500)) < 0);
        dgk_midi_panic();
        free(evs);
    }
    dgk_midi_close();
    printf("MIDPLAY end\n");
    return 0;
}

#else
#include "dgk/mix.h"

#define RATE 44100
void plat_audio_lock(void) {}
void plat_audio_unlock(void) {}

static int16_t *pcm;
static uint32_t frames, cap;
static uint32_t clock_ms;

static void render_to(uint32_t ms)                   /* the mix run on to `ms` */
{
    while (clock_ms < ms) {
        int n = (int)((uint64_t)(clock_ms + 1) * RATE / 1000 - frames);
        if (frames + (uint32_t)n > cap) {
            cap = cap ? cap * 2 : RATE * 60;
            pcm = realloc(pcm, cap * sizeof *pcm);
        }
        dgk_midi_service(clock_ms);
        dgk_mix_render(pcm + frames, n);
        frames += (uint32_t)n;
        clock_ms++;
    }
}

static void wav(const char *path)
{
    FILE *f = fopen(path, "wb");
    uint32_t data = frames * 2, v;
    if (!f) {
        perror(path);
        exit(1);
    }
    fwrite("RIFF", 1, 4, f), v = data + 36, fwrite(&v, 4, 1, f), fwrite("WAVEfmt ", 1, 8, f);
    v = 16, fwrite(&v, 4, 1, f);
    {
        uint16_t h[2] = { 1, 1 };
        fwrite(h, 2, 2, f);
    }
    v = RATE, fwrite(&v, 4, 1, f), v = RATE * 2, fwrite(&v, 4, 1, f);
    {
        uint16_t h[2] = { 2, 16 };
        fwrite(h, 2, 2, f);
    }
    fwrite("data", 1, 4, f), fwrite(&data, 4, 1, f);
    fwrite(pcm, 2, frames, f);
    fclose(f);
}

static double rms(uint32_t from, uint32_t n)
{
    double e = 0;
    uint32_t i;
    for (i = 0; i < n; i++)
        e += (double)pcm[from + i] * pcm[from + i];
    return sqrt(e / n);
}

static int peak(uint32_t from, uint32_t n)
{
    int p = 0;
    uint32_t i;
    for (i = 0; i < n; i++)
        p = abs(pcm[from + i]) > p ? abs(pcm[from + i]) : p;
    return p;
}

static double power(uint32_t from, uint32_t n, double hz)   /* Goertzel's */
{
    double k = 2.0 * cos(2.0 * 3.14159265358979 * hz / RATE), s1 = 0, s2 = 0, s0;
    uint32_t i;
    for (i = 0; i < n; i++) {
        s0 = pcm[from + i] + k * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return s1 * s1 + s2 * s2 - k * s1 * s2;
}

/* The share of the energy above 4 kHz (a second-order high-pass): what whistles or hisses. */
static double bright(uint32_t from, uint32_t n)
{
    double w = tan(3.14159265358979 * 4000.0 / RATE), q = 0.7071, k = 1.0 / (1 + w / q + w * w);
    double b0 = k, b1 = -2 * k, b2 = k, a1 = 2 * (w * w - 1) * k, a2 = (1 - w / q + w * w) * k;
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0, e = 0, eh = 0;
    uint32_t i;
    for (i = 0; i < n; i++) {
        double x = pcm[from + i], y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1, x1 = x, y2 = y1, y1 = y;
        e += x * x, eh += y * y;
    }
    return e > 0 ? 100.0 * eh / e : 0;
}

static int strongest(uint32_t from, uint32_t n)              /* the MIDI note with the most power, 24-108 */
{
    int m, best = 24;
    double bp = -1;
    for (m = 24; m <= 108; m++) {
        double p = power(from, n, 440.0 * pow(2.0, (m - 69) / 12.0));
        if (p > bp)
            bp = p, best = m;
    }
    return best;
}

static void sweep(const char *out)
{
    static const int notes[3] = { 48, 60, 72 };
    int p, k, i;
    printf("prog   rms C3     C4     C5   peak  hf%%C5  strongest (pitch class C)\n");
    for (p = 0; p < 128; p++) {
        uint32_t at = clock_ms;
        double r[3], hf = 0;
        int pk = 0, s[3];
        dgk_midi_send(at, 0xC0, (uint8_t)p, 0);
        for (i = 0; i < 3; i++) {
            uint32_t on = at + 10 + (uint32_t)i * 2500, f0;
            dgk_midi_send(on, 0x90, (uint8_t)notes[i], 100);
            dgk_midi_send(on + 1000, 0x80, (uint8_t)notes[i], 0);
            render_to(on);
            f0 = frames;
            render_to(on + 2500);
            r[i] = rms(f0 + RATE / 20, RATE * 9 / 10);
            pk = DGK_MAX(pk, peak(f0, RATE));
            s[i] = strongest(f0 + RATE * 6 / 10, RATE / 4);
            if (i == 2)
                hf = bright(f0, RATE);
        }
        printf("%3d   %6.0f %6.0f %6.0f %6d %6.1f   %d %d %d%s\n", p, r[0], r[1], r[2], pk, hf, s[0], s[1], s[2],
               s[0] % 12 == 0 && s[1] % 12 == 0 && s[2] % 12 == 0 ? "" : "   (pitch?)");
    }
    printf("drum  rms   peak   hf%%\n");
    for (k = 35; k <= 81; k++) {
        uint32_t on = clock_ms + 10, f0;
        dgk_midi_send(on, 0x99, (uint8_t)k, 100);
        dgk_midi_send(on + 300, 0x89, (uint8_t)k, 0);
        render_to(on);
        f0 = frames;
        render_to(on + 600);
        printf("%3d   %6.0f %6d %6.1f\n", k, rms(f0, RATE * 3 / 10), peak(f0, RATE / 2), bright(f0, RATE / 2));
    }
    wav(out);
}

int main(int argc, char **argv)
{
    uint32_t end, limit;
    int pos = 0;
    if (argc < 3) {
        fprintf(stderr, "midplay FILE.MID OUT.WAV [SECONDS] | midplay -sweep OUT.WAV\n");
        return 1;
    }
    dgk_mix_init(RATE);
    if (dgk_midi_open(DGK_MIDI_FM) != DGK_MIDI_FM) {
        fprintf(stderr, "midplay: no FM\n");
        return 1;
    }
    if (!strcmp(argv[1], "-sweep")) {
        sweep(argv[2]);
        return 0;
    }
    if (load(argv[1], &end) < 0) {
        fprintf(stderr, "midplay: %s: not a MIDI file\n", argv[1]);
        return 1;
    }
    limit = argc > 3 && atof(argv[3]) > 0 ? (uint32_t)(atof(argv[3]) * 1000) : end + 2000;
    if (argc > 4) {                                  /* the other channels' notes dropped */
        unsigned keep = 0;
        const char *c = argv[4];
        int i, j;
        while (*c) {
            keep |= 1u << ((atoi(c) - 1) & 15);
            while (*c && *c != ',')
                c++;
            if (*c)
                c++;
        }
        for (i = j = 0; i < nev; i++)
            if ((evs[i].s & 0xE0) != 0x80 || keep >> (evs[i].s & 15) & 1)
                evs[j++] = evs[i];
        nev = j;
    }
    while (clock_ms < limit && clock_ms < end + 2000) {
        while (pos < nev && evs[pos].ms <= clock_ms + 100) {
            dgk_midi_send(evs[pos].ms, evs[pos].s, evs[pos].d1, evs[pos].d2);
            pos++;
        }
        render_to(clock_ms + 10);
    }
    wav(argv[2]);
    printf("midplay %s: %d messages, %.1f s into %s\n", argv[1], nev, frames / (double)RATE, argv[2]);
    return 0;
}
#endif
