/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — core types & math declarations
   Horizon Engine (original work). No third-party runtime dependencies.
   Units: meters, seconds, right-handed, +Y up. Fixed tick = 1/60 s (Spec 10.6)
   ══════════════════════════════════════════════════════════════════════════ */
#ifndef DH_TYPES_H
#define DH_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <math.h>

#define DH_VERSION_MAJOR 0
#define DH_VERSION_MINOR 2
#define DH_VERSION_PATCH 0
#define DH_BUILD_NAME    "M2 Combat"

#define DH_STR2(x) #x
#define DH_STR(x)  DH_STR2(x)
#define DH_VERSION_STRING \
    DH_STR(DH_VERSION_MAJOR) "." DH_STR(DH_VERSION_MINOR) "." DH_STR(DH_VERSION_PATCH)

#define DH_TICK_HZ       60.0f
#define DH_TICK_DT       (1.0f / DH_TICK_HZ)

/* ── scalar helpers ─────────────────────────────────────────────────────── */
#define DH_PI  3.14159265358979323846f
#define DH_TAU 6.28318530717958647692f
#define DH_DEG2RAD (DH_PI / 180.0f)
#define DH_RAD2DEG (180.0f / DH_PI)

static inline float dh_clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline int   dh_clampi(int v, int lo, int hi)       { return v < lo ? lo : (v > hi ? hi : v); }
static inline float dh_lerp(float a, float b, float t)     { return a + (b - a) * t; }
static inline float dh_minf(float a, float b)              { return a < b ? a : b; }
static inline float dh_maxf(float a, float b)              { return a > b ? a : b; }
static inline float dh_absf(float a)                       { return a < 0 ? -a : a; }
/* frame-rate independent exponential smoothing — WHY: a naive lerp(a,b,0.1)
   changes speed with frame rate; this keeps feel identical at 30/60/144 Hz. */
static inline float dh_damp(float a, float b, float lambda, float dt) {
    return dh_lerp(a, b, 1.0f - expf(-lambda * dt));
}
static inline float dh_sign(float v) { return v < 0.0f ? -1.0f : (v > 0.0f ? 1.0f : 0.0f); }
static inline int   dh_isfinite_f(float v) { return isfinite(v) ? 1 : 0; }

/* ── vectors ────────────────────────────────────────────────────────────── */
typedef struct { float x, y, z; }       Vec3;
typedef struct { float x, y, z, w; }    Vec4;
typedef struct { float x, y; }          Vec2;
typedef struct { uint8_t r, g, b, a; }  RGBA8;
typedef struct { float r, g, b, a; }    RGBAf;
typedef struct { float m[16]; }         Mat4;   /* column-major, GL-compatible */

static inline Vec3 v3(float x, float y, float z) { Vec3 r = {x, y, z}; return r; }
static inline Vec2 v2(float x, float y)          { Vec2 r = {x, y};    return r; }

Vec3  v3_add(Vec3 a, Vec3 b);
Vec3  v3_sub(Vec3 a, Vec3 b);
Vec3  v3_mul(Vec3 a, float s);
Vec3  v3_mulv(Vec3 a, Vec3 b);
Vec3  v3_div(Vec3 a, float s);
float v3_dot(Vec3 a, Vec3 b);
Vec3  v3_cross(Vec3 a, Vec3 b);
float v3_len(Vec3 a);
float v3_len2(Vec3 a);
float v3_dist(Vec3 a, Vec3 b);
float v3_dist_xz(Vec3 a, Vec3 b);
Vec3  v3_norm(Vec3 a);
Vec3  v3_lerp(Vec3 a, Vec3 b, float t);
Vec3  v3_min(Vec3 a, Vec3 b);
Vec3  v3_max(Vec3 a, Vec3 b);
Vec3  v3_neg(Vec3 a);
int   v3_valid(Vec3 a);                       /* NaN guard (Spec 18.4: no NaN) */
Vec3  v3_safe(Vec3 a, Vec3 fallback);

/* ── matrices ───────────────────────────────────────────────────────────── */
Mat4  m4_identity(void);
Mat4  m4_mul(Mat4 a, Mat4 b);
Mat4  m4_translate(Vec3 t);
Mat4  m4_scale(Vec3 s);
Mat4  m4_scale1(float s);
Mat4  m4_rot_x(float rad);
Mat4  m4_rot_y(float rad);
Mat4  m4_rot_z(float rad);
Mat4  m4_look_at(Vec3 eye, Vec3 target, Vec3 up);
Mat4  m4_perspective(float fov_deg, float aspect, float zn, float zf);
Mat4  m4_ortho(float l, float r, float b, float t, float n, float f);
Mat4  m4_trs(Vec3 pos, float yaw, float pitch, float roll, Vec3 scale);
Vec3  m4_xform_p(Mat4 m, Vec3 p);             /* point  (w divide) */
Vec3  m4_xform_d(Mat4 m, Vec3 d);             /* direction (no translation) */
Mat4  m4_invert(Mat4 m);                      /* rigid transform inverse */
Vec3  m4_get_pos(Mat4 m);

