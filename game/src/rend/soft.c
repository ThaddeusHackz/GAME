/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — SOFTWARE RASTERIZER (backend "SOFT")
   A complete z-buffered, perspective-correct, mip-mapped, lit, fogged CPU
   renderer. Three jobs:
     1. Headless verification — dumps BMP/PNG so frames can be inspected
        without a GPU or display server (this sandbox has neither).
     2. `--soft` potato fallback for machines with a broken GL driver.
     3. Reference implementation the GL 1.1 backend is checked against.
   Deliberately simple and branch-light; correctness over cleverness.
   ══════════════════════════════════════════════════════════════════════════ */
#include "rend.h"
#include "../core/dh_log.h"
#include "../core/settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { float m[9]; } Mat3;   /* row-major 3x3 for normals */

static uint32_t *s_fb = NULL;
static float    *s_z  = NULL;
static int       s_w = 0, s_h = 0;
static Mat4      s_mvp, s_model, s_view, s_proj;
static Mat3      s_nmat;
static SceneLight s_light;
static Vec3      s_cam;
static uint32_t  s_clear = 0xFF8FA8C0u;
static Frustum   s_frustum;

static Mat3 mat3_normal_from(Mat4 model) {
    /* For the rigid + uniform-scale transforms this game uses, the upper 3x3
       of the model matrix IS the normal matrix. A true inverse-transpose per
       item cost measurable time for zero visual difference, so we take the
       cheap path that is correct for our data and renormalize per vertex. */
    Mat3 r;
    r.m[0]=model.m[0]; r.m[1]=model.m[4]; r.m[2]=model.m[8];
    r.m[3]=model.m[1]; r.m[4]=model.m[5]; r.m[5]=model.m[9];
    r.m[6]=model.m[2]; r.m[7]=model.m[6]; r.m[8]=model.m[10];
    return r;
}
static Vec3 mat3_mul(Mat3 m, Vec3 v) {
    return v3(m.m[0]*v.x + m.m[1]*v.y + m.m[2]*v.z,
              m.m[3]*v.x + m.m[4]*v.y + m.m[5]*v.z,
              m.m[6]*v.x + m.m[7]*v.y + m.m[8]*v.z);
}

/* ── framebuffer ───────────────────────────────────────────────────────── */
static int alloc_fb(int w, int h) {
    free(s_fb); free(s_z);
    s_fb = (uint32_t*)malloc((size_t)w * h * 4);
    s_z  = (float*)malloc((size_t)w * h * sizeof(float));
    if (!s_fb || !s_z) { free(s_fb); free(s_z); s_fb = NULL; s_z = NULL; return 0; }
    s_w = w; s_h = h;
    RendState *r = rend();
    r->fb = s_fb; r->zbuf = s_z; r->w = w; r->h = h;
    return 1;
}
int rend_be_init(int w, int h) {
    if (!alloc_fb(w, h)) { DH_ERROR("soft", "framebuffer alloc failed %dx%d", w, h); return 0; }
    DH_INFO("soft", "software rasterizer online: %dx%d (%.1f MB fb+z)", w, h,
            (double)w*h*8.0/1048576.0);
    return 1;
}
void rend_be_shutdown(void) {
    free(s_fb); free(s_z); s_fb = NULL; s_z = NULL;
    RendState *r = rend(); r->fb = NULL; r->zbuf = NULL;
}
void rend_be_resize(int w, int h) {
    if (w == s_w && h == s_h) return;
    if (!alloc_fb(w, h)) DH_ERROR("soft", "resize failed %dx%d", w, h);
}
void rend_be_frame(const Mat4 *view, const Mat4 *proj, Vec3 cam_pos, const SceneLight *light) {
    s_view = *view; s_proj = *proj; s_cam = cam_pos;
    if (light) s_light = *light;
    s_frustum = frustum_from_vp(m4_mul(s_proj, s_view));
    if (!s_fb) return;
    /* Clear colour = fog colour so the horizon blend is seamless. */
    uint32_t fr = (uint32_t)dh_clampi((int)(s_light.fog_color.x * 255.0f), 0, 255);
    uint32_t fg = (uint32_t)dh_clampi((int)(s_light.fog_color.y * 255.0f), 0, 255);
    uint32_t fbl= (uint32_t)dh_clampi((int)(s_light.fog_color.z * 255.0f), 0, 255);
    s_clear = 0xFF000000u | (fbl << 16) | (fg << 8) | fr;
    uint32_t n = (uint32_t)s_w * (uint32_t)s_h;
    for (uint32_t i = 0; i < n; i++) { s_fb[i] = s_clear; s_z[i] = 1.0f; }
}

