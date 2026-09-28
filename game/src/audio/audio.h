/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — audio (M7, Spec §14 / §32)
   Honest scope: every sound is SYNTHESISED at startup from code (no sample
   packs are reachable from the build sandbox — see M7 report). A small
   software mixer (22.05 kHz mono, 24 voices) plays one-shot SFX and an
   adaptive two-layer procedural score (calm pad ↔ combat pulse) that
   crossfades on an intensity value. Pure C, no platform code: the Win32
   layer streams audio_mix() into waveOut; headless runs mix into scratch so
   tests can measure it.
   ══════════════════════════════════════════════════════════════════════════ */
#ifndef DH_AUDIO_H
#define DH_AUDIO_H

#include <stdint.h>

#define AUDIO_RATE   22050
#define AUDIO_VOICES 24

typedef enum {
    SFX_SHOT_PISTOL = 0, SFX_SHOT_RIFLE, SFX_SHOT_SHOTGUN, SFX_SHOT_SNIPER,
    SFX_RELOAD, SFX_DRY, SFX_STEP, SFX_STEP_WATER, SFX_HIT, SFX_KILL,
    SFX_HURT, SFX_UI_CLICK, SFX_CARD, SFX_MISSION, SFX_ALARM, SFX_SIREN,
    SFX_PICKUP, SFX_JUMP, SFX_LAND, SFX_BIRDS, SFX_N
} SfxId;

typedef struct {
    float master, music, sfx;     /* 0..1 (from Settings) */
    float duck;                   /* 0..1 extra attenuation (pause, cards) */
} AudioMix;

int   audio_init(void);                        /* synthesises the bank */
void  audio_shutdown(void);
int   audio_ready(void);
void  audio_set_mix(const AudioMix *m);
/* play one-shot; vol 0..1, pitch 0.5..2 (1 = natural). Returns voice or -1. */
int   audio_play(SfxId id, float vol, float pitch);
/* adaptive score: 0 = explore pad, 1 = full combat layer; eased internally */
void  audio_set_intensity(float target);
void  audio_set_music_on(int on);
/* fill `n` mono int16 samples (called by the platform stream) */
void  audio_mix(int16_t *out, int n);

/* introspection for tests / HUD */
int   audio_active_voices(void);
float audio_intensity(void);
int   audio_sfx_len(SfxId id);                /* samples in bank */
const char *audio_sfx_name(SfxId id);
uint64_t audio_plays_total(void);

#endif
