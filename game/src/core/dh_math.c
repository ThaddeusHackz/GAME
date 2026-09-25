/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — math implementation (original work, no dependencies)
   ══════════════════════════════════════════════════════════════════════════ */
#include "dh_types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ── vectors ────────────────────────────────────────────────────────────── */
Vec3  v3_add(Vec3 a, Vec3 b)          { return v3(a.x+b.x, a.y+b.y, a.z+b.z); }
Vec3  v3_sub(Vec3 a, Vec3 b)          { return v3(a.x-b.x, a.y-b.y, a.z-b.z); }
Vec3  v3_mul(Vec3 a, float s)         { return v3(a.x*s, a.y*s, a.z*s); }
Vec3  v3_mulv(Vec3 a, Vec3 b)         { return v3(a.x*b.x, a.y*b.y, a.z*b.z); }
Vec3  v3_div(Vec3 a, float s)         { return s != 0.0f ? v3(a.x/s, a.y/s, a.z/s) : v3(0,0,0); }
float v3_dot(Vec3 a, Vec3 b)          { return a.x*b.x + a.y*b.y + a.z*b.z; }
Vec3  v3_cross(Vec3 a, Vec3 b)        { return v3(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x); }
float v3_len2(Vec3 a)                 { return a.x*a.x + a.y*a.y + a.z*a.z; }
float v3_len(Vec3 a)                  { return sqrtf(v3_len2(a)); }
float v3_dist(Vec3 a, Vec3 b)         { return v3_len(v3_sub(a,b)); }
float v3_dist_xz(Vec3 a, Vec3 b)      { float dx=a.x-b.x, dz=a.z-b.z; return sqrtf(dx*dx+dz*dz); }
Vec3  v3_neg(Vec3 a)                  { return v3(-a.x,-a.y,-a.z); }
Vec3  v3_min(Vec3 a, Vec3 b)          { return v3(dh_minf(a.x,b.x), dh_minf(a.y,b.y), dh_minf(a.z,b.z)); }
Vec3  v3_max(Vec3 a, Vec3 b)          { return v3(dh_maxf(a.x,b.x), dh_maxf(a.y,b.y), dh_maxf(a.z,b.z)); }
Vec3  v3_lerp(Vec3 a, Vec3 b, float t){ return v3(dh_lerp(a.x,b.x,t), dh_lerp(a.y,b.y,t), dh_lerp(a.z,b.z,t)); }

Vec3 v3_norm(Vec3 a) {
    float l = v3_len(a);
    if (l < 1e-8f) return v3(0, 0, 0);
    return v3(a.x/l, a.y/l, a.z/l);
}
int v3_valid(Vec3 a) { return dh_isfinite_f(a.x) && dh_isfinite_f(a.y) && dh_isfinite_f(a.z); }
Vec3 v3_safe(Vec3 a, Vec3 fb) { return v3_valid(a) ? a : fb; }

/* ── matrices (column-major: m[col*4 + row]) ────────────────────────────── */
Mat4 m4_identity(void) {
    Mat4 r; memset(&r, 0, sizeof(r));
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}
Mat4 m4_mul(Mat4 a, Mat4 b) {
    Mat4 r;
    for (int c = 0; c < 4; c++) {
        for (int rw = 0; rw < 4; rw++) {
            float s = 0.0f;
            for (int k = 0; k < 4; k++) s += a.m[k*4 + rw] * b.m[c*4 + k];
            r.m[c*4 + rw] = s;
        }
    }
    return r;
}
Mat4 m4_translate(Vec3 t) {
    Mat4 r = m4_identity();
    r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z;
    return r;
}
Mat4 m4_scale(Vec3 s) {
    Mat4 r; memset(&r, 0, sizeof(r));
    r.m[0]=s.x; r.m[5]=s.y; r.m[10]=s.z; r.m[15]=1.0f;
    return r;
}
Mat4 m4_scale1(float s) { return m4_scale(v3(s,s,s)); }

