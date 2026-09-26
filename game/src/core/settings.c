/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — SettingsManager implementation
   ══════════════════════════════════════════════════════════════════════════ */
#include "settings.h"
#include "dh_log.h"
#include "dh_json.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static Settings g_settings;
static int g_init = 0;

Settings *settings(void) {
    if (!g_init) { settings_defaults(&g_settings); g_init = 1; }
    return &g_settings;
}

/* ── default bindings ───────────────────────────────────────────────────── */
typedef struct { DhAction a; const char *key; const char *alt; const char *pad; BindMode mode; } BindDef;
static const BindDef kDefaultBinds[] = {
    { ACT_MOVE_FWD,       "W",            NULL,   "LS_UP",     BIND_HOLD },
    { ACT_MOVE_BACK,      "S",            NULL,   "LS_DOWN",   BIND_HOLD },
    { ACT_MOVE_LEFT,      "A",            NULL,   "LS_LEFT",   BIND_HOLD },
    { ACT_MOVE_RIGHT,     "D",            NULL,   "LS_RIGHT",  BIND_HOLD },
    { ACT_SPRINT,         "LSHIFT",       NULL,   "LS_BTN",    BIND_HOLD },
    { ACT_CROUCH,         "LCTRL",        "C",    "B",         BIND_HOLD },
    { ACT_JUMP,           "SPACE",        NULL,   "A",         BIND_HOLD },
    { ACT_WALK_TOGGLE,    "CAPS",         NULL,   "DPAD_DOWN", BIND_TOGGLE },
    { ACT_FIRE,           "MOUSE1",       NULL,   "RT",        BIND_HOLD },
    { ACT_ADS,            "MOUSE2",       NULL,   "LT",        BIND_HOLD },
    { ACT_RELOAD,         "R",            NULL,   "X",         BIND_HOLD },
    { ACT_MELEE,          "V",            NULL,   "RB",        BIND_HOLD },
    { ACT_TAKEDOWN,       "F",            NULL,   "LB",        BIND_HOLD },
    { ACT_THROWABLE_1,    "G",            NULL,   "DPAD_LEFT", BIND_HOLD },
    { ACT_THROWABLE_2,    "H",            NULL,   "DPAD_RIGHT",BIND_HOLD },
    { ACT_USE_TOOL,       "T",            NULL,   "DPAD_UP",   BIND_HOLD },
    { ACT_WEAPON_1,       "1",            NULL,   "Y",         BIND_TOGGLE },
    { ACT_WEAPON_2,       "2",            NULL,   "Y",         BIND_TOGGLE },
    { ACT_WEAPON_3,       "3",            NULL,   "Y",         BIND_TOGGLE },
    { ACT_WEAPON_4,       "4",            NULL,   "Y",         BIND_TOGGLE },
    { ACT_WEAPON_NEXT,    "MWHEEL_UP",    NULL,   "Y",         BIND_TOGGLE },
    { ACT_WEAPON_PREV,    "MWHEEL_DOWN",  NULL,   "Y",         BIND_TOGGLE },
    { ACT_INTERACT,       "E",            NULL,   "A",         BIND_HOLD },
    { ACT_ENTER_VEHICLE,  "E",            NULL,   "A",         BIND_HOLD },
    { ACT_DOG_COMMAND,    "Q",            NULL,   "BACK",      BIND_HOLD },
    { ACT_TAG,            "Z",            NULL,   "LT+RT",     BIND_HOLD },
    { ACT_HOLD_BREATH,    "LSHIFT",       NULL,   "LS_BTN",    BIND_HOLD },
    { ACT_WHISTLE,        "X",            NULL,   "DPAD_UP",   BIND_HOLD },
    { ACT_LEAN_LEFT,      "J",            NULL,   "LT",        BIND_HOLD },
    { ACT_LEAN_RIGHT,     "L",            NULL,   "RT",        BIND_HOLD },
    { ACT_PAUSE,          "ESCAPE",       NULL,   "START",     BIND_TOGGLE },
    { ACT_MAP,            "M",            NULL,   "BACK",      BIND_TOGGLE },
    { ACT_INVENTORY,      "I",            NULL,   "LB+RB",     BIND_TOGGLE },
    { ACT_SKILLS,         "K",            NULL,   "DPAD_DOWN", BIND_TOGGLE },
    { ACT_PHONE,          "TAB",          NULL,   "DPAD_UP",   BIND_TOGGLE },
    { ACT_PHOTO_MODE,     "P",            NULL,   "START+BACK",BIND_TOGGLE },
    { ACT_PING,           "B",            NULL,   "RS_BTN",    BIND_HOLD },
    { ACT_RADIO_NEXT,     "N",            NULL,   "DPAD_RIGHT",BIND_TOGGLE },
    { ACT_EMOTE_WHEEL,    "Y",            NULL,   "DPAD_DOWN", BIND_HOLD },
    { ACT_VEHICLE_HANDBRAKE,"SPACE",      NULL,   "A",         BIND_HOLD },
    { ACT_VEHICLE_HORN,   "G",            NULL,   "DPAD_LEFT", BIND_HOLD },
    { ACT_VEHICLE_BOOST,  "LSHIFT",       NULL,   "X",         BIND_HOLD },
    { ACT_VEHICLE_CAMERA, "C",            NULL,   "RS_BTN",    BIND_TOGGLE },
    { ACT_DOG_STAY,       "F1",           NULL,   "DPAD_UP",   BIND_TOGGLE },
    { ACT_DOG_FOLLOW,     "F2",           NULL,   "DPAD_DOWN", BIND_TOGGLE },
    { ACT_DOG_FETCH,      "F3",           NULL,   "DPAD_LEFT", BIND_TOGGLE },
    { ACT_DOG_DISTRACT,   "F4",           NULL,   "DPAD_RIGHT",BIND_TOGGLE },
    { ACT_PET_DOG,        "E",            NULL,   "A",         BIND_HOLD },
    { ACT_MENU_UP,        "W",            "UP",   "DPAD_UP",   BIND_HOLD },
    { ACT_MENU_DOWN,      "S",            "DOWN", "DPAD_DOWN", BIND_HOLD },
    { ACT_MENU_LEFT,      "A",            "LEFT", "DPAD_LEFT", BIND_HOLD },
    { ACT_MENU_RIGHT,     "D",            "RIGHT","DPAD_RIGHT",BIND_HOLD },
    { ACT_MENU_ACCEPT,    "ENTER",        "SPACE","A",         BIND_HOLD },
    { ACT_MENU_BACK,      "ESCAPE",       "BACKSPACE","B",     BIND_HOLD },
    { ACT_MENU_TAB,       "TAB",          NULL,   "LB",        BIND_TOGGLE },
};

