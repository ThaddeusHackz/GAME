/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — enemy AI implementation (M2)
   §65 health: grunt 100 / bruiser 160 / officer 130.
   §82 detection: rate = base·light·stance / dist², decay 0.2/s, thresholds
   0.35 SUSPICIOUS / 0.8 COMBAT, team share ≤ 8 m, LKP search.
   Everything is deterministic given the same inputs — smoke-testable.
   ══════════════════════════════════════════════════════════════════════════ */
#include "enemy.h"
#include "../core/dh_log.h"
#include <math.h>
#include <string.h>

/* archetype tuning tables */
static const float A_HP[3]     = { 100.f, 160.f, 130.f };
static const float A_SPEED[3]  = { 2.2f, 1.8f, 2.5f };
static const float A_RAD[3]    = { 0.38f, 0.52f, 0.40f };
static const float A_BASE[3]   = { 640.f, 520.f, 860.f }; /* detection constant */
static const float A_ACC[3]    = { 0.50f, 0.32f, 0.62f }; /* base hit chance */
static const float A_DMG[3]    = { 12.f, 22.f, 16.f };    /* per enemy bullet */
static const float A_FINT[3]   = { 0.18f, 0.55f, 0.14f }; /* seconds between rounds */
static const int   A_BURST[3]  = { 4, 2, 5 };
static const float A_PREF[3]   = { 18.f, 7.f, 25.f };     /* preferred range (m) */

float enemy_shot_damage(int arch)
{
    if (arch < 0 || arch > 2) return 12.f;
    return A_DMG[arch];
}

const char *enemy_state_name(int st)
{
    switch (st) {
    case EN_PATROL: return "PATROL"; case EN_SUSPICIOUS: return "SUSPICIOUS";
    case EN_SEARCH: return "SEARCH"; case EN_COMBAT: return "COMBAT";
    case EN_FLEE: return "FLEE"; case EN_DEAD: return "DEAD";
    default: return "?";
    }
}
const char *enemy_arch_name(int arch)
{
    switch (arch) {
    case EN_GRUNT: return "grunt"; case EN_BRUISER: return "bruiser";
    case EN_OFFICER: return "officer"; default: return "?";
    }
}

void enemies_init(EnemySet *s)
{
    if (!s) return;
    memset(s, 0, sizeof *s);
}

int enemies_spawn(EnemySet *s, Vec3 pos, int arch, int faction,
                  Vec3 patrol_a, Vec3 patrol_b)
{
    if (!s || s->count >= ENEMY_MAX || arch < 0 || arch > 2) return -1;
    Enemy *e = &s->v[s->count];
    memset(e, 0, sizeof *e);
    e->pos = pos;
    e->arch = arch;
    e->faction = faction;
    e->health = e->health_max = A_HP[arch];
    e->speed = A_SPEED[arch];
    e->state = EN_PATROL;
    e->patrol_a = patrol_a;
    e->patrol_b = patrol_b;
    e->patrol_leg = 1;
    e->burst_left = A_BURST[arch];
    e->yaw = atan2f(patrol_b.x - patrol_a.x, patrol_b.z - patrol_a.z);
    if (!v3_valid(e->pos)) e->pos = v3(0.f, 0.f, 0.f);   /* 18.4: no NaN */
    s->count++;
    s->alive_count++;
    return s->count - 1;
}

/* ── perception ─────────────────────────────────────────────────────────── */
static int los_clear(const Enemy *e, const EnemyView *pv,
                     const Terrain *t, const ObstacleSet *ob)
{
    Vec3 a = v3_add(e->pos, v3(0.f, 1.55f, 0.f));
    Vec3 b = pv->eye;
    if (!obstacles_segment_clear(ob, a, b)) return 0;
    /* terrain ridge occlusion: march the segment, compare against ground */
    float dist = v3_dist(a, b);
    int steps = (int)(dist / 1.5f);
    if (steps < 4) steps = 4;
    if (steps > 64) steps = 64;
    for (int i = 1; i < steps; i++) {
        float f = (float)i / (float)steps;
        Vec3 p = v3_lerp(a, b, f);
        if (terrain_height(t, p.x, p.z) > p.y + 0.15f) return 0;
    }
    return 1;
}

