/* dgk/app.h - the main loop: fixed 60 Hz simulation ticks (at most four a
 * frame), then a draw with the fraction of a tick that has passed, then the
 * swap. SDL gets a turn (dgk_service) at the start of each frame, after each
 * tick and just before the swap: on DOS its threads switch only there, and
 * the Sound Blaster's ring holds about 45 ms.
 *
 * Command line (all targets):
 *   -mode WxH       window / screen size (default 640x480)
 *   -novsync        swaps do not wait for the retrace
 *   -frames N       quit after N frames
 *   -fixed          one tick per frame (deterministic frames for tests)
 *   -shot F:NAME    save frame F as NAME (DOS: C:\OUT\NAME.PPM; else out/NAME.ppm)
 *   -test           report on COM1 with the HX- protocol (DOS; Loop A)
 *   -nosound        no audio device
 *   -nodraw         draw only the frames -shot saves (long simulation tests)
 *   -noexit         with -test: do not end the Loop A run at the end (a job
 *                   that runs more programs after this one)
 *
 * The first joystick (a DOS gameport stick or wheel through SDL) is read
 * into dgk_app.joy_*, and the mouse into dgk_app.mouse_* (on DOS through
 * the INT 33h driver, under GLOS through its pointer). Typed text arrives
 * in dgk_app.text while dgk_text_input(1) is in force.
 *
 * Headless, DGK_INPUT (or DGK_JOY) scripts input: a comma list of items,
 * each applied at its tick (as Loop A's --keys items are in 86Box):
 *   TICK:axis:N:VALUE      joystick axis N (-32768..32767)
 *   TICK:button:N:0|1      joystick button N up, down
 *   TICK:key:SCANCODE:1|0  a key down, up
 *   TICK:mouse:X:Y         the pointer moves to X,Y (drawing pixels)
 *   TICK:mrel:DX:DY        the mouse moves by DX,DY (relative mode)
 *   TICK:mbutton:N:0|1     mouse button N (DGK_MOUSE_*) up, down
 *   TICK:wheel:N           N wheel notches (positive: away from the player)
 *   TICK:text:CHARS        CHARS typed (no commas)
 */
#ifndef DGK_APP_H
#define DGK_APP_H

#include "dgk/base.h"

#define DGK_TICK_HZ 60

/* Keys the kit names: SDL (USB HID) scancodes, so positions on the
 * keyboard, not the characters a layout prints (text input gives those).
 * Letters, digits and function keys run in order: DGK_KEY_LETTER('W'),
 * DGK_KEY_DIGIT(0..9), DGK_KEY_FN(1..12). */
enum {
    DGK_KEY_A = 4, DGK_KEY_Z = 29,
    DGK_KEY_1 = 30, DGK_KEY_0 = 39,
    DGK_KEY_RETURN = 40, DGK_KEY_ESCAPE = 41, DGK_KEY_BACKSPACE = 42, DGK_KEY_TAB = 43, DGK_KEY_SPACE = 44,
    DGK_KEY_MINUS = 45, DGK_KEY_EQUALS = 46, DGK_KEY_LBRACKET = 47, DGK_KEY_RBRACKET = 48,
    DGK_KEY_SEMICOLON = 51, DGK_KEY_APOSTROPHE = 52, DGK_KEY_GRAVE = 53, DGK_KEY_COMMA = 54,
    DGK_KEY_PERIOD = 55, DGK_KEY_SLASH = 56,
    DGK_KEY_F1 = 58, DGK_KEY_F12 = 69,
    DGK_KEY_INSERT = 73, DGK_KEY_HOME = 74, DGK_KEY_PAGEUP = 75, DGK_KEY_DELETE = 76, DGK_KEY_END = 77,
    DGK_KEY_PAGEDOWN = 78,
    DGK_KEY_RIGHT = 79, DGK_KEY_LEFT = 80, DGK_KEY_DOWN = 81, DGK_KEY_UP = 82,
    DGK_KEY_KP_MINUS = 86, DGK_KEY_KP_PLUS = 87, DGK_KEY_KP_ENTER = 88,
    DGK_KEY_LCTRL = 224, DGK_KEY_LSHIFT = 225, DGK_KEY_LALT = 226,
    DGK_KEY_RCTRL = 228, DGK_KEY_RSHIFT = 229, DGK_KEY_RALT = 230,
    DGK_KEY_MAX = 512
};
#define DGK_KEY_LETTER(c) (DGK_KEY_A + ((c) - 'A'))
#define DGK_KEY_DIGIT(d)  ((d) == 0 ? DGK_KEY_0 : DGK_KEY_1 + (d) - 1)
#define DGK_KEY_FN(n)     (DGK_KEY_F1 + (n) - 1)

