/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — Player traversal controller implementation
   ══════════════════════════════════════════════════════════════════════════ */
#include "player.h"
#include "../core/dh_log.h"
#include <string.h>

static const float PITCH_LIMIT = 1.5358f;  /* ~88° — never fully vertical */

void player_init(Player *p, const Terrain *t, const ObstacleSet *obs, Vec3 spawn) {
    memset(p, 0, sizeof(*p));
    p->terrain = t;
    p->obs = obs;
    p->yaw = 0.0f; p->pitch = 0.0f;
    p->stamina = PL_STAMINA_MAX;
    p->breath_max = PL_BREATH_BASE;
    p->breath = p->breath_max;
    p->health = 100.0f;
    p->health_max = 100.0f;
    p->stamina_mul = 1.0f;
    p->ground_normal = v3(0, 1, 0);
    p->stance = PL_ST_AIR;
    player_set_spawn(p, spawn);
}

void player_set_spawn(Player *p, Vec3 spawn) {
    float gy = 0.0f;
    if (p->terrain) {
        gy = terrain_height(p->terrain, spawn.x, spawn.z);
        if (gy < p->terrain->sea_level + 0.2f) gy = p->terrain->sea_level + 0.2f;
    }
    p->pos = v3(spawn.x, gy + 0.05f, spawn.z);
    p->vel = v3(0, 0, 0);
    p->ground_y = gy;
    p->grounded = 1;
    p->stance = PL_ST_GROUND;
    p->coyote = PL_COYOTE;
}

void player_respawn(Player *p, Vec3 spawn) {
    if (p->health_max < 1.0f) p->health_max = 100.0f;
    p->health = p->health_max;
    p->breath = p->breath_max;
    p->stamina = PL_STAMINA_MAX;
    p->stance = PL_ST_GROUND;
    player_set_spawn(p, spawn);
}

int player_is_down(const Player *p) { return p->stance == PL_ST_DOWN || p->health <= 0.0f; }

Vec3 player_forward(const Player *p) {
    return v3(sinf(p->yaw), 0.0f, cosf(p->yaw));
}

float player_eye_y(const Player *p) { return p->pos.y + p->eye; }

void player_camera(const Player *p, Vec3 *out_pos, Vec3 *out_dir) {
    if (out_pos) *out_pos = v3(p->pos.x, p->pos.y + p->eye, p->pos.z);
    if (out_dir) {
        float cp = cosf(p->pitch), sp = sinf(p->pitch);
        *out_dir = v3_norm(v3(cp * sinf(p->yaw), sp, cp * cosf(p->yaw)));
    }
}

const char *player_stance_name(PlayerStance s) {
    switch (s) {
    case PL_ST_GROUND: return "GROUND";
    case PL_ST_AIR:    return "AIR";
    case PL_ST_SWIM:   return "SWIM";
    case PL_ST_MANTLE: return "MANTLE";
    case PL_ST_SLIDE:  return "SLIDE";
    case PL_ST_ZIP:    return "ZIPLINE";
    case PL_ST_DOWN:   return "DOWN";
    }
    return "?";
}

/* ── helpers ─────────────────────────────────────────────────────────────── */
static float current_height(const Player *p) {
    if (p->stance == PL_ST_SWIM) return 0.6f;
    return p->crouched ? PL_CROUCH_HEIGHT : PL_HEIGHT;
}

static void start_mantle(Player *p, Vec3 dir, float top_y, int is_vault) {
    float rise = top_y - p->pos.y;
    p->mantle_from = p->pos;
    /* land just past the wall face */
    Vec3 target = v3_add(p->pos, v3_mul(dir, PL_RADIUS + 0.55f));
    target.y = top_y + 0.03f;
    p->mantle_to = target;
    p->mantle_dur = 0.26f + 0.10f * (rise / PL_VAULT_HEIGHT);
    if (p->mantle_dur < 0.22f) p->mantle_dur = 0.22f;
    if (p->mantle_dur > 0.70f) p->mantle_dur = 0.70f;
    p->mantle_t = 0.0f;
    p->stance = PL_ST_MANTLE;
    p->vel = v3(0, 0, 0);
    p->grounded = 0;
    if (is_vault) p->stats.vaults++; else p->stats.mantles++;
}