float enemy_detect_rate(const Enemy *e, const EnemyView *pv)
{
    if (!e || !pv || !pv->alive) return 0.f;
    float dist = v3_dist(v3_add(e->pos, v3(0.f, 1.55f, 0.f)), pv->eye);
    if (dist > 120.f) return 0.f;
    float d2 = dist * dist;
    if (d2 < 1.f) d2 = 1.f;
    float stance = pv->crouched ? 0.45f : 1.f;
    if (pv->moving) stance *= 1.6f;
    float light = pv->light > 0.05f ? pv->light : 0.05f;
    return A_BASE[e->arch] * light * stance / d2;
}

float enemy_accuracy(const Enemy *e, const EnemyView *pv, float dist)
{
    if (!e || !pv) return 0.f;
    float a = A_ACC[e->arch];
    a *= dh_clampf(1.1f - dist / 70.f, 0.2f, 1.f);
    if (pv->crouched) a *= 0.75f;
    if (pv->moving)   a *= 0.80f;
    a *= pv->light > 0.05f ? pv->light : 0.05f;
    return dh_clampf(a, 0.f, 0.95f);
}

int enemy_apply_damage(Enemy *e, float dmg, int zone)
{
    (void)zone; /* caller already applied zone+falloff multipliers */
    if (!e || e->state == EN_DEAD || !(dmg > 0.f) || !isfinite(dmg)) return 0;
    if (e->armor > 0.f) dmg *= 1.f - dh_clampf(e->armor, 0.f, 1.f);
    e->health -= dmg;
    e->hit_t = 0.18f;
    e->meter = 1.2f;                       /* getting shot ends all subtlety */
    if (e->state != EN_COMBAT) { e->state = EN_COMBAT; e->state_t = 0.f; }
    if (e->health <= 0.f) {
        e->health = 0.f;
        e->state = EN_DEAD;
        e->state_t = 0.f;
        e->shot_this_frame = 0;
        return 1;
    }
    return 0;
}

/* ── movement helper ────────────────────────────────────────────────────── */
static void move_enemy(Enemy *e, Vec3 wish, float speed, float dt,
                       const Terrain *t, const ObstacleSet *ob)
{
    float wl = v3_len(v3(wish.x, 0.f, wish.z));
    if (wl > 1e-5f) {
        wish = v3_mul(v3(wish.x, 0.f, wish.z), speed / wl);
        e->pos.x += wish.x * dt;
        e->pos.z += wish.z * dt;
        e->vel = wish;
    } else {
        e->vel = v3(0.f, 0.f, 0.f);
    }
    float r = A_RAD[e->arch];
    obstacles_resolve(ob, &e->pos, r, 1.75f, NULL);
    float ground = terrain_height(t, e->pos.x, e->pos.z);
    float sup = obstacles_support(ob, e->pos, r, e->pos.y, 0.6f);
    if (sup > ground) ground = sup;
    e->pos.y = ground;                     /* AI stays glued to walkable ground */
    e->grounded = 1;
    if (!v3_valid(e->pos)) e->pos = v3(0.f, ground, 0.f);
}

static void face_toward(Enemy *e, Vec3 target, float dt)
{
    float want = atan2f(target.x - e->pos.x, target.z - e->pos.z);
    float diff = want - e->yaw;
    while (diff >  DH_PI) diff -= 2.f * DH_PI;
    while (diff < -DH_PI) diff += 2.f * DH_PI;
    float turn = 8.f * dt;
    if (diff >  turn) diff =  turn;
    if (diff < -turn) diff = -turn;
    e->yaw += diff;
}

static void enter_combat(Enemy *e, const EnemyView *pv)
{
    e->state = EN_COMBAT;
    e->state_t = 0.f;
    e->lkp = pv->eye;
    e->lkp_age = 0.f;
    e->burst_left = A_BURST[e->arch];
    e->fire_cd = 0.25f;                    /* reaction delay — player gets a beat */
}