Mat4 m4_rot_x(float a) {
    Mat4 r = m4_identity(); float c = cosf(a), s = sinf(a);
    r.m[5]=c; r.m[6]=s; r.m[9]=-s; r.m[10]=c; return r;
}
Mat4 m4_rot_y(float a) {
    Mat4 r = m4_identity(); float c = cosf(a), s = sinf(a);
    r.m[0]=c; r.m[2]=-s; r.m[8]=s; r.m[10]=c; return r;
}
Mat4 m4_rot_z(float a) {
    Mat4 r = m4_identity(); float c = cosf(a), s = sinf(a);
    r.m[0]=c; r.m[1]=s; r.m[4]=-s; r.m[5]=c; return r;
}
Mat4 m4_look_at(Vec3 eye, Vec3 tgt, Vec3 up) {
    Vec3 f = v3_norm(v3_sub(tgt, eye));
    if (v3_len(f) < 1e-6f) f = v3(0,0,-1);
    Vec3 s = v3_norm(v3_cross(f, up));
    if (v3_len(s) < 1e-6f) s = v3(1,0,0);         /* guard: up parallel to f */
    Vec3 u = v3_cross(s, f);
    Mat4 r = m4_identity();
    r.m[0]=s.x; r.m[4]=s.y; r.m[8]=s.z;
    r.m[1]=u.x; r.m[5]=u.y; r.m[9]=u.z;
    r.m[2]=-f.x; r.m[6]=-f.y; r.m[10]=-f.z;
    r.m[12]=-v3_dot(s,eye); r.m[13]=-v3_dot(u,eye); r.m[14]=v3_dot(f,eye);
    return r;
}
Mat4 m4_perspective(float fov_deg, float aspect, float zn, float zf) {
    Mat4 r; memset(&r, 0, sizeof(r));
    float f = 1.0f / tanf(dh_clampf(fov_deg, 10.0f, 170.0f) * 0.5f * DH_DEG2RAD);
    if (aspect < 1e-4f) aspect = 1e-4f;
    if (zf <= zn) zf = zn + 1.0f;
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (zf + zn) / (zn - zf);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * zf * zn) / (zn - zf);
    return r;
}
Mat4 m4_ortho(float l, float rr, float b, float t, float n, float f) {
    Mat4 r = m4_identity();
    r.m[0] = 2.0f/(rr-l); r.m[5] = 2.0f/(t-b); r.m[10] = -2.0f/(f-n);
    r.m[12] = -(rr+l)/(rr-l); r.m[13] = -(t+b)/(t-b); r.m[14] = -(f+n)/(f-n);
    return r;
}
/* YXZ order: yaw then pitch then roll — WHY: matches FPS camera convention
   where pitch is applied in the character's local frame, avoiding roll drift. */
Mat4 m4_trs(Vec3 pos, float yaw, float pitch, float roll, Vec3 scale) {
    Mat4 m = m4_mul(m4_rot_y(yaw), m4_rot_x(pitch));
    if (roll != 0.0f) m = m4_mul(m, m4_rot_z(roll));
    if (scale.x != 1.0f || scale.y != 1.0f || scale.z != 1.0f) m = m4_mul(m, m4_scale(scale));
    m.m[12] = pos.x; m.m[13] = pos.y; m.m[14] = pos.z;
    return m;
}
Vec3 m4_xform_p(Mat4 m, Vec3 p) {
    float x = m.m[0]*p.x + m.m[4]*p.y + m.m[8]*p.z  + m.m[12];
    float y = m.m[1]*p.x + m.m[5]*p.y + m.m[9]*p.z  + m.m[13];
    float z = m.m[2]*p.x + m.m[6]*p.y + m.m[10]*p.z + m.m[14];
    float w = m.m[3]*p.x + m.m[7]*p.y + m.m[11]*p.z + m.m[15];
    if (dh_absf(w) > 1e-8f && dh_absf(w - 1.0f) > 1e-6f) { x/=w; y/=w; z/=w; }
    return v3(x,y,z);
}
Vec3 m4_xform_d(Mat4 m, Vec3 d) {
    return v3(m.m[0]*d.x + m.m[4]*d.y + m.m[8]*d.z,
              m.m[1]*d.x + m.m[5]*d.y + m.m[9]*d.z,
              m.m[2]*d.x + m.m[6]*d.y + m.m[10]*d.z);
}
Vec3 m4_get_pos(Mat4 m) { return v3(m.m[12], m.m[13], m.m[14]); }

