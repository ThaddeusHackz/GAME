/* ══════════════════════════════════════════════════════════════════════════
   M9 — Photo mode (Spec §60). P toggles. The simulation freezes; a free
   camera flies within 60 m of the player. Honest scope: filters are colour
   grade overlays drawn as full-screen 2D passes (so they work identically on
   the CPU rasterizer and GL 1.1) — no DOF, no pose system.
     WASD / mouse  fly + look      Space / Ctrl  up / down   Shift  fast
     1 / 2         zoom in / out   R  next filter            V  time +1 h
     E or F        capture PNG → <user dir>/photos/           P / Esc  exit
   ══════════════════════════════════════════════════════════════════════════ */

static const char *k_ph_filters[] = { "NATURAL", "GOLDEN HOUR", "MONSOON", "MOONLIGHT", "CINEMA", "POSTCARD" };
#define PH_FILTERS 6
int game_photo_filter_count(void) { return PH_FILTERS; }
const char *game_photo_filter_name(int i) { return (i >= 0 && i < PH_FILTERS) ? k_ph_filters[i] : "?"; }

int game_photo_enter(Game *g) {
    if (!g || g->mode != GM_PLAY || g->ui != UI_NONE) return 0;
    player_camera(&g->player, &g->ph_pos, &g->cam_dir);
    g->ph_yaw = atan2f(g->cam_dir.x, g->cam_dir.z);
    g->ph_pitch = asinf(dh_clampf(g->cam_dir.y, -1.f, 1.f));
    g->ph_fov = g->cam_fov > 0.1f ? g->cam_fov : 1.2f;
    g->ph_day0 = g->day_t;
    g->ph_capture = 0;
    g->mode = GM_PHOTO;
    return 1;
}

static void photo_frame(Game *g, const PlatInput *in, float dt) {
    if (in->pressed & (BTN_PHOTO | BTN_MENU)) {
        g->mode = GM_PLAY;
        game_message(g, "PHOTO MODE OFF  (%d PHOTO%s THIS SESSION)", g->ph_shots, g->ph_shots == 1 ? "" : "S");
        g->message_t = 2.f;
        return;
    }
    Settings *s = settings();
    g->ph_yaw   += in->look_dx * s->sensitivity * (s->invert_x ? -1.f : 1.f);
    g->ph_pitch += in->look_dy * s->sensitivity * (s->invert_y ? -1.f : 1.f);
    g->ph_pitch = dh_clampf(g->ph_pitch, -1.5f, 1.5f);
    float sp = (in->buttons & BTN_SPRINT) ? 18.f : 6.f;
    Vec3 fw = v3(sinf(g->ph_yaw) * cosf(g->ph_pitch), sinf(g->ph_pitch), cosf(g->ph_yaw) * cosf(g->ph_pitch));
    Vec3 rt = v3(cosf(g->ph_yaw), 0.f, -sinf(g->ph_yaw));
    Vec3 mv = v3_add(v3_mul(fw, in->my), v3_mul(rt, -in->mx));
    if (in->buttons & BTN_JUMP)   mv.y += 1.f;
    if (in->buttons & BTN_CROUCH) mv.y -= 1.f;
    g->ph_pos = v3_add(g->ph_pos, v3_mul(mv, sp * dt));
    /* tether + keep above ground */
    Vec3 eye = v3(g->player.pos.x, g->player.pos.y + 1.6f, g->player.pos.z);
    Vec3 off = v3_sub(g->ph_pos, eye);
    float l = v3_len(off);
    if (l > 60.f) g->ph_pos = v3_add(eye, v3_mul(off, 60.f / l));
    float gy0 = ground_y(g, g->ph_pos.x, g->ph_pos.z) + 0.4f;
    if (g->ph_pos.y < gy0) g->ph_pos.y = gy0;
    if (in->buttons & BTN_SLOT1) g->ph_fov -= 0.8f * dt;
    if (in->buttons & BTN_SLOT2) g->ph_fov += 0.8f * dt;
    g->ph_fov = dh_clampf(g->ph_fov, 0.3f, 1.9f);
    if (in->pressed & BTN_RELOAD) g->ph_filter = (g->ph_filter + 1) % PH_FILTERS;
    if (in->pressed & BTN_ROLL) game_set_time(g, fmodf(g->day_t + 1.f / 24.f, 1.f));
    if (in->pressed & (BTN_USE | BTN_FIRE)) g->ph_capture = 1;
}