/* ── pixel helpers ─────────────────────────────────────────────────────── */
static inline void put_blend(int x, int y, uint32_t src, float a) {
    uint32_t *p = &s_fb[(size_t)y * s_w + x];
    uint32_t d = *p;
    float ia = 1.0f - a;
    uint32_t r = (uint32_t)(((d       & 0xFF) * ia + (src       & 0xFF) * a) + 0.5f);
    uint32_t g = (uint32_t)((((d>>8)  & 0xFF) * ia + ((src>>8)  & 0xFF) * a) + 0.5f);
    uint32_t b = (uint32_t)((((d>>16) & 0xFF) * ia + ((src>>16) & 0xFF) * a) + 0.5f);
    *p = 0xFF000000u | (b << 16) | (g << 8) | r;
}
static inline void put_opaque(int x, int y, uint32_t c) {
    s_fb[(size_t)y * s_w + x] = c | 0xFF000000u;
}

/* ── texture sampling ──────────────────────────────────────────────────── */
static inline uint32_t sample_tex_mip(const Texture *t, float u, float v, int level) {
    if (!t) return 0xFFFFFFFFu;
    if (level < 0 || level >= TEX_MIP_LEVELS || !t->mip[level]) level = 0;
    int w = t->mipw[level], h = t->miph[level];
    if (w <= 0 || h <= 0) return 0xFFFFFFFFu;
    int x = (int)(u * (float)w);
    int y = (int)(v * (float)h);
    x %= w; if (x < 0) x += w;
    y %= h; if (y < 0) y += h;
    /* Nearest at the selected level: stylized art (Spec 3) reads better with
       crisp texels than blurred bilinear, and it's ~3x faster. */
    return t->mip[level][(size_t)y * w + x];
}

