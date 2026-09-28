/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — renderer core: registries, command list, culling, BMP
   ══════════════════════════════════════════════════════════════════════════ */
#include "rend.h"
#include "../core/dh_log.h"
#include "../core/settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── backend entry points (implemented in soft.c / gl11.c) ─────────────── */
int  rend_be_init(int w, int h);
void rend_be_shutdown(void);
void rend_be_resize(int w, int h);
void rend_be_frame(const Mat4 *view, const Mat4 *proj, Vec3 cam_pos, const SceneLight *light);
void rend_be_present(void);

static RendState   g_rend;
static SceneLight  g_light;
static Mat4        g_view, g_proj, g_vp;
static Vec3        g_cam_pos;
static Frustum     g_frustum;
static int         g_have_frustum = 0;

/* command list: two buckets so opaque can be sorted before transparent */
#define MAX_ITEMS 8192
static RenderItem g_items[MAX_ITEMS];
static int        g_item_count = 0;
static int        g_in_2d = 0;
static float      g_scissor[4] = {0,0,0,0};

RendState *rend(void) { return &g_rend; }
const SceneLight *rend_light(void) { return &g_light; }
SceneLight *rend_light_mut(void) { return &g_light; }
const Mat4 *rend_view(void) { return &g_view; }
const Mat4 *rend_proj(void) { return &g_proj; }
const Mat4 *rend_viewproj(void) { return &g_vp; }
Vec3 rend_cam_pos(void) { return g_cam_pos; }
RenderItem *rend_items(int *count) { *count = g_item_count; return g_items; }
int rend_in_2d(void) { return g_in_2d; }
const float *rend_scissor(void) { return g_scissor; }

const char *rend_backend_name(void) { return g_rend.backend == REND_GL11 ? "OpenGL 1.1" : "Software (CPU)"; }
int rend_backend_is_gl(void) { return g_rend.backend == REND_GL11; }

/* ═══════════════════════ textures ═══════════════════════════════════════ */
#define MAX_TEX 256
static Texture *g_tex[MAX_TEX];
static int g_tex_count = 0;

