/* ============================================================================
   DIVIDED HORIZON — Horizon Engine
   src/city/heat.h — wanted-level system (M4, Spec 16 / GTA-DNA slice).
   Evidence → stars 0..5; response ladder; escape = no cop LOS + >100 m for
   20 s per star. Deliberately small and unit-testable.
   ========================================================================== */
#ifndef DH_CITY_HEAT_H
#define DH_CITY_HEAT_H

#define HEAT_ESCAPE_S 20.0f   /* seconds clean per star lost */
#define HEAT_CLEAN_DIST 100.0f

typedef struct {
    float evidence;      /* accumulated reports */
    int   stars;         /* 0..5 */
    float escape_t;      /* clean streak progress toward the next star drop */
    int   searching;     /* escape clock running (HUD cone/bar) */
    int   just_dropped;  /* set on the frame a star was lost */
    int   just_raised;   /* set on the frame stars increased */
} Heat;

void heat_init(Heat *h);
/* Add evidence from a crime; recomputes stars from thresholds {1,3,6,10,15}. */
void heat_add_evidence(Heat *h, float e);
/* Advance escape logic. dist_cop = distance to nearest pursuing cruiser,
   los = that cruiser currently has line of sight to the player. */
void heat_tick(Heat *h, float dt, float dist_cop, int los);
/* Response ladder: cruisers fielded per star level (1★→2 … 5★→6). */
int  heat_response_cruisers(const Heat *h);
int  heat_has_heli(const Heat *h);   /* ≥4★: spotlight chopper */
const char *heat_star_name(int stars);

#endif
