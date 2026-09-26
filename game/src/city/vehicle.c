#include "../city/vehicle.h"
#include "../core/dh_json.h"
#include "../core/dh_log.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static VehClass cls_from_str(const char *s) {
    if (!strcmp(s, "muscle")) return VC_MUSCLE;
    if (!strcmp(s, "taxi")) return VC_TAXI;
    if (!strcmp(s, "police")) return VC_POLICE;
    if (!strcmp(s, "motorcycle")) return VC_MOTO;
    return VC_COMPACT;
}

/* Embedded fallback (identical to game/data/vehicles.json) — Honesty Contract:
   the game must run even if the data file is missing. */
static void vehicle_builtin(VehicleDef defs[VEH_DEFS_MAX], int *count) {
    static const struct { const char *id, *name, *cls; float top, t100, grip, hp; int seats, price, heat, radio; } B[] = {
        {"V01", "Coralina",   "compact",    145, 8.0f, 0.85f, 450, 4, 3500,  1, 1},
        {"V02", "Tiburon GT", "muscle",     195, 5.2f, 0.70f, 550, 2, 18000, 1, 1},
        {"V05", "Guagua",     "taxi",       135, 9.0f, 0.82f, 500, 4, 4000,  1, 1},
        {"V06", "Sirena",     "police",     180, 6.0f, 0.85f, 800, 4, 0,     2, 1},
        {"V07", "Fenix",      "motorcycle", 205, 4.2f, 0.90f, 300, 2, 9500,  1, 0},
    };
    *count = 0;
    for (size_t i = 0; i < sizeof B / sizeof B[0]; i++) {
        VehicleDef *d = &defs[*count];
        memset(d, 0, sizeof *d);
        snprintf(d->id, sizeof d->id, "%s", B[i].id);
        snprintf(d->name, sizeof d->name, "%s", B[i].name);
        d->cls = cls_from_str(B[i].cls);
        d->top_ms = B[i].top / 3.6f;
        d->grip = B[i].grip;
        d->hp = B[i].hp;
        d->seats = B[i].seats;
        d->price = B[i].price;
        d->steal_heat = B[i].heat;
        d->radio = B[i].radio;
        d->accel_ms2 = (27.8f / B[i].t100) * 1.35f;
        switch (d->cls) {
            case VC_MOTO:   d->width = 0.7f; d->length = 2.0f; d->height = 1.2f; break;
            case VC_MUSCLE: d->width = 1.9f; d->length = 4.7f; d->height = 1.35f; break;
            default:        d->width = 1.8f; d->length = 4.2f; d->height = 1.5f; break;
        }
        (*count)++;
    }
}

static void veh_body(VehicleDef *d) {
    switch (d->cls) {
        case VC_MOTO:   d->width = 0.7f; d->length = 2.0f; d->height = 1.2f; break;
        case VC_MUSCLE: d->width = 1.9f; d->length = 4.7f; d->height = 1.35f; break;
        default:        d->width = 1.8f; d->length = 4.2f; d->height = 1.5f; break;
    }
}

int vehicle_defs_load(VehicleDef defs[VEH_DEFS_MAX], const char *json_path) {
    char pathbuf[512];
    const char *path = json_path;
    if (!path || !*path) {
        snprintf(pathbuf, sizeof pathbuf, "%s/vehicles.json", dh_fs_data_dir());
        path = pathbuf;
        if (!dh_fs_exists(path)) path = "game/data/vehicles.json";
    }
    size_t len = 0;
    char *text = dh_fs_read_text(path, &len);
    if (!text) {
        DH_WARN("vehicle", "cannot read %s - using embedded fallback table", path);
        int n; vehicle_builtin(defs, &n); return n;
    }
    const char *err = NULL;
    JsonValue *root = json_parse(text, &err);
    free(text);
    if (!root) {
        DH_WARN("vehicle", "vehicles.json parse error (%s) - embedded fallback", err ? err : "?");
        int n; vehicle_builtin(defs, &n); return n;
    }
    JsonValue *arr = json_obj_get(root, "vehicles");
    int n = arr ? json_arr_len(arr) : 0;
    int count = 0;
    for (int i = 0; i < n && count < VEH_DEFS_MAX; i++) {
        JsonValue *o = json_arr_get(arr, i);
        if (!o) continue;
        VehicleDef *d = &defs[count];
        memset(d, 0, sizeof *d);
        snprintf(d->id, sizeof d->id, "%s", json_get_str(o, "id", ""));
        snprintf(d->name, sizeof d->name, "%s", json_get_str(o, "name", ""));
        d->cls = cls_from_str(json_get_str(o, "cls", ""));
        d->top_ms = json_get_flt(o, "top_kmh", 0) / 3.6f;
        float t100 = json_get_flt(o, "accel_0_100_s", 0);
        d->grip = json_get_flt(o, "grip", 0);
        d->hp = json_get_flt(o, "hp", 0);
        d->seats = json_get_int(o, "seats", 4);
        d->price = json_get_int(o, "price", 0);
        d->steal_heat = json_get_int(o, "steal_heat", 1);
        d->radio = json_get_int(o, "radio", 1);
        if (d->top_ms <= 1.0f || t100 <= 0.1f || d->grip <= 0.0f || d->hp <= 0.0f) {
            DH_WARN("vehicle", "%s failed validation - row skipped", d->id);
            continue;
        }
        d->accel_ms2 = (27.8f / t100) * 1.35f;
        veh_body(d);
        count++;
    }
    json_free(root);
    if (count == 0) {
        DH_WARN("vehicle", "no valid rows in vehicles.json - embedded fallback");
        vehicle_builtin(defs, &count);
    }
    DH_INFO("vehicle", "loaded %d vehicle defs", count);
    return count;
}

