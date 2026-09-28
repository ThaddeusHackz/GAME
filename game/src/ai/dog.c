/* Poncho companion sim — see dog.h */
#include "dog.h"
#include <math.h>
#include <string.h>

static float gy(const DogWorld *w, float x, float z) { return w->ground ? w->ground(w->ud, x, z) : 0.f; }

void dog_init(Dog *d, Vec3 pos, int trapped) {
    memset(d, 0, sizeof *d);
    d->pos = pos; d->hp = DOG_HP_MAX;
    d->state = trapped ? DOG_TRAPPED : DOG_FOLLOW;
    d->cmd = DOG_CMD_FOLLOW; d->stay_pos = pos;
}
int dog_active(const Dog *d) { return d->state != DOG_TRAPPED && d->state != DOG_KO; }
static void set_state(Dog *d, int st) { d->state = st; d->state_t = 0.f; }
static int cmd_state(int cmd) { return cmd == DOG_CMD_STAY ? DOG_STAY : cmd == DOG_CMD_HUNT ? DOG_HUNT : DOG_FOLLOW; }

void dog_free(Dog *d) { if (d->state == DOG_TRAPPED) { set_state(d, DOG_FOLLOW); d->cmd = DOG_CMD_FOLLOW; d->bark_cd = 0.f; } }
void dog_command(Dog *d, int cmd) {
    if (cmd < 0 || cmd >= DOG_CMD_N) return;
    d->cmd = cmd;
    if (!dog_active(d)) return;
    if (cmd == DOG_CMD_STAY) d->stay_pos = d->pos;
    set_state(d, cmd_state(cmd));
}
int dog_cycle_command(Dog *d) { dog_command(d, (d->cmd + 1) % DOG_CMD_N); return d->cmd; }
int dog_distract(Dog *d, Vec3 t) {
    if (!dog_active(d)) return 0;
    d->goal = t; set_state(d, DOG_DISTRACT); d->distracts++;
    return 1;
}
int dog_damage(Dog *d, float amt) {
    if (!dog_active(d) || amt <= 0.f) return 0;
    d->hp -= amt;
    if (d->hp > 0.f) return 0;
    d->hp = 0.f; d->revive_t = 0.f; d->vel = v3(0, 0, 0);
    set_state(d, DOG_KO); d->knockouts++;
    return 1;
}
int dog_bond_level(const Dog *d) { int l = 1 + d->bond_xp / 50; return l > 5 ? 5 : l; }
float dog_sniff_radius(const Dog *d) { return 20.f + 2.f * (float)(dog_bond_level(d) - 1); }
int dog_pet(Dog *d) {
    if (!dog_active(d) || d->pet_cd > 0.f) return 0;
    d->pet_cd = 20.f; d->bond_xp += 10; d->pets++;
    return 1;
}

static void snap_behind(Dog *d, const DogWorld *w) {
    float s = sinf(w->player_yaw), c = cosf(w->player_yaw);
    d->pos.x = w->player.x - s * 2.f + c * 1.f;
    d->pos.z = w->player.z - c * 2.f - s * 1.f;
    d->pos.y = gy(w, d->pos.x, d->pos.z);
    d->vel = v3(0, 0, 0); d->lost_t = 0.f; d->teleports++;
}

/* steer toward goal; returns remaining distance */
static float steer(Dog *d, const DogWorld *w, Vec3 goal, float arrive, float dt, float *speed_out) {
    float dx = goal.x - d->pos.x, dz = goal.z - d->pos.z;
    float dist = sqrtf(dx * dx + dz * dz);
    float want = 0.f;
    if (dist > arrive) want = dist > 7.f ? 9.5f : (dist > 2.5f ? 6.5f : 3.f);
    float vx = 0.f, vz = 0.f;
    if (want > 0.f) { vx = dx / dist * want; vz = dz / dist * want; }
    float k = dh_clampf(8.f * dt, 0.f, 1.f);
    d->vel.x += (vx - d->vel.x) * k; d->vel.z += (vz - d->vel.z) * k;
    d->pos.x += d->vel.x * dt; d->pos.z += d->vel.z * dt;
    float ny = gy(w, d->pos.x, d->pos.z);
    d->pos.y += (ny - d->pos.y) * dh_clampf(15.f * dt, 0.f, 1.f);
    float sp = sqrtf(d->vel.x * d->vel.x + d->vel.z * d->vel.z);
    if (sp > 0.3f) {
        float ty = atan2f(d->vel.x, d->vel.z), dy = ty - d->yaw;
        while (dy > 3.14159f) dy -= 6.28318f;
        while (dy < -3.14159f) dy += 6.28318f;
        d->yaw += dy * dh_clampf(10.f * dt, 0.f, 1.f);
    }
    *speed_out = sp;
    return dist;
}

