/* test_dgk.c - the kit's own checks (make test): the main loop driven by a
 * fake platform (no SDL, no GL), so its input handling is checked tick by
 * tick, the vector helpers, and the music (GLOS's FM driver on ymfm). */
#include "dgk/app.h"
#include "dgk/midi.h"
#include "dgk/mix.h"
#include "dgk/vec.h"
#include <math.h>
#include "plat.h"
#include <stdio.h>
#include <string.h>

static int checks, failures;

#define CHECK(cond, ...)                                                                                  \
    do {                                                                                                  \
        checks++;                                                                                         \
        if (!(cond)) {                                                                                    \
            failures++;                                                                                   \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                   \
            printf(__VA_ARGS__);                                                                          \
            printf("\n");                                                                                 \
        }                                                                                                 \
    } while (0)

/* ---- The fake platform: events queued for given frames ------------------ */

#define W 320
#define H 240

static struct fake_ev { uint32_t frame; plat_event e; } script[64];
static int nscript, next_ev, relative_calls, last_relative = -1, text_on;
static uint32_t frame_no;
static uint64_t now_us;

static void at(uint32_t frame, plat_event e)
{
    script[nscript].frame = frame;
    script[nscript].e = e;
    nscript++;
}
static plat_event move(int x, int y, int dx, int dy)
{
    plat_event e = { PLAT_EV_MOUSE_MOVE, 0, 0, x, y, dx, dy, NULL };
    return e;
}
static plat_event ev(int type, int key, int value, const char *name)
{
    plat_event e = { type, key, value, 0, 0, 0, 0, name };
    return e;
}

int plat_open(const plat_config *c, int *width, int *height)
{
    DGK_UNUSED(c);
    *width = W;
    *height = H;
    return 0;
}
void plat_close(void) {}
void plat_pump(void) {}
int plat_next_event(plat_event *e)
{
    while (next_ev < nscript && script[next_ev].frame <= frame_no) {
        const struct fake_ev *f = &script[next_ev++];
        if (f->e.type == PLAT_EV_TEXT && !text_on)
            continue;                                /* as SDL: text only while it is on */
        *e = f->e;
        return 1;
    }
    return 0;
}
void plat_swap(void)
{
    frame_no++;
    now_us += 1000000 / 60;
}
uint64_t plat_now_us(void) { return now_us; }
int plat_snapshot(const char *name) { DGK_UNUSED(name); return 0; }
void plat_audio_open(void) {}
void plat_audio_close(void) {}
int plat_audio_underruns(int *u, int *c) { *u = *c = 0; return 0; }
void plat_audio_lock(void) {}
void plat_audio_unlock(void) {}
const char *plat_describe(void) { return "fake"; }
int plat_modes(int (*wh)[2], int max) { DGK_UNUSED(wh); DGK_UNUSED(max); return 0; }
void plat_vsync(int on) { DGK_UNUSED(on); }
int plat_mouse_present(void) { return 0; }
void plat_mouse_relative(int on) { relative_calls++; last_relative = on; }
void plat_mouse_cursor(int show) { DGK_UNUSED(show); }
void plat_text_input(int on) { text_on = on; }

/* What app.c needs from modules that draw or time (gfx.c, bench.c). */
uint32_t dgk_gfx_tris, dgk_gfx_draws;
void dgk_bench_begin(const char *test, int warmup) { DGK_UNUSED(test); DGK_UNUSED(warmup); }
void dgk_bench_end(const char *status, const char *notes) { DGK_UNUSED(status); DGK_UNUSED(notes); }
void dgk_bench_frame(uint32_t us, uint32_t tris) { DGK_UNUSED(us); DGK_UNUSED(tris); }

/* ---- A game that keeps what each tick saw ------------------------------- */

#define TICKS 12
static dgk_app_state seen[TICKS];

static void tick(void *u)
{
    DGK_UNUSED(u);
    if (dgk_app.ticks < TICKS)
        seen[dgk_app.ticks] = dgk_app;
    if (dgk_app.ticks == 5)
        dgk_text_input(1);                           /* text from frame 6 on */
    if (dgk_app.ticks == 8) {
        dgk_mouse_relative(1);
        dgk_text_input(0);
    }
}