static const char *kActionNames[ACT_COUNT] = {
    "move_fwd","move_back","move_left","move_right","sprint","crouch","jump","walk_toggle",
    "fire","ads","reload","melee","takedown","throwable_1","throwable_2","use_tool",
    "weapon_1","weapon_2","weapon_3","weapon_4","weapon_next","weapon_prev",
    "interact","enter_vehicle","dog_command","tag","hold_breath","whistle","lean_left","lean_right",
    "pause","map","inventory","skills","phone","photo_mode","ping","radio_next","emote_wheel",
    "vehicle_handbrake","vehicle_horn","vehicle_boost","vehicle_camera",
    "dog_stay","dog_follow","dog_fetch","dog_distract","pet_dog",
    "menu_up","menu_down","menu_left","menu_right","menu_accept","menu_back","menu_tab"
};

void settings_reset_binds(Settings *s) {
    for (int i = 0; i < ACT_COUNT; i++) {
        memset(&s->binds[i], 0, sizeof(KeyBind));
        s->binds[i].mode = BIND_HOLD;
    }
    int n = (int)(sizeof(kDefaultBinds) / sizeof(kDefaultBinds[0]));
    for (int i = 0; i < n; i++) {
        const BindDef *d = &kDefaultBinds[i];
        if (d->a < 0 || d->a >= ACT_COUNT) continue;
        KeyBind *b = &s->binds[d->a];
        dh_strcpy_safe(b->key, KEY_NAME_LEN, d->key ? d->key : "");
        dh_strcpy_safe(b->key_alt, KEY_NAME_LEN, d->alt ? d->alt : "");
        dh_strcpy_safe(b->pad, KEY_NAME_LEN, d->pad ? d->pad : "");
        b->mode = d->mode;
    }
}

const char *settings_action_name(DhAction a) {
    return (a >= 0 && a < ACT_COUNT) ? kActionNames[a] : "unknown";
}
DhAction settings_action_from_name(const char *n) {
    if (!n) return ACT_COUNT;
    for (int i = 0; i < ACT_COUNT; i++) if (strcmp(kActionNames[i], n) == 0) return (DhAction)i;
    return ACT_COUNT;
}
const char *settings_quality_name(int q) {
    switch (q) { case QUALITY_LOW: return "Low"; case QUALITY_MEDIUM: return "Medium";
                 case QUALITY_HIGH: return "High"; case QUALITY_ULTRA: return "Ultra"; }
    return "Low";
}
const char *settings_mutator_name(int m) {
    static const char *names[MUT_COUNT] = {
        "BIG HEAD MODE","LOW GRAVITY","PAINTBALL","MIRROR WORLD",
        "TURBO NIGHT","INFINITE AMMO","PONCHO GIANT","PHOTOREAL MODE"
    };
    return (m >= 0 && m < MUT_COUNT) ? names[m] : "?";
}
static const char *kLangCodes[LANG_COUNT] = {
    "en","es","pt-BR","fr","de","it","pl","tr","zh-Hans","ja","ko","hi","ar"
};
const char *settings_language_code(int l) { return (l >= 0 && l < LANG_COUNT) ? kLangCodes[l] : "en"; }
int settings_language_rtl(int l) { return l == LANG_AR; }