int dog_update(Dog *d, const DogWorld *w, float dt) {
    int ev = 0;
    if (dt <= 0.f) return 0;
    d->state_t += dt;
    if (d->bark_cd > 0.f) d->bark_cd -= dt;
    if (d->pet_cd > 0.f) d->pet_cd -= dt;
    float pdist = v3_dist_xz(d->pos, w->player);

    if (d->state == DOG_TRAPPED) {          /* whimpers/barks for help */
        if (d->bark_cd <= 0.f && pdist < 40.f) { d->bark_cd = 3.5f; d->barks++; ev |= DOG_EV_BARK; }
        return ev;
    }
    if (d->state == DOG_KO) {
        if (w->reviving && pdist < 2.6f) d->revive_t += dt;
        else d->revive_t = dh_maxf(0.f, d->revive_t - dt * 0.5f);
        if (d->revive_t >= DOG_REVIVE_HOLD || d->state_t >= DOG_AUTO_RECOVER) {
            d->hp = DOG_HP_MAX * 0.6f; d->revives++;
            set_state(d, cmd_state(d->cmd)); d->stay_pos = d->pos;
            ev |= DOG_EV_REVIVED;
        }
        return ev;
    }
    /* slow regen while active */
    d->hp = dh_minf(DOG_HP_MAX, d->hp + 2.f * dt);

    /* safety net: act swap / fast travel teleports instantly; a merely lost
       dog (stuck behind geometry, left behind at a zipline) is snapped back
       after DOG_LOST_TELEPORT seconds. STAY honours the order up to 120 m. */
    int staying = (d->state == DOG_STAY);
    if (pdist > DOG_FAR_SNAP) {
        snap_behind(d, w); ev |= DOG_EV_TELEPORT;
        if (staying) set_state(d, DOG_FOLLOW), d->cmd = DOG_CMD_FOLLOW;
        return ev;
    }
    if (!staying && d->state != DOG_DISTRACT && pdist > DOG_LOST_DIST) {
        d->lost_t += dt;
        if (d->lost_t >= DOG_LOST_TELEPORT) { snap_behind(d, w); return ev | DOG_EV_TELEPORT; }
    } else d->lost_t = 0.f;

    float s = sinf(w->player_yaw), c = cosf(w->player_yaw), sp = 0.f;
    Vec3 goal;
    switch (d->state) {
    case DOG_STAY: goal = d->stay_pos; steer(d, w, goal, 0.5f, dt, &sp); break;
    case DOG_HUNT:                              /* ranges ahead, nose down */
        goal = v3(w->player.x + s * 6.f + sinf(d->state_t * 0.7f) * 3.f, 0,
                  w->player.z + c * 6.f + cosf(d->state_t * 0.9f) * 3.f);
        steer(d, w, goal, 1.0f, dt, &sp); break;
    case DOG_DISTRACT: {
        float r = steer(d, w, d->goal, 1.2f, dt, &sp);
        if (r < 1.5f) {
            if (d->state_t < 100.f) { ev |= DOG_EV_ARRIVED; d->state_t = 100.f; }
            if (d->bark_cd <= 0.f) { d->bark_cd = 0.9f; d->barks++; ev |= DOG_EV_BARK; }
            if (d->state_t > 105.f) set_state(d, cmd_state(d->cmd));
        } else if (d->state_t > 20.f && d->state_t < 100.f) set_state(d, cmd_state(d->cmd)); /* unreachable */
        break; }
    default:                                    /* heel: behind-right of player */
        goal = v3(w->player.x - s * 2.2f + c * 1.2f, 0, w->player.z - c * 2.2f - s * 1.2f);
        steer(d, w, goal, 0.7f, dt, &sp); break;
    }
    d->gait += sp * dt * 2.2f;
    d->step_acc += sp * dt;
    if (d->step_acc > 1.3f) { d->step_acc = 0.f; if (sp > 4.f) ev |= DOG_EV_JINGLE; }
    return ev;
}

const char *dog_state_name(int st) {
    static const char *n[] = { "TRAPPED", "FOLLOW", "STAY", "HUNT", "DISTRACT", "DOWN" };
    return (st >= 0 && st <= DOG_KO) ? n[st] : "?";
}
const char *dog_cmd_name(int c) {
    static const char *n[] = { "HEEL", "STAY", "HUNT" };
    return (c >= 0 && c < DOG_CMD_N) ? n[c] : "?";
}