static void loop_input(void)
{
    static const char long_text[] = "this piece is far too long to fit in one tick";
    char *argv[] = { (char *)"test", (char *)"-fixed", (char *)"-frames", (char *)"12", NULL };
    dgk_app_desc d;
    memset(&d, 0, sizeof d);
    d.title = "test_dgk";
    d.tick = tick;

    at(0, move(100, 50, 10, 0));
    at(0, move(105, 50, 5, 0));
    at(1, ev(PLAT_EV_KEY_DOWN, DGK_KEY_SPACE, 0, NULL));
    at(2, ev(PLAT_EV_MOUSE_DOWN, DGK_MOUSE_LEFT, 0, NULL));
    at(4, ev(PLAT_EV_MOUSE_UP, DGK_MOUSE_LEFT, 0, NULL));
    at(4, ev(PLAT_EV_MOUSE_DOWN, 7, 0, NULL));        /* a button the kit doesn't keep */
    at(5, ev(PLAT_EV_WHEEL, 0, 1, NULL));
    at(5, ev(PLAT_EV_WHEEL, 0, 1, NULL));
    at(5, ev(PLAT_EV_TEXT, 0, 0, "lost"));            /* text input still off */
    at(6, ev(PLAT_EV_TEXT, 0, 0, "ab"));
    at(6, ev(PLAT_EV_TEXT, 0, 0, "c"));
    at(7, ev(PLAT_EV_TEXT, 0, 0, long_text));
    at(7, ev(PLAT_EV_TEXT, 0, 0, "d"));
    at(9, move(-5, 9999, -3, 4));
    at(10, ev(PLAT_EV_TEXT, 0, 0, "off"));            /* text input off again at tick 8 */

    CHECK(dgk_app_run(&d, NULL, 4, argv) == 0, "dgk_app_run failed");
    CHECK(dgk_app.ticks == 12, "%lu ticks", (unsigned long)dgk_app.ticks);

    CHECK(seen[0].mouse_present && seen[0].mouse_x == 105 && seen[0].mouse_y == 50,
          "tick 0: present %d at %d,%d", seen[0].mouse_present, seen[0].mouse_x, seen[0].mouse_y);
    CHECK(seen[0].mouse_dx == 15 && seen[0].mouse_dy == 0, "tick 0: motion %d,%d", seen[0].mouse_dx,
          seen[0].mouse_dy);
    CHECK(seen[1].mouse_dx == 0 && seen[1].mouse_x == 105, "tick 1: motion %d, x %d", seen[1].mouse_dx,
          seen[1].mouse_x);
    CHECK(seen[1].key_pressed[DGK_KEY_SPACE] && seen[1].key_down[DGK_KEY_SPACE], "tick 1: Space");
    CHECK(seen[2].mouse_pressed[DGK_MOUSE_LEFT] && seen[2].mouse_down[DGK_MOUSE_LEFT], "tick 2: left pressed");
    CHECK(!seen[3].mouse_pressed[DGK_MOUSE_LEFT] && seen[3].mouse_down[DGK_MOUSE_LEFT], "tick 3: left held");
    CHECK(!seen[4].mouse_down[DGK_MOUSE_LEFT], "tick 4: left up");
    CHECK(seen[5].mouse_wheel == 2 && seen[6].mouse_wheel == 0, "wheel %d then %d", seen[5].mouse_wheel,
          seen[6].mouse_wheel);
    CHECK(seen[5].text[0] == 0, "tick 5: text '%s' before text input", seen[5].text);
    CHECK(!strcmp(seen[6].text, "abc"), "tick 6: text '%s'", seen[6].text);
    CHECK(!strcmp(seen[7].text, "d"), "tick 7: text '%s' (the long piece dropped)", seen[7].text);
    CHECK(seen[8].text[0] == 0, "tick 8: text '%s'", seen[8].text);
    CHECK(seen[9].mouse_relative && relative_calls == 1 && last_relative == 1, "relative mode");
    CHECK(seen[9].mouse_x == 0 && seen[9].mouse_y == H - 1 && seen[9].mouse_dx == -3 && seen[9].mouse_dy == 4,
          "tick 9: clamped to %d,%d, motion %d,%d", seen[9].mouse_x, seen[9].mouse_y, seen[9].mouse_dx,
          seen[9].mouse_dy);
    CHECK(seen[10].text[0] == 0, "tick 10: text '%s' after text input ended", seen[10].text);
}

static void vectors(void)
{
    dgk_d3 far = dgk_d3_make(1.0e12 + 1.5, -2.0e11, 3.25), eye = dgk_d3_make(1.0e12, -2.0e11 - 0.5, 0.0);
    dgk_v3 r = dgk_d3_rel(far, eye);
    dgk_v3 z = dgk_v3_cross(dgk_v3_make(1, 0, 0), dgk_v3_make(0, 1, 0));
    dgk_d3 zd = dgk_d3_cross(dgk_d3_make(1, 0, 0), dgk_d3_make(0, 1, 0));
    dgk_d3 n = dgk_d3_norm(dgk_d3_make(3, 0, 4)), zero = dgk_d3_norm(dgk_d3_make(0, 0, 0));
    dgk_d3 mid = dgk_d3_lerp(dgk_d3_make(0, 0, 0), dgk_d3_make(2, 4, 6), 0.5);
    CHECK(r.x == 1.5f && r.y == 0.5f && r.z == 3.25f, "relative %g %g %g", r.x, r.y, r.z);
    CHECK(z.x == 0 && z.y == 0 && z.z == 1 && zd.z == 1.0, "cross");
    CHECK(fabs(n.x - 0.6) < 1e-15 && fabs(n.z - 0.8) < 1e-15 && fabs(dgk_d3_len(n) - 1.0) < 1e-15, "norm %g %g",
          n.x, n.z);
    CHECK(zero.x == 0 && zero.y == 0 && zero.z == 0, "the zero vector normalised");
    CHECK(mid.x == 1 && mid.y == 2 && mid.z == 3, "lerp");
    CHECK(dgk_d3_dist(dgk_d3_make(1, 2, 3), dgk_d3_make(4, 6, 3)) == 5.0, "dist");
    CHECK(dgk_v3_len(dgk_v3_norm(dgk_v3_make(0, 3, 4))) > 0.9999f, "v3 norm");
    CHECK(DGK_KEY_LETTER('W') == 26 && DGK_KEY_DIGIT(0) == DGK_KEY_0 && DGK_KEY_DIGIT(1) == DGK_KEY_1 &&
              DGK_KEY_FN(12) == DGK_KEY_F12,
          "key names");
}

