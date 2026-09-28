/* ══════════════════════════════════════════════════════════════════════════
   M9 — Poncho glue (sim lives in ai/dog.c). Included from game.c.
   Scope (honest): one companion, box-built like the rest of the fauna.
   Heel / stay / hunt orders (O), aim + O sends him to bark at a guard
   (pulls that guard to SUSPICIOUS), HUNT highlights pickups + game within
   the sniff radius, guards in COMBAT can knock him out (never killed):
   hold E 3 s to revive or he gets up on his own after 30 s. Pet with E.
   No bite/takedown, no fetch, no animation rig — logged in M9 report.
   ══════════════════════════════════════════════════════════════════════════ */

static float poncho_ground(void *ud, float x, float z) { return ground_y((Game *)ud, x, z); }

static void poncho_init(Game *g) {
    Vec3 p = v3(g->spawn.x + 7.f, 0.f, g->spawn.z - 5.f);   /* caught in a snare by the beach */
    p.y = ground_y(g, p.x, p.z);
    dog_init(&g->dog, p, 1);
    g->dog_scents = 0;
}

static void poncho_cue(Game *g, SfxId id, float vol) {
    float d = v3_dist(g->cam_pos, g->dog.pos);
    if (d > 45.f) return;
    pol_cue(g, id, vol * (1.f - d / 50.f), 0.95f + 0.1f * rng_f(&g->rng));
}

/* enemy nearest the crosshair (within 40 m, not already fighting) */
static int poncho_pick_target(Game *g) {
    int best = -1; float bd = 0.97f;
    Vec3 eye, fw;
    player_camera(&g->player, &eye, &fw);          /* sim-side: don't rely on last render */
    for (int i = 0; i < g->enemies.count; i++) {
        Enemy *e = &g->enemies.v[i];
        if (e->state == EN_DEAD || e->state == EN_COMBAT) continue;
        Vec3 to = v3_sub(v3(e->pos.x, e->pos.y + 1.2f, e->pos.z), eye);
        float l = v3_len(to);
        if (l < 1.f || l > 40.f) continue;
        float dt = v3_dot(to, fw) / l;
        if (dt > bd) { bd = dt; best = i; }
    }
    return best;
}