/* Landing bookkeeping shared by the pre-move snap and the post-move clamp.
   WHY shared: at ~12 m/s impact a whole frame of fall is 0.2 m, so the snap
   test (feet within 7.5 cm of support) usually misses and the post-move clamp
   catches the landing instead — if damage only lived in the snap branch, fast
   falls would deal no damage at all. */
static void land_on(Player *p, float support, int was_grounded) {
    if (!was_grounded) {
        float fall = p->fall_apex - support;
        if (fall > p->stats.max_fall_m) p->stats.max_fall_m = fall;
        if (fall > PL_FALL_SAFE_M) {
            int rolled = (p->slide_t > 0.0f);   /* slide_t doubles as roll window */
            float dmg = (fall - PL_FALL_SAFE_M) * PL_FALL_DMG_PER_M;
            if (rolled) dmg = 0.0f;             /* roll-cancel (6.1) */
            else { p->health -= dmg; p->stats.fall_damage_taken += dmg; }
            if (p->health <= 0.0f) {
                p->health = 0.0f; p->stance = PL_ST_DOWN;
            }
        }
        p->stats.landings++;
    }
    p->grounded = 1;
    p->ground_y = support;
    p->pos.y = support;
    p->fall_apex = support;
    if (p->vel.y < 0.0f) p->vel.y = 0.0f;
    p->coyote = PL_COYOTE;
    p->stats.max_fall_m = 0.0f;
}

