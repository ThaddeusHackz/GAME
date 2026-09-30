/* ============================================================================
   DIVIDED HORIZON — Horizon Engine
   src/city/vehicle.h — arcade vehicle handling (M4).
   Spec 69 anchors are law: top speed / 0-100 / grip / HP are data-driven from
   vehicles.json; handling is DERIVED (accel = top / (0-100 scaled)).
   ========================================================================== */
#ifndef DH_CITY_VEHICLE_H
#define DH_CITY_VEHICLE_H

#include "../core/dh_types.h"
#include <stdint.h>

#define VEH_DEFS_MAX 12
#define VEHICLES_MAX 48

typedef enum { VC_COMPACT, VC_MUSCLE, VC_TAXI, VC_POLICE, VC_MOTO,
               VC_VAN, VC_PICKUP, VC_ARMORED, VC_BUGGY, VC_COUNT  /* M22 */ } VehClass;

typedef struct {
    char id[8];
    char name[16];
    VehClass cls;
    float top_ms;         /* top_kmh / 3.6 — LAW */
    float accel_ms2;      /* derived: top / (0-100_s * 0.62) so 0-100 lands near the table */
    float grip;           /* 0..1 — LAW; steers authority + handbrake slip */
    float hp;             /* damage capacity — LAW */
    int   seats;
    int   price;
    int   steal_heat;     /* evidence added when THIS type is stolen (V06 = 2) */
    int   radio;          /* has radio stub (motorcycles: 0) */
    float width, length, height; /* body box */
} VehicleDef;

typedef enum { VS_PARKED, VS_TRAFFIC, VS_DRIVEN, VS_CHASE, VS_WRECK } VehState;

typedef struct {
    int   def;            /* index into defs */
    Vec3  pos;
    float yaw;
    float speed;          /* m/s along heading, negative = reverse */
    float slip;           /* 0..1 lateral drift (handbrake/collision) */
    float damage;         /* 0..hp; hp -> wreck */
    VehState state;
    int   occupant;       /* traffic driver ped index, or -1 empty; player drives via g->in_vehicle */
    int   loop;           /* traffic spline loop id, -1 for parked */
    float loop_s;         /* arc position on loop */
    float panic_t;        /* scatter timer */
    int   radio_station;
    float ai_throttle, ai_steer; /* traffic inputs */
} Vehicle;

/* Load defs from vehicles.json. Returns count (>0). */
int vehicle_defs_load(VehicleDef defs[VEH_DEFS_MAX], const char *path);

/* Derived handling constants for a def (cached on load, exposed for tests). */
typedef struct {
    float accel_ms2;      /* full-throttle forward accel at low speed */
    float brake_ms2;      /* service brake */
    float reverse_ms2;    /* reverse top accel */
    float steer_rate;     /* rad/s at reference speed */
    float drag_k;         /* quadratic drag so top speed is the LAW value */
    float reverse_top;    /* reverse speed cap (m/s) */
} VehHandling;

void vehicle_derive(const VehicleDef *d, VehHandling *h);

/* Arcade step. throttle/brake in 0..1, steer in -1..1, handbrake 0/1. */
void vehicle_step(Vehicle *v, const VehicleDef *defs, float dt,
                  float throttle, float brake, float steer, int handbrake);

/* Collision impact: adds damage, bleeds speed, injects slip. */
void vehicle_impact(Vehicle *v, float impact_speed);

int vehicle_wrecked(const Vehicle *v, const VehicleDef *defs);

#endif