/* ── vertex lighting (same maths the GL fixed-function path reproduces) ── */
static void light_vertex(Vec3 wp, Vec3 wn, uint32_t base, int lit, int fogged,
                         float *or_, float *og, float *ob) {
    float br = ((base      ) & 0xFF) / 255.0f;
    float bg = ((base >>  8) & 0xFF) / 255.0f;
    float bb = ((base >> 16) & 0xFF) / 255.0f;
    if (!lit) { *or_ = br; *og = bg; *ob = bb; return; }
    Vec3 n = v3_norm(wn);
    float ndl = -v3_dot(n, s_light.sun_dir);
    /* Wrap lighting — WHY: a hard ndl>0 cutoff makes stylized foliage look
       like cut paper. A small wrap softens the terminator for ~free. */
    const float wrap = 0.15f;
    float t = dh_clampf((ndl + wrap) / (1.0f + wrap), 0.0f, 1.0f);
    t *= t;
    float si = s_light.sun_intensity;
    float dr = s_light.sun_color.x * si * t;
    float dg = s_light.sun_color.y * si * t;
    float db = s_light.sun_color.z * si * t;
    float h = n.y * 0.5f + 0.5f;
    float ar = dh_lerp(s_light.hemi_ground.x, s_light.hemi_sky.x, h) + s_light.ambient.x;
    float ag = dh_lerp(s_light.hemi_ground.y, s_light.hemi_sky.y, h) + s_light.ambient.y;
    float ab = dh_lerp(s_light.hemi_ground.z, s_light.hemi_sky.z, h) + s_light.ambient.z;
    for (int i = 0; i < s_light.point_count && i < MAX_LIGHTS; i++) {
        if (!s_light.point[i].on) continue;
        Vec3 d = v3_sub(s_light.point[i].pos, wp);
        float dist = v3_len(d);
        float rad = s_light.point[i].radius > 0.01f ? s_light.point[i].radius : 1.0f;
        if (dist > rad) continue;
        float att = 1.0f - dist / rad;
        att *= att;
        att *= s_light.point[i].intensity;
        float l = dist > 1e-5f ? v3_dot(n, v3_mul(d, 1.0f/dist)) : 0.0f;
        if (l < 0.0f) l = 0.0f;
        ar += s_light.point[i].color.x * att * (l + 0.25f);
        ag += s_light.point[i].color.y * att * (l + 0.25f);
        ab += s_light.point[i].color.z * att * (l + 0.25f);
    }
    float r = br * (ar + dr), g = bg * (ag + dg), b = bb * (ab + db);
    if (fogged && s_light.fog_enabled && s_light.fog_far > s_light.fog_near + 0.01f) {
        float dist = v3_dist(wp, s_cam);
        float f = (dist - s_light.fog_near) / (s_light.fog_far - s_light.fog_near);
        f = dh_clampf(f, 0.0f, 1.0f);
        f = f * f * (3.0f - 2.0f * f);           /* smoothstep: no visible fog band */
        r = dh_lerp(r, s_light.fog_color.x, f);
        g = dh_lerp(g, s_light.fog_color.y, f);
        b = dh_lerp(b, s_light.fog_color.z, f);
    }
    *or_ = r; *og = g; *ob = b;
}

/* ── rasterizer core ───────────────────────────────────────────────────── */
typedef struct {
    float x, y;         /* screen px */
    float iw;           /* 1/w */
    float u, v;         /* premultiplied by iw */
    float zd;           /* window-z * iw */
    float r, g, b, a;   /* premultiplied by iw */
} SV;

static inline int project_sv(const Vec3 *wp, const Vec3 *wn, float u, float v,
                             uint32_t base, float item_alpha, int lit, int fogged,
                             SV *out) {
    const float *m = s_mvp.m;
    float cx = m[0]*wp->x + m[4]*wp->y + m[8]*wp->z  + m[12];
    float cy = m[1]*wp->x + m[5]*wp->y + m[9]*wp->z  + m[13];
    float cz = m[2]*wp->x + m[6]*wp->y + m[10]*wp->z + m[14];
    float cw = m[3]*wp->x + m[7]*wp->y + m[11]*wp->z + m[15];
    if (!(cw > 1e-4f)) return 0;                   /* behind near plane */
    float invw = 1.0f / cw;
    out->x = (cx * invw * 0.5f + 0.5f) * (float)s_w;
    out->y = (0.5f - cy * invw * 0.5f) * (float)s_h;
    float wz = cz * invw * 0.5f + 0.5f;
    out->iw = invw;
    out->zd = wz * invw;
    float lr, lg, lb;
    light_vertex(*wp, *wn, base, lit, fogged, &lr, &lg, &lb);
    out->r = lr * invw; out->g = lg * invw; out->b = lb * invw;
    out->a = item_alpha * invw;
    out->u = u * invw; out->v = v * invw;
    if (!dh_isfinite_f(out->x) || !dh_isfinite_f(out->y) || !dh_isfinite_f(out->iw)) return 0;
    return 1;
}