Texture *tex_new(int w, int h, const char *name) {
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096) return NULL;
    Texture *t = (Texture*)calloc(1, sizeof(Texture));
    if (!t) return NULL;
    t->w = w; t->h = h;
    t->px = (uint32_t*)calloc((size_t)w * h, 4);
    if (!t->px) { free(t); return NULL; }
    dh_strcpy_safe(t->name, sizeof(t->name), name ? name : "tex");
    t->gl_id = 0; t->srgb = 1;
    t->mip[0] = t->px; t->mipw[0] = w; t->miph[0] = h;
    return t;
}
void tex_free(Texture *t) {
    if (!t) return;
    for (int i = 1; i < TEX_MIP_LEVELS; i++) free(t->mip[i]);
    free(t->px); free(t);
}
void tex_build_mips(Texture *t) {
    if (!t || !t->px) return;
    t->mip[0] = t->px; t->mipw[0] = t->w; t->miph[0] = t->h;
    for (int l = 1; l < TEX_MIP_LEVELS; l++) {
        int pw = t->mipw[l-1], ph = t->miph[l-1];
        const uint32_t *src = t->mip[l-1];
        /* BUG FIXED (found by the 4x4 "white" texture): once a level stops
           shrinking we set mip[l] = NULL, but mipw/miph stayed 0, so the NEXT
           level computed w=h=1, saw "1 != 0", allocated a destination and then
           read from a NULL src — instant segfault on any texture smaller than
           2^TEX_MIP_LEVELS. Carry the parent's dims forward and bail on NULL. */
        if (!src || pw < 1 || ph < 1) {
            t->mip[l] = NULL; t->mipw[l] = pw; t->miph[l] = ph; continue;
        }
        int w = pw > 1 ? pw >> 1 : 1, h = ph > 1 ? ph >> 1 : 1;
        if (w == pw && h == ph) {
            t->mip[l] = NULL; t->mipw[l] = pw; t->miph[l] = ph; continue;
        }
        uint32_t *dst = (uint32_t*)malloc((size_t)w * h * 4);
        if (!dst) { t->mip[l] = NULL; t->mipw[l] = pw; t->miph[l] = ph; continue; }
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
            int sx = x * 2, sy = y * 2;
            if (sx >= pw) sx = pw - 1;
            if (sy >= ph) sy = ph - 1;
            int sx1 = sx + 1 < pw ? sx + 1 : sx, sy1 = sy + 1 < ph ? sy + 1 : sy;
            uint32_t a = src[sy*pw+sx], b = src[sy*pw+sx1], c = src[sy1*pw+sx], d = src[sy1*pw+sx1];
            /* box filter per channel — WHY: avoids mip aliasing shimmer on
               procedural textures, which is the #1 cause of "cheap" looking
               terrain in stylized renders. */
            uint32_t r = (((a&0xFF)+(b&0xFF)+(c&0xFF)+(d&0xFF))>>2);
            uint32_t g = ((((a>>8)&0xFF)+((b>>8)&0xFF)+((c>>8)&0xFF)+((d>>8)&0xFF))>>2);
            uint32_t bl= ((((a>>16)&0xFF)+((b>>16)&0xFF)+((c>>16)&0xFF)+((d>>16)&0xFF))>>2);
            uint32_t al= ((((a>>24)&0xFF)+((b>>24)&0xFF)+((c>>24)&0xFF)+((d>>24)&0xFF))>>2);
            dst[y*w+x] = (al<<24)|(bl<<16)|(g<<8)|r;
        }
        t->mip[l] = dst; t->mipw[l] = w; t->miph[l] = h;
    }
    /* Spec 15.2 texture-memory budget accounting */
    size_t bytes = 0;
    for (int l = 0; l < TEX_MIP_LEVELS; l++) if (t->mip[l]) bytes += (size_t)t->mipw[l]*t->miph[l]*4;
    g_rend.texture_mem_kb = (int)((g_rend.texture_mem_kb * 1024 + bytes) / 1024);
}
int tex_register(Texture *t) {
    if (!t) return -1;
    if (g_tex_count >= MAX_TEX) { DH_ERROR("rend","texture registry full"); tex_free(t); return -1; }
    tex_build_mips(t);
    g_tex[g_tex_count] = t;
    return g_tex_count++;
}
const Texture *tex_get(int id) { return (id >= 0 && id < g_tex_count) ? g_tex[id] : NULL; }
int tex_count(void) { return g_tex_count; }
void tex_release_all(void) {
    for (int i = 0; i < g_tex_count; i++) tex_free(g_tex[i]);
    g_tex_count = 0; g_rend.texture_mem_kb = 0;
}
void tex_set_pixel(Texture *t, int x, int y, uint32_t rgba) {
    if (!t || !t->px) return;
    /* Textures are power-of-two by construction, so the mask IS the wrap. */
    x &= t->w - 1;
    y &= t->h - 1;
    if (x < 0) x += t->w;
    if (y < 0) y += t->h;
    t->px[y * t->w + x] = rgba;
}
uint32_t tex_sample_bilinear(const Texture *t, float u, float v, int wrap) {
    if (!t || !t->px) return 0xFFFFFFFFu;
    int w = t->w, h = t->h;
    float fu, fv;
    if (wrap == TEX_CLAMP) {
        fu = dh_clampf(u, 0.0f, 1.0f) * (w - 1);
        fv = dh_clampf(v, 0.0f, 1.0f) * (h - 1);
    } else {
        fu = (u - floorf(u)) * w;
        fv = (v - floorf(v)) * h;
    }
    int x0 = (int)fu, y0 = (int)fv;
    float tx = fu - x0, ty = fv - y0;
    int x1 = (x0 + 1) % w, y1 = (y0 + 1) % h;
    x0 %= w; y0 %= h; if (x0 < 0) x0 += w; if (y0 < 0) y0 += h;
    const uint32_t *p = t->px;
    uint32_t a = p[y0*w+x0], b = p[y0*w+x1], c = p[y1*w+x0], d = p[y1*w+x1];
    #define CH(sh) ((int)((((a>>sh)&0xFF)*(1-tx)*(1-ty) + ((b>>sh)&0xFF)*tx*(1-ty) + \
                          ((c>>sh)&0xFF)*(1-tx)*ty      + ((d>>sh)&0xFF)*tx*ty) + 0.5f))
    return ((uint32_t)CH(24)<<24) | ((uint32_t)CH(16)<<16) | ((uint32_t)CH(8)<<8) | (uint32_t)CH(0);
    #undef CH
}