void vehicle_derive(const VehicleDef *d, VehHandling *h) {
    h->accel_ms2 = d->accel_ms2;
    h->drag_k = h->accel_ms2 / (d->top_ms * d->top_ms);
    h->brake_ms2 = 11.0f;
    h->reverse_ms2 = 4.0f;
    h->reverse_top = 9.0f;
    h->steer_rate = 2.4f * d->grip;
}

void vehicle_step(Vehicle *v, const VehicleDef *defs, float dt,
                  float throttle, float brake, float steer, int handbrake) {
    if (v->state == VS_WRECK || dt <= 0) return;
    const VehicleDef *d = &defs[v->def];
    VehHandling h;
    vehicle_derive(d, &h);

    /* wreck limp: top speed and accel fall off with damage */
    float dmg_frac = dh_clampf(v->damage / d->hp, 0.0f, 1.0f);
    float top = d->top_ms * (1.0f - 0.55f * dmg_frac);
    float acc = h.accel_ms2 * (1.0f - 0.45f * dmg_frac);

    /* steering: needs motion, fades at top speed, boosted while slipping */
    float sp = fabsf(v->speed);
    float steer_eff = steer * h.steer_rate
                    * dh_clampf(sp / 5.0f, 0.0f, 1.0f)
                    * (1.0f - 0.55f * dh_clampf(sp / d->top_ms, 0.0f, 1.0f));
    if (handbrake && sp > 3.0f) steer_eff *= 1.45f;
    if (v->speed < -0.2f) steer_eff = -steer_eff; /* reverse steers inverted */
    v->yaw += steer_eff * dt;
    v->yaw += v->slip * steer * 0.9f * dt;        /* drift-lite yaw from slip */

    /* longitudinal */
    if (throttle > 0.0f) v->speed += acc * throttle * dt;
    if (brake > 0.0f) {
        if (v->speed > 0.4f) v->speed -= h.brake_ms2 * brake * dt;
        else if (v->speed > -h.reverse_top) v->speed -= h.reverse_ms2 * brake * dt;
        if (v->speed < 0 && v->speed > -0.4f && brake < 0.5f) v->speed = 0;
    }
    if (handbrake && sp > 0.1f) {
        v->speed -= 7.0f * dt * (v->speed > 0 ? 1 : -1);
        v->slip = dh_clampf(v->slip + 2.2f * dt, 0.0f, 1.0f);
    }
    /* quadratic drag */
    v->speed -= h.drag_k * v->speed * fabsf(v->speed) * dt;
    v->speed = dh_clampf(v->speed, -h.reverse_top, top);
    if (throttle == 0 && brake == 0 && fabsf(v->speed) < 0.15f) v->speed = 0;

    if (!handbrake) v->slip = dh_maxf(0.0f, v->slip - 2.5f * dt);

    /* integrate — engine yaw convention: forward = (sin yaw, cos yaw) */
    v->pos.x += sinf(v->yaw) * v->speed * dt;
    v->pos.z += cosf(v->yaw) * v->speed * dt;
}

void vehicle_impact(Vehicle *v, float impact_speed) {
    if (impact_speed < 2.0f || v->state == VS_WRECK) return;
    v->damage += impact_speed * 6.0f;
    v->speed *= -0.12f;                       /* small bounce-back */
    v->slip = dh_clampf(v->slip + impact_speed / 22.0f, 0.0f, 1.0f);
}

int vehicle_wrecked(const Vehicle *v, const VehicleDef *defs) {
    return v->state == VS_WRECK || v->damage >= defs[v->def].hp;
}
