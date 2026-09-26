/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — OpenGL 1.1 fixed-function backend (M1b)
   Same rend.h command list as soft.c, executed on any GPU with baseline
   OpenGL 1.1 (i.e. every Windows PC since 2001 and every Intel/AMD/Radeon
   driver shipping with it). No extensions required, no VBOs (GL 1.1 core),
   client-side vertex arrays only — which is also why this backend needs no
   driver-specific code paths and cannot fail to create a context on a
   "low-end" machine (Spec 15.1 target).

   Honesty notes (Spec 1):
     · Hemisphere fill is folded into GL ambient (FF has no hemi term).
     · Fog is GL linear distance fog; soft.c uses smoothstep. Close, not
       identical — both honour fog_near/fog_far from settings.
     · rend_apply_post() (colourblind/brightness) operates on the readback
       framebuffer, so on GL it reaches *captures* immediately and the screen
       one frame later (the readback below feeds g_rend.fb). A fullscreen
       post quad is M7 polish.
   ══════════════════════════════════════════════════════════════════════════ */
#ifdef _WIN32

#include <windows.h>
#include <GL/gl.h>

#include "rend.h"
#include "../core/dh_log.h"

#include <stdlib.h>
#include <string.h>

static HDC   s_hdc;
static int   s_w, s_h;
static Mat4  s_view, s_proj;
static Vec3  s_cam;
static SceneLight s_light;
static uint32_t *s_readback;          /* g_rend.fb target for screenshots */

/* 0xAABBGGRR → GL floats */
static void col_f(uint32_t c, float *r, float *g, float *b, float *a) {
    *r = (float)(c & 0xFF) / 255.0f;
    *g = (float)((c >> 8) & 0xFF) / 255.0f;
    *b = (float)((c >> 16) & 0xFF) / 255.0f;
    *a = (float)((c >> 24) & 0xFF) / 255.0f;
}

/* ── textures: upload lazily, mip chain included ───────────────────────── */
static void ensure_tex(int id) {
    const Texture *t = tex_get(id);
    if (!t || t->gl_id) return;
    GLuint gl;
    glGenTextures(1, &gl);
    glBindTexture(GL_TEXTURE_2D, gl);
    int levels = 0;
    for (int i = 0; i < TEX_MIP_LEVELS; i++) {
        if (!t->mip[i] || t->mipw[i] <= 0 || t->miph[i] <= 0) break;
        glTexImage2D(GL_TEXTURE_2D, i, GL_RGBA8, t->mipw[i], t->miph[i], 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, t->mip[i]);
        levels = i + 1;
    }
    if (levels == 0) {
        static const uint32_t white = 0xFFFFFFFFu;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, &white);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    levels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    ((Texture *)t)->gl_id = (int)gl;
    RendState *r = rend();
    r->texture_mem_kb += (t->w * t->h * 4) / 1024 * 4 / 3;
}

static void bind_tex(int id) {
    if (id < 0) { glDisable(GL_TEXTURE_2D); return; }
    ensure_tex(id);
    const Texture *t = tex_get(id);
    if (!t || !t->gl_id) { glDisable(GL_TEXTURE_2D); return; }
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, (GLuint)t->gl_id);
}

/* ── lifecycle ─────────────────────────────────────────────────────────── */
int rend_be_init(int w, int h) {
    s_w = w; s_h = h;
    free(s_readback);
    s_readback = (uint32_t *)malloc((size_t)w * h * 4);
    RendState *r = rend();
    r->fb = s_readback; r->zbuf = NULL; r->w = w; r->h = h;

    glViewport(0, 0, w, h);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);          /* meshes self-report winding; play safe */
    glShadeModel(GL_SMOOTH);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glDisable(GL_LIGHT1);
    glEnable(GL_NORMALIZE);
    glClearColor(0.62f, 0.72f, 0.85f, 1.0f);
    DH_INFO("gl11", "OpenGL 1.1 backend online: %dx%d, vendor=%s renderer=%s",
            w, h, glGetString(GL_VENDOR), glGetString(GL_RENDERER));
    return 1;
}