/* full-screen grade overlays (2D pass, after the 3D scene) */
static void photo_draw_filter(Game *g) {
    float W = (float)g->w, H = (float)g->h;
    switch (g->ph_filter) {
    case 1: rend_quad2d(0, 0, W, H, -1, 0, 0, 1, 1, 0x3020A0FFu); break;          /* warm amber */
    case 2: rend_quad2d(0, 0, W, H, -1, 0, 0, 1, 1, 0x50605040u); break;          /* grey-teal rain light */
    case 3: rend_quad2d(0, 0, W, H, -1, 0, 0, 1, 1, 0x70602010u); break;          /* deep blue night */
    case 4: {                                                                     /* 2.39:1 letterbox */
        float bh = (H - W / 2.39f) * 0.5f; if (bh < 0) bh = 0;
        rend_quad2d(0, 0, W, H, -1, 0, 0, 1, 1, 0x18003040u);
        rend_quad2d(0, 0, W, bh, -1, 0, 0, 1, 1, 0xFF000000u);
        rend_quad2d(0, H - bh, W, bh, -1, 0, 0, 1, 1, 0xFF000000u); break; }
    case 5: {                                                                     /* vignette + border */
        for (int i = 0; i < 6; i++) {
            float t = (float)(i + 1) * 0.035f * W * 0.5f / 3.f;
            uint32_t a = (uint32_t)(0x22) << 24;
            rend_quad2d(0, 0, W, t, -1, 0, 0, 1, 1, a); rend_quad2d(0, H - t, W, t, -1, 0, 0, 1, 1, a);
            rend_quad2d(0, 0, t, H, -1, 0, 0, 1, 1, a); rend_quad2d(W - t, 0, t, H, -1, 0, 0, 1, 1, a);
        }
        float b = 10.f;
        rend_quad2d(0, 0, W, b, -1, 0, 0, 1, 1, 0xFFE8F4F8u); rend_quad2d(0, H - b * 3.f, W, b * 3.f, -1, 0, 0, 1, 1, 0xFFE8F4F8u);
        rend_quad2d(0, 0, b, H, -1, 0, 0, 1, 1, 0xFFE8F4F8u); rend_quad2d(W - b, 0, b, H, -1, 0, 0, 1, 1, 0xFFE8F4F8u);
        font_text_center(W * 0.5f, H - b * 2.2f, 1.6f, g->act == 0 ? "GREETINGS FROM ISLA SOMBRA" : "GREETINGS FROM MERIDIAN CITY", 0xFF404850u);
        break; }
    default: break;
    }
}

static void photo_draw_hint(Game *g) {
    float ui = settings()->ui_scale;
    char buf[200];
    snprintf(buf, sizeof buf, "PHOTO  |  FILTER [R] %s  |  FOV [1/2] %d  |  TIME [V] %02d:00  |  [E] CAPTURE  |  [P] EXIT",
             k_ph_filters[g->ph_filter], (int)(g->ph_fov * 57.2958f), ((int)(g->day_t * 24.f) + 6) % 24);
    font_text_shadow(12.f * ui, 12.f * ui, 1.3f * ui, buf, C_WHITE);
    if (g->message_t > 0.f) font_text_shadow(12.f * ui, 30.f * ui, 1.3f * ui, g->message, C_GOLD);
}

/* called after post, before present: the capture holds scene + filter only */
static void photo_capture(Game *g) {
    if (!g->ph_capture) return;
    g->ph_capture = 0;
    char dir[160], path[200];
    snprintf(dir, sizeof dir, "%s/photos", dh_fs_user_dir());
    dh_fs_mkdirs(dir);
    for (int n = g->ph_shots + 1; n < 10000; n++) {
        snprintf(path, sizeof path, "%s/dh_photo_%04d.png", dir, n);
        if (!dh_fs_exists(path)) break;
    }
    if (rend_save_png(path)) {
        g->ph_shots++;
        snprintf(g->ph_last, sizeof g->ph_last, "%s", path);
        game_message(g, "SAVED %s", path);
        pol_cue(g, SFX_SHUTTER, 0.8f, 1.f);
    } else game_message(g, "PHOTO FAILED - could not write %s", dir);
    g->message_t = 3.f;
}