static void raster_tri(const SV *v0, const SV *v1, const SV *v2,
                       const Texture *tex, int blend, int alpha_test,
                       int depth_write, int depth_test, const float *scis) {
    float x0=v0->x, y0=v0->y, x1=v1->x, y1=v1->y, x2=v2->x, y2=v2->y;
    float a0 = y1 - y2, b0 = x2 - x1, c0 = x1*y2 - x2*y1;
    float a1 = y2 - y0, b1 = x0 - x2, c1 = x2*y0 - x0*y2;
    float area = a0*x0 + b0*y0 + c0;
    if (area > -0.05f && area < 0.05f) return;     /* degenerate / sub-pixel */
    float inv_area = 1.0f / area;
    int pos = area > 0.0f;

    float minx = dh_minf(dh_minf(x0,x1),x2), maxx = dh_maxf(dh_maxf(x0,x1),x2);
    float miny = dh_minf(dh_minf(y0,y1),y2), maxy = dh_maxf(dh_maxf(y0,y1),y2);
    /* Reject triangles entirely off one screen edge before the bbox walk. */
    if (maxx < 0 || minx > (float)s_w || maxy < 0 || miny > (float)s_h) return;
    int ix0 = (int)floorf(minx), ix1 = (int)ceilf(maxx);
    int iy0 = (int)floorf(miny), iy1 = (int)ceilf(maxy);
    if (scis) {
        int sx0 = (int)scis[0], sy0 = (int)scis[1];
        int sx1 = sx0 + (int)scis[2], sy1 = sy0 + (int)scis[3];
        if (ix0 < sx0) ix0 = sx0;
        if (iy0 < sy0) iy0 = sy0;
        if (ix1 > sx1) ix1 = sx1;
        if (iy1 > sy1) iy1 = sy1;
    }
    if (ix0 < 0) ix0 = 0;
    if (iy0 < 0) iy0 = 0;
    if (ix1 > s_w-1) ix1 = s_w-1;
    if (iy1 > s_h-1) iy1 = s_h-1;
    if (ix0 > ix1 || iy0 > iy1) return;

    /* Mip level from texel density, chosen once per triangle. */
    int mip = 0;
    if (tex && tex->mip[1]) {
        float u0 = v0->u/v0->iw, u1 = v1->u/v1->iw, u2 = v2->u/v2->iw;
        float q0 = v0->v/v0->iw, q1 = v1->v/v1->iw, q2 = v2->v/v2->iw;
        float du = fabsf(u0-u1) + fabsf(u1-u2) + fabsf(u2-u0);
        float dv = fabsf(q0-q1) + fabsf(q1-q2) + fabsf(q2-q0);
        float px = fabsf(area) * 0.5f + 1.0f;
        float density = (du + dv) * 0.5f * (float)tex->w / px;
        if (density > 1.0f) {
            mip = (int)(logf(density) * 1.4426950f);
            mip = dh_clampi(mip, 0, TEX_MIP_LEVELS-1);
            while (mip > 0 && !tex->mip[mip]) mip--;
        }
        rend()->lod_swaps++;
    }

    float fx = (float)ix0 + 0.5f, fy = (float)iy0 + 0.5f;
    float w0row = a0*fx + b0*fy + c0;
    float w1row = a1*fx + b1*fy + c1;
    int drawn_any = 0;
    for (int y = iy0; y <= iy1; y++, w0row += b0, w1row += b1) {
        float w0 = w0row, w1 = w1row;
        float *zrow = s_z + (size_t)y * s_w;
        for (int x = ix0; x <= ix1; x++, w0 += a0, w1 += a1) {
            float w2 = area - w0 - w1;
            if (pos) { if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue; }
            else     { if (w0 > 0.0f || w1 > 0.0f || w2 > 0.0f) continue; }
            float l0 = w0 * inv_area, l1 = w1 * inv_area, l2 = w2 * inv_area;
            float iw = l0*v0->iw + l1*v1->iw + l2*v2->iw;
            if (!(iw > 1e-8f)) continue;
            float w = 1.0f / iw;
            float z = (l0*v0->zd + l1*v1->zd + l2*v2->zd) * w;
            if (depth_test && z >= zrow[x]) continue;
            float a = (l0*v0->a + l1*v1->a + l2*v2->a) * w;
            float r = (l0*v0->r + l1*v1->r + l2*v2->r) * w;
            float g = (l0*v0->g + l1*v1->g + l2*v2->g) * w;
            float b = (l0*v0->b + l1*v1->b + l2*v2->b) * w;
            if (tex) {
                float tu = (l0*v0->u + l1*v1->u + l2*v2->u) * w;
                float tv = (l0*v0->v + l1*v1->v + l2*v2->v) * w;
                uint32_t s = sample_tex_mip(tex, tu, tv, mip);
                float ta = ((s >> 24) & 0xFF) * (1.0f/255.0f);
                if (alpha_test && ta < 0.5f) continue;
                a *= ta;
                r *= ( s        & 0xFF) * (1.0f/255.0f);
                g *= ((s >>  8) & 0xFF) * (1.0f/255.0f);
                b *= ((s >> 16) & 0xFF) * (1.0f/255.0f);
            }
            if (a <= 0.004f) continue;
            uint32_t col = ((uint32_t)dh_clampi((int)(b*255.0f+0.5f),0,255) << 16)
                         | ((uint32_t)dh_clampi((int)(g*255.0f+0.5f),0,255) << 8)
                         |  (uint32_t)dh_clampi((int)(r*255.0f+0.5f),0,255);
            if (blend && a < 0.995f) put_blend(x, y, col, a);
            else                   put_opaque(x, y, col);
            if (depth_write) zrow[x] = z;
            drawn_any = 1;
        }
    }
    if (drawn_any) rend()->tris_drawn++;
}