/* §82: alert squad-mates within 8 m of a newly-combat enemy. */
static void team_share(EnemySet *s, Enemy *src, const EnemyView *pv)
{
    for (int i = 0; i < s->count; i++) {
        Enemy *o = &s->v[i];
        if (o == src || o->state == EN_DEAD || o->state == EN_COMBAT) continue;
        if (v3_dist(o->pos, src->pos) > 8.f) continue;
        o->meter = 0.9f;
        o->lkp = pv->eye;
        o->lkp_age = 0.f;
        if (o->state == EN_PATROL || o->state == EN_SUSPICIOUS) {
            o->state = EN_SEARCH;
            o->state_t = 0.f;
        }
    }
}

void enemies_update(EnemySet *s, const EnemyView *pv, float dt,
                    const Terrain *t, const ObstacleSet *ob)
{
    if (!s || !pv || dt <= 0.f) return;
    if (dt > 0.1f) dt = 0.1f;              /* clamp: no giant AI leaps on hitches */
    int alive = 0;

    for (int i = 0; i < s->count; i++) {
        Enemy *e = &s->v[i];
        e->shot_this_frame = 0;
        if (e->state == EN_DEAD) {
            if (e->hit_t > 0.f) e->hit_t -= dt;
            continue;
        }
        alive++;

        e->state_t += dt;
        e->lkp_age += dt;
        if (e->fire_cd > 0.f)  e->fire_cd -= dt;
        if (e->noise_t > 0.f)  e->noise_t -= dt;
        if (e->hit_t > 0.f)    e->hit_t -= dt;
        if (e->burst_pause > 0.f) e->burst_pause -= dt;

        float dist = v3_dist(e->pos, pv->eye);
        int los = pv->alive && los_clear(e, pv, t, ob);

        /* gunshots + sprint noise alert through walls inside 40 m (§82) */
        if (pv->alive && pv->noise > 0.5f && dist < 40.f && e->state < EN_COMBAT) {
            e->meter += 0.5f;
            e->lkp = pv->eye;
            e->lkp_age = 0.f;
            if (e->state == EN_PATROL) { e->state = EN_SEARCH; e->state_t = 0.f; }
        }
        /* ally gunfire carries: enemies near a fighting squad-mate wake up */
        if (e->noise_t > 0.f && e->state < EN_COMBAT) {
            e->meter += 0.6f * dt;
            if (e->state == EN_PATROL) { e->state = EN_SEARCH; e->state_t = 0.f; }
        }

        /* detection meter */
        if (los) {
            float rate = enemy_detect_rate(e, pv);
            if (e->state == EN_SUSPICIOUS) rate *= 1.5f;
            e->meter += rate * dt;
        } else {
            e->meter -= 0.2f * dt;          /* §82 decay */
        }
        if (!(e->meter >= 0.f)) e->meter = 0.f;   /* NaN clamp (18.4) */
        if (e->meter > 1.2f) e->meter = 1.2f;

        /* ── FSM ── */
        switch (e->state) {
        case EN_PATROL: {
            Vec3 tgt = e->patrol_leg ? e->patrol_b : e->patrol_a;
            Vec3 d = v3_sub(tgt, e->pos); d.y = 0.f;
            if (v3_len(d) < 0.8f) e->patrol_leg ^= 1;
            face_toward(e, tgt, dt);
            move_enemy(e, d, e->speed * 0.55f, dt, t, ob);
            if (e->meter >= 0.35f) { e->state = EN_SUSPICIOUS; e->state_t = 0.f; }
            break;
        }
        case EN_SUSPICIOUS: {
            face_toward(e, pv->eye, dt);
            move_enemy(e, v3(0.f, 0.f, 0.f), 0.f, dt, t, ob);
            if (e->meter >= 0.8f) { enter_combat(e, pv); team_share(s, e, pv); }
            else if (e->meter < 0.35f && e->state_t > 3.f) {
                e->state = EN_PATROL; e->state_t = 0.f;
            }
            break;
        }
        case EN_SEARCH: {
            Vec3 d = v3_sub(e->lkp, e->pos); d.y = 0.f;
            if (v3_len(d) < 1.5f) {
                /* arrived at LKP: sweep-look around (deterministic wobble) */
                float sweep = sinf(e->state_t * 2.2f) * 1.2f;
                Vec3 ahead = v3_add(e->pos, v3(sinf(e->yaw + sweep) * 5.f, 0.f,
                                               cosf(e->yaw + sweep) * 5.f));
                face_toward(e, ahead, dt);
                move_enemy(e, v3(0.f, 0.f, 0.f), 0.f, dt, t, ob);
            } else {
                face_toward(e, e->lkp, dt);
                move_enemy(e, d, e->speed * 1.1f, dt, t, ob);
            }
            if (los && e->meter >= 0.8f) { enter_combat(e, pv); team_share(s, e, pv); }
            else if (e->lkp_age > 6.f && e->meter < 0.35f) {
                e->state = EN_PATROL; e->state_t = 0.f;
            }
            break;
        }
        case EN_COMBAT: {
            if (los) { e->lkp = pv->eye; e->lkp_age = 0.f; e->meter = 1.2f; }
            if (!pv->alive) { e->state = EN_SEARCH; e->state_t = 0.f; break; }
            if (!los || dist > 75.f) {
                e->state = EN_SEARCH; e->state_t = 0.f;
                break;
            }
            face_toward(e, pv->eye, dt);
            /* range-keeping: hold preferred distance, strafe otherwise */
            Vec3 to_p = v3_sub(pv->eye, e->pos); to_p.y = 0.f;
            Vec3 fwd = v3_norm(to_p);
            Vec3 right = v3(fwd.z, 0.f, -fwd.x);
            Vec3 wish;
            float pref = A_PREF[e->arch];
            if (dist > pref + 4.f)      wish = fwd;
            else if (dist < pref - 4.f) wish = v3_neg(fwd);
            else {
                float ph = sinf(e->state_t * 1.3f + (float)i * 2.1f);
                wish = v3_mul(right, ph);
                if (dist > pref + 1.f) wish = v3_add(wish, v3_mul(fwd, 0.4f));
            }
            move_enemy(e, wish, e->speed * 1.25f, dt, t, ob);
            /* fire control: bursts with pauses; each shot flags the game layer */
            if (los && e->burst_pause <= 0.f && e->fire_cd <= 0.f) {
                if (e->burst_left <= 0) {
                    e->burst_left = A_BURST[e->arch];
                    e->burst_pause = 0.f;
                }
                e->shot_this_frame = 1;
                e->shot_acc = enemy_accuracy(e, pv, dist);
                e->burst_left--;
                e->fire_cd = A_FINT[e->arch];
                if (e->burst_left <= 0)
                    e->burst_pause = 1.1f + 0.7f * sinf(e->state_t * 0.9f + (float)i);
                /* gunfire is team-wide noise */
                for (int j = 0; j < s->count; j++)
                    if (j != i && s->v[j].state != EN_DEAD &&
                        v3_dist(s->v[j].pos, e->pos) < 25.f)
                        s->v[j].noise_t = 2.0f;
            }
            if (e->health < e->health_max * 0.25f && e->faction != 0) {
                e->state = EN_FLEE; e->state_t = 0.f;   /* cartel breaks; cult doesn't */
            }
            break;
        }
        case EN_FLEE: {
            Vec3 away = v3_sub(e->pos, e->lkp); away.y = 0.f;
            if (v3_len(away) < 0.1f) away = v3(1.f, 0.f, 0.f);
            face_toward(e, v3_add(e->pos, away), dt);
            move_enemy(e, away, e->speed * 1.5f, dt, t, ob);
            if (e->lkp_age > 10.f) { e->state = EN_SEARCH; e->state_t = 0.f; }
            break;
        }
        default:
            e->state = EN_PATROL;
            break;
        }
    }
    s->alive_count = alive;
}
