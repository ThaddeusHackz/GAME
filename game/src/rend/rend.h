/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — renderer interface
   Two backends, one command list (Spec 88 "always playable" + Spec 15):
     • GL11  — OpenGL 1.1 fixed-function. opengl32.dll ships with every
               Windows since 2000 → the .exe runs on machines with no
               DirectX 11/12 and no vendor driver update.
     • SOFT  — CPU z-buffered rasterizer. Headless verification, CI smoke
               tests, and the "potato" fallback when no GL context exists.
   Both consume identical RenderItems, so the game code is backend-agnostic.
   ══════════════════════════════════════════════════════════════════════════ */
#ifndef DH_REND_H
#define DH_REND_H

#include "../core/dh_types.h"

/* ── textures (all procedural — zero external image files, Spec 3) ─────── */
#define TEX_MIP_LEVELS 5
typedef struct {
    int w, h;
    uint32_t *px;                     /* level 0, RGBA8 packed 0xAABBGGRR */
    uint32_t *mip[TEX_MIP_LEVELS];    /* mip[0] == px */
    int  mipw[TEX_MIP_LEVELS], miph[TEX_MIP_LEVELS];
    char name[32];
    int  gl_id;                       /* filled by GL backend */
    int  srgb;
} Texture;

typedef enum {
    TEX_REPEAT = 0, TEX_CLAMP = 1
} TexWrap;

Texture *tex_new(int w, int h, const char *name);
void     tex_free(Texture *t);
void     tex_build_mips(Texture *t);
int      tex_register(Texture *t);           /* returns id */
const Texture *tex_get(int id);
int      tex_count(void);
void     tex_release_all(void);
void     tex_set_pixel(Texture *t, int x, int y, uint32_t rgba);
uint32_t tex_sample_bilinear(const Texture *t, float u, float v, int wrap) ;

/* ── meshes ────────────────────────────────────────────────────────────── */
#define MESH_TWO_SIDED   (1<<0)
#define MESH_UNLIT       (1<<1)
#define MESH_NO_FOG      (1<<2)
#define MESH_VERTEXCOL   (1<<3)
#define MESH_ALPHA_TEST  (1<<4)     /* cutout (leaves, chain-link) */
#define MESH_DECAL       (1<<5)

typedef struct {
    char     name[32];
    int      vcount, icount;        /* icount = index count (multiple of 3) */
    Vec3    *pos;
    Vec3    *nrm;
    Vec2    *uv;
    uint32_t *col;                  /* optional vertex colours (RGBA) */
    uint32_t *idx;
    int      tex;                   /* default texture id, -1 = white */
    int      flags;
    Vec3     center;                /* local-space bounding sphere */
    float    radius;
    /* Self-calibrated winding convention, computed in mesh_finalize() by
       voting geometric face normals against authored vertex normals.
       +1 = triangles wound CCW-from-outside, -1 = CW-from-outside.
       WHY this exists: every mesh in this game is procedurally generated, so
       a hard-coded winding assumption would silently render an entire world
       invisible the moment one generator disagreed with it — and invisible
       geometry is the single worst bug to diagnose without a GPU profiler.
       The rasterizer tests `dot(face_normal, vertex_normal) * winding`, so any
       *consistently* wound mesh draws correctly under either convention. */
    int      winding;
    int      gl_vbo;                /* unused in GL1.1 path, kept for parity */
    int      tris;                  /* icount/3 — budget accounting (Spec 15.2) */
} Mesh;

Mesh *mesh_new(const char *name, int vcount, int icount);
void  mesh_free(Mesh *m);
void  mesh_finalize(Mesh *m);       /* compute bounds, tris, validate */
int   mesh_register(Mesh *m);
const Mesh *mesh_get(int id);
int   mesh_count(void);
void  mesh_release_all(void);
void  mesh_unregister(int id);   /* M5: free one mesh, slot is reused */

/* ── render items ──────────────────────────────────────────────────────── */
typedef enum {
    RI_MESH = 0, RI_LINES, RI_QUAD2D, RI_PARTICLES
} RenderItemKind;

typedef struct {
    RenderItemKind kind;
    int      mesh;                  /* mesh id (RI_MESH) */
    Mat4     model;
    int      tex;                   /* -1 → use mesh default */
    uint32_t color;                 /* modulate, 0xAABBGGRR */
    float    alpha;                 /* 1 = opaque */
    int      lit;                   /* apply scene lighting */
    int      fogged;
    int      depth_test, depth_write;
    int      billboard;             /* face camera (RI_MESH) */
    int      alpha_test;            /* discard < 0.5 (RI_MESH) */
    int      sort_key;              /* material bucket for batching */
    float    depth_sort;            /* camera-space depth, computed at submit */
    /* RI_LINES: world-space segment list (pairs) */
    const Vec3 *lines; int line_count; float line_width;
    /* RI_QUAD2D: screen-space, pixels, origin top-left */
    float sx, sy, sw, sh;
    float u0, v0, u1, v1;
    float scissor[4];               /* x,y,w,h or all zero */
    /* RI_PARTICLES */
    const struct Particle *parts; int part_count; float part_size;
} RenderItem;