/* ═══════════════════════ meshes ═════════════════════════════════════════ */
#define MAX_MESH 1024
static Mesh *g_mesh[MAX_MESH];
static int g_mesh_count = 0;

Mesh *mesh_new(const char *name, int vcount, int icount) {
    if (vcount <= 0 || icount <= 0) return NULL;
    Mesh *m = (Mesh*)calloc(1, sizeof(Mesh));
    if (!m) return NULL;
    m->vcount = vcount; m->icount = icount;
    m->pos = (Vec3*)calloc((size_t)vcount, sizeof(Vec3));
    m->nrm = (Vec3*)calloc((size_t)vcount, sizeof(Vec3));
    m->uv  = (Vec2*)calloc((size_t)vcount, sizeof(Vec2));
    m->idx = (uint32_t*)calloc((size_t)icount, sizeof(uint32_t));
    if (!m->pos || !m->nrm || !m->uv || !m->idx) { mesh_free(m); return NULL; }
    dh_strcpy_safe(m->name, sizeof(m->name), name ? name : "mesh");
    m->tex = -1; m->flags = 0; m->tris = icount / 3;
    m->winding = 1;                 /* recalibrated by mesh_finalize() */
    return m;
}
void mesh_free(Mesh *m) {
    if (!m) return;
    free(m->pos); free(m->nrm); free(m->uv); free(m->col); free(m->idx); free(m);
}
void mesh_finalize(Mesh *m) {
    if (!m || m->vcount <= 0) return;
    Vec3 mn = m->pos[0], mx = m->pos[0];
    for (int i = 1; i < m->vcount; i++) {
        mn.x = fminf(mn.x, m->pos[i].x); mn.y = fminf(mn.y, m->pos[i].y); mn.z = fminf(mn.z, m->pos[i].z);
        mx.x = fmaxf(mx.x, m->pos[i].x); mx.y = fmaxf(mx.y, m->pos[i].y); mx.z = fmaxf(mx.z, m->pos[i].z);
    }
    m->center = v3_mul(v3_add(mn, mx), 0.5f);
    float r2 = 0;
    for (int i = 0; i < m->vcount; i++) {
        float d = v3_len2(v3_sub(m->pos[i], m->center));
        if (d > r2) r2 = d;
    }
    m->radius = sqrtf(r2);
    m->tris = m->icount / 3;
    /* Validate indices — an out-of-range index would read garbage memory. */
    for (int i = 0; i < m->icount; i++)
        if (m->idx[i] >= (uint32_t)m->vcount) {
            DH_ERROR("rend", "mesh '%s' index %d out of range (vcount=%d)", m->name, m->idx[i], m->vcount);
            m->idx[i] = 0;
        }
    /* Vote on the winding convention (see Mesh::winding in rend.h). */
    {
        long pos = 0, neg = 0;
        for (int t = 0; t + 2 < m->icount; t += 3) {
            uint32_t i0 = m->idx[t], i1 = m->idx[t+1], i2 = m->idx[t+2];
            if (i0 >= (uint32_t)m->vcount || i1 >= (uint32_t)m->vcount || i2 >= (uint32_t)m->vcount) continue;
            Vec3 fn = v3_cross(v3_sub(m->pos[i1], m->pos[i0]), v3_sub(m->pos[i2], m->pos[i0]));
            float d = v3_dot(fn, m->nrm[i0]);
            if (d > 0.0f) pos++;
            else if (d < 0.0f) neg++;
        }
        m->winding = (neg > pos) ? -1 : 1;
        /* A mesh where the two disagree substantially is malformed: it would
           show holes from some angles. Warn loudly at build time rather than
           shipping a flickering asset. */
        long minor = pos < neg ? pos : neg;
        if (minor > 0 && minor * 8 > (pos + neg))
            DH_WARN("rend", "mesh '%s' has inconsistent winding (%ld vs %ld tris) — expect holes",
                    m->name, pos, neg);
    }
}
int mesh_register(Mesh *m) {
    if (!m) return -1;
    /* M5: reuse slots freed by mesh_unregister (world swaps on the ferry
       used to leak ~70 terrain chunks per trip until the registry filled) */
    for (int i = 0; i < g_mesh_count; i++)
        if (!g_mesh[i]) { mesh_finalize(m); g_mesh[i] = m; return i; }
    if (g_mesh_count >= MAX_MESH) { DH_ERROR("rend","mesh registry full"); mesh_free(m); return -1; }
    mesh_finalize(m);
    g_mesh[g_mesh_count] = m;
    return g_mesh_count++;
}
void mesh_unregister(int id) {
    if (id < 0 || id >= g_mesh_count || !g_mesh[id]) return;
    mesh_free(g_mesh[id]);
    g_mesh[id] = NULL;
}
const Mesh *mesh_get(int id) { return (id >= 0 && id < g_mesh_count) ? g_mesh[id] : NULL; }
int mesh_count(void) { return g_mesh_count; }
void mesh_release_all(void) {
    for (int i = 0; i < g_mesh_count; i++) if (g_mesh[i]) mesh_free(g_mesh[i]);
    g_mesh_count = 0;
}