/* ── main update ─────────────────────────────────────────────────────────── */
void player_input(Player *p, const PlayerInput *in, float dt) {
    if (!p || dt <= 0.0f) return;
    if (p->stance == PL_ST_DOWN) return;

    /* look */
    p->yaw   += in->look_dx;
    p->pitch  = dh_clampf(p->pitch + in->look_dy, -PITCH_LIMIT, PITCH_LIMIT);
    if (p->yaw > DH_TAU) p->yaw -= DH_TAU;
    if (p->yaw < 0.0f)   p->yaw += DH_TAU;

    const Terrain *T = p->terrain;
    const float water = T ? T->sea_level : 0.0f;
    const float h = current_height(p);

    /* ── environment probe ── */
    float terr_y = T ? terrain_height(T, p->pos.x, p->pos.z) : 0.0f;
    float slope  = T ? terrain_steepness(T, p->pos.x, p->pos.z) : 0.0f;
    Vec3  nrm    = T ? terrain_normal(T, p->pos.x, p->pos.z) : v3(0,1,0);
    float obs_sup = obstacles_support(p->obs,
                                      p->pos, PL_RADIUS, p->pos.y, PL_STEP_HEIGHT);
    if (obs_sup < -1e8f) obs_sup = -1e9f;
    float support = dh_maxf(terr_y, obs_sup);

    int body_in_water = (p->pos.y + h * 0.5f) < water - 0.05f;
    int wading        = (!body_in_water) && (p->pos.y < water - 0.02f);

    /* ── mantle animation takes over everything ── */
    if (p->stance == PL_ST_MANTLE) {
        p->mantle_t += dt;
        float k = dh_clampf(p->mantle_t / p->mantle_dur, 0.0f, 1.0f);
        float ease = k * k * (3.0f - 2.0f * k);         /* smoothstep */
        Vec3 a = p->mantle_from, b = p->mantle_to;
        float arc = sinf(k * DH_PI) * 0.22f;            /* slight lift over the lip */
        p->pos = v3(dh_lerp(a.x, b.x, ease),
                    dh_lerp(a.y, b.y, ease) + arc,
                    dh_lerp(a.z, b.z, ease));
        if (k >= 1.0f) {
            p->pos = b;
            p->stance = PL_ST_GROUND;
            p->grounded = 1;
            p->ground_y = b.y;
            p->vel = v3(0, 0, 0);
            p->coyote = PL_COYOTE;
        }
        p->eye = dh_lerp(p->eye, p->crouched ? PL_EYE_CROUCH : PL_EYE_STAND,
                          1.0f - expf(-12.0f * dt));
        return;
    }

    /* ── zipline (6.1): mount with USE near a cable, ride, drop with SPACE ── */
    int mounted = 0;
    if (p->stance != PL_ST_ZIP && p->zips && in->use_pressed) {
        int zi = -1; float zs = 0.0f, zd = 0.0f;
        Vec3 chest = v3(p->pos.x, p->pos.y + h * 0.80f, p->pos.z);
        if (zip_nearest(p->zips, chest, 1.25f, &zi, &zs, &zd)) {
            p->stance = PL_ST_ZIP;
            p->zip_i = zi; p->zip_s = zs; p->zip_speed = 1.5f;
            p->zip_sway = 0.0f; p->zip_sway_v = 0.0f;
            p->grounded = 0; p->coyote = 0.0f; p->vel = v3(0, 0, 0);
            p->stats.zip_rides++;
            mounted = 1;
        }
    }
    if (p->stance == PL_ST_ZIP) {
        const Zipline *z = &p->zips->v[p->zip_i];
        Vec3 ab = v3_sub(z->b, z->a);
        float len = v3_len(ab);
        Vec3 dir = v3_mul(ab, 1.0f / len);
        /* gravity component along the cable drives the ride; rolling drag and
           a brake when the player holds crouch keep speeds honest (44 feel) */
        float accel = -PL_GRAVITY * 0.62f * dir.y;
        accel -= 0.30f * p->zip_speed;
        if (in->crouch) accel -= 9.0f * (p->zip_speed > 0.5f ? 1.0f : 0.0f);
        p->zip_speed = dh_clampf(p->zip_speed + accel * dt, -3.0f, 15.0f);
        p->zip_s += p->zip_speed * dt;
        p->stats.zip_m += fabsf(p->zip_speed * dt);
        /* lateral pendulum: excited by speed changes, damped spring */
        p->zip_sway_v += (-14.0f * p->zip_sway - 1.8f * p->zip_sway_v) * dt
                         + (in->mx * 2.2f) * dt;
        p->zip_sway = dh_clampf(p->zip_sway + p->zip_sway_v * dt, -1.1f, 1.1f);

        /* USE on the mount frame must not instantly release */
        int drop = in->jump_pressed || (in->use_pressed && !mounted);
        if (p->zip_s <= 0.0f)  { p->zip_s = 0.0f; p->zip_speed = dh_maxf(0.0f, p->zip_speed); }
        if (p->zip_s >= len || drop) {
            /* release: keep most of the cable momentum + a small lift */
            Vec3 lat = v3_norm(v3_cross(dir, v3(0, 1, 0)));
            p->vel = v3_add(v3_mul(dir, p->zip_speed * 0.88f),
                            v3_mul(lat, p->zip_sway_v * 0.5f));
            p->vel.y = in->jump_pressed ? 3.1f : -0.5f;
            p->stance = PL_ST_AIR;
            p->grounded = 0; p->coyote = 0.0f;
            p->fall_apex = p->pos.y;
        } else {
            Vec3 base = v3_add(z->a, v3_mul(dir, p->zip_s));
            Vec3 lat = v3_norm(v3_cross(dir, v3(0, 1, 0)));
            base = v3_add(base, v3_mul(lat, p->zip_sway));
            /* hands on the cable: feet hang ~1.55 m below it */
            p->pos = v3(base.x, base.y - 1.55f, base.z);
            p->grounded = 0; p->coyote = 0.0f;
            p->eye = dh_lerp(p->eye, PL_EYE_STAND, 1.0f - expf(-12.0f * dt));
            if (p->pos.y < water - 0.1f) { p->stance = PL_ST_AIR; }  /* dip → swim */
            p->stats.distance_m += fabsf(p->zip_speed * dt);
            return;
        }
    }

    /* ── swimming ── */
    if (body_in_water) {
        if (p->stance != PL_ST_SWIM) {
            p->stance = PL_ST_SWIM;
            p->vel = v3_mul(p->vel, 0.35f);   /* water kills momentum */
        }
        p->grounded = 0;
        p->coyote = 0.0f;

        /* breath */
        p->breath -= dt;
        if (p->breath <= 0.0f) {
            p->breath = 0.0f;
            p->health -= 8.0f * dt;           /* drowning */
            if (p->health <= 0.0f) { p->health = 0.0f; p->stance = PL_ST_DOWN; return; }
        }
        p->stats.swim_time_s += (int)dt;

        Vec3 fwd = player_forward(p);
        Vec3 right = v3(fwd.z, 0.0f, -fwd.x);
        Vec3 wish = v3_norm(v3_add(v3_mul(fwd, in->my), v3_mul(right, in->mx)));
        float speed = in->sprint && p->stamina > 1.0f ? 4.4f : 2.9f;

        p->vel.x = dh_lerp(p->vel.x, wish.x * speed, 1.0f - expf(-6.0f * dt));
        p->vel.z = dh_lerp(p->vel.z, wish.z * speed, 1.0f - expf(-6.0f * dt));

        /* vertical: crouch dives, jump surfaces, otherwise buoyancy */
        float vy = 0.0f;
        if (in->crouch) vy = -2.6f;
        else if (in->jump) vy = 3.0f;
        else {
            /* float toward surface slowly */
            float surf = water - 0.45f;
            vy = dh_clampf((surf - p->pos.y) * 1.2f, -0.8f, 0.9f);
        }
        if (in->sprint && p->stamina > 1.0f) { p->stamina -= PL_STAMINA_DRAIN * dt / (p->stamina_mul > 0.1f ? p->stamina_mul : 1.0f); }
        p->vel.y = dh_lerp(p->vel.y, vy, 1.0f - expf(-7.0f * dt));

        p->pos = v3_add(p->pos, v3_mul(p->vel, dt));

        /* never sink through the seabed */
        if (p->pos.y < terr_y + 0.05f) { p->pos.y = terr_y + 0.05f; p->vel.y = 0.0f; }
        /* keep head above the sky when surfacing */
        if (p->pos.y + 0.6f > water + 0.55f) p->pos.y = water - 0.05f;

        obstacles_resolve(p->obs, &p->pos,
                          PL_RADIUS, 0.6f, NULL);
        p->eye = dh_lerp(p->eye, PL_EYE_SWIM, 1.0f - expf(-10.0f * dt));
        p->stats.distance_m += dt * sqrtf(p->vel.x*p->vel.x + p->vel.z*p->vel.z);
        return;
    }

    /* surfaced → refill breath fast */
    if (p->stance == PL_ST_SWIM) { p->stance = PL_ST_AIR; }
    p->breath = dh_minf(p->breath_max, p->breath + 14.0f * dt);

    /* ── ground detection ── */
    int was_grounded = p->grounded;
    float snap_tol = 0.075f;
    if (p->pos.y <= support + snap_tol && p->vel.y <= 1.2f) {
        land_on(p, support, was_grounded);
        p->steepness = slope;
        p->ground_normal = nrm;
    } else {
        if (was_grounded) {
            p->stats.max_fall_m = 0.0f;   /* start tracking from this height */
            p->fall_apex = p->pos.y;      /* apex tracking begins at take-off */
        }
        p->grounded = 0;
        p->coyote = dh_maxf(0.0f, p->coyote - dt);
        p->steepness = slope;
        p->ground_normal = nrm;
        /* fall distance = apex - current feet; the landing handler converts it */
        if (p->pos.y > p->fall_apex) p->fall_apex = p->pos.y;
        float drop = p->fall_apex - p->pos.y;
        if (drop > p->stats.max_fall_m) p->stats.max_fall_m = drop;
    }

    /* roll-cancel window tick */
    p->slide_t = dh_maxf(0.0f, p->slide_t - dt);
    if (in->roll) p->slide_t = PL_ROLL_CANCEL;

    /* ── crouch ── */
    p->crouched = in->crouch ? 1 : 0;
    float height_now = current_height(p);

    /* ── wish direction & speed ── */
    Vec3 fwd = player_forward(p);
    Vec3 right = v3(fwd.z, 0.0f, -fwd.x);
    Vec3 wish = v3_add(v3_mul(fwd, in->my), v3_mul(right, in->mx));
    float wish_len = v3_len(wish);
    if (wish_len > 1.0f) wish = v3_mul(wish, 1.0f / wish_len);

    int want_sprint = in->sprint && !p->crouched && p->stamina > 1.0f &&
                      in->my > 0.35f && !in->walk_only;
    float target;
    if (p->crouched)              target = 1.45f;
    else if (in->walk_only)       target = 2.45f;
    else if (want_sprint)         target = 7.2f;
    else if (in->my > 0.05f || in->mx != 0.0f) target = 4.7f;
    else                          target = 0.0f;
    if (wading) target *= 0.62f;

    if (want_sprint) {
        p->stamina -= PL_STAMINA_DRAIN * dt / (p->stamina_mul > 0.1f ? p->stamina_mul : 1.0f);
        p->stamina_idle = 0.0f;
        if (p->stamina < 0.0f) p->stamina = 0.0f;
    } else {
        p->stamina_idle += dt;
        if (p->stamina_idle > PL_STAMINA_DELAY) {
            p->stamina = dh_minf(PL_STAMINA_MAX, p->stamina + PL_STAMINA_REGEN * dt);
        }
    }

    float accel = p->grounded ? PL_GROUND_ACCEL : PL_AIR_ACCEL;
    Vec3 wish_vel = v3_mul(wish, target);

    if (p->grounded && wish_len < 0.01f) {
        /* friction to a stop (feels weighty, not slippery) */
        float sp = v3_len(v3(p->vel.x, 0, p->vel.z));
        if (sp > 0.001f) {
            float drop = sp * PL_GROUND_FRICTION * dt;
            float k = dh_maxf(0.0f, sp - drop) / sp;
            p->vel.x *= k; p->vel.z *= k;
        }
    } else {
        p->vel.x += (wish_vel.x - p->vel.x) * dh_clampf(accel * dt, 0.0f, 1.0f);
        p->vel.z += (wish_vel.z - p->vel.z) * dh_clampf(accel * dt, 0.0f, 1.0f);
    }

    /* ── jump / vault / mantle ── */
    if (in->jump_pressed) p->jump_buffer = PL_JUMP_BUFFER;
    else p->jump_buffer = dh_maxf(0.0f, p->jump_buffer - dt);

    int can_jump = p->grounded || p->coyote > 0.0f;
    int moving_forward = in->my > 0.25f || wish_len > 0.25f;

    if (p->jump_buffer > 0.0f && can_jump) {
        /* Is there a wall in front we should vault/mantle instead of hopping?
           Two senses: a probe slightly ahead (jumping early), and the volume
           that physically blocked us last frame (jumping on contact). Both are
           what a player means by "I jumped at the wall". */
        float top = -1e9f;
        Vec3 probe_dir = (wish_len > 0.01f) ? wish : fwd;
        int wall = obstacles_wall_ahead(p->obs,
                                        p->pos, probe_dir, PL_RADIUS,
                                        height_now, PL_VAULT_HEIGHT, &top);
        if (!wall && p->blocked && p->block_top > p->pos.y + 0.05f &&
            p->block_top <= p->pos.y + PL_VAULT_HEIGHT) {
            wall = 1; top = p->block_top;
        }
        if (wall && top > p->pos.y + PL_STEP_HEIGHT && top <= p->pos.y + PL_VAULT_HEIGHT
            && moving_forward) {
            start_mantle(p, v3_norm(probe_dir), top, 1);
            p->jump_buffer = 0.0f;
            p->stats.jumps++;
            return;
        }
        /* steep slope → no jump, but a slide launch */
        p->vel.y = PL_JUMP_VEL;
        p->grounded = 0;
        p->coyote = 0.0f;
        p->jump_buffer = 0.0f;
        p->stance = PL_ST_AIR;
        p->stats.jumps++;
        p->stats.last_jump_height = 0.0f;
    }
    /* variable jump height: releasing early cuts the arc */
    if (!in->jump && p->vel.y > 0.0f && p->stance == PL_ST_AIR) {
        p->vel.y *= (1.0f - dh_clampf(3.0f * dt, 0.0f, 0.5f));
    }

    /* air-mantle: catch a ledge while falling into it */
    if (!p->grounded && p->vel.y <= 0.0f && moving_forward) {
        float top = -1e9f;
        Vec3 probe_dir = (wish_len > 0.01f) ? wish : fwd;
        int wall = obstacles_wall_ahead(p->obs,
                                        p->pos, probe_dir, PL_RADIUS,
                                        height_now, PL_LEDGE_HEIGHT, &top);
        if (wall && top > p->pos.y + 0.15f && top <= p->pos.y + PL_VAULT_HEIGHT) {
            start_mantle(p, v3_norm(probe_dir), top, 0);
            return;
        }
    }

    /* ── stance: slide on steep slopes ── */
    if (p->grounded) {
        if (p->steepness > PL_SLIDE_STEEP) {
            p->stance = PL_ST_SLIDE;
            /* accelerate down the fall line */
            /* The horizontal part of the surface normal points downhill
               (normal = (hl-hr, 2e, hd-hu): on a slope rising toward +x the
               x term is negative, i.e. away from the rise). The first version
               negated it and "slid" players uphill. */
            Vec3 downhill = v3_norm(v3(p->ground_normal.x, 0.0f, p->ground_normal.z));
            if (v3_len2(downhill) > 0.01f) {
                p->vel.x += downhill.x * 12.0f * dt;
                p->vel.z += downhill.z * 12.0f * dt;
            }
        } else if (p->stance == PL_ST_SLIDE) {
            p->stance = PL_ST_GROUND;
        } else {
            p->stance = PL_ST_GROUND;
        }
    } else if (p->stance != PL_ST_AIR) {
        p->stance = PL_ST_AIR;
    }

    /* ── gravity & integrate ── */
    if (!p->grounded) {
        p->vel.y -= PL_GRAVITY * dt;
        if (p->vel.y < -PL_TERMINAL_VEL) p->vel.y = -PL_TERMINAL_VEL;
    }

    Vec3 before = p->pos;
    p->pos = v3_add(p->pos, v3_mul(p->vel, dt));

    /* ── horizontal obstacle resolution + step-up ── */
    float top_reach = -1e9f;
    p->blocked = obstacles_resolve(p->obs, &p->pos,
                                   PL_RADIUS, height_now, &top_reach);
    p->block_top = top_reach;
    /* Step-up only while we are actually pressed against the volume: snapping
       up whenever a nearby top exists made players yo-yo back onto platforms
       they had just walked off. */
    float hspeed = sqrtf(p->vel.x*p->vel.x + p->vel.z*p->vel.z);
    if (p->grounded && p->blocked && p->vel.y > -2.0f && hspeed > 0.4f &&
        top_reach > -1e8f) {
        float rise = top_reach - p->pos.y;
        if (rise > 0.001f && rise <= PL_STEP_HEIGHT) {
            p->pos.y = top_reach;              /* walk up crates/steps smoothly */
            p->ground_y = top_reach;
            p->vel.y = 0.0f;
            p->fall_apex = top_reach;
            /* Nudge horizontally onto the volume: without this the cylinder
               centre is still a radius short of the box, the support query
               finds nothing, and the player yo-yos between floor and top. */
            obstacles_step_onto(p->obs, &p->pos, PL_RADIUS, top_reach, 0.30f);
        }
    }

    /* ── re-clamp to ground (post-move, catches walking off ledges) ── */
    float terr2 = T ? terrain_height(T, p->pos.x, p->pos.z) : 0.0f;
    float sup2 = obstacles_support(p->obs,
                                   p->pos, PL_RADIUS, p->pos.y, PL_STEP_HEIGHT);
    float support2 = dh_maxf(terr2, sup2);
    if (p->pos.y < support2) {
        int was2 = p->grounded;
        land_on(p, support2, was2);
        if (p->stance == PL_ST_DOWN) return;
    } else {
        p->ground_y = support2;
    }

    /* world bounds: soft-fail back inside rather than falling out (18.4).
       Terrain space is [0,size] — the island is NOT centred on the origin. */
    if (T && T->size > 0.0f) {
        float m = 16.0f;
        float hi = T->size - m;
        if (p->pos.x < m)   { p->pos.x = m;  p->vel.x = 0.0f; }
        if (p->pos.x > hi)  { p->pos.x = hi; p->vel.x = 0.0f; }
        if (p->pos.z < m)   { p->pos.z = m;  p->vel.z = 0.0f; }
        if (p->pos.z > hi)  { p->pos.z = hi; p->vel.z = 0.0f; }
        /* never fall out of the world even if collision misses (18.4) */
        if (p->pos.y < -60.0f) {
            DH_WARN("player", "fell out of world at (%.1f,%.1f) — respawning",
                    p->pos.x, p->pos.z);
            player_set_spawn(p, v3(p->pos.x, 0.0f, p->pos.z));
        }
    }

    /* ── eye smoothing (crouch/swim transitions feel good when eased) ── */
    float eye_target = p->crouched ? PL_EYE_CROUCH : PL_EYE_STAND;
    p->eye = dh_lerp(p->eye, eye_target, 1.0f - expf(-14.0f * dt));

    /* ── stats ── */
    float dx = p->pos.x - before.x, dz = p->pos.z - before.z;
    p->stats.distance_m += sqrtf(dx*dx + dz*dz);
    if (p->vel.y > 0.0f) {
        float rise = p->pos.y - p->ground_y;
        if (rise > p->stats.last_jump_height) p->stats.last_jump_height = rise;
    }
}