/* ── defaults ───────────────────────────────────────────────────────────── */
void settings_defaults(Settings *s) {
    memset(s, 0, sizeof(*s));
    s->schema = 1;

    s->fullscreen = 1; s->vsync = 1; s->quality = QUALITY_MEDIUM;
    s->res_w = 1280; s->res_h = 720;
    s->fov = 85.0f; s->render_scale = 1.0f;
    s->motion_blur = 0; s->shadows = 1; s->ssao = 0; s->aa_mode = AA_FXAA;
    s->hd_textures = 0;
    s->draw_distance = 100.0f; s->foliage_density = 0.6f;
    s->traffic_density = 0.6f; s->ped_density = 0.6f;
    s->brightness = 1.0f; s->potato_hud = 0;

    s->vol_master = 0.85f; s->vol_music = 0.7f; s->vol_sfx = 0.9f;
    s->vol_voice = 1.0f; s->vol_radio = 0.6f;
    s->duck_strength = 0.55f; s->dynamic_range = 0.0f;
    s->subtitle_lang = 0; s->adaptive_music = 1;

    s->difficulty = DIFF_NORMAL;
    s->enemy_damage = 1.0f; s->enemy_aim = 1.0f; s->detection_speed = 1.0f;
    s->puzzle_timer_scale = 1.0f;
    s->aim_assist = 0.35f; s->aim_magnetism = 0.2f; s->slow_motion = 1.0f;
    s->auto_win_qte = 0; s->hints = 1; s->gore_reduced = 0; s->damage_numbers = 0;
    s->camera_shake = 1; s->hit_marker = 1; s->crosshair_on = 1;
    s->iron_sight_hold = 1; s->tutorial_level = 1; s->killcam_lite = 1;
    s->day_night_minutes = 24.0f; s->heatseed = 0.5f;

    s->invert_y = 0; s->invert_x = 0;
    s->sensitivity = 1.0f; s->ads_sensitivity = 0.75f; s->aim_smoothing = 0.35f;
    s->rumble = 1; s->rumble_strength = 0.7f;
    s->glyph_set = 0; s->one_hand_preset = 0;
    settings_reset_binds(s);

    /* Accessibility defaults follow Spec 37.2 research-backed choices:
       subtitles ON by default, photosensitivity asked once on first run. */
    s->subtitles = 1; s->subtitle_size = 2; s->subtitle_bg_opacity = 60;
    s->speaker_colors = 1; s->subtitle_max_cps = 21;
    s->colorblind_mode = 0; s->reticle_color = 0; s->reticle_shape = 0;
    s->photosensitivity = 0;
    s->ui_scale = 1.0f; s->safe_area = 1.0f;
    s->dyslexic_font = 0; s->narration = 0; s->audio_cues = 1; s->co_pilot = 0;
    s->high_contrast_outline = 0; s->reduced_gore_cw = 0;

    s->language = LANG_EN; s->first_run_done = 0; s->recommended_quality = QUALITY_MEDIUM;
    s->ng_plus_allowed = 1; s->session_timer_min = 0; s->clean_capture = 0;
    settings_apply_preset(s, (QualityPreset)s->quality);
}