/* ── mesh item ─────────────────────────────────────────────────────────── */
static void draw_mesh_item(const RenderItem *it) {
    const Mesh *m = mesh_get(it->mesh);
    if (!m || !m->pos || !m->idx || m->icount < 3) return;
    s_model = it->model;
    s_mvp = m4_mul(s_proj, m4_mul(s_view, s_model));
    s_nmat = mat3_normal_from(s_model);

    Vec3 wc = m4_xform_p(s_model, m->center);
    float wr = m->radius;
    { float sx = fabsf(s_model.m[0]) + fabsf(s_model.m[4]) + fabsf(s_model.m[8]);
      float sy = fabsf(s_model.m[1]) + fabsf(s_model.m[5]) + fabsf(s_model.m[9]);
      float sz = fabsf(s_model.m[2]) + fabsf(s_model.m[6]) + fabsf(s_model.m[10]);
      float smax = dh_maxf(dh_maxf(sx,sy),sz);
      if (smax > 0.0f) wr *= smax; }
    if (!frustum_has_sphere(s_frustum, wc, wr)) { rend()->frustum_culled++; return; }

    const Texture *tex = tex_get(it->tex >= 0 ? it->tex : m->tex);
    int two_sided  = ((m->flags & MESH_TWO_SIDED) || (it->sort_key & 1)) ? 1 : 0;
    int blend      = it->alpha < 0.995f;
    int alpha_test = it->alpha_test || (m->flags & MESH_ALPHA_TEST) ? 1 : 0;
    const float *scis = (it->scissor[2] > 0.0f) ? it->scissor : NULL;

    uint32_t base = it->color;
    int vcol = (m->col && (m->flags & MESH_VERTEXCOL)) ? 1 : 0;

    for (int t = 0; t + 2 < m->icount; t += 3) {
        uint32_t i0 = m->idx[t], i1 = m->idx[t+1], i2 = m->idx[t+2];
        if (i0 >= (uint32_t)m->vcount || i1 >= (uint32_t)m->vcount || i2 >= (uint32_t)m->vcount) continue;
        Vec3 p0 = m4_xform_p(s_model, m->pos[i0]);
        Vec3 p1 = m4_xform_p(s_model, m->pos[i1]);
        Vec3 p2 = m4_xform_p(s_model, m->pos[i2]);
        Vec3 n0 = v3_norm(mat3_mul(s_nmat, m->nrm[i0]));
        Vec3 n1 = v3_norm(mat3_mul(s_nmat, m->nrm[i1]));
        Vec3 n2 = v3_norm(mat3_mul(s_nmat, m->nrm[i2]));
        /* Backface reject that is agnostic to winding convention: compare the
           geometric face normal against the authored vertex normal, signed by
           the mesh's self-calibrated convention (see Mesh::winding). Without
           the sign term, a generator that winds the other way would render the
           entire world invisible. */
        if (!two_sided) {
            Vec3 fn = v3_cross(v3_sub(p1,p0), v3_sub(p2,p0));
            if (v3_dot(fn, n0) * (float)m->winding < 0.0f) continue;
        }
        uint32_t c0 = vcol ? m->col[i0] : base;
        SV a, b, c;
        if (!project_sv(&p0,&n0, m->uv[i0].x, m->uv[i0].y, c0, it->alpha, it->lit, it->fogged, &a)) continue;
        uint32_t c1 = vcol ? m->col[i1] : base;
        if (!project_sv(&p1,&n1, m->uv[i1].x, m->uv[i1].y, c1, it->alpha, it->lit, it->fogged, &b)) continue;
        uint32_t c2 = vcol ? m->col[i2] : base;
        if (!project_sv(&p2,&n2, m->uv[i2].x, m->uv[i2].y, c2, it->alpha, it->lit, it->fogged, &c)) continue;
        raster_tri(&a, &b, &c, tex, blend, alpha_test, it->depth_write, it->depth_test, scis);
    }
    rend()->draw_calls++;
}

