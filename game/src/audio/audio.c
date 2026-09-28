/* DIVIDED HORIZON — procedural SFX bank + software mixer (M7). See audio.h */
#include "audio.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define TAU 6.28318530718f

typedef struct { float *s; int n; } Bank;
typedef struct { int id; float pos, step, vol; int live; } Voice;

static Bank  g_bank[SFX_N];
static Voice g_v[AUDIO_VOICES];
static AudioMix g_mix = { 0.8f, 0.6f, 0.9f, 0.f };
static int   g_ready, g_music_on = 1;
static float g_int, g_int_target;
static double g_mt;               /* music clock, samples */
static uint32_t g_seed = 0x9E3779B9u;
static uint64_t g_plays;

static const char *k_names[SFX_N] = {
    "shot_pistol","shot_rifle","shot_shotgun","shot_sniper","reload","dry",
    "step","step_water","hit","kill","hurt","ui_click","card","mission",
    "alarm","siren","pickup","jump","land","birds","bark","jingle","whine","shutter" };

static float nz(void) {                 /* deterministic white noise -1..1 */
    g_seed ^= g_seed << 13; g_seed ^= g_seed >> 17; g_seed ^= g_seed << 5;
    return (float)(g_seed & 0xFFFFFF) / 8388608.f - 1.f;
}
static float *alloc_bank(SfxId id, float secs) {
    int n = (int)(secs * AUDIO_RATE);
    g_bank[id].s = (float *)calloc((size_t)n, sizeof(float));
    g_bank[id].n = g_bank[id].s ? n : 0;
    return g_bank[id].s;
}
/* gunshot: noise burst through a one-pole lowpass + low thump, exp decay */
static void syn_shot(SfxId id, float secs, float lp, float thump_hz, float decay, float crack) {
    float *b = alloc_bank(id, secs); if (!b) return;
    float y = 0.f, ph = 0.f;
    for (int i = 0; i < g_bank[id].n; i++) {
        float t = (float)i / AUDIO_RATE, env = expf(-t * decay);
        y += lp * (nz() - y);
        ph += TAU * thump_hz * (1.f - t * 1.5f) / AUDIO_RATE;
        float c = (t < 0.004f) ? nz() * crack : 0.f;
        b[i] = (y * 0.8f + sinf(ph) * 0.6f * expf(-t * decay * 1.6f)) * env + c;
    }
}
static void syn_tone(SfxId id, float secs, float f0, float f1, float decay, int square) {
    float *b = alloc_bank(id, secs); if (!b) return;
    float ph = 0.f; int n = g_bank[id].n;
    for (int i = 0; i < n; i++) {
        float t = (float)i / AUDIO_RATE, k = (float)i / n;
        ph += TAU * (f0 + (f1 - f0) * k) / AUDIO_RATE;
        float s = square ? (sinf(ph) > 0 ? 0.5f : -0.5f) : sinf(ph);
        float att = t < 0.005f ? t / 0.005f : 1.f;
        b[i] = s * att * expf(-t * decay);
    }
}
static void syn_click(SfxId id, float secs, float lp, float decay, int clicks) {
    float *b = alloc_bank(id, secs); if (!b) return;
    int n = g_bank[id].n; float y = 0.f;
    for (int c = 0; c < clicks; c++) {
        int st = (int)((float)c / clicks * n * 0.7f);
        for (int i = st; i < n; i++) {
            float t = (float)(i - st) / AUDIO_RATE;
            y += lp * (nz() - y);
            b[i] += y * expf(-t * decay) * 0.9f;
        }
    }
}
static void syn_notes(SfxId id, const float *hz, int count, float note_s) {
    float *b = alloc_bank(id, note_s * count + 0.4f); if (!b) return;
    for (int k = 0; k < count; k++) {
        int st = (int)(k * note_s * AUDIO_RATE);
        for (int i = st; i < g_bank[id].n; i++) {
            float t = (float)(i - st) / AUDIO_RATE, p = TAU * hz[k] * t;
            b[i] += (sinf(p) + 0.3f * sinf(2.f * p)) * 0.45f * expf(-t * 5.f)
                    * (t < 0.004f ? t / 0.004f : 1.f);
        }
    }
}