/* ── colour ─────────────────────────────────────────────────────────────── */
static inline RGBA8 rgba8(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    RGBA8 c = {r, g, b, a}; return c;
}
static inline RGBAf rgbaf(float r, float g, float b, float a) {
    RGBAf c = {r, g, b, a}; return c;
}
RGBAf  rgbaf_mix(RGBAf a, RGBAf b, float t);
RGBAf  rgbaf_mul(RGBAf c, float s);
RGBAf  rgbaf_from8(RGBA8 c);
RGBA8  rgbaf_to8(RGBAf c);
/* Colorblind-safe remap (Spec 33): mode 0=off 1=deuter 2=prot 3=trit */
RGBAf  rgbaf_colorblind(RGBAf c, int mode);

/* ── deterministic RNG (mulberry32 — Spec 82 contract seed law) ─────────── */
typedef struct { uint32_t s; } Rng;
static inline void rng_seed(Rng *r, uint32_t seed) { r->s = seed ? seed : 0x9E3779B9u; }
static inline uint32_t rng_u32(Rng *r) {
    uint32_t t = (r->s += 0x6D2B79F5u);
    t = (t ^ (t >> 15)) * (t | 1u);
    t ^= t + (t ^ (t >> 7)) * (t | 61u);
    return t ^ (t >> 14);
}
static inline float rng_f(Rng *r)            { return (float)(rng_u32(r) >> 8) * (1.0f / 16777216.0f); }
static inline float rng_range(Rng *r, float a, float b) { return a + (b - a) * rng_f(r); }
static inline int   rng_int(Rng *r, int n)   { return n > 0 ? (int)(rng_u32(r) % (uint32_t)n) : 0; }
uint32_t dh_date_seed(int year, int month, int day);   /* Contract-of-the-Day (Spec 30) */

/* ── value noise / fbm (FastNoiseLite-equivalent, original impl) ────────── */
typedef struct { uint32_t seed; int octaves; float freq, lacunarity, gain; } NoiseCfg;
float noise2(const NoiseCfg *n, float x, float y);
float noise_fbm2(const NoiseCfg *n, float x, float y);
float noise3(const NoiseCfg *n, float x, float y, float z);
float noise_ridge2(const NoiseCfg *n, float x, float y);

/* ── geometry helpers ───────────────────────────────────────────────────── */
typedef struct { Vec3 min, max; } AABB;
AABB  aabb_from_center(Vec3 c, Vec3 half);
int   aabb_overlaps(AABB a, AABB b);
Vec3  aabb_center(AABB b);

typedef struct { Vec3 n; float d; } Plane;
Plane plane_from_points(Vec3 a, Vec3 b, Vec3 c);
float plane_dist(Plane p, Vec3 pt);

typedef struct { Vec3 o, d; } Ray;
int   ray_sphere(Ray r, Vec3 c, float rad, float *t_out);
int   ray_aabb(Ray r, AABB b, float *t_out);
int   ray_plane_y(Ray r, float y, float *t_out, Vec3 *hit_out);
/* Swept capsule vs world-height sampling lives in world/terrain.c */

/* ── frustum (Spec 15.2 draw-distance budgets) ──────────────────────────── */
typedef struct { Plane p[6]; } Frustum;
Frustum frustum_from_vp(Mat4 vp);
int     frustum_has_sphere(Frustum f, Vec3 c, float r);
int     frustum_has_aabb(Frustum f, AABB b);

/* ── pooled containers (Spec 11.3: no busy-work nodes, pool everything) ─── */
#define DH_ARRAY_INIT_CAP 16
typedef struct {
    void *data; int count, cap, stride;
} DynArray;
void dh_arr_init(DynArray *a, int stride);
void dh_arr_free(DynArray *a);
void dh_arr_clear(DynArray *a);
void *dh_arr_push(DynArray *a);              /* returns pointer to new slot */
void *dh_arr_at(DynArray *a, int i);
void  dh_arr_reserve(DynArray *a, int n);

/* ── string interning / small string helpers ───────────────────────────── */
typedef struct { char buf[64]; } Str64;
int  dh_strcpy_safe(char *dst, int dstsz, const char *src);
int  dh_streq(const char *a, const char *b);
void dh_str_lower(char *s);

#endif /* DH_TYPES_H */