/* ── lines (debug, waypoints, laser sights, tag outlines) ──────────────── */
static void line_seg(Vec3 w0, Vec3 w1, uint32_t col, float alpha, int depth_test) {
    const float *m = s_mvp.m;
    float cx[2], cy[2], cz[2], cw[2];
    Vec3 ws[2] = { w0, w1 };
    for (int i = 0; i < 2; i++) {
        cx[i] = m[0]*ws[i].x + m[4]*ws[i].y + m[8]*ws[i].z  + m[12];
        cy[i] = m[1]*ws[i].x + m[5]*ws[i].y + m[9]*ws[i].z  + m[13];
        cz[i] = m[2]*ws[i].x + m[6]*ws[i].y + m[10]*ws[i].z + m[14];
        cw[i] = m[3]*ws[i].x + m[7]*ws[i].y + m[11]*ws[i].z + m[15];
    }
    if (!(cw[0] > 1e-4f) || !(cw[1] > 1e-4f)) return;
    float sx[2], sy[2], sz[2];
    for (int i = 0; i < 2; i++) {
        float iw = 1.0f / cw[i];
        sx[i] = (cx[i]*iw*0.5f + 0.5f) * (float)s_w;
        sy[i] = (0.5f - cy[i]*iw*0.5f) * (float)s_h;
        sz[i] = cz[i]*iw*0.5f + 0.5f;
    }
    int x0 = (int)sx[0], y0 = (int)sy[0], x1 = (int)sx[1], y1 = (int)sy[1];
    int dx = abs(x1-x0), sx_ = x0<x1?1:-1;
    int dy = -abs(y1-y0), sy_ = y0<y1?1:-1;
    int err = dx + dy;
    int steps = dx > -dy ? dx : -dy;
    if (steps <= 0) steps = 1;
    if (steps > 4096) steps = 4096;                 /* hard cap: no hangs */
    int total = abs(x1-x0) + abs(y1-y0) + 1;
    int guard = 0;
    for (;;) {
        if (++guard > steps + 2) break;
        if (x0 >= 0 && x0 < s_w && y0 >= 0 && y0 < s_h) {
            float frac = (float)guard / (float)total;
            float z = sz[0] + (sz[1]-sz[0]) * frac;
            z = dh_clampf(z, 0.0f, 1.0f);
            if (!depth_test || z < s_z[(size_t)y0*s_w + x0]) {
                if (alpha >= 0.995f) put_opaque(x0,y0,col); else put_blend(x0,y0,col,alpha);
                if (depth_test) s_z[(size_t)y0*s_w + x0] = z;
            }
        }
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2*err;
        if (e2 >= dy) { err += dy; x0 += sx_; }
        if (e2 <= dx) { err += dx; y0 += sy_; }
    }
}
static void draw_lines_item(const RenderItem *it) {
    if (!it->lines || it->line_count <= 0) return;
    s_mvp = m4_mul(s_proj, s_view);                 /* lines are already world-space */
    float a = ((it->color >> 24) & 0xFF) / 255.0f;
    for (int i = 0; i < it->line_count; i++)
        line_seg(it->lines[i*2+0], it->lines[i*2+1], it->color, a, it->depth_test);
    rend()->draw_calls++;
}