void rend_be_shutdown(void) {
    free(s_readback); s_readback = NULL;
    RendState *r = rend(); r->fb = NULL;
}

void rend_be_resize(int w, int h) {
    s_w = w; s_h = h;
    free(s_readback);
    s_readback = (uint32_t *)malloc((size_t)w * h * 4);
    RendState *r = rend(); r->fb = s_readback; r->w = w; r->h = h;
    glViewport(0, 0, w, h);
}

int  rend_gl_create(void *hdc) { s_hdc = (HDC)hdc; return hdc ? 1 : 0; }
void rend_gl_destroy(void)    { s_hdc = NULL; }
void rend_gl_swap(void)       { if (s_hdc) SwapBuffers(s_hdc); }

/* ── frame begin: clear + fixed matrices + scene light ─────────────────── */
void rend_be_frame(const Mat4 *view, const Mat4 *proj, Vec3 cam_pos,
                   const SceneLight *light) {
    s_view = *view; s_proj = *proj; s_cam = cam_pos;
    if (light) s_light = *light;

    float fr, fg, fb, fa;
    col_f(0xFF000000u | ((uint32_t)(s_light.fog_color.z * 255) << 16) |
          ((uint32_t)(s_light.fog_color.y * 255) << 8) |
          (uint32_t)(s_light.fog_color.x * 255), &fr, &fg, &fb, &fa);
    glClearColor(fr, fg, fb, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(proj->m);
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(view->m);

    /* sun as a directional light; hemisphere fill folded into ambient */
    GLfloat lp[4] = { -s_light.sun_dir.x, -s_light.sun_dir.y, -s_light.sun_dir.z, 0.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, lp);
    GLfloat dif[4] = { s_light.sun_color.x * s_light.sun_intensity,
                       s_light.sun_color.y * s_light.sun_intensity,
                       s_light.sun_color.z * s_light.sun_intensity, 1.0f };
    glLightfv(GL_LIGHT0, GL_DIFFUSE, dif);
    GLfloat amb[4] = { s_light.ambient.x + (s_light.hemi_sky.x + s_light.hemi_ground.x) * 0.25f,
                       s_light.ambient.y + (s_light.hemi_sky.y + s_light.hemi_ground.y) * 0.25f,
                       s_light.ambient.z + (s_light.hemi_sky.z + s_light.hemi_ground.z) * 0.25f,
                       1.0f };
    glLightfv(GL_LIGHT0, GL_AMBIENT, amb);
    GLfloat spe[4] = { 0, 0, 0, 1 };
    glLightfv(GL_LIGHT0, GL_SPECULAR, spe);

    if (s_light.fog_enabled) {
        glEnable(GL_FOG);
        GLfloat fc[4] = { fr, fg, fb, 1.0f };
        glFogfv(GL_FOG_COLOR, fc);
        glFogi(GL_FOG_MODE, GL_LINEAR);
        glFogf(GL_FOG_START, s_light.fog_near);
        glFogf(GL_FOG_END,   s_light.fog_far);
    } else {
        glDisable(GL_FOG);
    }
}

/* ── item drawing ──────────────────────────────────────────────────────── */
static void draw_mesh_item(const RenderItem *it) {
    const Mesh *m = mesh_get(it->mesh);
    if (!m || !m->pos || !m->idx || m->icount <= 0) return;

    Mat4 model = it->model;
    if (it->billboard) {
        /* camera-facing: rotation = transpose(view 3x3), keep translation */
        Mat4 b;
        memset(&b, 0, sizeof(b));
        b.m[0] = s_view.m[0]; b.m[1] = s_view.m[4]; b.m[2] = s_view.m[8];
        b.m[4] = s_view.m[1]; b.m[5] = s_view.m[5]; b.m[6] = s_view.m[9];
        b.m[8] = s_view.m[2]; b.m[9] = s_view.m[6]; b.m[10] = s_view.m[10];
        b.m[12] = it->model.m[12]; b.m[13] = it->model.m[13]; b.m[14] = it->model.m[14];
        b.m[15] = 1.0f;
        model = b;
    }

    glPushMatrix();
    glMultMatrixf(model.m);

    /* Culling stays off: meshes self-report winding (Mesh.winding), and a
       wrong GL front-face guess would hide whole procedural meshes. */
    (void)(m->flags & MESH_TWO_SIDED);

    int lit = it->lit && !(m->flags & MESH_UNLIT);
    if (lit) glEnable(GL_LIGHTING); else glDisable(GL_LIGHTING);
    if (it->fogged && !(m->flags & MESH_NO_FOG)) glEnable(GL_FOG); else glDisable(GL_FOG);

    float r, g, b, a;
    uint32_t c = it->color ? it->color : 0xFFFFFFFFu;
    col_f(c, &r, &g, &b, &a);
    a *= it->alpha;
    glColor4f(r, g, b, a);

    if (a < 0.999f) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(it->depth_write ? GL_TRUE : GL_FALSE);
    } else {
        glDisable(GL_BLEND);
        glDepthMask(it->depth_write ? GL_TRUE : GL_FALSE);
    }
    glDepthFunc(GL_LEQUAL);
    if (!it->depth_test) glDisable(GL_DEPTH_TEST); else glEnable(GL_DEPTH_TEST);

    if (it->alpha_test || (m->flags & MESH_ALPHA_TEST)) {
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GEQUAL, 0.5f);
    } else {
        glDisable(GL_ALPHA_TEST);
    }

    int tex = it->tex >= 0 ? it->tex : m->tex;
    bind_tex(tex);

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, m->pos);
    if (m->nrm) { glEnableClientState(GL_NORMAL_ARRAY); glNormalPointer(GL_FLOAT, 0, m->nrm); }
    else glDisableClientState(GL_NORMAL_ARRAY);
    if (m->uv && tex >= 0) { glEnableClientState(GL_TEXTURE_COORD_ARRAY); glTexCoordPointer(2, GL_FLOAT, 0, m->uv); }
    else glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    glDrawElements(GL_TRIANGLES, m->icount, GL_UNSIGNED_INT, m->idx);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisable(GL_ALPHA_TEST);
    glDepthMask(GL_TRUE);
    glPopMatrix();

    RendState *rs = rend();
    rs->draw_calls++;
    rs->tris_drawn += m->tris;
}