int audio_init(void) {
    if (g_ready) return 1;
    syn_shot(SFX_SHOT_PISTOL, 0.28f, 0.45f, 140.f, 18.f, 0.6f);
    syn_shot(SFX_SHOT_RIFLE,  0.38f, 0.60f, 110.f, 13.f, 0.9f);
    syn_shot(SFX_SHOT_SHOTGUN,0.55f, 0.35f,  80.f,  8.f, 1.0f);
    syn_shot(SFX_SHOT_SNIPER, 0.90f, 0.55f,  70.f,  5.f, 1.0f);
    syn_click(SFX_RELOAD, 0.45f, 0.7f, 90.f, 3);
    syn_click(SFX_DRY, 0.06f, 0.9f, 160.f, 1);
    syn_click(SFX_STEP, 0.09f, 0.18f, 60.f, 1);
    syn_click(SFX_STEP_WATER, 0.20f, 0.5f, 22.f, 2);
    syn_tone(SFX_HIT, 0.07f, 1900.f, 1500.f, 40.f, 0);
    syn_tone(SFX_KILL, 0.16f, 1200.f, 2400.f, 18.f, 0);
    syn_shot(SFX_HURT, 0.25f, 0.12f, 60.f, 14.f, 0.f);
    syn_tone(SFX_UI_CLICK, 0.04f, 900.f, 700.f, 60.f, 1);
    { const float n[] = { 392.f, 523.25f }; syn_notes(SFX_CARD, n, 2, 0.12f); }
    { const float n[] = { 392.f, 493.9f, 587.3f, 784.f }; syn_notes(SFX_MISSION, n, 4, 0.14f); }
    syn_tone(SFX_ALARM, 1.2f, 660.f, 880.f, 0.4f, 1);
    { /* siren: two-tone warble */
        float *b = alloc_bank(SFX_SIREN, 1.6f);
        if (b) { float ph = 0.f;
            for (int i = 0; i < g_bank[SFX_SIREN].n; i++) {
                float t = (float)i / AUDIO_RATE;
                ph += TAU * (720.f + 180.f * sinf(TAU * 1.25f * t)) / AUDIO_RATE;
                b[i] = sinf(ph) * 0.35f * (t < 0.05f ? t / 0.05f : 1.f) * (t > 1.4f ? (1.6f - t) / 0.2f : 1.f);
            } }
    }
    { const float n[] = { 880.f, 1318.5f }; syn_notes(SFX_PICKUP, n, 2, 0.06f); }
    syn_click(SFX_JUMP, 0.10f, 0.25f, 40.f, 1);
    syn_shot(SFX_LAND, 0.14f, 0.10f, 70.f, 30.f, 0.f);
    { /* birds: chirp series */
        float *b = alloc_bank(SFX_BIRDS, 1.0f);
        if (b) for (int c = 0; c < 7; c++) {
            int st = (int)(c * 0.13f * AUDIO_RATE); float ph = 0.f, f = 2600.f + 700.f * nz();
            for (int i = st; i < st + (int)(0.07f * AUDIO_RATE) && i < g_bank[SFX_BIRDS].n; i++) {
                float t = (float)(i - st) / AUDIO_RATE;
                ph += TAU * (f + 9000.f * t) / AUDIO_RATE;
                b[i] += sinf(ph) * 0.3f * sinf(3.14159f * t / 0.07f);
            }
        }
    }
    { /* M9 Poncho bark: two gruff formant pulses (saw + noise, falling pitch) */
        float *b = alloc_bank(SFX_BARK, 0.42f);
        if (b) for (int k = 0; k < 2; k++) {
            int st = (int)(k * 0.2f * AUDIO_RATE); float ph = 0.f;
            for (int i = st; i < st + (int)(0.13f * AUDIO_RATE) && i < g_bank[SFX_BARK].n; i++) {
                float t = (float)(i - st) / AUDIO_RATE;
                ph += (520.f - 1800.f * t) / AUDIO_RATE; ph -= floorf(ph);
                float env = sinf(3.14159f * t / 0.13f);
                b[i] = ((ph * 2.f - 1.f) * 0.5f + nz() * 0.25f) * env * 0.7f;
            }
        }
    }
    { /* collar jingle: three detuned bell partials */
        float *b = alloc_bank(SFX_JINGLE, 0.35f);
        if (b) for (int i = 0; i < g_bank[SFX_JINGLE].n; i++) {
            float t = (float)i / AUDIO_RATE, e = expf(-t * 14.f);
            b[i] = (sinf(TAU * 3520.f * t) + 0.7f * sinf(TAU * 4410.f * t) + 0.5f * sinf(TAU * 5280.f * t + 1.f)) * 0.14f * e;
        }
    }
    syn_tone(SFX_WHINE, 0.7f, 900.f, 620.f, 5.f, 0);
    syn_click(SFX_SHUTTER, 0.08f, 0.5f, 90.f, 1);
    for (int i = 0; i < SFX_N; i++) if (!g_bank[i].s) return 0;
    g_ready = 1;
    return 1;
}
void audio_shutdown(void) {
    for (int i = 0; i < SFX_N; i++) { free(g_bank[i].s); g_bank[i].s = NULL; g_bank[i].n = 0; }
    memset(g_v, 0, sizeof g_v); g_ready = 0;
}
int  audio_ready(void) { return g_ready; }
void audio_set_mix(const AudioMix *m) { if (m) g_mix = *m; }
void audio_set_intensity(float t) { g_int_target = t < 0 ? 0 : t > 1 ? 1 : t; }
void audio_set_music_on(int on) { g_music_on = on; }
float audio_intensity(void) { return g_int; }
int  audio_sfx_len(SfxId id) { return (id >= 0 && id < SFX_N) ? g_bank[id].n : 0; }
const char *audio_sfx_name(SfxId id) { return (id >= 0 && id < SFX_N) ? k_names[id] : "?"; }
uint64_t audio_plays_total(void) { return g_plays; }
int audio_active_voices(void) { int c = 0; for (int i = 0; i < AUDIO_VOICES; i++) c += g_v[i].live; return c; }