/* ── particles (camera-facing quads, batched) ──────────────────────────── */
static void draw_particles_item(const RenderItem *it) {
    if (!it->parts || it->part_count <= 0) return;
    const Texture *tex = tex_get(it->tex);
    /* Camera basis so every billboard faces the viewer exactly. */
    Vec3 fwd = v3_norm(v3_sub(v3(0,0,0), v3(s_view.m[2], s_view.m[6], s_view.m[10])));
    (void)fwd;
    Vec3 right = v3(s_view.m[0], s_view.m[4], s_view.m[8]);
    Vec3 up    = v3(s_view.m[1], s_view.m[5], s_view.m[9]);
    s_mvp = m4_mul(s_proj, s_view);
    for (int i = 0; i < it->part_count; i++) {
        const Particle *p = &it->parts[i];
        if (p->life <= 0.0f) continue;
        float lf = p->max_life > 0.0f ? dh_clampf(p->life / p->max_life, 0.0f, 1.0f) : 1.0f;
        float sz = p->size > 0.0f ? p->size : it->part_size;
        /* smoke expands and fades; sparks shrink and stay bright */
        if (p->kind == 1) sz *= (1.0f + (1.0f - lf) * 2.0f);
        else              sz *= (0.4f + 0.6f * lf);
        Vec3 r = v3_mul(right, sz), u = v3_mul(up, sz);
        Vec3 q[4] = {
            v3_sub(v3_sub(p->pos, r), u),
            v3_add(v3_sub(p->pos, r), u),
            v3_add(v3_add(p->pos, r), u),
            v3_sub(v3_add(p->pos, r), u)
        };
        float uvs[4][2] = {{0,1},{0,0},{1,0},{1,1}};
        uint32_t col = p->color;
        float cr = ((col      ) & 0xFF)/255.0f, cg = ((col >>  8) & 0xFF)/255.0f;
        float cb = ((col >> 16) & 0xFF)/255.0f, ca = ((col >> 24) & 0xFF)/255.0f * lf;
        if (ca <= 0.01f) continue;
        SV sv[4]; int ok = 1;
        for (int k = 0; k < 4; k++) {
            const float *m = s_mvp.m;
            float cx = m[0]*q[k].x + m[4]*q[k].y + m[8]*q[k].z  + m[12];
            float cy = m[1]*q[k].x + m[5]*q[k].y + m[9]*q[k].z  + m[13];
            float cz = m[2]*q[k].x + m[6]*q[k].y + m[10]*q[k].z + m[14];
            float cw = m[3]*q[k].x + m[7]*q[k].y + m[11]*q[k].z + m[15];
            if (!(cw > 1e-4f)) { ok = 0; break; }
            float iw = 1.0f / cw;
            sv[k].x = (cx*iw*0.5f + 0.5f) * (float)s_w;
            sv[k].y = (0.5f - cy*iw*0.5f) * (float)s_h;
            sv[k].iw = iw; sv[k].zd = (cz*iw*0.5f+0.5f) * iw;
            sv[k].r = cr*iw; sv[k].g = cg*iw; sv[k].b = cb*iw; sv[k].a = ca*iw;
            sv[k].u = uvs[k][0]*iw; sv[k].v = uvs[k][1]*iw;
        }
        if (!ok) continue;
        raster_tri(&sv[0], &sv[1], &sv[2], tex, 1, 0, 0, 1, NULL);
        raster_tri(&sv[0], &sv[2], &sv[3], tex, 1, 0, 0, 1, NULL);
    }
    rend()->draw_calls++;
}