static void draw_lines_item(const RenderItem *it) {
    if (!it->lines || it->line_count <= 0) return;
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    float r, g, b, a; col_f(it->color, &r, &g, &b, &a);
    glColor4f(r, g, b, a);
    glLineWidth(it->line_width > 0.0f ? it->line_width : 1.0f);
    glBegin(GL_LINES);
    for (int i = 0; i < it->line_count * 2; i++)
        glVertex3f(it->lines[i].x, it->lines[i].y, it->lines[i].z);
    glEnd();
    RendState *rs = rend();
    rs->draw_calls++;
    rs->tris_drawn += it->line_count * 2;
}

static void draw_quad2d(const RenderItem *it) {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    /* origin top-left, +y down — matches rend_quad2d's contract */
    glOrtho(0.0, (GLdouble)s_w, (GLdouble)s_h, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    float r, g, b, a; col_f(it->color, &r, &g, &b, &a);
    a *= it->alpha;
    if (a < 0.999f) { glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); }
    else glDisable(GL_BLEND);

    if (it->scissor[2] > 0.0f && it->scissor[3] > 0.0f) {
        glEnable(GL_SCISSOR_TEST);
        glScissor((int)it->scissor[0], (int)(s_h - it->scissor[1] - it->scissor[3]),
                  (int)it->scissor[2], (int)it->scissor[3]);
    } else glDisable(GL_SCISSOR_TEST);

    bind_tex(it->tex);
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glTexCoord2f(it->u0, it->v0); glVertex2f(it->sx,          it->sy);
    glTexCoord2f(it->u1, it->v0); glVertex2f(it->sx + it->sw, it->sy);
    glTexCoord2f(it->u1, it->v1); glVertex2f(it->sx + it->sw, it->sy + it->sh);
    glTexCoord2f(it->u0, it->v1); glVertex2f(it->sx,          it->sy + it->sh);
    glEnd();
    glDisable(GL_SCISSOR_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    RendState *rs = rend();
    rs->draw_calls++;
    rs->tris_drawn += 2;
}

static void draw_particles(const RenderItem *it) {
    if (!it->parts || it->part_count <= 0) return;
    Vec3 right = v3(s_view.m[0], s_view.m[4], s_view.m[8]);
    Vec3 up    = v3(s_view.m[1], s_view.m[5], s_view.m[9]);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    bind_tex(it->tex);
    for (int i = 0; i < it->part_count; i++) {
        const Particle *p = &it->parts[i];
        float life_k = p->max_life > 0.0f ? p->life / p->max_life : 1.0f;
        float r, g, b, a; col_f(p->color, &r, &g, &b, &a);
        a *= life_k;
        float sz = p->size > 0.0f ? p->size : it->part_size;
        glColor4f(r, g, b, a);
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0);
        glVertex3f(p->pos.x - right.x * sz - up.x * sz, p->pos.y - right.y * sz - up.y * sz, p->pos.z - right.z * sz - up.z * sz);
        glTexCoord2f(1, 0);
        glVertex3f(p->pos.x + right.x * sz - up.x * sz, p->pos.y + right.y * sz - up.y * sz, p->pos.z + right.z * sz - up.z * sz);
        glTexCoord2f(1, 1);
        glVertex3f(p->pos.x + right.x * sz + up.x * sz, p->pos.y + right.y * sz + up.y * sz, p->pos.z + right.z * sz + up.z * sz);
        glTexCoord2f(0, 1);
        glVertex3f(p->pos.x - right.x * sz + up.x * sz, p->pos.y - right.y * sz + up.y * sz, p->pos.z - right.z * sz + up.z * sz);
        glEnd();
    }
    glDepthMask(GL_TRUE);
    RendState *rs = rend();
    rs->draw_calls++;
    rs->tris_drawn += it->part_count * 2;
}

