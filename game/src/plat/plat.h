/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — platform abstraction
   One interface, two implementations:
     headless.c — no window, scripted input, frame captures. Used by CI smoke
                  tests and by `DividedHorizon.exe --headless`, which means the
                  *shipped exe* can be verified on a machine with no display
                  (Spec 18.4: never fall through to a broken build).
     win32.c    — real window, keyboard/mouse, GL11 context (M1b).
   Game code only ever sees PlatInput, so input devices and OS quirks stay out
   of gameplay.
   ══════════════════════════════════════════════════════════════════════════ */
#ifndef DH_PLAT_H
#define DH_PLAT_H

#include "../core/dh_types.h"

/* ── abstract buttons (device-independent, Spec 14 remappable) ─────────── */
#define BTN_JUMP    (1u << 0)
#define BTN_SPRINT  (1u << 1)
#define BTN_CROUCH  (1u << 2)
#define BTN_ROLL    (1u << 3)
#define BTN_FIRE    (1u << 4)
#define BTN_AIM     (1u << 5)
#define BTN_USE     (1u << 6)
#define BTN_RELOAD  (1u << 7)
#define BTN_MELEE   (1u << 8)
#define BTN_WALK    (1u << 9)
#define BTN_PHOTO   (1u << 10)
#define BTN_MAP     (1u << 11)
#define BTN_MENU    (1u << 12)
#define BTN_PAUSE   (1u << 13)
#define BTN_UP      (1u << 14)
#define BTN_DOWN    (1u << 15)
#define BTN_LEFT    (1u << 16)
#define BTN_RIGHT   (1u << 17)
#define BTN_SLOT1   (1u << 18)   /* M2: weapon slots (sidearm / primary1 / primary2) */
#define BTN_SLOT2   (1u << 19)
#define BTN_SLOT3   (1u << 20)
#define BTN_ARENA   (1u << 21)   /* M2: deploy to the combat arena (dev/DoD) */

typedef struct {
    uint32_t buttons;        /* held this frame */
    uint32_t pressed;        /* went down this frame (edge) */
    float    mx, my;         /* move axes, -1..1 (my = forward) */
    float    look_dx, look_dy;
    int      quit;           /* window closed / script ended */
} PlatInput;

typedef struct Plat Plat;

struct Plat {
    const char *name;
    int   width, height;
    int   running;
    void *user;

    /* Fill `in` for this frame. Returns 0 when the app should exit. */
    int   (*poll)(Plat *p, PlatInput *in);
    /* Hand the finished software framebuffer to the platform (window/capture).
       GL backends ignore `fb` and swap their own context. */
    void  (*present)(Plat *p, const uint32_t *fb, int w, int h);
    void  (*destroy)(Plat *p);
};

/* Wall-clock helpers shared by all platforms. */
uint64_t plat_now_us(void);
void     plat_sleep_ms(int ms);

Plat *plat_headless_create(int w, int h, const char *script_path,
                           float duration_s, const char *shot_dir);
Plat *plat_win32_create(int w, int h, const char *title);   /* M1b */

void  plat_destroy(Plat *p);
/* Frames captured so far by the headless platform (0 for windowed). */
int   plat_headless_shots_taken(void);

#endif /* DH_PLAT_H */
