/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — bitmap font (hand-authored 5×7 glyphs, 6×8 cells)
   Zero external assets (Spec 3): every pixel is in the table below, authored
   for this project. Crisp at scale 1, readable at 2–3 for HUD and menus, and
   it satisfies Spec 33 (scalable UI text) without a TTF dependency that would
   bloat the exe or break on a clean Windows install.
   ══════════════════════════════════════════════════════════════════════════ */
#ifndef DH_FONT_H
#define DH_FONT_H

#include "../core/dh_types.h"

#define FONT_FIRST  32
#define FONT_LAST   126
#define FONT_CELL_W 8
#define FONT_CELL_H 8
#define FONT_GLYPH_W 5
#define FONT_ADVANCE 6

void  font_init(void);
void  font_shutdown(void);
int   font_tex(void);              /* atlas texture id (128×64, RGBA) */
int   font_ready(void);

/* Screen-space text. x,y = top-left of first cell, in pixels. */
void  font_text(float x, float y, float scale, const char *s, uint32_t color);
void  font_text_fmt(float x, float y, float scale, uint32_t color, const char *fmt, ...);
/* Right-aligned / centred helpers for menus. */
void  font_text_right(float right_x, float y, float scale, const char *s, uint32_t color);
void  font_text_center(float cx, float y, float scale, const char *s, uint32_t color);
float font_width(const char *s, float scale);
float font_height(float scale);
/* Text with a dark drop shadow — the HUD default (Spec 33 legibility). */
void  font_text_shadow(float x, float y, float scale, const char *s, uint32_t color);

#endif /* DH_FONT_H */