static void poncho_frame(Game *g, const PlatInput *in, float dt) {
    Dog *d = &g->dog;
    Player *p = &g->player;
    if (g->dog_msg_cd > 0.f) g->dog_msg_cd -= dt;
    float pd = v3_dist_xz(p->pos, d->pos);
    int on_foot = g->in_vehicle < 0 && !player_is_down(p);

    /* E: free / pet (revive is a hold, handled via DogWorld.reviving) */
    if ((in->pressed & BTN_USE) && on_foot && pd < 2.4f) {
        if (d->state == DOG_TRAPPED && g->act == 0) {
            dog_free(d);
            game_message(g, "PONCHO IS FREE! [O] HEEL/STAY/HUNT  -  AIM + [O] SEND HIM TO DISTRACT");
            g->message_t = 5.f;
            poncho_cue(g, SFX_BARK, 0.9f);
        } else if (dog_active(d) && dog_pet(d)) {
            game_message(g, "GOOD BOY, PONCHO  (BOND %d)", dog_bond_level(d));
            g->message_t = 2.f;
            poncho_cue(g, SFX_JINGLE, 0.8f);
        }
    }
    /* O: orders */
    if ((in->pressed & BTN_DOG) && d->state != DOG_TRAPPED) {
        if (d->state == DOG_KO) {
            game_message(g, "PONCHO IS DOWN - HOLD [E] BESIDE HIM"); g->message_t = 2.f;
        } else if (in->buttons & BTN_AIM) {
            int t = poncho_pick_target(g);
            if (t >= 0 && dog_distract(d, g->enemies.v[t].pos)) {
                game_message(g, "PONCHO - GET 'EM! (DISTRACT)"); g->message_t = 2.f;
            } else { game_message(g, "NO GUARD UNDER THE CROSSHAIR (40 M)"); g->message_t = 2.f; }
        } else {
            int c = dog_cycle_command(d);
            game_message(g, "PONCHO: %s", dog_cmd_name(c)); g->message_t = 2.f;
        }
        poncho_cue(g, SFX_BARK, 0.5f);
    }

    DogWorld w = { p->pos, p->yaw, (in->buttons & BTN_USE) && on_foot, poncho_ground, g };
    int ev = dog_update(d, &w, dt);

    if (ev & DOG_EV_BARK)    poncho_cue(g, d->state == DOG_TRAPPED ? SFX_WHINE : SFX_BARK, 0.8f);
    if (ev & DOG_EV_JINGLE)  poncho_cue(g, SFX_JINGLE, 0.35f);
    if (ev & DOG_EV_REVIVED) { game_message(g, "PONCHO IS BACK ON HIS PAWS"); g->message_t = 2.5f; poncho_cue(g, SFX_BARK, 0.8f); }
    if (ev & DOG_EV_ARRIVED) {                    /* the nearest calm guard comes to look */
        int best = -1; float bd = 9.f;
        for (int i = 0; i < g->enemies.count; i++) {
            Enemy *e = &g->enemies.v[i];
            if (e->state == EN_DEAD || e->state == EN_COMBAT) continue;
            float dd = v3_dist_xz(e->pos, d->pos);
            if (dd < bd) { bd = dd; best = i; }
        }
        if (best >= 0) {
            Enemy *e = &g->enemies.v[best];
            e->state = EN_SUSPICIOUS; e->state_t = 0.f;
            if (e->meter < 0.4f) e->meter = 0.4f;
            e->lkp = d->pos; e->lkp_age = 0.f;
        }
    }

    /* guards fighting at close range knock him out (never lethal) */
    if (dog_active(d)) {
        for (int i = 0; i < g->enemies.count; i++) {
            const Enemy *e = &g->enemies.v[i];
            if (e->state != EN_COMBAT) continue;
            if (v3_dist_xz(e->pos, d->pos) < 2.6f && dog_damage(d, 22.f * dt)) {
                game_message(g, "PONCHO IS DOWN! HOLD [E] TO HELP HIM (AUTO 30 S)");
                g->message_t = 3.f;
                poncho_cue(g, SFX_WHINE, 1.f);
                break;
            }
        }
    }

    /* sniff: count scents within radius while hunting */
    g->dog_scents = 0;
    if (d->state == DOG_HUNT) {
        float r = dog_sniff_radius(d);
        for (int i = 0; i < g->pickups.count; i++)
            if (g->pickups.v[i].live && v3_dist(g->pickups.v[i].pos, d->pos) < r) g->dog_scents++;
        if (g->act == 0)
            for (int i = 0; i < g->critter_count; i++)
                if (g->critters[i].state < 3 && v3_dist(g->critters[i].pos, d->pos) < r) g->dog_scents++;
    }
}

static void poncho_box(Game *g, Vec3 c, float yaw, Vec3 sc, uint32_t col) {
    Mat4 m = m4_mul(m4_translate(c), m4_mul(m4_rot_y(yaw), m4_scale(sc)));
    rend_mesh_lit(g->mesh_box, &m, -1, col, 1.f, 1, 1, 1, 0, 1);
}