Mat4 m4_invert(Mat4 in) {
    /* Full 4x4 inverse via cofactors — needed for view/projection extraction. */
    Mat4 out; float *m = in.m, *o = out.m;
    float a00=m[0],a01=m[1],a02=m[2],a03=m[3], a10=m[4],a11=m[5],a12=m[6],a13=m[7],
          a20=m[8],a21=m[9],a22=m[10],a23=m[11], a30=m[12],a31=m[13],a32=m[14],a33=m[15];
    float b00=a00*a11-a01*a10, b01=a00*a12-a02*a10, b02=a00*a13-a03*a10,
          b03=a01*a12-a02*a11, b04=a01*a13-a03*a11, b05=a02*a13-a03*a12,
          b06=a20*a31-a21*a30, b07=a20*a32-a22*a30, b08=a20*a33-a23*a30,
          b09=a21*a32-a22*a31, b10=a21*a33-a23*a31, b11=a22*a33-a23*a32;
    float det = b00*b11 - b01*b10 + b02*b09 + b03*b08 - b04*b07 + b05*b06;
    if (dh_absf(det) < 1e-12f) return m4_identity();
    det = 1.0f / det;
    o[0]=(a11*b11-a12*b10+a13*b09)*det;  o[1]=(a02*b10-a01*b11-a03*b09)*det;
    o[2]=(a31*b05-a32*b04+a33*b03)*det;  o[3]=(a22*b04-a21*b05-a23*b03)*det;
    o[4]=(a12*b08-a10*b11-a13*b07)*det;  o[5]=(a00*b11-a02*b08+a03*b07)*det;
    o[6]=(a32*b02-a30*b05-a33*b01)*det;  o[7]=(a20*b05-a22*b02+a23*b01)*det;
    o[8]=(a10*b10-a11*b08+a13*b06)*det;  o[9]=(a01*b08-a00*b10-a03*b06)*det;
    o[10]=(a30*b04-a31*b02+a33*b00)*det; o[11]=(a21*b02-a20*b04-a23*b00)*det;
    o[12]=(a11*b07-a10*b09-a12*b06)*det; o[13]=(a00*b09-a01*b07+a02*b06)*det;
    o[14]=(a31*b01-a30*b03-a32*b00)*det; o[15]=(a20*b03-a21*b01+a22*b00)*det;
    return out;
}

/* ── colour ─────────────────────────────────────────────────────────────── */
RGBAf rgbaf_mix(RGBAf a, RGBAf b, float t) {
    return rgbaf(dh_lerp(a.r,b.r,t), dh_lerp(a.g,b.g,t), dh_lerp(a.b,b.b,t), dh_lerp(a.a,b.a,t));
}
RGBAf rgbaf_mul(RGBAf c, float s) { return rgbaf(c.r*s, c.g*s, c.b*s, c.a); }
RGBAf rgbaf_from8(RGBA8 c) { return rgbaf(c.r/255.0f, c.g/255.0f, c.b/255.0f, c.a/255.0f); }
RGBA8 rgbaf_to8(RGBAf c) {
    return rgba8((uint8_t)(dh_clampf(c.r,0,1)*255.0f+0.5f),
                 (uint8_t)(dh_clampf(c.g,0,1)*255.0f+0.5f),
                 (uint8_t)(dh_clampf(c.b,0,1)*255.0f+0.5f),
                 (uint8_t)(dh_clampf(c.a,0,1)*255.0f+0.5f));
}
/* Simple, honest channel-collapse approximation of the three common CVDs.
   WHY this form: a full LMS matrix is overkill for UI/faction colour checks;
   collapsing the weak channel toward its neighbours is what players actually
   perceive, and it keeps the code auditable (Spec 33 colorblind modes). */
