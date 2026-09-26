/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — logging, filesystem, timing (portable C99 + Win32 hooks)
   ══════════════════════════════════════════════════════════════════════════ */
/* WHY: clock_gettime/usleep/gmtime_r are POSIX extensions hidden by strict
   -std=c99. Defining the feature macro keeps the Windows path untouched and
   stops the Linux verification build from silently falling back to 1-second
   clock() resolution — which would make every perf counter in the engine lie. */
#ifndef _WIN32
  #define _POSIX_C_SOURCE 200809L
#endif

#include "dh_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <io.h>
#else
  #include <unistd.h>
  #include <sys/time.h>
#endif

#define DH_ACTION_RING 64
#define DH_ACTION_LEN  128

static FILE *g_logfp = NULL;
static char  g_user_dir[512] = ".";
static char  g_data_dir[512] = ".";
static char  g_action_ring[DH_ACTION_RING][DH_ACTION_LEN];
static int   g_action_idx = 0;
static int   g_action_filled = 0;
static double g_t0 = 0.0;

double dh_now_sec(void) {
#ifdef _WIN32
    static LARGE_INTEGER freq = {0};
    LARGE_INTEGER c;
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
#endif
}
uint64_t dh_now_ms(void) { return (uint64_t)(dh_now_sec() * 1000.0); }
void dh_sleep_ms(int ms) {
#ifdef _WIN32
    Sleep((DWORD)(ms > 0 ? ms : 0));
#else
    /* nanosleep, not usleep: usleep was removed from POSIX.1-2008 and is not
       exposed under a strict _POSIX_C_SOURCE, so it would be an implicit
       declaration that silently breaks under a stricter toolchain. */
    struct timespec ts;
    if (ms <= 0) return;
    ts.tv_sec  = (time_t)(ms / 1000);
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
#endif
}

void dh_fs_set_dirs(const char *data_dir, const char *user_dir) {
    if (data_dir && *data_dir) dh_strcpy_safe(g_data_dir, sizeof(g_data_dir), data_dir);
    if (user_dir && *user_dir) dh_strcpy_safe(g_user_dir, sizeof(g_user_dir), user_dir);
}
const char *dh_fs_user_dir(void) { return g_user_dir; }
const char *dh_fs_data_dir(void) { return g_data_dir; }

void dh_fs_join(char *out, int outsz, const char *a, const char *b) {
#ifdef _WIN32
    const char sep = '\\';
#else
    const char sep = '/';
#endif
    if (!a || !*a) { dh_strcpy_safe(out, outsz, b ? b : ""); return; }
    int n = (int)strlen(a);
    char tmp[1024];
    dh_strcpy_safe(tmp, sizeof(tmp), a);
    if (n > 0 && tmp[n-1] != sep && tmp[n-1] != '/') { tmp[n] = sep; tmp[n+1] = '\0'; }
    snprintf(out, (size_t)outsz, "%s%s", tmp, b ? b : "");
}

int dh_fs_mkdirs(const char *path) {
    if (!path || !*path) return 0;
    char buf[1024];
    dh_strcpy_safe(buf, sizeof(buf), path);
    int len = (int)strlen(buf);
    if (len == 0) return 0;
    for (int i = 1; i < len; i++) {
        if (buf[i] == '/' || buf[i] == '\\') {
            char sv = buf[i]; buf[i] = '\0';
#ifdef _WIN32
            CreateDirectoryA(buf, NULL);
#else
            mkdir(buf, 0755);
#endif
            buf[i] = sv;
        }
    }
#ifdef _WIN32
    if (CreateDirectoryA(buf, NULL) || GetLastError() == ERROR_ALREADY_EXISTS) return 1;
    return 0;
#else
    mkdir(buf, 0755);
    return 1;   /* EEXIST is fine — we only care that the path exists after */
#endif
}

char *dh_fs_read_text(const char *path, size_t *out_len) {
    if (out_len) *out_len = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0 || sz > 256 * 1024 * 1024) { fclose(f); return NULL; }
    char *buf = (char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = '\0';
    if (out_len) *out_len = rd;
    return buf;
}
void *dh_fs_read_bin(const char *path, size_t *out_len) { return dh_fs_read_text(path, out_len); }

int dh_fs_write_text(const char *path, const char *data) {
    return dh_fs_write_bin(path, data, data ? strlen(data) : 0);
}
/* Write atomically via temp+rename — WHY: Spec 37.4 promises corrupted-save
   auto-repair; a half-written save must never replace the last good one. */