/* The music: a General MIDI flute's A (440 Hz) through GLOS's FM driver and instruments on the emulated OPL3,
 * out of the mixer. */
static double tone(const int16_t *pcm, int n, double hz, uint32_t rate)   /* its power at hz, Goertzel's */
{
    double k = 2.0 * cos(2.0 * 3.14159265358979 * hz / rate), s1 = 0, s2 = 0, s0;
    int i;
    for (i = 0; i < n; i++) {
        s0 = pcm[i] + k * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return (s1 * s1 + s2 * s2 - k * s1 * s2) / ((double)n * n);
}

static double rms(const int16_t *pcm, int n)
{
    double e = 0;
    int i;
    for (i = 0; i < n; i++)
        e += (double)pcm[i] * pcm[i];
    return sqrt(e / n);
}

static void music(void)
{
    static int16_t pcm[22050];
    uint32_t h1, n1;
    double on, a4, a5, below, quiet;
    dgk_mix_init(22050);
    CHECK(dgk_midi_open(DGK_MIDI_OFF) == DGK_MIDI_OFF, "OFF opened something");
    CHECK(dgk_midi_open(DGK_MIDI_MPU) == DGK_MIDI_OFF, "an MPU-401 off DOS");
    CHECK(dgk_midi_open(DGK_MIDI_AUTO) == DGK_MIDI_FM, "no FM chip: %s", dgk_midi_name());
    dgk_midi_send(0, 0xC0, 73, 0);                   /* the flute */
    dgk_midi_send(10, 0x90, 69, 110);                /* A, 440 Hz */
    dgk_midi_send(600, 0x80, 69, 0);
    dgk_midi_service(0);
    dgk_mix_render(pcm, 2205);                       /* 100 ms: not yet due */
    quiet = rms(pcm, 2205);
    dgk_midi_service(100);
    dgk_mix_render(pcm, 2205);                       /* the attack */
    dgk_mix_render(pcm, 8820);                       /* 400 ms of it */
    on = rms(pcm, 8820);
    a4 = tone(pcm, 8820, 440, 22050);
    a5 = tone(pcm, 8820, 880, 22050);
    below = tone(pcm, 8820, 415.3, 22050);
    CHECK(quiet < 1.0, "sound before the note: rms %.1f", quiet);
    CHECK(on > 300, "the note too quiet: rms %.1f", on);
    CHECK(a4 > 20 * below && a4 > a5, "not an A: 440 Hz %.0f, 415 Hz %.0f, 880 Hz %.0f", a4, below, a5);
    dgk_midi_service(700);
    dgk_mix_render(pcm, 22050);                      /* off: the release, then silence */
    CHECK(rms(pcm + 17640, 4410) < 2.0, "still sounding after its release: rms %.1f", rms(pcm + 17640, 4410));
    h1 = dgk_midi_hash(), n1 = dgk_midi_count();
    CHECK(n1 == 3, "%u messages counted", (unsigned)n1);
    dgk_midi_send(800, 0x90, 60, 100);               /* a note at half volume: quieter by 6 dB or so */
    dgk_midi_volume(50);
    dgk_midi_service(800);
    dgk_mix_render(pcm, 4410);
    dgk_mix_render(pcm, 8820);
    on = rms(pcm, 8820);
    dgk_midi_panic();
    dgk_mix_render(pcm, 22050);
    CHECK(rms(pcm + 17640, 4410) < 2.0, "sounding after a panic: rms %.1f", rms(pcm + 17640, 4410));
    CHECK(on > 50, "half volume silent: rms %.1f", on);
    CHECK(dgk_midi_hash() != h1 && dgk_midi_count() == 4, "the hash or count did not move");
    dgk_midi_close();
    CHECK(!strcmp(dgk_midi_name(), "none"), "closed, yet %s", dgk_midi_name());
}

int main(void)
{
    loop_input();
    vectors();
    music();
    printf("test_dgk: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}