RGBAf rgbaf_colorblind(RGBAf c, int mode) {
    switch (mode) {
        case 1: { /* deuteranopia: green weak -> blend green toward red */
            float g = 0.45f*c.g + 0.55f*c.r; return rgbaf(c.r, g, c.b, c.a);
        }
        case 2: { /* protanopia: red weak -> blend red toward green */
            float r = 0.45f*c.r + 0.55f*c.g; return rgbaf(r, c.g, c.b, c.a);
        }
        case 3: { /* tritanopia: blue weak -> blend blue toward green */
            float b = 0.45f*c.b + 0.55f*c.g; return rgbaf(c.r, c.g, b, c.a);
        }
        default: return c;
    }
}

/* ── date seed (Spec 30: Contract of the Day must be identical worldwide) ── */
uint32_t dh_date_seed(int year, int month, int day) {
    return (uint32_t)(year * 10000 + month * 100 + day) * 7u;
}

/* ── value noise ────────────────────────────────────────────────────────── */
static float hash2(const NoiseCfg *n, int xi, int yi) {
    uint32_t h = (uint32_t)xi * 374761393u + (uint32_t)yi * 668265263u + n->seed * 1442695041u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (float)(h & 0xFFFFFFu) * (1.0f / 16777215.0f);
}
static float hash3(const NoiseCfg *n, int xi, int yi, int zi) {
    uint32_t h = (uint32_t)xi * 374761393u + (uint32_t)yi * 668265263u
               + (uint32_t)zi * 2147483647u + n->seed * 1442695041u;
    h = (h ^ (h >> 13)) * 1274126177u; h ^= h >> 16;
    return (float)(h & 0xFFFFFFu) * (1.0f / 16777215.0f);
}
static float smoothstep_f(float t) { return t * t * (3.0f - 2.0f * t); }

float noise2(const NoiseCfg *n, float x, float y) {
    float fx = x * n->freq, fy = y * n->freq;
    int xi = (int)floorf(fx), yi = (int)floorf(fy);
    float tx = smoothstep_f(fx - (float)xi), ty = smoothstep_f(fy - (float)yi);
    float a = hash2(n, xi, yi),     b = hash2(n, xi+1, yi);
    float c = hash2(n, xi, yi+1),   d = hash2(n, xi+1, yi+1);
    return dh_lerp(dh_lerp(a,b,tx), dh_lerp(c,d,tx), ty) * 2.0f - 1.0f;
}
float noise_fbm2(const NoiseCfg *n, float x, float y) {
    float amp = 1.0f, f = 1.0f, sum = 0.0f, norm = 0.0f;
    int oct = dh_clampi(n->octaves, 1, 8);
    for (int i = 0; i < oct; i++) {
        NoiseCfg c = *n; c.freq = n->freq * f;
        sum += noise2(&c, x, y) * amp;
        norm += amp;
        amp *= n->gain; f *= n->lacunarity;
    }
    return norm > 0.0f ? sum / norm : 0.0f;
}
float noise3(const NoiseCfg *n, float x, float y, float z) {
    float fx=x*n->freq, fy=y*n->freq, fz=z*n->freq;
    int xi=(int)floorf(fx), yi=(int)floorf(fy), zi=(int)floorf(fz);
    float tx=smoothstep_f(fx-xi), ty=smoothstep_f(fy-yi), tz=smoothstep_f(fz-zi);
    float c000=hash3(n,xi,yi,zi),     c100=hash3(n,xi+1,yi,zi);
    float c010=hash3(n,xi,yi+1,zi),   c110=hash3(n,xi+1,yi+1,zi);
    float c001=hash3(n,xi,yi,zi+1),   c101=hash3(n,xi+1,yi,zi+1);
    float c011=hash3(n,xi,yi+1,zi+1), c111=hash3(n,xi+1,yi+1,zi+1);
    float x00=dh_lerp(c000,c100,tx), x10=dh_lerp(c010,c110,tx);
    float x01=dh_lerp(c001,c101,tx), x11=dh_lerp(c011,c111,tx);
    float y0=dh_lerp(x00,x10,ty), y1=dh_lerp(x01,x11,ty);
    return dh_lerp(y0,y1,tz) * 2.0f - 1.0f;
}
float noise_ridge2(const NoiseCfg *n, float x, float y) {
    float amp=1.0f, f=1.0f, sum=0.0f, norm=0.0f;
    int oct = dh_clampi(n->octaves, 1, 8);
    for (int i = 0; i < oct; i++) {
        NoiseCfg c = *n; c.freq = n->freq * f;
        float v = 1.0f - dh_absf(noise2(&c, x, y));
        sum += v*v*amp; norm += amp;
        amp *= n->gain; f *= n->lacunarity;
    }
    return norm > 0.0f ? sum/norm : 0.0f;
}

