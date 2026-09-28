/* DIVIDED HORIZON — benchmark statistics (Spec 37.1). Pure C, testable.
   Frame times in ms → AVG / 1% low / 0.1% low FPS + a recommended preset. */
#ifndef DH_BENCH_H
#define DH_BENCH_H
typedef struct {
    char  name[32];
    int   frames;
    float avg_fps, low1_fps, low01_fps, worst_ms;
    int   max_draw_calls, max_tris;
} BenchResult;
/* sorts a private copy; ms array untouched */
void bench_compute(const float *ms, int n, BenchResult *r);
/* 0 LOW .. 3 ULTRA from the weakest scene's 1% low at the tested preset */
int  bench_recommend(const BenchResult *r, int n, int tested_preset);
int  bench_write_report(const char *path, const BenchResult *r, int n,
                        int preset, int w, int h, const char *backend);
#endif