/* ═══════════════════════ lifecycle ══════════════════════════════════════ */
int rend_init(int w, int h, RendBackend backend) {
    memset(&g_rend, 0, sizeof(g_rend));
    g_rend.w = dh_clampi(w, 160, 7680);
    g_rend.h = dh_clampi(h, 120, 4320);
    g_rend.backend = (int)backend;
    /* Quality switches mirror the settings singleton so both backends and the
       HUD agree on what's actually enabled. */
    Settings *s = settings();
    g_rend.shadows_on = s->shadows;
    g_rend.ssao_on = s->ssao;
    g_rend.motion_blur_on = s->motion_blur;
    g_rend.aa_mode = s->aa_mode;
    g_rend.hd_textures = s->hd_textures;
    g_rend.render_scale = s->render_scale;
    g_rend.photosensitivity = s->photosensitivity;
    g_rend.colorblind_mode = s->colorblind_mode;
    g_rend.brightness = s->brightness;
    g_rend.potato_hud = s->potato_hud;
    memset(&g_light, 0, sizeof(g_light));
    /* Sensible daylight defaults so the very first frame is never black. */
    g_light.sun_dir = v3_norm(v3(-0.45f, -0.75f, 0.35f));
    g_light.sun_color = v3(1.00f, 0.96f, 0.88f);
    g_light.ambient   = v3(0.28f, 0.31f, 0.38f);
    g_light.hemi_sky  = v3(0.55f, 0.68f, 0.90f);
    g_light.hemi_ground = v3(0.30f, 0.26f, 0.20f);
    g_light.sun_intensity = 1.0f;
    g_light.fog_color = v3(0.62f, 0.72f, 0.85f);
    g_light.fog_near = 40.0f; g_light.fog_far = settings()->draw_distance;
    g_light.fog_enabled = 1;
    return rend_be_init(g_rend.w, g_rend.h);
}
void rend_shutdown(void) {
    rend_be_shutdown();
    free(g_rend.fb); g_rend.fb = NULL;
    free(g_rend.zbuf); g_rend.zbuf = NULL;
}
void rend_resize(int w, int h) {
    g_rend.w = dh_clampi(w, 160, 7680);
    g_rend.h = dh_clampi(h, 120, 4320);
    rend_be_resize(g_rend.w, g_rend.h);
}