/* ── quality preset table (Spec 15.3 + 15.2 budgets) ───────────────────── */
void settings_apply_preset(Settings *s, QualityPreset q) {
    s->quality = dh_clampi((int)q, 0, QUALITY_ULTRA);
    switch (s->quality) {
        case QUALITY_LOW:                                  /* 60 FPS @ 720p on iGPU */
            s->shadows = 1;
            s->ssao = 0; s->aa_mode = AA_FXAA; s->motion_blur = 0;
            s->draw_distance = 60.0f; s->foliage_density = 0.35f;
            s->traffic_density = 0.4f; s->ped_density = 0.4f;
            s->hd_textures = 0; s->render_scale = 1.0f; s->potato_hud = 1;
            break;
        case QUALITY_MEDIUM:
            s->shadows = 1; s->ssao = 0; s->aa_mode = AA_FXAA; s->motion_blur = 0;
            s->draw_distance = 100.0f; s->foliage_density = 0.6f;
            s->traffic_density = 0.6f; s->ped_density = 0.6f;
            s->hd_textures = 0; s->potato_hud = 0;
            break;
        case QUALITY_HIGH:
            s->shadows = 1; s->ssao = 1; s->aa_mode = AA_TAA; s->motion_blur = 1;
            s->draw_distance = 160.0f; s->foliage_density = 0.85f;
            s->traffic_density = 0.85f; s->ped_density = 0.85f;
            s->hd_textures = 0; s->potato_hud = 0;
            break;
        default: /* ULTRA */
            s->shadows = 1; s->ssao = 1; s->aa_mode = AA_TAA; s->motion_blur = 1;
            s->draw_distance = 250.0f; s->foliage_density = 1.0f;
            s->traffic_density = 1.0f; s->ped_density = 1.0f;
            s->hd_textures = 1; s->potato_hud = 0;
            break;
    }
}

void settings_clamp(Settings *s) {
    s->fov              = dh_clampf(s->fov, 70.0f, 110.0f);
    s->render_scale     = dh_clampf(s->render_scale, 0.5f, 1.0f);
    s->draw_distance    = dh_clampf(s->draw_distance, 30.0f, 400.0f);
    s->foliage_density  = dh_clampf(s->foliage_density, 0.0f, 1.0f);
    s->traffic_density  = dh_clampf(s->traffic_density, 0.0f, 1.0f);
    s->ped_density      = dh_clampf(s->ped_density, 0.0f, 1.0f);
    s->brightness       = dh_clampf(s->brightness, 0.5f, 2.0f);
    s->vol_master       = dh_clampf(s->vol_master, 0.0f, 1.0f);
    s->vol_music        = dh_clampf(s->vol_music, 0.0f, 1.0f);
    s->vol_sfx          = dh_clampf(s->vol_sfx, 0.0f, 1.0f);
    s->vol_voice        = dh_clampf(s->vol_voice, 0.0f, 1.0f);
    s->vol_radio        = dh_clampf(s->vol_radio, 0.0f, 1.0f);
    s->duck_strength    = dh_clampf(s->duck_strength, 0.0f, 1.0f);
    s->dynamic_range    = dh_clampf(s->dynamic_range, 0.0f, 1.0f);
    s->enemy_damage     = dh_clampf(s->enemy_damage, 0.0f, 3.0f);
    s->enemy_aim        = dh_clampf(s->enemy_aim, 0.0f, 2.0f);
    s->detection_speed  = dh_clampf(s->detection_speed, 0.25f, 3.0f);
    s->puzzle_timer_scale = dh_clampf(s->puzzle_timer_scale, 0.25f, 4.0f);
    s->aim_assist       = dh_clampf(s->aim_assist, 0.0f, 1.0f);
    s->aim_magnetism    = dh_clampf(s->aim_magnetism, 0.0f, 1.0f);
    s->slow_motion      = dh_clampf(s->slow_motion, 0.5f, 1.0f);
    s->sensitivity      = dh_clampf(s->sensitivity, 0.1f, 5.0f);
    s->ads_sensitivity  = dh_clampf(s->ads_sensitivity, 0.1f, 3.0f);
    s->aim_smoothing    = dh_clampf(s->aim_smoothing, 0.0f, 1.0f);
    s->rumble_strength  = dh_clampf(s->rumble_strength, 0.0f, 1.0f);
    s->ui_scale         = dh_clampf(s->ui_scale, 0.8f, 1.5f);
    s->safe_area        = dh_clampf(s->safe_area, 0.85f, 1.0f);
    s->day_night_minutes= dh_clampf(s->day_night_minutes, 2.0f, 180.0f);
    s->heatseed         = dh_clampf(s->heatseed, 0.0f, 1.0f);
    s->subtitle_size    = dh_clampi(s->subtitle_size, 0, 4);
    s->subtitle_bg_opacity = dh_clampi(s->subtitle_bg_opacity, 0, 100);
    s->subtitle_max_cps = dh_clampi(s->subtitle_max_cps, 10, 30);
    s->colorblind_mode  = dh_clampi(s->colorblind_mode, 0, 3);
    s->difficulty       = dh_clampi(s->difficulty, 0, DIFF_COUNT - 1);
    s->quality          = dh_clampi(s->quality, 0, QUALITY_ULTRA);
    s->aa_mode          = dh_clampi(s->aa_mode, 0, AA_COUNT - 1);
    s->language         = dh_clampi(s->language, 0, LANG_COUNT - 1);
    s->glyph_set        = dh_clampi(s->glyph_set, 0, 3);
    s->res_w            = dh_clampi(s->res_w, 640, 7680);
    s->res_h            = dh_clampi(s->res_h, 480, 4320);
    for (int i = 0; i < MUT_COUNT; i++) s->mutators[i] = s->mutators[i] ? 1 : 0;
}