/* ── 2D quads (UI, HUD, subtitles, menus) ──────────────────────────────── */
static void draw_quad2d(const RenderItem *it) {
    const Texture *tex = tex_get(it->tex);
    if (it->sw <= 0.0f || it->sh <= 0.0f) return;
    int x0 = (int)floorf(it->sx), y0 = (int)floorf(it->sy);
    int x1 = (int)ceilf(it->sx + it->sw), y1 = (int)ceilf(it->sy + it->sh);
    int cx0 = 0, cy0 = 0, cx1 = s_w, cy1 = s_h;
    if (it->scissor[2] > 0.0f) {
        cx0 = (int)it->scissor[0]; cy0 = (int)it->scissor[1];
        cx1 = cx0 + (int)it->scissor[2]; cy1 = cy0 + (int)it->scissor[3];
    }
    if (x0 < cx0) x0 = cx0;
    if (y0 < cy0) y0 = cy0;
    if (x1 > cx1) x1 = cx1;
    if (y1 > cy1) y1 = cy1;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > s_w) x1 = s_w;
    if (y1 > s_h) y1 = s_h;
    if (x0 >= x1 || y0 >= y1) return;
    float cr = ((it->color      ) & 0xFF) / 255.0f;
    float cg = ((it->color >>  8) & 0xFF) / 255.0f;
    float cb = ((it->color >> 16) & 0xFF) / 255.0f;
    float ca = ((it->color >> 24) & 0xFF) / 255.0f;
    float invw = 1.0f / it->sw, invh = 1.0f / it->sh;
    float du = it->u1 - it->u0, dv = it->v1 - it->v0;
    for (int y = y0; y < y1; y++) {
        float tv = it->v0 + dv * (((float)y + 0.5f - it->sy) * invh);
        for (int x = x0; x < x1; x++) {
            float tu = it->u0 + du * (((float)x + 0.5f - it->sx) * invw);
            float r = cr, g = cg, b = cb, a = ca;
            if (tex) {
                uint32_t s = tex_sample_bilinear(tex, tu, tv, TEX_CLAMP);
                r *= ( s        & 0xFF) / 255.0f;
                g *= ((s >>  8) & 0xFF) / 255.0f;
                b *= ((s >> 16) & 0xFF) / 255.0f;
                a *= ((s >> 24) & 0xFF) / 255.0f;
            }
            if (a <= 0.004f) continue;
            uint32_t col = ((uint32_t)dh_clampi((int)(b*255.0f+0.5f),0,255) << 16)
                         | ((uint32_t)dh_clampi((int)(g*255.0f+0.5f),0,255) << 8)
                         |  (uint32_t)dh_clampi((int)(r*255.0f+0.5f),0,255);
            if (a >= 0.995f) put_opaque(x, y, col); else put_blend(x, y, col, a);
        }
    }
    rend()->draw_calls++;
}

/* ── command-list dispatch ─────────────────────────────────────────────── */
/* Walks the sorted item list produced by rend_end_frame(). Sorting already
   put opaque items first (near→far) and 2D quads last in submission order,
   so a single linear pass here is correct for both backends. */
static void draw_all_items(void) {
    int n = 0;
    RenderItem *items = rend_items(&n);
    RendState *st = rend();
    double t0 = dh_now_sec();
    for (int i = 0; i < n; i++) {
        const RenderItem *it = &items[i];
        switch (it->kind) {
            case RI_MESH:      draw_mesh_item(it);       break;
            case RI_LINES:     draw_lines_item(it);      break;
            case RI_QUAD2D:    draw_quad2d(it);          break;
            case RI_PARTICLES: draw_particles_item(it);  break;
        }
    }
    st->cpu_ms = (dh_now_sec() - t0) * 1000.0;
    st->gpu_ms = 0.0;                       /* no GPU on this path, honestly */
}

void rend_be_present(void) {
    draw_all_items();
    rend_apply_post();
}