/* ── geometry ───────────────────────────────────────────────────────────── */
AABB aabb_from_center(Vec3 c, Vec3 half) {
    AABB b; b.min = v3_sub(c, half); b.max = v3_add(c, half); return b;
}
int aabb_overlaps(AABB a, AABB b) {
    return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
           (a.min.y <= b.max.y && a.max.y >= b.min.y) &&
           (a.min.z <= b.max.z && a.max.z >= b.min.z);
}
Vec3 aabb_center(AABB b) { return v3_mul(v3_add(b.min, b.max), 0.5f); }

Plane plane_from_points(Vec3 a, Vec3 b, Vec3 c) {
    Plane p; p.n = v3_norm(v3_cross(v3_sub(b,a), v3_sub(c,a))); p.d = -v3_dot(p.n, a); return p;
}
float plane_dist(Plane p, Vec3 pt) { return v3_dot(p.n, pt) + p.d; }

int ray_sphere(Ray r, Vec3 c, float rad, float *t_out) {
    Vec3 oc = v3_sub(r.o, c);
    float b = v3_dot(oc, r.d), cc = v3_dot(oc, oc) - rad*rad;
    float h = b*b - cc;
    if (h < 0.0f) return 0;
    h = sqrtf(h);
    float t = -b - h;
    if (t < 0.0f) t = -b + h;
    if (t < 0.0f) return 0;
    if (t_out) *t_out = t;
    return 1;
}
int ray_aabb(Ray r, AABB b, float *t_out) {
    float tmin = -1e30f, tmax = 1e30f;
    float o[3] = {r.o.x, r.o.y, r.o.z}, d[3] = {r.d.x, r.d.y, r.d.z};
    float lo[3] = {b.min.x, b.min.y, b.min.z}, hi[3] = {b.max.x, b.max.y, b.max.z};
    for (int i = 0; i < 3; i++) {
        if (dh_absf(d[i]) < 1e-8f) { if (o[i] < lo[i] || o[i] > hi[i]) return 0; continue; }
        float t1 = (lo[i] - o[i]) / d[i], t2 = (hi[i] - o[i]) / d[i];
        if (t1 > t2) { float tt = t1; t1 = t2; t2 = tt; }
        if (t1 > tmin) tmin = t1;
        if (t2 < tmax) tmax = t2;
        if (tmin > tmax) return 0;
    }
    if (tmax < 0.0f) return 0;
    if (t_out) *t_out = tmin >= 0.0f ? tmin : tmax;
    return 1;
}
int ray_plane_y(Ray r, float y, float *t_out, Vec3 *hit_out) {
    if (dh_absf(r.d.y) < 1e-8f) return 0;
    float t = (y - r.o.y) / r.d.y;
    if (t < 0.0f) return 0;
    if (t_out) *t_out = t;
    if (hit_out) *hit_out = v3_add(r.o, v3_mul(r.d, t));
    return 1;
}