typedef struct Particle {
    Vec3  pos, vel;
    float life, max_life, size;
    uint32_t color;
    int   kind;                     /* 0 spark, 1 smoke, 2 blood-lite, 3 leaf, 4 water */
} Particle;

/* ── scene lighting / atmosphere ───────────────────────────────────────── */
#define MAX_LIGHTS 8
typedef struct {
    Vec3  sun_dir;                      /* normalized, points FROM the sun */
    Vec3  sun_color;                    /* linear-ish 0..1 */
    Vec3  ambient;
    Vec3  hemi_sky, hemi_ground;    /* hemisphere fill */
    float sun_intensity;
    Vec3  fog_color;
    float fog_near, fog_far;
    int   fog_enabled;
    struct { Vec3 pos; Vec3 color; float radius, intensity; int on; } point[MAX_LIGHTS];
    int   point_count;
} SceneLight;

typedef enum { REND_SOFT = 0, REND_GL11 = 1 } RendBackend;

typedef struct {
    int w, h;
    int backend;
    uint32_t *fb;                   /* software backbuffer (NULL for GL) */
    float    *zbuf;
    int      frame_index;
    /* perf counters — Spec 15.2 budgets are enforced against these */
    int      draw_calls, tris_submitted, tris_drawn, items;
    int      overdraw_px;
    double   cpu_ms, gpu_ms;
    int      texture_mem_kb;
    int      frustum_culled, dist_culled, lod_swaps;
    float    fps, frame_ms;
    /* quality switches actually honoured by both backends */
    int      shadows_on, ssao_on, motion_blur_on, aa_mode, hd_textures;
    float    render_scale;
    int      photosensitivity;      /* clamps strobe/bloom/shake (Spec 33) */
    int      colorblind_mode;
    float    brightness;
    int      potato_hud;
} RendState;

/* ── lifecycle ─────────────────────────────────────────────────────────── */
int   rend_init(int w, int h, RendBackend backend);
void  rend_shutdown(void);
void  rend_resize(int w, int h);
RendState *rend(void);
const char *rend_backend_name(void);
int   rend_backend_is_gl(void);

/* Per-frame. `cam` is the world→clip matrix; `view` world→camera. */
void  rend_begin_frame(const Mat4 *view, const Mat4 *proj, Vec3 cam_pos, const SceneLight *light);
void  rend_end_frame(void);
void  rend_present(void);

/* ── submission ────────────────────────────────────────────────────────── */
void  rend_mesh(int mesh_id, const Mat4 *model, int tex_override, uint32_t color, float alpha, int flags_override);
void  rend_mesh_lit(int mesh_id, const Mat4 *model, int tex_override, uint32_t color,
                    float alpha, int lit, int fogged, int depth_write, int alpha_test, int two_sided);
void  rend_lines(const Vec3 *pts, int seg_count, uint32_t color, float width);
void  rend_quad2d(float x, float y, float w, float h, int tex,
                  float u0, float v0, float u1, float v1, uint32_t color);
void  rend_particles(const Particle *parts, int count, int tex, float size);
void  rend_set_scissor(float x, float y, float w, float h);   /* 0,0,0,0 clears */
void  rend_push_2d(void);      /* switch to screen-space ortho for UI */
void  rend_pop_2d(void);
int   rend_item_count(void);

/* Frustum + distance culling helper used by the world/scene code. */
int   rend_should_draw(const Vec3 *center, float radius);

/* ── accessors used by the backends and by gameplay (camera, lighting) ─── */
const SceneLight *rend_light(void);
SceneLight *rend_light_mut(void);
const Mat4 *rend_view(void);
const Mat4 *rend_proj(void);
const Mat4 *rend_viewproj(void);
Vec3        rend_cam_pos(void);
RenderItem *rend_items(int *count);
int         rend_in_2d(void);
const float *rend_scissor(void);
void        rend_colorblind_matrix(int mode, float m[9]);

/* ── screenshot (Spec 60 Photo Mode / clean capture) ───────────────────── */
int   rend_save_bmp(const char *path);
int   rend_save_png(const char *path);       /* minimal uncompressed-ish PNG */
/* Post: colourblind matrix + brightness, applied to fb (soft) or as overlay (GL). */
void  rend_apply_post(void);

/* ── GL backend hooks (implemented in gl11.c, no-op in soft builds) ────── */
int   rend_gl_create(void *hdc_or_display);  /* platform passes HDC */
void  rend_gl_destroy(void);
void  rend_gl_swap(void);

#endif /* DH_REND_H */