/* ── present: execute the command list, then read back for captures ────── */
void rend_be_present(void) {
    int count = 0;
    RenderItem *items = rend_items(&count);
    RendState *rs = rend();
    rs->items = count;

    /* opaque first (rend.c already sorted), then transparent — the list
       arrives pre-sorted, so a single pass is correct */
    for (int i = 0; i < count; i++) {
        switch (items[i].kind) {
        case RI_MESH:      draw_mesh_item(&items[i]); break;
        case RI_LINES:     draw_lines_item(&items[i]); break;
        case RI_QUAD2D:    draw_quad2d(&items[i]); break;
        case RI_PARTICLES: draw_particles(&items[i]); break;
        }
    }
    glDisable(GL_BLEND);
    glFlush();

    /* readback so screenshots + the post pass keep working on GL (Spec 60) */
    if (s_readback) {
        glReadPixels(0, 0, s_w, s_h, GL_RGBA, GL_UNSIGNED_BYTE, s_readback);
        /* GL origin is bottom-left; g_rend.fb is top-left */
        uint32_t *tmp = (uint32_t *)malloc((size_t)s_w * 4);
        if (tmp) {
            for (int y = 0; y * 2 < s_h; y++) {
                memcpy(tmp, s_readback + (size_t)y * s_w, (size_t)s_w * 4);
                memcpy(s_readback + (size_t)y * s_w,
                       s_readback + (size_t)(s_h - 1 - y) * s_w, (size_t)s_w * 4);
                memcpy(s_readback + (size_t)(s_h - 1 - y) * s_w, tmp, (size_t)s_w * 4);
            }
            free(tmp);
        }
    }
}

#endif /* _WIN32 */