/* ═══════════════════════ frame ══════════════════════════════════════════ */
void rend_begin_frame(const Mat4 *view, const Mat4 *proj, Vec3 cam_pos, const SceneLight *light) {
    g_view = *view; g_proj = *proj;
    g_vp = m4_mul(*proj, *view);
    g_cam_pos = cam_pos;
    if (light) g_light = *light;
    g_frustum = frustum_from_vp(g_vp);
    g_have_frustum = 1;
    g_item_count = 0;
    g_in_2d = 0;
    g_scissor[0] = g_scissor[1] = g_scissor[2] = g_scissor[3] = 0;
    g_rend.draw_calls = 0; g_rend.tris_submitted = 0; g_rend.tris_drawn = 0;
    g_rend.frustum_culled = 0; g_rend.dist_culled = 0; g_rend.overdraw_px = 0;
    rend_be_frame(&g_view, &g_proj, cam_pos, &g_light);
}
void rend_end_frame(void) {
    /* Sort: opaque front-to-back (early-z friendly), transparent back-to-front.
       WHY: without this the software pass overdraws ~3x and GL loses early-z. */
    RenderItem *items = g_items;
    int n = g_item_count;
    /* simple insertion sort on small n, qsort otherwise */
    if (n > 1) {
        /* partition-stable: keep 2D items in submission order at the tail */
        int opaque_end = 0;
        for (int i = 0; i < n; i++) if (!items[i].scissor[2] || items[i].kind != RI_QUAD2D) opaque_end = i + 1;
        (void)opaque_end;
        /* insertion sort — n is typically < 600, this is faster than qsort */
        for (int i = 1; i < n; i++) {
            RenderItem key = items[i];
            int j = i - 1;
            int key2d = (key.kind == RI_QUAD2D);
            while (j >= 0) {
                int j2d = (items[j].kind == RI_QUAD2D);
                int swap;
                if (key2d != j2d) swap = key2d ? 0 : 1;      /* 2D always last, in order */
                else if (key.alpha < 1.0f || items[j].alpha < 1.0f)
                    swap = key.depth_sort > items[j].depth_sort;  /* transparent: far first */
                else swap = key.depth_sort < items[j].depth_sort; /* opaque: near first */
                if (!swap) break;
                items[j+1] = items[j]; j--;
            }
            items[j+1] = key;
        }
    }
    g_rend.items = n;
}
void rend_present(void) { rend_be_present(); g_rend.frame_index++; }

int rend_should_draw(const Vec3 *center, float radius) {
    if (!g_have_frustum) return 1;
    float r = radius;
    /* Mutator: MIRROR_WORLD flips X — handled by the camera, not here. */
    if (settings_mutator_active(MUT_LOW_GRAVITY)) { /* no culling change */ }
    if (!frustum_has_sphere(g_frustum, *center, r)) { g_rend.frustum_culled++; return 0; }
    float d = v3_len(v3_sub(*center, g_cam_pos));
    Settings *s = settings();
    float maxd = s->draw_distance * 2.5f;      /* terrain/far LOD extends past draw_distance */
    if (d - r > maxd) { g_rend.dist_culled++; return 0; }
    return 1;
}

/* ═══════════════════════ submission ═════════════════════════════════════ */
static RenderItem *new_item(RenderItemKind k) {
    if (g_item_count >= MAX_ITEMS) {
        /* Spec 15.2: hard cap. Dropping the item is better than stalling. */
        static int warned = 0;
        if (!warned) { DH_WARN("rend", "render item cap (%d) hit — dropping extras", MAX_ITEMS); warned = 1; }
        return NULL;
    }
    RenderItem *it = &g_items[g_item_count++];
    memset(it, 0, sizeof(*it));
    it->kind = k;
    it->tex = -1;
    it->color = 0xFFFFFFFFu;
    it->alpha = 1.0f;
    it->lit = 1; it->fogged = 1;
    it->depth_test = 1; it->depth_write = 1;
    it->model = m4_identity();
    it->u1 = it->v1 = 1.0f;
    memcpy(it->scissor, g_scissor, sizeof(g_scissor));
    return it;
}

