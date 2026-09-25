/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — SettingsManager (autoload-singleton equivalent)
   Covers Spec 6.11 (settings), 15.3 (quality presets), 33 (accessibility),
   37.2 (first-run wizard), 29.2 (mutators), 34 (i18n language).
   Rule: NOTHING about balance lives here — only player preferences.
   ══════════════════════════════════════════════════════════════════════════ */
#ifndef DH_SETTINGS_H
#define DH_SETTINGS_H

#include "dh_types.h"

/* ── input actions (rebindable — Spec 12 / 33 / 18.9) ───────────────────── */
typedef enum {
    ACT_MOVE_FWD = 0, ACT_MOVE_BACK, ACT_MOVE_LEFT, ACT_MOVE_RIGHT,
    ACT_SPRINT, ACT_CROUCH, ACT_JUMP, ACT_WALK_TOGGLE,
    ACT_FIRE, ACT_ADS, ACT_RELOAD, ACT_MELEE, ACT_TAKEDOWN,
    ACT_THROWABLE_1, ACT_THROWABLE_2, ACT_USE_TOOL,
    ACT_WEAPON_1, ACT_WEAPON_2, ACT_WEAPON_3, ACT_WEAPON_4, ACT_WEAPON_NEXT, ACT_WEAPON_PREV,
    ACT_INTERACT, ACT_ENTER_VEHICLE, ACT_DOG_COMMAND, ACT_TAG,
    ACT_HOLD_BREATH, ACT_WHISTLE, ACT_LEAN_LEFT, ACT_LEAN_RIGHT,
    ACT_PAUSE, ACT_MAP, ACT_INVENTORY, ACT_SKILLS, ACT_PHONE,
    ACT_PHOTO_MODE, ACT_PING, ACT_RADIO_NEXT, ACT_EMOTE_WHEEL,
    ACT_VEHICLE_HANDBRAKE, ACT_VEHICLE_HORN, ACT_VEHICLE_BOOST, ACT_VEHICLE_CAMERA,
    ACT_DOG_STAY, ACT_DOG_FOLLOW, ACT_DOG_FETCH, ACT_DOG_DISTRACT, ACT_PET_DOG,
    ACT_MENU_UP, ACT_MENU_DOWN, ACT_MENU_LEFT, ACT_MENU_RIGHT,
    ACT_MENU_ACCEPT, ACT_MENU_BACK, ACT_MENU_TAB,
    ACT_COUNT
} DhAction;

/* Hold vs toggle mode for the actions Spec 33 explicitly requires both for. */
typedef enum { BIND_HOLD = 0, BIND_TOGGLE = 1 } BindMode;

#define KEY_NAME_LEN 16
typedef struct {
    char key[KEY_NAME_LEN];       /* primary binding, platform-neutral name  */
    char key_alt[KEY_NAME_LEN];   /* secondary binding                       */
    char pad[KEY_NAME_LEN];       /* gamepad glyph name                      */
    BindMode mode;
} KeyBind;

/* ── quality presets (Spec 15.3) ───────────────────────────────────────── */
typedef enum { QUALITY_LOW = 0, QUALITY_MEDIUM, QUALITY_HIGH, QUALITY_ULTRA, QUALITY_COUNT } QualityPreset;
typedef enum { DIFF_EXPLORER = 0, DIFF_NORMAL, DIFF_HARDCORE, DIFF_CUSTOM, DIFF_COUNT } Difficulty;
typedef enum { AA_OFF = 0, AA_FXAA, AA_TAA, AA_COUNT } AaMode;

/* ── mutators (Spec 29.2 — max 3 active, save is flagged) ───────────────── */
#define MUT_COUNT 8
typedef enum {
    MUT_BIG_HEAD = 0, MUT_LOW_GRAVITY, MUT_PAINTBALL, MUT_MIRROR_WORLD,
    MUT_TURBO_NIGHT, MUT_INFINITE_AMMO, MUT_PONCHO_GIANT, MUT_PHOTOREAL
} Mutator;

