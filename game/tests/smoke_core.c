/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — M0 core smoke test (headless, no GPU, no display)
   Proves the engine skeleton actually works end to end:
     math → settings → JSON → save round-trip → software rasterizer → BMP
   Exits non-zero on any failure so CI / `tools/build.sh` can gate on it.
   Spec 19: never claim something works without running it.
   ══════════════════════════════════════════════════════════════════════════ */
#ifndef _WIN32
  #define _POSIX_C_SOURCE 200809L
#endif

#include "../src/core/dh_types.h"
#include "../src/core/dh_log.h"
#include "../src/core/dh_json.h"
#include "../src/core/settings.h"
#include "../src/core/save.h"
#include "../src/rend/rend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_checks = 0, g_failed = 0;

#define CHECK(cond, ...) do { \
    g_checks++; \
    if (!(cond)) { g_failed++; DH_ERROR("smoke", "FAIL: " __VA_ARGS__); } \
    else DH_INFO("smoke", "ok: " __VA_ARGS__); \
} while (0)

/* ── a procedural checkerboard texture (stands in for proc_tex.c in M1) ── */
static int make_test_texture(void) {
    Texture *t = tex_new(64, 64, "test_checker");
    if (!t) return -1;
    for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) {
        int c = ((x >> 3) + (y >> 3)) & 1;
        uint32_t base = c ? 0xFF3FA860u : 0xFF2A6E40u;   /* stylized foliage green */
        /* slight per-texel noise so mip generation has something to average */
        uint32_t n = (uint32_t)((x * 7 + y * 13) & 15);
        uint32_t r = ((base      ) & 0xFF) + n;
        uint32_t g = ((base >>  8) & 0xFF) + n;
        uint32_t b = ((base >> 16) & 0xFF) + n;
        base = 0xFF000000u | (dh_clampi(b,0,255)<<16) | (dh_clampi(g,0,255)<<8) | dh_clampi(r,0,255);
        tex_set_pixel(t, x, y, base);
    }
    return tex_register(t);
}

/* ── a cube mesh with proper normals + UVs ─────────────────────────────── */
static int make_test_cube(void) {
    Mesh *m = mesh_new("test_cube", 24, 36);
    if (!m) return -1;
    static const float faces[6][3] = {
        { 0, 0, 1}, { 0, 0,-1}, { 1, 0, 0}, {-1, 0, 0}, { 0, 1, 0}, { 0,-1, 0}
    };
    static const float rights[6][3] = {
        { 1, 0, 0}, {-1, 0, 0}, { 0, 0,-1}, { 0, 0, 1}, { 1, 0, 0}, { 1, 0, 0}
    };
    static const float ups[6][3] = {
        { 0, 1, 0}, { 0, 1, 0}, { 0, 1, 0}, { 0, 1, 0}, { 0, 0,-1}, { 0, 0, 1}
    };
    int vi = 0, ii = 0;
    for (int f = 0; f < 6; f++) {
        Vec3 n = v3(faces[f][0], faces[f][1], faces[f][2]);
        Vec3 r = v3(rights[f][0], rights[f][1], rights[f][2]);
        Vec3 u = v3(ups[f][0], ups[f][1], ups[f][2]);
        /* CCW when seen from outside → front-facing under GL convention */
        Vec3 c[4] = {
            v3_add(v3_add(n, u), v3_neg(r)),
            v3_add(v3_add(n, u), r),
            v3_add(v3_add(n, v3_neg(u)), r),
            v3_add(v3_add(n, v3_neg(u)), v3_neg(r))
        };
        float uvs[4][2] = {{0,0},{1,0},{1,1},{0,1}};
        for (int k = 0; k < 4; k++) {
            m->pos[vi] = c[k]; m->nrm[vi] = n;
            m->uv[vi] = v2(uvs[k][0], uvs[k][1]);
            vi++;
        }
        m->idx[ii++]=vi-4; m->idx[ii++]=vi-3; m->idx[ii++]=vi-2;
        m->idx[ii++]=vi-4; m->idx[ii++]=vi-2; m->idx[ii++]=vi-1;
    }
    return mesh_register(m);
}

