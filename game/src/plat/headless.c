/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — headless platform
   Runs the real game loop with scripted input and captures frames to disk.
   This is how every traversal/combat behaviour is verified in CI without a
   GPU, and it is also a shipping mode (`--headless`) so a user on a machine
   with no display can still prove the exe works.

   Script format (whitespace separated, '#' comments):
       key  <time> <mx> <my> <buttons> <look_dx> <look_dy>
       shot <time> <name>
       end  <time>
   A keyframe's input is held until the next keyframe. `buttons` may be
   decimal or 0x-prefixed hex. With no script file a built-in "prove the
   traversal works" script is used instead.
   ══════════════════════════════════════════════════════════════════════════ */
#define _POSIX_C_SOURCE 200809L
#include "plat.h"
#include "../core/dh_log.h"
#include "../rend/rend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_KEYS 256
#define MAX_SHOTS 64

typedef struct {
    float t, mx, my, lx, ly;
    uint32_t buttons;
} KeyFrame;

typedef struct {
    float t;
    char  name[64];
    int   done;
} Shot;

typedef struct {
    KeyFrame keys[MAX_KEYS]; int nkeys;
    Shot shots[MAX_SHOTS];   int nshots;
    float duration;
    float sim_t;
    uint32_t prev_buttons;
    char shot_dir[256];
    int shots_taken;
    int uses_script_file;
} Headless;

static Headless *g_hl = NULL;

/* ── clock ─────────────────────────────────────────────────────────────── */
uint64_t plat_now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ull + (uint64_t)(ts.tv_nsec / 1000);
}
void plat_sleep_ms(int ms) {
    if (ms <= 0) return;
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

/* ── default script: run, sprint, jump, vault, swim, look around ───────── */
/* WHY a function and not a macro: the first version used KEY(t,mx,my,...) and
   the macro parameters substituted the *struct member names* too, turning
   `h->keys[k].t` into `h->keys[k].0.0f`. Real functions do not do that. */
static void add_key(Headless *h, float t, float mx, float my,
                    uint32_t buttons, float lx, float ly) {
    if (h->nkeys >= MAX_KEYS) return;
    KeyFrame *k = &h->keys[h->nkeys++];
    k->t = t; k->mx = mx; k->my = my; k->buttons = buttons; k->lx = lx; k->ly = ly;
}
static void add_shot(Headless *h, float t, const char *name) {
    if (h->nshots >= MAX_SHOTS) return;
    Shot *s = &h->shots[h->nshots++];
    s->t = t; s->done = 0;
    dh_strcpy_safe(s->name, sizeof(s->name), name);
}

static void default_script(Headless *h) {
    add_key(h, 0.0f,  0.0f, 0.0f, 0,                          0.0f,  0.0f); /* stand */
    add_key(h, 0.6f,  0.0f, 1.0f, 0,                          0.0f,  0.0f); /* walk */
    add_key(h, 2.0f,  0.0f, 1.0f, BTN_SPRINT,                 0.0f,  0.0f); /* sprint */
    add_key(h, 3.0f,  0.0f, 1.0f, BTN_SPRINT | BTN_JUMP,      0.0f,  0.0f); /* jump */
    add_key(h, 3.2f,  0.0f, 1.0f, BTN_SPRINT,                 0.0f,  0.0f);
    add_key(h, 4.2f,  0.0f, 1.0f, BTN_JUMP,                   0.0f,  0.0f); /* hop */
    add_key(h, 4.4f,  0.0f, 1.0f, 0,                          0.0f,  0.0f);
    add_key(h, 5.4f,  1.0f, 0.0f, BTN_SPRINT,                 0.0f,  0.0f); /* strafe R */
    add_key(h, 6.2f, -1.0f, 0.4f, 0,                          0.30f, 0.0f); /* strafe L + turn */
    add_key(h, 7.0f,  0.0f, 1.0f, BTN_CROUCH,                 0.0f,  0.0f); /* crouch */
    add_key(h, 8.0f,  0.0f, 1.0f, BTN_SPRINT | BTN_JUMP,      0.0f,  0.0f); /* vault */
    add_key(h, 8.3f,  0.0f, 1.0f, BTN_SPRINT,                 0.0f,  0.0f);
    add_key(h, 9.5f,  0.0f, 0.0f, 0,                          0.0f, -0.12f);/* look up */
    add_key(h, 10.5f, 0.0f, 0.0f, 0,                          0.0f,  0.12f);/* look down */
    add_key(h, 11.5f, 0.0f, 1.0f, BTN_ROLL,                   0.0f,  0.0f); /* roll */
    add_key(h, 12.2f, 0.0f, 1.0f, 0,                          0.0f,  0.0f);
    h->duration = 14.0f;

    add_shot(h, 1.5f,  "m1_walk");
    add_shot(h, 3.1f,  "m1_sprint_jump");
    add_shot(h, 4.3f,  "m1_hop");
    add_shot(h, 6.5f,  "m1_strafe");
    add_shot(h, 7.6f,  "m1_crouch");
    add_shot(h, 8.6f,  "m1_vault");
    add_shot(h, 10.0f, "m1_vista");
    add_shot(h, 13.0f, "m1_course");
}

/* ── script parser ─────────────────────────────────────────────────────── */
static int parse_script(Headless *h, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { DH_WARN("plat", "script '%s' not found — using default", path); return 0; }
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *s = line;
        while (*s == ' ' || *s == '\t') s++;
        if (*s == '#' || *s == '\n' || *s == 0) continue;
        if (strncmp(s, "key", 3) == 0) {
            if (h->nkeys >= MAX_KEYS) continue;
            float t, mx, my, lx, ly; unsigned int b = 0;
            int n = sscanf(s + 3, "%f %f %f %i %f %f", &t, &mx, &my, &b, &lx, &ly);
            if (n < 4) continue;
            if (n < 6) { lx = 0.0f; ly = 0.0f; }
            KeyFrame *k = &h->keys[h->nkeys++];
            k->t = t; k->mx = mx; k->my = my; k->lx = lx; k->ly = ly;
            k->buttons = (uint32_t)b;
            if (t + 1.0f > h->duration) h->duration = t + 1.0f;
        } else if (strncmp(s, "shot", 4) == 0) {
            if (h->nshots >= MAX_SHOTS) continue;
            float t; char nm[64] = "shot";
            sscanf(s + 4, "%f %63s", &t, nm);
            Shot *sh = &h->shots[h->nshots++];
            sh->t = t; sh->done = 0;
            dh_strcpy_safe(sh->name, 64, nm);
        } else if (strncmp(s, "end", 3) == 0) {
            float t = 0.0f;
            if (sscanf(s + 3, "%f", &t) == 1) h->duration = t;
        }
    }
    fclose(f);
    h->uses_script_file = (h->nkeys > 0);
    DH_INFO("plat", "script '%s': %d keys, %d shots, %.1fs",
            path, h->nkeys, h->nshots, h->duration);
    return h->nkeys > 0;
}