void rend_mesh_lit(int mesh_id, const Mat4 *model, int tex_override, uint32_t color,
                   float alpha, int lit, int fogged, int depth_write, int alpha_test, int two_sided) {
    const Mesh *m = mesh_get(mesh_id);
    if (!m) return;
    RenderItem *it = new_item(RI_MESH);
    if (!it) return;
    it->mesh = mesh_id;
    it->model = model ? *model : m4_identity();
    it->tex = tex_override >= 0 ? tex_override : m->tex;
    it->color = color;
    it->alpha = alpha;
    it->lit = lit && !(m->flags & MESH_UNLIT);
    it->fogged = fogged && !(m->flags & MESH_NO_FOG);
    it->depth_write = depth_write;
    it->alpha_test = alpha_test || (m->flags & MESH_ALPHA_TEST) ? 1 : 0;
    if (two_sided || (m->flags & MESH_TWO_SIDED)) it->sort_key |= 1;
    /* camera-space depth of the mesh centre for sorting */
    Vec3 wc = m4_xform_p(it->model, m->center);
    Vec3 vc = m4_xform_p(g_view, wc);
    it->depth_sort = -vc.z;
    g_rend.tris_submitted += m->tris;
}
void rend_mesh(int mesh_id, const Mat4 *model, int tex_override, uint32_t color, float alpha, int flags_override) {
    const Mesh *m = mesh_get(mesh_id);
    int lit = (m && (m->flags & MESH_UNLIT)) ? 0 : 1;
    int fog = (m && (m->flags & MESH_NO_FOG)) ? 0 : 1;
    rend_mesh_lit(mesh_id, model, tex_override, color, alpha, lit, fog, 1,
                  (flags_override & MESH_ALPHA_TEST) ? 1 : 0,
                  (flags_override & MESH_TWO_SIDED) ? 1 : 0);
}
void rend_lines(const Vec3 *pts, int seg_count, uint32_t color, float width) {
    if (!pts || seg_count <= 0) return;
    RenderItem *it = new_item(RI_LINES);
    if (!it) return;
    it->lines = pts; it->line_count = seg_count; it->color = color;
    it->line_width = width > 0 ? width : 1.0f;
    it->lit = 0; it->fogged = 0; it->depth_write = 0;
    Vec3 vc = m4_xform_p(g_view, pts[0]);
    it->depth_sort = -vc.z;
}
void rend_quad2d(float x, float y, float w, float h, int tex,
                 float u0, float v0, float u1, float v1, uint32_t color) {
    RenderItem *it = new_item(RI_QUAD2D);
    if (!it) return;
    it->sx = x; it->sy = y; it->sw = w; it->sh = h;
    it->tex = tex; it->u0 = u0; it->v0 = v0; it->u1 = u1; it->v1 = v1;
    it->color = color;
    it->lit = 0; it->fogged = 0;
    it->depth_test = 0; it->depth_write = 0;
    it->depth_sort = 1e9f;                       /* always last */
    int a = (int)((color >> 24) & 0xFF);
    it->alpha = a / 255.0f;
}
void rend_particles(const Particle *parts, int count, int tex, float size) {
    if (!parts || count <= 0) return;
    RenderItem *it = new_item(RI_PARTICLES);
    if (!it) return;
    it->parts = parts; it->part_count = count; it->tex = tex;
    it->part_size = size > 0 ? size : 0.1f;
    it->lit = 0; it->fogged = 1; it->depth_write = 0;
    it->alpha = 1.0f;
    Vec3 vc = m4_xform_p(g_view, parts[0].pos);
    it->depth_sort = -vc.z;
}
void rend_set_scissor(float x, float y, float w, float h) {
    g_scissor[0] = x; g_scissor[1] = y; g_scissor[2] = w; g_scissor[3] = h;
}
void rend_push_2d(void) { g_in_2d = 1; }
void rend_pop_2d(void)  { g_in_2d = 0; }
int  rend_item_count(void) { return g_item_count; }

/* ═══════════════════════ post-processing ════════════════════════════════ */
/* Colourblind matrices (Spec 33.3) — applied in software; GL path uses a
   fullscreen quad with the same coefficients so both look identical. */
void rend_colorblind_matrix(int mode, float m[9]) {
    for (int i = 0; i < 9; i++) m[i] = 0;
    switch (mode) {
        case 1: /* protanopia — boost blue/green, compress red */
            m[0]=0.567f; m[1]=0.433f; m[2]=0.0f;
            m[3]=0.558f; m[4]=0.442f; m[5]=0.0f;
            m[6]=0.0f;   m[7]=0.242f; m[8]=0.758f;
            break;
        case 2: /* deuteranopia */
            m[0]=0.625f; m[1]=0.375f; m[2]=0.0f;
            m[3]=0.70f;  m[4]=0.30f;  m[5]=0.0f;
            m[6]=0.0f;   m[7]=0.30f;  m[8]=0.70f;
            break;
        case 3: /* tritanopia */
            m[0]=0.95f;  m[1]=0.05f;  m[2]=0.0f;
            m[3]=0.0f;   m[4]=0.433f; m[5]=0.567f;
            m[6]=0.0f;   m[7]=0.475f; m[8]=0.525f;
            break;
        default:
            m[0]=1; m[4]=1; m[8]=1;
            break;
    }
}