#define LANG_COUNT 13
typedef enum {
    LANG_EN = 0, LANG_ES, LANG_PT_BR, LANG_FR, LANG_DE, LANG_IT, LANG_PL,
    LANG_TR, LANG_ZH_HANS, LANG_JA, LANG_KO, LANG_HI, LANG_AR
} DhLanguage;

typedef struct {
    int schema;                 /* always 1 */

    /* display */
    int   fullscreen, vsync, quality;
    int   res_w, res_h;
    float fov;                  /* 70..110 (Spec 6.11) */
    float render_scale;         /* 0.50..1.00 (Spec 58.3) */
    int   motion_blur, shadows, ssao, aa_mode, hd_textures;
    float draw_distance;        /* metres: 60/100/160/250 */
    float foliage_density, traffic_density, ped_density;
    float brightness;           /* 0.5..2.0 gamma pattern (Spec 33) */
    int   potato_hud;           /* Spec 58.3 */

    /* audio */
    float vol_master, vol_music, vol_sfx, vol_voice, vol_radio;
    float duck_strength, dynamic_range;
    int   subtitle_lang;
    int   adaptive_music;

    /* gameplay */
    int   difficulty;
    float enemy_damage, enemy_aim, detection_speed, puzzle_timer_scale;
    float aim_assist, aim_magnetism, slow_motion;
    int   auto_win_qte, hints, gore_reduced, damage_numbers;
    int   camera_shake, hit_marker, crosshair_on, iron_sight_hold, tutorial_level;
    int   killcam_lite;
    float day_night_minutes;    /* Spec 6.7 default 24 */
    float heatseed;             /* Chaos Dial (Spec 29.3) 0..1 */

    /* controls */
    int   invert_y, invert_x;
    float sensitivity, ads_sensitivity, aim_smoothing;
    int   rumble;
    float rumble_strength;   /* 0..1 — clang caught this declared as int, which
                                silently truncated the player's chosen strength
                                to 0 on every load. gcc accepted it quietly. */
    int   glyph_set;            /* 0 auto, 1 keyboard, 2 xbox, 3 generic */
    KeyBind binds[ACT_COUNT];
    int   one_hand_preset;

    /* accessibility (Spec 33 CORE — every one must persist, Spec 39) */
    int   subtitles, subtitle_size, subtitle_bg_opacity, speaker_colors, subtitle_max_cps;
    int   colorblind_mode, reticle_color, reticle_shape;
    int   photosensitivity;     /* kills shake/strobe/bloom spikes/chromatic FX */
    float ui_scale;             /* 0.80..1.50 */
    float safe_area;            /* 0.90..1.00 */
    int   dyslexic_font, narration, audio_cues, co_pilot;
    int   high_contrast_outline, reduced_gore_cw;

    /* meta */
    int   language;
    int   first_run_done;
    int   recommended_quality;  /* set by first-run GPU detect (Spec 37.2) */
    int   mutators[MUT_COUNT];
    int   ng_plus_allowed;      /* false while mutators active (Spec 29.2) */
    int   session_timer_min;    /* 0 = off (Spec 60 wellbeing) */
    int   clean_capture;        /* HUD-less render (Spec 60) */
} Settings;

Settings *settings(void);                     /* the singleton */
void      settings_defaults(Settings *s);
void      settings_apply_preset(Settings *s, QualityPreset q);   /* 15.3 table */
int       settings_save(const Settings *s);                      /* user://settings.json */
int       settings_load(Settings *s);
void      settings_clamp(Settings *s);                           /* never out of range */
int       settings_mutator_active(int m);
int       settings_mutator_count(void);
void      settings_set_mutator(int m, int on);
void      settings_reset_binds(Settings *s);
const char *settings_action_name(DhAction a);
DhAction   settings_action_from_name(const char *n);
const char *settings_quality_name(int q);
const char *settings_language_code(int lang);
int        settings_language_rtl(int lang);                      /* AR full RTL (34.1) */
const char *settings_mutator_name(int m);
void      settings_first_run_recommend(Settings *s, int integrated_gpu, int vram_mb);

#endif /* DH_SETTINGS_H */