static void draw_poncho(Game *g) {
    const Dog *d = &g->dog;
    if (d->state == DOG_TRAPPED && g->act != 0) return;
    if (v3_dist(g->cam_pos, d->pos) > 150.f || !rend_should_draw(&d->pos, 1.5f)) return;
    const uint32_t fur = 0xFF3A6EA8u, dark = 0xFF1E3452u, collar = 0xFF2828D0u;  /* ABGR: warm tan, red collar */
    Vec3 fw = v3(sinf(d->yaw), 0.f, cosf(d->yaw));
    Vec3 rt = v3(fw.z, 0.f, -fw.x);
    Vec3 b = d->pos;
    if (d->state == DOG_KO) {                        /* lying on his side */
        poncho_box(g, v3(b.x, b.y + 0.14f, b.z), d->yaw, v3(0.5f, 0.26f, 0.72f), fur);
        poncho_box(g, v3_add(v3(b.x, b.y + 0.14f, b.z), v3_mul(fw, 0.45f)), d->yaw, v3(0.26f, 0.22f, 0.3f), fur);
        return;
    }
    float sp = sqrtf(d->vel.x * d->vel.x + d->vel.z * d->vel.z);
    float bob = sp > 0.5f ? fabsf(sinf(d->gait * 3.f)) * 0.05f : 0.f;
    Vec3 body = v3(b.x, b.y + 0.48f + bob, b.z);
    poncho_box(g, body, d->yaw, v3(0.3f, 0.28f, 0.68f), fur);
    Vec3 head = v3_add(v3_add(body, v3_mul(fw, 0.42f)), v3(0, 0.2f, 0));
    if (d->state == DOG_HUNT) head.y -= 0.18f;       /* nose to the ground */
    poncho_box(g, head, d->yaw, v3(0.24f, 0.24f, 0.28f), fur);
    poncho_box(g, v3_add(head, v3_mul(fw, 0.18f)), d->yaw, v3(0.14f, 0.12f, 0.16f), dark);   /* muzzle */
    poncho_box(g, v3_add(v3_add(head, v3_mul(rt,  0.1f)), v3(0, 0.15f, -0.0f)), d->yaw, v3(0.06f, 0.12f, 0.06f), dark);
    poncho_box(g, v3_add(v3_add(head, v3_mul(rt, -0.1f)), v3(0, 0.15f, -0.0f)), d->yaw, v3(0.06f, 0.12f, 0.06f), dark);
    poncho_box(g, v3_add(body, v3_add(v3_mul(fw, 0.3f), v3(0, 0.12f, 0))), d->yaw, v3(0.32f, 0.06f, 0.06f), collar);
    float wag = sinf(g->time * (d->state == DOG_TRAPPED ? 4.f : 12.f)) * 0.12f;
    poncho_box(g, v3_add(v3_add(body, v3_mul(fw, -0.4f)), v3_add(v3_mul(rt, wag), v3(0, 0.14f, 0))), d->yaw,
               v3(0.06f, 0.06f, 0.24f), fur);
    for (int l = 0; l < 4; l++) {                    /* legs swing with the gait */
        float fz = (l < 2) ? 0.24f : -0.24f, fx = (l & 1) ? 0.1f : -0.1f;
        float sw = sp > 0.5f ? sinf(d->gait * 3.f + (l == 0 || l == 3 ? 0.f : 3.14f)) * 0.08f : 0.f;
        Vec3 lp = v3_add(v3_add(v3(b.x, b.y + 0.18f, b.z), v3_mul(fw, fz + sw)), v3_mul(rt, fx));
        poncho_box(g, lp, d->yaw, v3(0.08f, 0.36f, 0.08f), dark);
    }
    if (d->state == DOG_TRAPPED) {                   /* snare: stake + taut rope lines */
        Vec3 stake = v3_add(b, v3_mul(rt, 0.7f));
        poncho_box(g, v3(stake.x, stake.y + 0.25f, stake.z), 0.f, v3(0.08f, 0.5f, 0.08f), 0xFF20405Au);
        Vec3 ln[2] = { v3(stake.x, stake.y + 0.3f, stake.z), v3_add(body, v3_mul(fw, 0.3f)) };
        rend_lines(ln, 1, 0xFF70B0D0u, 2.f);
    }
    /* HUNT: gold scent beams on everything in the sniff radius */
    if (d->state == DOG_HUNT) {
        Vec3 pts[2 * 48]; int n = 0; float r = dog_sniff_radius(d);
        for (int i = 0; i < g->pickups.count && n < 48; i++) {
            const Pickup *k = &g->pickups.v[i];
            if (!k->live || v3_dist(k->pos, d->pos) >= r) continue;
            pts[n * 2] = k->pos; pts[n * 2 + 1] = v3(k->pos.x, k->pos.y + 3.f, k->pos.z); n++;
        }
        if (g->act == 0)
            for (int i = 0; i < g->critter_count && n < 48; i++) {
                const Critter *c = &g->critters[i];
                if (c->state >= 3 || v3_dist(c->pos, d->pos) >= r) continue;
                pts[n * 2] = c->pos; pts[n * 2 + 1] = v3(c->pos.x, c->pos.y + 4.f, c->pos.z); n++;
            }
        if (n) rend_lines(pts, n, 0xFF40D0F0u, 2.f);
    }
}