int settings_mutator_active(int m) {
    if (m < 0 || m >= MUT_COUNT) return 0;
    return settings()->mutators[m];
}
int settings_mutator_count(void) {
    int n = 0;
    for (int i = 0; i < MUT_COUNT; i++) if (settings()->mutators[i]) n++;
    return n;
}
/* Spec 29.2: combine up to 3; NG+ disabled while any are active (shown, no traps). */
void settings_set_mutator(int m, int on) {
    if (m < 0 || m >= MUT_COUNT) return;
    if (on && settings_mutator_count() >= 3 && !settings()->mutators[m]) {
        DH_WARN("settings", "mutator cap reached (3 max) — ignored %s", settings_mutator_name(m));
        return;
    }
    settings()->mutators[m] = on ? 1 : 0;
    settings()->ng_plus_allowed = settings_mutator_count() == 0;
}

/* ── first-run wizard (Spec 37.2) ──────────────────────────────────────── */
void settings_first_run_recommend(Settings *s, int integrated_gpu, int vram_mb) {
    int q = QUALITY_HIGH;
    if (integrated_gpu) q = QUALITY_LOW;                 /* the promise: Low on iGPU */
    else if (vram_mb > 0 && vram_mb < 2048) q = QUALITY_MEDIUM;
    else if (vram_mb >= 6000) q = QUALITY_ULTRA;
    s->recommended_quality = q;
    s->quality = q;
    settings_apply_preset(s, (QualityPreset)q);
    /* Research-backed + kindness defaults: subtitles ON, hints ON, no gore. */
    s->subtitles = 1;
    s->hints = 1;
    s->first_run_done = 0;   /* the wizard itself flips this after the player confirms */
}

/* ── persistence ───────────────────────────────────────────────────────── */
static void settings_path(char *out, int outsz) {
    dh_fs_join(out, outsz, dh_fs_user_dir(), "settings.json");
}