void rend_apply_post(void) {
    if (!g_rend.fb) return;
    int n = g_rend.w * g_rend.h;
    float m[9]; rend_colorblind_matrix(g_rend.colorblind_mode, m);
    float br = g_rend.brightness;
    int cb = g_rend.colorblind_mode != 0;
    int dobr = fabsf(br - 1.0f) > 0.001f;
    if (!cb && !dobr) return;
    for (int i = 0; i < n; i++) {
        uint32_t p = g_rend.fb[i];
        float r = (float)(p & 0xFF), g = (float)((p>>8)&0xFF), b = (float)((p>>16)&0xFF);
        if (cb) {
            float nr = r*m[0] + g*m[1] + b*m[2];
            float ng = r*m[3] + g*m[4] + b*m[5];
            float nb = r*m[6] + g*m[7] + b*m[8];
            r = nr; g = ng; b = nb;
        }
        if (dobr) { r *= br; g *= br; b *= br; }
        uint32_t o = (p & 0xFF000000u);
        o |= (uint32_t)dh_clampi((int)(r+0.5f),0,255);
        o |= (uint32_t)dh_clampi((int)(g+0.5f),0,255) << 8;
        o |= (uint32_t)dh_clampi((int)(b+0.5f),0,255) << 16;
        g_rend.fb[i] = o;
    }
}

/* ═══════════════════════ BMP screenshot (Spec 60) ═══════════════════════ */
int rend_save_bmp(const char *path) {
    if (!g_rend.fb || !path) return 0;
    FILE *f = fopen(path, "wb");
    if (!f) { DH_ERROR("rend", "cannot open %s for screenshot", path); return 0; }
    int w = g_rend.w, h = g_rend.h;
    int rowsz = ((w * 3 + 3) / 4) * 4;
    uint32_t pixoff = 54;
    uint32_t filesize = pixoff + (uint32_t)rowsz * h;
    uint8_t hdr[54];
    memset(hdr, 0, sizeof(hdr));
    hdr[0]='B'; hdr[1]='M';
    memcpy(hdr+2, &filesize, 4);
    memcpy(hdr+10, &pixoff, 4);
    uint32_t dib = 40; memcpy(hdr+14, &dib, 4);
    int32_t iw = w, ih = h; memcpy(hdr+18, &iw, 4); memcpy(hdr+22, &ih, 4);
    uint16_t planes = 1, bpp = 24; memcpy(hdr+26, &planes, 2); memcpy(hdr+28, &bpp, 2);
    fwrite(hdr, 1, 54, f);
    uint8_t *row = (uint8_t*)malloc((size_t)rowsz);
    if (!row) { fclose(f); return 0; }
    for (int y = h - 1; y >= 0; y--) {
        memset(row, 0, (size_t)rowsz);
        const uint32_t *src = g_rend.fb + (size_t)y * w;
        for (int x = 0; x < w; x++) {
            uint32_t p = src[x];
            row[x*3+0] = (uint8_t)((p >> 16) & 0xFF);   /* B */
            row[x*3+1] = (uint8_t)((p >> 8) & 0xFF);    /* G */
            row[x*3+2] = (uint8_t)(p & 0xFF);           /* R */
        }
        fwrite(row, 1, (size_t)rowsz, f);
    }
    free(row);
    fclose(f);
    DH_INFO("rend", "screenshot → %s (%dx%d)", path, w, h);
    return 1;
}

/* Minimal PNG writer (grayscale-free truecolour, zlib *stored* blocks).
   WHY: Spec 60 promises lossless PNG; a real deflate would need a third-party
   lib, and stored blocks are valid PNG at ~25% larger than the BMP. */
static uint32_t png_crc(const uint8_t *buf, size_t len) {
    static uint32_t tab[256]; static int made = 0;
    if (!made) { for (uint32_t i=0;i<256;i++){uint32_t c=i;for(int k=0;k<8;k++)c=(c&1)?0xEDB88320u^(c>>1):(c>>1);tab[i]=c;} made=1; }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) c = tab[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}