/* ── frustum ────────────────────────────────────────────────────────────── */
Frustum frustum_from_vp(Mat4 m) {
    Frustum f;
    float *a = m.m;
    /* Gribb-Hartmann row extraction. */
    f.p[0].n = v3(a[3]+a[0], a[7]+a[4], a[11]+a[8]);  f.p[0].d = a[15]+a[12]; /* left   */
    f.p[1].n = v3(a[3]-a[0], a[7]-a[4], a[11]-a[8]);  f.p[1].d = a[15]-a[12]; /* right  */
    f.p[2].n = v3(a[3]+a[1], a[7]+a[5], a[11]+a[9]);  f.p[2].d = a[15]+a[13]; /* bottom */
    f.p[3].n = v3(a[3]-a[1], a[7]-a[5], a[11]-a[9]);  f.p[3].d = a[15]-a[13]; /* top    */
    f.p[4].n = v3(a[3]+a[2], a[7]+a[6], a[11]+a[10]); f.p[4].d = a[15]+a[14]; /* near   */
    f.p[5].n = v3(a[3]-a[2], a[7]-a[6], a[11]-a[10]); f.p[5].d = a[15]-a[14]; /* far    */
    for (int i = 0; i < 6; i++) {
        float l = v3_len(f.p[i].n);
        if (l < 1e-8f) { f.p[i].n = v3(0,1,0); f.p[i].d = 1e30f; continue; }
        f.p[i].n = v3_mul(f.p[i].n, 1.0f/l);
        f.p[i].d /= l;
    }
    return f;
}
int frustum_has_sphere(Frustum f, Vec3 c, float r) {
    for (int i = 0; i < 6; i++)
        if (v3_dot(f.p[i].n, c) + f.p[i].d < -r) return 0;
    return 1;
}
int frustum_has_aabb(Frustum f, AABB b) {
    for (int i = 0; i < 6; i++) {
        Plane p = f.p[i];
        Vec3 pos;
        pos.x = p.n.x > 0 ? b.max.x : b.min.x;
        pos.y = p.n.y > 0 ? b.max.y : b.min.y;
        pos.z = p.n.z > 0 ? b.max.z : b.min.z;
        if (v3_dot(p.n, pos) + p.d < 0.0f) return 0;
    }
    return 1;
}

/* ── dynamic array ──────────────────────────────────────────────────────── */
void dh_arr_init(DynArray *a, int stride) { a->data = NULL; a->count = 0; a->cap = 0; a->stride = stride; }
void dh_arr_free(DynArray *a) { free(a->data); a->data = NULL; a->count = a->cap = 0; }
void dh_arr_clear(DynArray *a) { a->count = 0; }
void dh_arr_reserve(DynArray *a, int n) {
    if (n <= a->cap) return;
    int nc = a->cap ? a->cap : DH_ARRAY_INIT_CAP;
    while (nc < n) nc *= 2;
    void *nd = realloc(a->data, (size_t)nc * (size_t)a->stride);
    if (!nd) return;                       /* never crash on OOM (Spec 19) */
    a->data = nd; a->cap = nc;
}
void *dh_arr_push(DynArray *a) {
    dh_arr_reserve(a, a->count + 1);
    if (!a->data) return NULL;
    void *p = (char*)a->data + (size_t)a->count * (size_t)a->stride;
    a->count++;
    return p;
}
void *dh_arr_at(DynArray *a, int i) {
    if (i < 0 || i >= a->count || !a->data) return NULL;
    return (char*)a->data + (size_t)i * (size_t)a->stride;
}

/* ── strings ────────────────────────────────────────────────────────────── */
int dh_strcpy_safe(char *dst, int dstsz, const char *src) {
    if (!dst || dstsz <= 0) return 0;
    if (!src) { dst[0] = '\0'; return 0; }
    int i = 0;
    while (i < dstsz - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
    return i;
}
int dh_streq(const char *a, const char *b) {
    if (!a || !b) return a == b;
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}
void dh_str_lower(char *s) { if (!s) return; for (; *s; ++s) if (*s >= 'A' && *s <= 'Z') *s += 32; }