int audio_play(SfxId id, float vol, float pitch) {
    if (!g_ready || id < 0 || id >= SFX_N || vol <= 0.f) return -1;
    int best = -1; float oldest = -1.f;
    for (int i = 0; i < AUDIO_VOICES; i++) {
        if (!g_v[i].live) { best = i; break; }
        if (g_v[i].pos > oldest) { oldest = g_v[i].pos; best = i; }   /* steal oldest */
    }
    Voice *v = &g_v[best];
    v->id = id; v->pos = 0.f; v->step = pitch < 0.25f ? 0.25f : pitch > 4.f ? 4.f : pitch;
    v->vol = vol > 1.f ? 1.f : vol; v->live = 1;
    g_plays++;
    return best;
}

/* procedural score: Am–F–C–G progression, 84 BPM. Pad = detuned saws through
   a soft lowpass; combat layer = kick/hat pulse + bass ostinato. */
static float music_sample(void) {
    static const float roots[4] = { 220.f, 174.61f, 261.63f, 196.f };
    static float lp_state;
    double t = g_mt / AUDIO_RATE;
    double beat = t * (84.0 / 60.0);
    int bar = (int)(beat / 4.0) & 3;
    float r = roots[bar], pad = 0.f;
    const float iv[3] = { 1.f, 1.189f, 1.498f };          /* minor-ish triad */
    for (int k = 0; k < 3; k++) {
        double f = r * iv[k];
        pad += (float)(fmod(t * f, 1.0) * 2.0 - 1.0) * 0.5f;
        pad += (float)(fmod(t * f * 1.004, 1.0) * 2.0 - 1.0) * 0.5f;
    }
    lp_state += 0.04f * (pad - lp_state);
    float swell = 0.75f + 0.25f * sinf((float)(t * 0.2));
    float calm = lp_state * 0.10f * swell;
    float fb = (float)(beat - floor(beat));
    float kick = sinf(TAU * 55.f * fb * (1.f - fb * 0.5f) * 0.25f * 4.f) * expf(-fb * 14.f);
    float hb = (float)(beat * 2.0 - floor(beat * 2.0));
    float hat = nz() * expf(-hb * 40.f) * 0.25f;
    float bass = sinf((float)(TAU * r * 0.5 * t)) * (fb < 0.5f ? 1.f : 0.4f) * 0.35f;
    float combat = (kick * 0.6f + hat + bass) * 0.22f;
    return calm * (1.f - 0.4f * g_int) + combat * g_int;
}

void audio_mix(int16_t *out, int n) {
    float sfxg = g_mix.master * g_mix.sfx * (1.f - 0.6f * g_mix.duck);
    float musg = g_mix.master * g_mix.music * (1.f - 0.7f * g_mix.duck) * (g_music_on ? 1.f : 0.f);
    for (int i = 0; i < n; i++) {
        if ((i & 63) == 0) g_int += (g_int_target - g_int) * 0.0015f;  /* ~2 s ease */
        float acc = 0.f;
        if (g_ready) for (int v = 0; v < AUDIO_VOICES; v++) {
            Voice *vo = &g_v[v]; if (!vo->live) continue;
            const Bank *b = &g_bank[vo->id];
            int p = (int)vo->pos;
            if (p + 1 >= b->n) { vo->live = 0; continue; }
            float fr = vo->pos - p;
            acc += (b->s[p] * (1.f - fr) + b->s[p + 1] * fr) * vo->vol;
            vo->pos += vo->step;
        }
        float s = acc * sfxg * 0.5f + (musg > 0.f ? music_sample() * musg : 0.f);
        g_mt += 1.0;
        s = s / (1.f + fabsf(s));                         /* soft clip */
        out[i] = (int16_t)(s * 30000.f);
    }
}
