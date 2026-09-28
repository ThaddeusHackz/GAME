#include "bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static int cmp_desc(const void *a, const void *b) {
    float x = *(const float *)a, y = *(const float *)b;
    return (x < y) - (x > y);
}
/* "1% low" = average FPS of the slowest 1% of frames (industry convention) */
static float low_pct(const float *sorted_desc, int n, float pct) {
    int k = (int)(n * pct); if (k < 1) k = 1;
    double s = 0; for (int i = 0; i < k; i++) s += sorted_desc[i];
    double ms = s / k; return ms > 0 ? (float)(1000.0 / ms) : 0.f;
}
void bench_compute(const float *ms, int n, BenchResult *r) {
    r->frames = n; r->avg_fps = r->low1_fps = r->low01_fps = r->worst_ms = 0.f;
    if (n <= 0 || !ms) return;
    float *c = (float *)malloc(sizeof(float) * (size_t)n); if (!c) return;
    double s = 0; for (int i = 0; i < n; i++) { c[i] = ms[i] > 0.01f ? ms[i] : 0.01f; s += c[i]; }
    qsort(c, (size_t)n, sizeof(float), cmp_desc);
    r->avg_fps = (float)(1000.0 * n / s);
    r->low1_fps = low_pct(c, n, 0.01f);
    r->low01_fps = low_pct(c, n, 0.001f);
    r->worst_ms = c[0];
    free(c);
}
int bench_recommend(const BenchResult *r, int n, int tested) {
    float worst = 1e9f;
    for (int i = 0; i < n; i++) if (r[i].low1_fps < worst) worst = r[i].low1_fps;
    int rec = tested;
    if (worst < 45.f) rec = tested - 1;        /* stutters: step down */
    else if (worst > 110.f) rec = tested + 2;  /* huge headroom */
    else if (worst > 75.f) rec = tested + 1;
    return rec < 0 ? 0 : rec > 3 ? 3 : rec;
}
int bench_write_report(const char *path, const BenchResult *r, int n,
                       int preset, int w, int h, const char *backend) {
    static const char *pn[4] = { "Low", "Medium", "High", "Ultra" };
    FILE *f = fopen(path, "w"); if (!f) return 0;
    time_t t = time(NULL);
    fprintf(f, "DIVIDED HORIZON benchmark (Spec 37.1)\n");
    fprintf(f, "date: %s", ctime(&t));
    fprintf(f, "resolution %dx%d  preset %s  backend %s\n\n", w, h, pn[preset & 3], backend);
    fprintf(f, "%-18s %7s %9s %9s %10s %9s %10s\n", "scene", "frames", "avg fps", "1% low", "0.1% low", "worst ms", "max draws");
    for (int i = 0; i < n; i++)
        fprintf(f, "%-18s %7d %9.1f %9.1f %10.1f %9.2f %10d\n", r[i].name, r[i].frames,
                r[i].avg_fps, r[i].low1_fps, r[i].low01_fps, r[i].worst_ms, r[i].max_draw_calls);
    int rec = bench_recommend(r, n, preset);
    fprintf(f, "\nrecommended preset: %s\n", pn[rec]);
    fprintf(f, "note: frame time = simulation + render + present, measured on THIS machine.\n");
    fclose(f);
    return 1;
}