int settings_save(const Settings *s) {
    char path[1024]; settings_path(path, sizeof(path));
    JsonWriter w; jw_init(&w);
    if (w.err) return 0;
    jw_obj_begin(&w, NULL);
    jw_int(&w, "schema", 1);
    jw_obj_begin(&w, "display");
      jw_int(&w,"fullscreen",s->fullscreen); jw_int(&w,"vsync",s->vsync);
      jw_int(&w,"quality",s->quality); jw_int(&w,"res_w",s->res_w); jw_int(&w,"res_h",s->res_h);
      jw_num(&w,"fov",s->fov); jw_num(&w,"render_scale",s->render_scale);
      jw_int(&w,"motion_blur",s->motion_blur); jw_int(&w,"shadows",s->shadows);
      jw_int(&w,"ssao",s->ssao); jw_int(&w,"aa_mode",s->aa_mode);
      jw_int(&w,"hd_textures",s->hd_textures); jw_num(&w,"draw_distance",s->draw_distance);
      jw_num(&w,"foliage_density",s->foliage_density); jw_num(&w,"traffic_density",s->traffic_density);
      jw_num(&w,"ped_density",s->ped_density); jw_num(&w,"brightness",s->brightness);
      jw_int(&w,"potato_hud",s->potato_hud);
    jw_obj_end(&w);
    jw_obj_begin(&w, "audio");
      jw_num(&w,"master",s->vol_master); jw_num(&w,"music",s->vol_music);
      jw_num(&w,"sfx",s->vol_sfx); jw_num(&w,"voice",s->vol_voice);
      jw_num(&w,"radio",s->vol_radio); jw_num(&w,"duck_strength",s->duck_strength);
      jw_num(&w,"dynamic_range",s->dynamic_range); jw_int(&w,"subtitle_lang",s->subtitle_lang);
      jw_int(&w,"adaptive_music",s->adaptive_music);
    jw_obj_end(&w);
    jw_obj_begin(&w, "gameplay");
      jw_int(&w,"difficulty",s->difficulty); jw_num(&w,"enemy_damage",s->enemy_damage);
      jw_num(&w,"enemy_aim",s->enemy_aim); jw_num(&w,"detection_speed",s->detection_speed);
      jw_num(&w,"puzzle_timer_scale",s->puzzle_timer_scale);
      jw_num(&w,"aim_assist",s->aim_assist); jw_num(&w,"aim_magnetism",s->aim_magnetism);
      jw_num(&w,"slow_motion",s->slow_motion); jw_int(&w,"auto_win_qte",s->auto_win_qte);
      jw_int(&w,"hints",s->hints); jw_int(&w,"gore_reduced",s->gore_reduced);
      jw_int(&w,"damage_numbers",s->damage_numbers); jw_int(&w,"camera_shake",s->camera_shake);
      jw_int(&w,"hit_marker",s->hit_marker); jw_int(&w,"crosshair",s->crosshair_on);
      jw_int(&w,"iron_sight_hold",s->iron_sight_hold); jw_int(&w,"tutorial_level",s->tutorial_level);
      jw_int(&w,"killcam_lite",s->killcam_lite);
      jw_num(&w,"day_night_minutes",s->day_night_minutes); jw_num(&w,"heatseed",s->heatseed);
    jw_obj_end(&w);
    jw_obj_begin(&w, "controls");
      jw_int(&w,"invert_y",s->invert_y); jw_int(&w,"invert_x",s->invert_x);
      jw_num(&w,"sensitivity",s->sensitivity); jw_num(&w,"ads_sensitivity",s->ads_sensitivity);
      jw_num(&w,"aim_smoothing",s->aim_smoothing); jw_int(&w,"rumble",s->rumble);
      jw_num(&w,"rumble_strength",s->rumble_strength); jw_int(&w,"glyph_set",s->glyph_set);
      jw_int(&w,"one_hand_preset",s->one_hand_preset);
      jw_obj_begin(&w, "binds");
        for (int i = 0; i < ACT_COUNT; i++) {
          jw_obj_begin(&w, kActionNames[i]);
            jw_str(&w,"key",s->binds[i].key);
            jw_str(&w,"alt",s->binds[i].key_alt);
            jw_str(&w,"pad",s->binds[i].pad);
            jw_int(&w,"mode",(int)s->binds[i].mode);
          jw_obj_end(&w);
        }
      jw_obj_end(&w);
    jw_obj_end(&w);
    jw_obj_begin(&w, "accessibility");
      jw_int(&w,"subtitles",s->subtitles); jw_int(&w,"subtitle_size",s->subtitle_size);
      jw_int(&w,"subtitle_bg_opacity",s->subtitle_bg_opacity);
      jw_int(&w,"speaker_colors",s->speaker_colors); jw_int(&w,"subtitle_max_cps",s->subtitle_max_cps);
      jw_int(&w,"colorblind_mode",s->colorblind_mode); jw_int(&w,"reticle_color",s->reticle_color);
      jw_int(&w,"reticle_shape",s->reticle_shape); jw_int(&w,"photosensitivity",s->photosensitivity);
      jw_num(&w,"ui_scale",s->ui_scale); jw_num(&w,"safe_area",s->safe_area);
      jw_int(&w,"dyslexic_font",s->dyslexic_font); jw_int(&w,"narration",s->narration);
      jw_int(&w,"audio_cues",s->audio_cues); jw_int(&w,"co_pilot",s->co_pilot);
      jw_int(&w,"high_contrast_outline",s->high_contrast_outline);
      jw_int(&w,"reduced_gore_cw",s->reduced_gore_cw);
    jw_obj_end(&w);
    jw_obj_begin(&w, "meta");
      jw_int(&w,"language",s->language); jw_int(&w,"first_run_done",s->first_run_done);
      jw_int(&w,"recommended_quality",s->recommended_quality);
      jw_int(&w,"ng_plus_allowed",s->ng_plus_allowed);
      jw_int(&w,"session_timer_min",s->session_timer_min);
      jw_int(&w,"clean_capture",s->clean_capture);
      jw_arr_begin(&w,"mutators");
        for (int i = 0; i < MUT_COUNT; i++) jw_int(&w,NULL,s->mutators[i]);
      jw_arr_end(&w);
    jw_obj_end(&w);
    jw_obj_end(&w);
    char *txt = jw_take(&w);
    int ok = txt ? dh_fs_write_text(path, txt) : 0;
    free(txt);
    if (ok) DH_DEBUG("settings", "saved %s", path);
    else DH_WARN("settings", "FAILED to save %s", path);
    return ok;
}