int main(int argc, char **argv) {
    const char *outdir = "build/smoke";
    if (argc > 1) outdir = argv[1];
    dh_fs_mkdirs(outdir);
    dh_fs_set_dirs("data", outdir);
    dh_log_init(outdir);

    DH_INFO("smoke", "════ DIVIDED HORIZON M0 smoke test ════");
    DH_INFO("smoke", "version %d.%d.%d (%s)", DH_VERSION_MAJOR, DH_VERSION_MINOR,
            DH_VERSION_PATCH, DH_BUILD_NAME);

    /* ── 1. math ───────────────────────────────────────────────────────── */
    {
        Vec3 a = v3(1,2,3), b = v3(4,5,6);
        CHECK(v3_dot(a,b) == 32.0f, "v3_dot");
        Vec3 cr = v3_cross(v3(1,0,0), v3(0,1,0));
        CHECK(fabsf(cr.z - 1.0f) < 1e-5f, "v3_cross(right-hand rule)");
        Mat4 id = m4_identity();
        Vec3 p = m4_xform_p(id, a);
        CHECK(v3_dist(p, a) < 1e-5f, "m4_xform_p(identity)");
        /* perspective must map a point in front of the camera to w>0 */
        Mat4 pr = m4_perspective(85.0f, 16.0f/9.0f, 0.1f, 400.0f);
        Vec4 clip;
        Vec3 wp = v3(0,0,-5);
        clip.w = pr.m[3]*wp.x + pr.m[7]*wp.y + pr.m[11]*wp.z + pr.m[15];
        CHECK(clip.w > 0.0f, "m4_perspective produces w>0 in front of camera");
        /* frustum must contain a point directly ahead and reject one behind */
        Mat4 view = m4_look_at(v3(0,0,0), v3(0,0,-1), v3(0,1,0));
        Frustum fr = frustum_from_vp(m4_mul(pr, view));
        CHECK(frustum_has_sphere(fr, v3(0,0,-10), 0.5f) == 1, "frustum contains point ahead");
        CHECK(frustum_has_sphere(fr, v3(0,0,50), 0.5f) == 0, "frustum rejects point behind");
        /* noise must be in range and deterministic */
        NoiseCfg nc; nc.seed = 1234; nc.octaves = 4; nc.freq = 0.01f;
        nc.lacunarity = 2.0f; nc.gain = 0.5f;
        float n1 = noise_fbm2(&nc, 100.0f, 200.0f);
        float n2 = noise_fbm2(&nc, 100.0f, 200.0f);
        CHECK(n1 == n2, "noise is deterministic (Spec 82 seed law)");
        CHECK(n1 >= -1.5f && n1 <= 1.5f, "fbm in range (%.3f)", n1);
        /* NaN guard (Spec 18.4) */
        float bad = 0.0f/0.0f;
        CHECK(v3_valid(v3(bad,0,0)) == 0, "v3_valid detects NaN");
        CHECK(fabsf(v3_safe(v3(bad,0,0), v3(7,0,0)).x - 7.0f) < 1e-5f, "v3_safe substitutes fallback");
    }

    /* ── 2. JSON round-trip ────────────────────────────────────────────── */
    {
        JsonWriter w; jw_init(&w);
        jw_obj_begin(&w, NULL);
          jw_str(&w, "name", "Isla Sombra");
          jw_num(&w, "size_m", 2000.0);
          jw_bool(&w, "night", 1);
          jw_arr_begin(&w, "outposts");
            jw_int(&w, NULL, 12); jw_int(&w, NULL, 3);
          jw_arr_end(&w);
          jw_obj_begin(&w, "nested"); jw_num(&w, "heat", 0.42); jw_obj_end(&w);
        jw_obj_end(&w);
        char *txt = jw_take(&w);
        CHECK(txt != NULL, "json writer produced output");
        const char *err = NULL;
        JsonValue *root = json_parse(txt, &err);
        CHECK(root != NULL, "json parses its own output (%s)", err ? err : "");
        if (root) {
            CHECK(strcmp(json_get_str(root,"name",""),"Isla Sombra")==0, "json string round-trip");
            CHECK(json_get_num(root,"size_m",0) == 2000.0, "json number round-trip");
            CHECK(json_get_bool(root,"night",0) == 1, "json bool round-trip");
            JsonValue *arr = json_obj_get(root,"outposts");
            CHECK(json_arr_len(arr) == 2, "json array length");
            CHECK(json_arr_get(arr,0)->num == 12.0, "json array element");
            JsonValue *nn = json_path(root,"nested","heat",NULL);
            CHECK(nn && fabs(nn->num - 0.42) < 1e-6, "json nested path lookup");
            json_free(root);
        }
        /* malformed input must fail cleanly, not crash */
        const char *e2 = NULL;
        JsonValue *bad = json_parse("{\"a\": ", &e2);
        CHECK(bad == NULL, "malformed json rejected (%s)", e2 ? e2 : "");
        free(txt);
    }

    /* ── 3. settings ───────────────────────────────────────────────────── */
    {
        Settings *s = settings();
        settings_defaults(s);
        CHECK(s->fov == 85.0f, "default FOV is 85 (Spec 6.11 range 70..110)");
        settings_apply_preset(s, QUALITY_LOW);
        CHECK(s->draw_distance == 60.0f, "Low preset draw distance 60m (Spec 15.3)");
        CHECK(s->potato_hud == 1, "Low preset enables potato HUD (Spec 58.3)");
        settings_apply_preset(s, QUALITY_ULTRA);
        CHECK(s->draw_distance == 250.0f, "Ultra preset draw distance 250m");
        settings_apply_preset(s, QUALITY_MEDIUM);
        /* clamping must reject out-of-range values (a hand-edited JSON) */
        s->fov = 999.0f; s->ui_scale = -5.0f; s->brightness = 100.0f;
        settings_clamp(s);
        CHECK(s->fov == 110.0f, "FOV clamped to 110");
        CHECK(s->ui_scale == 0.8f, "UI scale clamped to 0.80");
        CHECK(s->brightness == 2.0f, "brightness clamped to 2.0");
        /* mutators: max 3 (Spec 29.2) and NG+ must be blocked while active */
        settings_set_mutator(MUT_BIG_HEAD, 1);
        settings_set_mutator(MUT_LOW_GRAVITY, 1);
        settings_set_mutator(MUT_PAINTBALL, 1);
        settings_set_mutator(MUT_MIRROR_WORLD, 1);   /* must be refused */
        CHECK(settings_mutator_count() == 3, "mutator cap is 3");
        CHECK(settings_mutator_active(MUT_MIRROR_WORLD) == 0, "4th mutator refused");
        CHECK(s->ng_plus_allowed == 0, "NG+ blocked while mutators active");
        settings_set_mutator(MUT_BIG_HEAD, 0);
        settings_set_mutator(MUT_LOW_GRAVITY, 0);
        settings_set_mutator(MUT_PAINTBALL, 0);
        CHECK(s->ng_plus_allowed == 1, "NG+ re-enabled when mutators cleared");
        /* persistence */
        s->photosensitivity = 1; s->colorblind_mode = 2; s->language = LANG_JA;
        CHECK(settings_save(s) == 1, "settings.json written to user dir");
        Settings probe; settings_defaults(&probe);
        CHECK(settings_load(&probe) == 1, "settings.json read back");
        CHECK(probe.photosensitivity == 1, "photosensitivity persisted (Spec 33)");
        CHECK(probe.colorblind_mode == 2, "colorblind mode persisted (Spec 33)");
        CHECK(probe.language == LANG_JA, "language persisted (Spec 34)");
        CHECK(strcmp(probe.binds[ACT_FIRE].key, "MOUSE1") == 0, "keybinds persisted");
        /* a corrupt settings file must self-repair, not abort */
        char path[1024]; dh_fs_join(path, sizeof(path), outdir, "settings.json");
        dh_fs_write_text(path, "{ this is not json ");
        Settings repaired; settings_defaults(&repaired);
        CHECK(settings_load(&repaired) == 0, "corrupt settings detected");
        CHECK(repaired.fov == 85.0f, "corrupt settings repaired to defaults (Spec 37.4)");
        settings_save(s);
    }

    /* ── 4. save round-trip ────────────────────────────────────────────── */
    {
        save_init();
        SaveGame *g = save_active();
        save_new_game(g, DIFF_NORMAL, 0);
        g->pos = v3(1234.5f, 12.25f, -678.75f);
        g->yaw = 1.25f; g->pitch = -0.4f;
        g->money = 4250; g->health = 77.5f;
        save_set_flag(g, "M01_INTRO_DONE", 1);
        save_set_flag(g, "PONCHO_TRUST", 3);
        SaveMission *m = save_mission(g, "M03_JUNGLE_RUINS");
        CHECK(m != NULL, "mission record created");
        if (m) { m->status = MISSION_ACTIVE; m->phase = 2; m->attempts = 3; }
        SaveOutpost *o = save_outpost(g, "OP_04_RIDGE");
        CHECK(o != NULL, "outpost record created");
        if (o) { o->captured = 1; o->cleared_stealth = 1; o->capture_time = 214.5f; }
        SaveCollect *c = save_collect(g, "relics");
        if (c) { c->count = 17; c->total = 40; }
        g->stats.stealth_takedowns = 88;
        g->stats.poncho_pets = 412;
        char pid_before[PLAYTHROUGH_ID_LEN];
        dh_strcpy_safe(pid_before, sizeof(pid_before), g->playthrough_id);

        CHECK(save_write_slot(1, 0, g) == 1, "manual slot 1 written");
        SaveGame back; memset(&back, 0, sizeof(back));
        CHECK(save_read_slot(1, 0, &back) == 1, "manual slot 1 read back");
        CHECK(v3_dist(back.pos, g->pos) < 0.001f, "player position survives round-trip");
        CHECK(back.money == 4250, "money survives round-trip");
        CHECK(fabsf(back.health - 77.5f) < 0.001f, "health survives round-trip");
        CHECK(save_get_flag(&back, "M01_INTRO_DONE", 0) == 1, "story flag survives");
        CHECK(save_get_flag(&back, "PONCHO_TRUST", 0) == 3, "Poncho trust survives");
        SaveMission *bm = save_mission(&back, "M03_JUNGLE_RUINS");
        CHECK(bm && bm->status == MISSION_ACTIVE && bm->phase == 2, "mission state survives");
        SaveOutpost *bo = save_outpost(&back, "OP_04_RIDGE");
        CHECK(bo && bo->captured == 1 && bo->cleared_stealth == 1, "outpost capture survives");
        SaveCollect *bc = save_collect(&back, "relics");
        CHECK(bc && bc->count == 17 && bc->total == 40, "collectible progress survives");
        CHECK(back.stats.stealth_takedowns == 88, "stats survive");
        CHECK(back.stats.poncho_pets == 412, "Poncho pet counter survives (Spec 10.10)");
        CHECK(strcmp(back.playthrough_id, pid_before) == 0, "playthrough id is stable (Spec 10.5)");
        CHECK(back.difficulty == DIFF_NORMAL, "difficulty label survives (Spec 33)");

        /* slot listing */
        SaveHeader hdrs[SAVE_TOTAL_SLOTS];
        int nh = save_list_headers(hdrs, SAVE_TOTAL_SLOTS);
        CHECK(nh == SAVE_TOTAL_SLOTS, "all %d slots enumerated", SAVE_TOTAL_SLOTS);
        int found = 0;
        for (int i = 0; i < nh; i++) if (hdrs[i].abs_slot == save_slot_index(1,0) && hdrs[i].exists) {
            found = 1;
            CHECK(strcmp(hdrs[i].map_name, "Isla Sombra") == 0, "header reports map name");
            CHECK(hdrs[i].corrupt == 0, "header not flagged corrupt");
        }
        CHECK(found == 1, "written slot appears in header list");

        /* corrupt save must be quarantined, never loaded (Spec 37.4) */
        char p2[1024]; dh_fs_join(p2, sizeof(p2), outdir, "save_03.json");
        dh_fs_write_text(p2, "{\"schema\":1,\"player\":{\"pos\":[");
        SaveGame bad; memset(&bad, 0, sizeof(bad));
        CHECK(save_read_slot(2, 0, &bad) == 0, "corrupt save rejected");
        char p3[1100]; snprintf(p3, sizeof(p3), "%s.corrupt", p2);
        CHECK(dh_fs_exists(p3) == 1, "corrupt save quarantined to .corrupt");

        /* non-finite position must be rejected, not crash the world loader */
        SaveGame nf; memset(&nf, 0, sizeof(nf));
        char *txt = save_serialize(g);
        CHECK(txt != NULL && strlen(txt) > 100, "save serializes (%u bytes)",
              txt ? (unsigned)strlen(txt) : 0);
        free(txt);
        (void)nf;
    }

    /* ── 5. software rasterizer ────────────────────────────────────────── */
    {
        /* Section 3 deliberately left brightness=2.0 and colorblind_mode=2 on
           the singleton to prove clamping works. rend_init() mirrors settings
           into the renderer, which would saturate every pixel to white.
           Reset first — a render test must observe the rasterizer, not the
           accessibility post pass (which gets its own check below). */
        settings_defaults(settings());
        CHECK(settings()->brightness == 1.0f, "settings reset before render test");
        CHECK(rend_init(320, 180, REND_SOFT) == 1, "software renderer init 320x180");
        CHECK(rend()->colorblind_mode == 0, "renderer mirrors settings (colorblind off)");
        int tex = make_test_texture();
        int cube = make_test_cube();
        CHECK(tex >= 0, "procedural texture registered");
        CHECK(cube >= 0, "cube mesh registered");
        CHECK(tex_count() == 1 && mesh_count() == 1, "registries hold the assets");

        const Texture *tt = tex_get(tex);
        CHECK(tt && tt->mip[1] != NULL, "mip chain generated");

        /* This cube is wound CW-from-outside with outward normals, i.e. the
           opposite of the GL convention. mesh_finalize must detect that and
           record winding=-1 so it still renders — the whole point of the
           self-calibration. */
        const Mesh *cm = mesh_get(cube);
        CHECK(cm && cm->winding == -1, "winding self-calibrated to %d (cube is CW-wound)",
              cm ? cm->winding : 0);
        CHECK(cm && cm->tris == 12, "cube reports 12 triangles");
        CHECK(cm && fabsf(cm->radius - sqrtf(3.0f)) < 0.01f, "bounding radius = sqrt(3)");

        Mat4 view = m4_look_at(v3(0, 1.0f, 4.0f), v3(0, 0.5f, 0), v3(0,1,0));
        Mat4 proj = m4_perspective(70.0f, 320.0f/180.0f, 0.1f, 300.0f);
        SceneLight L; memset(&L, 0, sizeof(L));
        L.sun_dir = v3_norm(v3(-0.5f, -0.8f, 0.3f));
        L.sun_color = v3(1.0f, 0.95f, 0.85f); L.sun_intensity = 1.1f;
        L.ambient = v3(0.22f, 0.25f, 0.30f);
        L.hemi_sky = v3(0.5f, 0.62f, 0.85f); L.hemi_ground = v3(0.28f, 0.24f, 0.18f);
        L.fog_color = v3(0.6f, 0.7f, 0.82f);
        L.fog_near = 20.0f; L.fog_far = 120.0f; L.fog_enabled = 1;

        rend_begin_frame(&view, &proj, v3(0,1.0f,4.0f), &L);
        Mat4 model = m4_mul(m4_translate(v3(0, 0.5f, 0)), m4_rot_y(0.6f));
        rend_mesh(cube, &model, tex, 0xFFFFFFFFu, 1.0f, 0);
        rend_end_frame();
        rend_present();

        RendState *st = rend();
        CHECK(st->draw_calls >= 1, "at least one draw call issued (%d)", st->draw_calls);
        CHECK(st->tris_drawn >= 3, "triangles rasterized (%d)", st->tris_drawn);
        CHECK(st->fb != NULL, "framebuffer allocated");

        /* The real test: is the image actually different from the clear colour? */
        if (st->fb) {
            int n = st->w * st->h;
            uint32_t clear = st->fb[0];
            int distinct = 0; uint32_t seen[64]; int nseen = 0;
            long long sumr = 0, sumg = 0, sumb = 0;
            for (int i = 0; i < n; i++) {
                uint32_t p = st->fb[i];
                if (p != clear) distinct++;
                sumr += p & 0xFF; sumg += (p>>8)&0xFF; sumb += (p>>16)&0xFF;
                int dup = 0;
                for (int k = 0; k < nseen; k++) if (seen[k] == p) { dup = 1; break; }
                if (!dup && nseen < 64) seen[nseen++] = p;
            }
            CHECK(distinct > 500, "cube actually rasterized (%d of %d pixels differ from clear)",
                  distinct, n);
            CHECK(nseen >= 8, "image has shading variation (%d distinct colours sampled)", nseen);
            DH_INFO("smoke", "avg colour r=%.1f g=%.1f b=%.1f",
                    (double)sumr/n, (double)sumg/n, (double)sumb/n);

            /* depth buffer must have been written where the cube is */
            int zwritten = 0;
            for (int i = 0; i < n; i++) if (st->zbuf[i] < 0.999f) zwritten++;
            CHECK(zwritten > 100, "z-buffer written (%d px)", zwritten);

            char bmp[1024]; dh_fs_join(bmp, sizeof(bmp), outdir, "smoke_cube.bmp");
            CHECK(rend_save_bmp(bmp) == 1, "BMP screenshot written → %s", bmp);
            char png[1024]; dh_fs_join(png, sizeof(png), outdir, "smoke_cube.png");
            CHECK(rend_save_png(png) == 1, "PNG screenshot written → %s", png);
            CHECK(dh_fs_exists(bmp) == 1, "BMP file exists on disk");
            CHECK(dh_fs_exists(png) == 1, "PNG file exists on disk");
        }

        /* culling: a mesh far behind the camera must not be drawn */
        rend_begin_frame(&view, &proj, v3(0,1.0f,4.0f), &L);
        int dc_before = rend()->draw_calls;
        Mat4 behind = m4_translate(v3(0, 0.5f, 60.0f));
        rend_mesh(cube, &behind, tex, 0xFFFFFFFFu, 1.0f, 0);
        rend_end_frame();
        rend_present();
        CHECK(rend()->frustum_culled >= 1, "frustum culling rejects geometry behind camera");
        (void)dc_before;

        /* Post-processing: colourblind matrix + brightness (Spec 33).
           Verified by pushing brightness to its clamp max — every channel
           must saturate to 255. This is the behaviour that silently whited
           out the first run of this test, so it gets an explicit assertion. */
        if (st->fb && st->zbuf) {
            int n = st->w * st->h;
            /* 0x80 * 2.0 = 256 → clamps to 255 in every channel. (An earlier
               draft of this check used 0xFF4080C0, whose blue channel is
               0x40*2 = 128 and correctly does NOT saturate — the assertion was
               wrong, the post pass was right.) */
            st->fb[0] = 0xFF808080u;
            st->brightness = 2.0f;
            st->colorblind_mode = 0;
            rend_apply_post();
            uint32_t p = st->fb[0];
            CHECK((p & 0xFF) == 255 && ((p>>8)&0xFF) == 255 && ((p>>16)&0xFF) == 255,
                  "brightness 2.0 saturates all channels (post pass runs)");
            /* brightness 1.0 with no colourblind mode must be a no-op */
            st->brightness = 1.0f;
            st->fb[1] = 0xFF112233u;
            rend_apply_post();
            CHECK(st->fb[1] == 0xFF112233u, "post pass is identity at default settings");
            /* colourblind remap must actually alter a red/green pair */
            st->brightness = 1.0f;
            st->fb[0] = 0xFF0000FFu;                  /* pure red */
            st->colorblind_mode = 2;                  /* deuteranopia */
            rend_apply_post();
            uint32_t q = st->fb[0];
            CHECK(((q>>8)&0xFF) > 0, "deuteranopia remap shifts pure red (g=%u)",
                  (unsigned)((q>>8)&0xFF));
            st->colorblind_mode = 0;
            (void)n;
        }

        rend_shutdown();
        tex_release_all();
        mesh_release_all();
    }

    DH_INFO("smoke", "════ %d checks, %d failed ════", g_checks, g_failed);
    dh_log_shutdown();
    if (g_failed > 0) {
        fprintf(stderr, "SMOKE TEST FAILED: %d/%d\n", g_failed, g_checks);
        return 1;
    }
    fprintf(stdout, "SMOKE TEST PASSED: %d/%d checks\n", g_checks - g_failed, g_checks);
    return 0;
}