int dh_fs_write_bin(const char *path, const void *data, size_t len) {
    if (!path || !data) return 0;
    char tmp[1100];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return 0;
    size_t w = fwrite(data, 1, len, f);
    fflush(f);
#ifdef _WIN32
    _commit(_fileno(f));
#endif
    fclose(f);
    if (w != len) { remove(tmp); return 0; }
    remove(path);
    if (rename(tmp, path) != 0) { return 0; }
    return 1;
}
int dh_fs_exists(const char *path) {
    if (!path) return 0;
    struct stat st;
    return stat(path, &st) == 0;
}

/* ── logging ────────────────────────────────────────────────────────────── */
void dh_log_init(const char *user_dir) {
    g_t0 = dh_now_sec();
    if (user_dir && *user_dir) dh_strcpy_safe(g_user_dir, sizeof(g_user_dir), user_dir);
    dh_fs_mkdirs(g_user_dir);
    char path[1024];
    dh_fs_join(path, sizeof(path), g_user_dir, "crash-log.txt");
    /* Rolling: keep the previous log as .1 so a hard crash still has context. */
    char prev[1024];
    dh_fs_join(prev, sizeof(prev), g_user_dir, "crash-log.1.txt");
    remove(prev);
    rename(path, prev);
    g_logfp = fopen(path, "wb");
    if (g_logfp) {
        fprintf(g_logfp, "DIVIDED HORIZON v%d.%d.%d (%s) — session log\n",
                DH_VERSION_MAJOR, DH_VERSION_MINOR, DH_VERSION_PATCH, DH_BUILD_NAME);
        fprintf(g_logfp, "This file is written locally and never transmitted (Spec 35: zero telemetry).\n\n");
        fflush(g_logfp);
    }
}
void dh_log_shutdown(void) {
    if (g_logfp) { fclose(g_logfp); g_logfp = NULL; }
}
static const char *lv_name(DhLogLevel lv) {
    switch (lv) { case DH_LOG_DEBUG: return "DBG"; case DH_LOG_INFO: return "INF";
                  case DH_LOG_WARN: return "WRN"; default: return "ERR"; }
}
static DhLogLevel g_min_level = DH_LOG_INFO;

/* WHY: milestone verification runs produce a lot of DEBUG noise, but a player
   running the shipped exe should never see engine chatter on stdout. */
void dh_log_set_level(DhLogLevel lv) { g_min_level = lv; }
DhLogLevel dh_log_get_level(void) { return g_min_level; }

void dh_log(DhLogLevel lv, const char *tag, const char *fmt, ...) {
    if (lv < g_min_level) return;
    char msg[1024];
    va_list ap; va_start(ap, fmt); vsnprintf(msg, sizeof(msg), fmt, ap); va_end(ap);
    double t = dh_now_sec() - g_t0;
#ifndef DH_NO_STDOUT
    fprintf(stdout, "[%7.3f][%s][%s] %s\n", t, lv_name(lv), tag ? tag : "-", msg);
    fflush(stdout);
#endif
    if (g_logfp) {
        fprintf(g_logfp, "[%7.3f][%s][%s] %s\n", t, lv_name(lv), tag ? tag : "-", msg);
        if (lv >= DH_LOG_WARN) fflush(g_logfp);
    }
}
void dh_log_action(const char *fmt, ...) {
    char msg[DH_ACTION_LEN];
    va_list ap; va_start(ap, fmt); vsnprintf(msg, sizeof(msg), fmt, ap); va_end(ap);
    dh_strcpy_safe(g_action_ring[g_action_idx], DH_ACTION_LEN, msg);
    g_action_idx = (g_action_idx + 1) % DH_ACTION_RING;
    if (g_action_filled < DH_ACTION_RING) g_action_filled++;
    if (g_logfp) { fprintf(g_logfp, "  · %s\n", msg); }
}
/* Dump the last N player actions — Spec 37.4 wants human-readable context. */
void dh_log_dump_actions(FILE *f) {
    if (!f) return;
    fprintf(f, "\n--- last %d actions ---\n", g_action_filled);
    int start = (g_action_idx - g_action_filled + DH_ACTION_RING) % DH_ACTION_RING;
    for (int i = 0; i < g_action_filled; i++)
        fprintf(f, "  %s\n", g_action_ring[(start + i) % DH_ACTION_RING]);
}