/* ── vtable ────────────────────────────────────────────────────────────── */
static int hl_poll(Plat *p, PlatInput *in) {
    Headless *h = (Headless*)p->user;
    memset(in, 0, sizeof(*in));
    h->sim_t += DH_TICK_DT;

    if (h->sim_t > h->duration) { in->quit = 1; p->running = 0; return 0; }

    /* current keyframe = last one with t <= sim_t */
    const KeyFrame *cur = NULL;
    for (int i = 0; i < h->nkeys; i++) {
        if (h->keys[i].t <= h->sim_t) cur = &h->keys[i];
        else break;
    }
    if (cur) {
        in->mx = cur->mx; in->my = cur->my;
        in->look_dx = cur->lx; in->look_dy = cur->ly;
        in->buttons = cur->buttons;
    }
    in->pressed = in->buttons & ~h->prev_buttons;
    h->prev_buttons = in->buttons;
    return 1;
}

static void hl_present(Plat *p, const uint32_t *fb, int w, int h) {
    (void)fb; (void)w; (void)h;
    Headless *hl = (Headless*)p->user;
    for (int i = 0; i < hl->nshots; i++) {
        Shot *s = &hl->shots[i];
        if (s->done || s->t > hl->sim_t) continue;
        s->done = 1;
        char bmp[512], png[512];
        snprintf(bmp, sizeof(bmp), "%s/%s.bmp", hl->shot_dir, s->name);
        snprintf(png, sizeof(png), "%s/%s.png", hl->shot_dir, s->name);
        if (rend_save_bmp(bmp)) hl->shots_taken++;
        rend_save_png(png);
        DH_INFO("plat", "captured %s @ t=%.2fs", s->name, hl->sim_t);
    }
}

static void hl_destroy(Plat *p) {
    if (!p) return;
    free(p->user);
    free(p);
    g_hl = NULL;
}

int plat_headless_shots_taken(void) { return g_hl ? g_hl->shots_taken : 0; }

Plat *plat_headless_create(int w, int h, const char *script_path,
                           float duration_s, const char *shot_dir) {
    Plat *p = (Plat*)calloc(1, sizeof(Plat));
    Headless *hl = (Headless*)calloc(1, sizeof(Headless));
    if (!p || !hl) { free(p); free(hl); return NULL; }

    p->name = "headless";
    p->width = w; p->height = h;
    p->running = 1;
    p->user = hl;
    p->poll = hl_poll;
    p->present = hl_present;
    p->destroy = hl_destroy;

    dh_strcpy_safe(hl->shot_dir, sizeof(hl->shot_dir), shot_dir ? shot_dir : ".");
    hl->duration = duration_s > 0.0f ? duration_s : 14.0f;

    if (script_path && script_path[0]) {
        if (!parse_script(hl, script_path)) default_script(hl);
    } else {
        default_script(hl);
    }
    if (duration_s > 0.0f) hl->duration = duration_s;   /* CLI overrides script */

    g_hl = hl;
    DH_INFO("plat", "headless %dx%d, %.1fs, shots → %s", w, h, hl->duration, hl->shot_dir);
    return p;
}

void plat_destroy(Plat *p) {
    if (p && p->destroy) p->destroy(p);
}