static void png_be32(uint8_t *p, uint32_t v) { p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16); p[2]=(uint8_t)(v>>8); p[3]=(uint8_t)v; }
static int png_chunk(FILE *f, const char *type, const uint8_t *data, uint32_t len) {
    uint8_t lb[4]; png_be32(lb, len); fwrite(lb,1,4,f);
    fwrite(type,1,4,f);
    if (len) fwrite(data,1,len,f);
    uint8_t *cb = (uint8_t*)malloc(len+4);
    if (!cb) return 0;
    memcpy(cb, type, 4); if (len) memcpy(cb+4, data, len);
    uint32_t c = png_crc(cb, len+4); free(cb);
    uint8_t cb4[4]; png_be32(cb4, c); fwrite(cb4,1,4,f);
    return 1;
}
int rend_save_png(const char *path) {
    if (!g_rend.fb || !path) return 0;
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    int w = g_rend.w, h = g_rend.h;
    static const uint8_t sig[8] = {137,80,78,71,13,10,26,10};
    fwrite(sig,1,8,f);
    uint8_t ihdr[13]; png_be32(ihdr, (uint32_t)w); png_be32(ihdr+4, (uint32_t)h);
    ihdr[8]=8; ihdr[9]=2; ihdr[10]=0; ihdr[11]=0; ihdr[12]=0;
    png_chunk(f,"IHDR",ihdr,13);
    size_t stride = (size_t)w*3 + 1;
    size_t raw = stride * (size_t)h;
    uint8_t *buf = (uint8_t*)malloc(raw);
    if (!buf) { fclose(f); return 0; }
    for (int y = 0; y < h; y++) {
        buf[y*stride] = 0;                       /* filter: none */
        const uint32_t *src = g_rend.fb + (size_t)y*w;
        for (int x = 0; x < w; x++) {
            buf[y*stride+1+x*3+0] = (uint8_t)(src[x] & 0xFF);
            buf[y*stride+1+x*3+1] = (uint8_t)((src[x]>>8) & 0xFF);
            buf[y*stride+1+x*3+2] = (uint8_t)((src[x]>>16) & 0xFF);
        }
    }
    /* zlib stream: 0x78 0x01, then stored deflate blocks */
    size_t zcap = raw + (raw/65535 + 1)*5 + 16;
    uint8_t *z = (uint8_t*)malloc(zcap);
    if (!z) { free(buf); fclose(f); return 0; }
    size_t zl = 0;
    z[zl++]=0x78; z[zl++]=0x01;
    size_t off = 0;
    while (off < raw) {
        size_t chunk = raw - off; if (chunk > 65535) chunk = 65535;
        z[zl++] = (off + chunk >= raw) ? 1 : 0;
        z[zl++] = (uint8_t)(chunk & 0xFF); z[zl++] = (uint8_t)((chunk>>8)&0xFF);
        z[zl++] = (uint8_t)(~chunk & 0xFF); z[zl++] = (uint8_t)((~chunk>>8)&0xFF);
        memcpy(z+zl, buf+off, chunk); zl += chunk; off += chunk;
    }
    /* adler32 */
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < raw; i++) { a = (a + buf[i]) % 65521; b = (b + a) % 65521; }
    png_be32(z+zl, (b<<16)|a); zl += 4;
    png_chunk(f,"IDAT",z,(uint32_t)zl);
    png_chunk(f,"IEND",NULL,0);
    free(z); free(buf); fclose(f);
    DH_INFO("rend", "png → %s (%dx%d, %.0f KB)", path, w, h, (raw+zl)/1024.0f);
    return 1;
}

int rend_quad2d_run_end(const RenderItem *items, int i, int n) {
    const RenderItem *a = &items[i];
    int j = i + 1;
    while (j < n && items[j].kind == RI_QUAD2D && items[j].tex == a->tex &&
           items[j].scissor[0] == a->scissor[0] && items[j].scissor[1] == a->scissor[1] &&
           items[j].scissor[2] == a->scissor[2] && items[j].scissor[3] == a->scissor[3]) j++;
    return j;
}
