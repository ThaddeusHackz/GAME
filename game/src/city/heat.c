/* ============================================================================
   DIVIDED HORIZON — Horizon Engine
   src/city/heat.c — wanted level. Thresholds mirror the genre standard:
   1★ petty (1 evidence), 2★ (3), 3★ (6), 4★ (10), 5★ (15). Stars only fall
   via the escape clock — evidence never decays on its own, so hiding mid-chase
   doesn't silently forgive you.
   ========================================================================== */
#include "../city/heat.h"
#include "../core/dh_types.h"
#include <string.h>

static const float STAR_TH[6] = { 0.0f, 1.0f, 3.0f, 6.0f, 10.0f, 15.0f };

void heat_init(Heat *h) { memset(h, 0, sizeof *h); }

static int stars_for(float evidence) {
    int s = 0;
    for (int i = 5; i >= 1; i--) {
        if (evidence >= STAR_TH[i]) { s = i; break; }
    }
    return s;
}

void heat_add_evidence(Heat *h, float e) {
    if (e <= 0.0f) return;
    h->evidence += e;
    int s = stars_for(h->evidence);
    h->just_raised = (s > h->stars);
    h->stars = s;
    h->escape_t = 0.0f;          /* a fresh crime resets the clean streak */
}

void heat_tick(Heat *h, float dt, float dist_cop, int los) {
    h->just_dropped = 0;
    if (h->just_raised) h->just_raised = 0; /* consumed by city HUD next read */
    if (h->stars <= 0) { h->searching = 0; h->escape_t = 0.0f; return; }

    int clean = (!los) && (dist_cop > HEAT_CLEAN_DIST);
    h->searching = clean;
    if (clean) {
        h->escape_t += dt;
        if (h->escape_t >= HEAT_ESCAPE_S) {
            h->stars--;
            h->escape_t = 0.0f;
            h->just_dropped = 1;
            if (h->stars == 0) h->searching = 0;
            /* snap evidence to the new star's threshold so a single lingering
               witness report can't instantly re-escalate */
            h->evidence = (h->stars > 0) ? STAR_TH[h->stars] : 0.0f;
        }
    } else {
        h->escape_t = dh_maxf(0.0f, h->escape_t - 3.0f * dt);
    }
}

int heat_response_cruisers(const Heat *h) {
    if (h->stars <= 0) return 0;
    return h->stars >= 5 ? 6 : h->stars + 1;
}

int heat_has_heli(const Heat *h) { return h->stars >= 4; }

const char *heat_star_name(int stars) {
    switch (stars) {
        case 0: return "CLEAN";
        case 1: return "PETTY";
        case 2: return "WANTED";
        case 3: return "CHASE";
        case 4: return "AIR SUPPORT";
        case 5: return "LOCKDOWN";
        default: return "?";
    }
}