/* paw icon + status, bottom-right above the ammo block */
static void poncho_draw_hud(Game *g) {
    const Dog *d = &g->dog;
    if (d->state == DOG_TRAPPED) return;
    float ui = settings()->ui_scale;
    float x = (float)g->w - 230.f * ui, y = (float)g->h - 150.f * ui, s = 6.f * ui;
    uint32_t col = d->state == DOG_KO ? 0xFF4040E0u : C_GOLD;
    rend_quad2d(x, y + s * 1.2f, s * 2.4f, s * 2.f, -1, 0, 0, 1, 1, col);            /* pad */
    for (int t = 0; t < 4; t++)
        rend_quad2d(x - s * 0.5f + t * s * 0.95f, y + (t == 0 || t == 3 ? s * 0.3f : -s * 0.2f),
                    s * 0.8f, s * 0.9f, -1, 0, 0, 1, 1, col);                          /* toes */
    char buf[96];
    if (d->state == DOG_KO) {
        float auto_left = DOG_AUTO_RECOVER - d->state_t;
        snprintf(buf, sizeof buf, "PONCHO DOWN  [E] %.1f/3  AUTO %ds", d->revive_t, (int)(auto_left > 0 ? auto_left : 0));
    } else if (d->state == DOG_HUNT)
        snprintf(buf, sizeof buf, "PONCHO HUNT  %d SCENT%s  %dM", g->dog_scents, g->dog_scents == 1 ? "" : "S", (int)dog_sniff_radius(d));
    else
        snprintf(buf, sizeof buf, "PONCHO %s  BOND %d", dog_state_name(d->state), dog_bond_level(d));
    font_text_shadow(x + s * 3.2f, y + s * 0.4f, 1.3f * ui, buf, col);
    bar(x + s * 3.2f, y + s * 2.3f, 120.f * ui, 3.f * ui, d->hp / DOG_HP_MAX, col, 0x60000000u);
}

static void poncho_save(Game *g, SaveGame *s) {
    save_set_flag(s, "poncho_freed", g->dog.state != DOG_TRAPPED);
    save_set_flag(s, "poncho_bond", g->dog.bond_xp);
    save_set_flag(s, "poncho_cmd", g->dog.cmd);
    s->stats.poncho_pets = g->dog.pets;
}
static void poncho_load(Game *g, const SaveGame *s) {
    Dog *d = &g->dog;
    if (save_get_flag(s, "poncho_freed", 0)) {
        dog_init(d, g->player.pos, 0);
        d->pos.x += 1.5f; d->pos.y = ground_y(g, d->pos.x, d->pos.z);
        d->bond_xp = save_get_flag(s, "poncho_bond", 0);
        dog_command(d, save_get_flag(s, "poncho_cmd", 0) == DOG_CMD_STAY ? DOG_CMD_FOLLOW : save_get_flag(s, "poncho_cmd", 0));
        d->pets = (int)s->stats.poncho_pets;
    }
}