int settings_load(Settings *s) {
    char path[1024]; settings_path(path, sizeof(path));
    size_t len = 0;
    char *txt = dh_fs_read_text(path, &len);
    if (!txt) { DH_INFO("settings", "no %s — using defaults", path); return 0; }
    const char *err = NULL;
    JsonValue *root = json_parse(txt, &err);
    free(txt);
    if (!root) {
        DH_WARN("settings", "corrupt settings.json (%s) — repairing from defaults", err ? err : "?");
        settings_defaults(s);                 /* Spec 37.4 auto-repair */
        return 0;
    }
    int schema = json_get_int(root, "schema", 1);
    if (schema != 1) DH_WARN("settings", "schema %d != 1 — migrating", schema);

    JsonValue *d = json_obj_get(root, "display");
    if (d) {
        s->fullscreen = json_get_bool(d,"fullscreen",s->fullscreen);
        s->vsync = json_get_bool(d,"vsync",s->vsync);
        s->res_w = json_get_int(d,"res_w",s->res_w);
        s->res_h = json_get_int(d,"res_h",s->res_h);
        s->fov = json_get_flt(d,"fov",s->fov);
        s->render_scale = json_get_flt(d,"render_scale",s->render_scale);
        s->motion_blur = json_get_bool(d,"motion_blur",s->motion_blur);
        s->shadows = json_get_bool(d,"shadows",s->shadows);
        s->ssao = json_get_bool(d,"ssao",s->ssao);
        s->aa_mode = json_get_int(d,"aa_mode",s->aa_mode);
        s->hd_textures = json_get_bool(d,"hd_textures",s->hd_textures);
        s->draw_distance = json_get_flt(d,"draw_distance",s->draw_distance);
        s->foliage_density = json_get_flt(d,"foliage_density",s->foliage_density);
        s->traffic_density = json_get_flt(d,"traffic_density",s->traffic_density);
        s->ped_density = json_get_flt(d,"ped_density",s->ped_density);
        s->brightness = json_get_flt(d,"brightness",s->brightness);
        s->potato_hud = json_get_bool(d,"potato_hud",s->potato_hud);
        int q = json_get_int(d,"quality",-1);
        if (q >= 0 && q < QUALITY_COUNT) { s->quality = q; settings_apply_preset(s,(QualityPreset)q); }
    }
    JsonValue *a = json_obj_get(root, "audio");
    if (a) {
        s->vol_master = json_get_flt(a,"master",s->vol_master);
        s->vol_music = json_get_flt(a,"music",s->vol_music);
        s->vol_sfx = json_get_flt(a,"sfx",s->vol_sfx);
        s->vol_voice = json_get_flt(a,"voice",s->vol_voice);
        s->vol_radio = json_get_flt(a,"radio",s->vol_radio);
        s->duck_strength = json_get_flt(a,"duck_strength",s->duck_strength);
        s->dynamic_range = json_get_flt(a,"dynamic_range",s->dynamic_range);
        s->subtitle_lang = json_get_int(a,"subtitle_lang",s->subtitle_lang);
        s->adaptive_music = json_get_bool(a,"adaptive_music",s->adaptive_music);
    }
    JsonValue *g = json_obj_get(root, "gameplay");
    if (g) {
        s->difficulty = json_get_int(g,"difficulty",s->difficulty);
        s->enemy_damage = json_get_flt(g,"enemy_damage",s->enemy_damage);
        s->enemy_aim = json_get_flt(g,"enemy_aim",s->enemy_aim);
        s->detection_speed = json_get_flt(g,"detection_speed",s->detection_speed);
        s->puzzle_timer_scale = json_get_flt(g,"puzzle_timer_scale",s->puzzle_timer_scale);
        s->aim_assist = json_get_flt(g,"aim_assist",s->aim_assist);
        s->aim_magnetism = json_get_flt(g,"aim_magnetism",s->aim_magnetism);
        s->slow_motion = json_get_flt(g,"slow_motion",s->slow_motion);
        s->auto_win_qte = json_get_bool(g,"auto_win_qte",s->auto_win_qte);
        s->hints = json_get_bool(g,"hints",s->hints);
        s->gore_reduced = json_get_bool(g,"gore_reduced",s->gore_reduced);
        s->damage_numbers = json_get_bool(g,"damage_numbers",s->damage_numbers);
        s->camera_shake = json_get_bool(g,"camera_shake",s->camera_shake);
        s->hit_marker = json_get_bool(g,"hit_marker",s->hit_marker);
        s->crosshair_on = json_get_bool(g,"crosshair",s->crosshair_on);
        s->iron_sight_hold = json_get_bool(g,"iron_sight_hold",s->iron_sight_hold);
        s->tutorial_level = json_get_int(g,"tutorial_level",s->tutorial_level);
        s->killcam_lite = json_get_bool(g,"killcam_lite",s->killcam_lite);
        s->day_night_minutes = json_get_flt(g,"day_night_minutes",s->day_night_minutes);
        s->heatseed = json_get_flt(g,"heatseed",s->heatseed);
    }
    JsonValue *c = json_obj_get(root, "controls");
    if (c) {
        s->invert_y = json_get_bool(c,"invert_y",s->invert_y);
        s->invert_x = json_get_bool(c,"invert_x",s->invert_x);
        s->sensitivity = json_get_flt(c,"sensitivity",s->sensitivity);
        s->ads_sensitivity = json_get_flt(c,"ads_sensitivity",s->ads_sensitivity);
        s->aim_smoothing = json_get_flt(c,"aim_smoothing",s->aim_smoothing);
        s->rumble = json_get_bool(c,"rumble",s->rumble);
        s->rumble_strength = json_get_flt(c,"rumble_strength",s->rumble_strength);
        s->glyph_set = json_get_int(c,"glyph_set",s->glyph_set);
        s->one_hand_preset = json_get_int(c,"one_hand_preset",s->one_hand_preset);
        JsonValue *b = json_obj_get(c, "binds");
        if (b) for (int i = 0; i < ACT_COUNT; i++) {
            JsonValue *e = json_obj_get(b, kActionNames[i]);
            if (!e) continue;
            dh_strcpy_safe(s->binds[i].key, KEY_NAME_LEN, json_get_str(e,"key",s->binds[i].key));
            dh_strcpy_safe(s->binds[i].key_alt, KEY_NAME_LEN, json_get_str(e,"alt",s->binds[i].key_alt));
            dh_strcpy_safe(s->binds[i].pad, KEY_NAME_LEN, json_get_str(e,"pad",s->binds[i].pad));
            s->binds[i].mode = (BindMode)json_get_int(e,"mode",(int)s->binds[i].mode);
        }
    }
    JsonValue *ac = json_obj_get(root, "accessibility");
    if (ac) {
        s->subtitles = json_get_bool(ac,"subtitles",s->subtitles);
        s->subtitle_size = json_get_int(ac,"subtitle_size",s->subtitle_size);
        s->subtitle_bg_opacity = json_get_int(ac,"subtitle_bg_opacity",s->subtitle_bg_opacity);
        s->speaker_colors = json_get_bool(ac,"speaker_colors",s->speaker_colors);
        s->subtitle_max_cps = json_get_int(ac,"subtitle_max_cps",s->subtitle_max_cps);
        s->colorblind_mode = json_get_int(ac,"colorblind_mode",s->colorblind_mode);
        s->reticle_color = json_get_int(ac,"reticle_color",s->reticle_color);
        s->reticle_shape = json_get_int(ac,"reticle_shape",s->reticle_shape);
        s->photosensitivity = json_get_bool(ac,"photosensitivity",s->photosensitivity);
        s->ui_scale = json_get_flt(ac,"ui_scale",s->ui_scale);
        s->safe_area = json_get_flt(ac,"safe_area",s->safe_area);
        s->dyslexic_font = json_get_bool(ac,"dyslexic_font",s->dyslexic_font);
        s->narration = json_get_bool(ac,"narration",s->narration);
        s->audio_cues = json_get_bool(ac,"audio_cues",s->audio_cues);
        s->co_pilot = json_get_bool(ac,"co_pilot",s->co_pilot);
        s->high_contrast_outline = json_get_bool(ac,"high_contrast_outline",s->high_contrast_outline);
        s->reduced_gore_cw = json_get_bool(ac,"reduced_gore_cw",s->reduced_gore_cw);
    }
    JsonValue *m = json_obj_get(root, "meta");
    if (m) {
        s->language = json_get_int(m,"language",s->language);
        s->first_run_done = json_get_bool(m,"first_run_done",s->first_run_done);
        s->recommended_quality = json_get_int(m,"recommended_quality",s->recommended_quality);
        s->ng_plus_allowed = json_get_bool(m,"ng_plus_allowed",s->ng_plus_allowed);
        s->session_timer_min = json_get_int(m,"session_timer_min",s->session_timer_min);
        s->clean_capture = json_get_bool(m,"clean_capture",s->clean_capture);
        JsonValue *mu = json_obj_get(m, "mutators");
        if (mu) for (int i = 0; i < MUT_COUNT && i < json_arr_len(mu); i++) {
            JsonValue *e = json_arr_get(mu, i);
            s->mutators[i] = e ? (e->num != 0.0) : 0;
        }
    }
    json_free(root);
    settings_clamp(s);
    DH_INFO("settings", "loaded %s (quality=%s lang=%s photosens=%d)", path,
            settings_quality_name(s->quality), settings_language_code(s->language), s->photosensitivity);
    return 1;
}