#define DGK_JOY_AXES 4
#define DGK_JOY_BUTTONS 8

enum { DGK_MOUSE_LEFT, DGK_MOUSE_RIGHT, DGK_MOUSE_MIDDLE, DGK_MOUSE_BUTTONS };
#define DGK_TEXT_MAX 32                   /* bytes of text a tick holds, with its terminating 0 */

typedef struct dgk_app_desc {
    const char *title;
    int (*init)(void *u);                 /* after the window and GL exist; 0 = go on */
    void (*tick)(void *u);                /* one simulation step of 1/DGK_TICK_HZ s */
    void (*draw)(void *u, float alpha);   /* alpha: 0..1 of a tick since the last one */
    void (*quit)(void *u);
} dgk_app_desc;

typedef struct dgk_app_state {
    int width, height;                    /* drawing size in pixels */
    uint32_t frame, ticks;                /* frames drawn, ticks run */
    uint8_t key_down[DGK_KEY_MAX];        /* held */
    uint8_t key_pressed[DGK_KEY_MAX];     /* went down since the last tick */
    int joy_present;                      /* a joystick is open */
    char joy_name[40];
    int16_t joy_axis[DGK_JOY_AXES];       /* -32768..32767 */
    uint8_t joy_down[DGK_JOY_BUTTONS];
    uint8_t joy_pressed[DGK_JOY_BUTTONS]; /* went down since the last tick */
    int mouse_present;                    /* SDL lists a mouse, or one has reported (DOS's driver lists none) */
    int mouse_x, mouse_y;                 /* the pointer in drawing pixels from the top left */
    int mouse_dx, mouse_dy;               /* motion since the last tick in pixels, in either mode */
    int mouse_wheel;                      /* notches since the last tick, positive away from the player */
    uint8_t mouse_down[DGK_MOUSE_BUTTONS];
    uint8_t mouse_pressed[DGK_MOUSE_BUTTONS]; /* went down since the last tick */
    int mouse_relative;                   /* dgk_mouse_relative(1) is in force */
    char text[DGK_TEXT_MAX];              /* typed since the last tick, UTF-8, with dgk_text_input(1) */
    int quit;                             /* set to end the loop */
    int fixed;                            /* -fixed */
    uint32_t frame_us;                    /* the last frame, swap to swap */
} dgk_app_state;

extern dgk_app_state dgk_app;

int  dgk_app_run(const dgk_app_desc *d, void *u, int argc, char **argv);
/* Time frames (swap to swap) and their triangles into a DOSBench test
 * (dgk/bench.h), from the next frame until stopped. */
void dgk_app_bench_start(const char *test, int warmup_frames);
void dgk_app_bench_stop(const char *status, const char *notes);
int  dgk_app_modes(int (*wh)[2], int max);   /* screen sizes on offer (DOS: DOS-GL's), smallest first */
void dgk_app_vsync(int on);
/* Relative mode: the pointer is hidden and held, and only motion counts
 * (mouse-look, flight). Off: the pointer moves freely (menus, maps). */
void dgk_mouse_relative(int on);
void dgk_mouse_cursor(int show);          /* the system's pointer over the window (Linux, GLOS) */
/* Text input: typed characters, in the keyboard's layout, fill dgk_app.text
 * each tick (names, chat). Keys still arrive as keys. */
void dgk_text_input(int on);
void dgk_service(void);                   /* give SDL (and its audio thread) a turn */
void dgk_service_if_due(void);            /* the same, if 8 ms have passed since the last */
uint64_t dgk_now_us(void);

#endif
