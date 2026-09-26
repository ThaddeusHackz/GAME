/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — logging, filesystem, crash log (Spec 37.4)
   ══════════════════════════════════════════════════════════════════════════ */
#ifndef DH_LOG_H
#define DH_LOG_H

#include "dh_types.h"

typedef enum {
    DH_LOG_DEBUG = 0,
    DH_LOG_INFO  = 1,
    DH_LOG_WARN  = 2,
    DH_LOG_ERROR = 3
} DhLogLevel;

void dh_log_init(const char *user_dir);      /* opens crash-log.txt (rolling) */
void dh_log_set_level(DhLogLevel lv);        /* filter: DEBUG is dev-only */
DhLogLevel dh_log_get_level(void);
void dh_log_shutdown(void);
void dh_log(DhLogLevel lv, const char *tag, const char *fmt, ...);
void dh_log_action(const char *fmt, ...);    /* human-readable recent action */

#define DH_DEBUG(tag, ...) dh_log(DH_LOG_DEBUG, tag, __VA_ARGS__)
#define DH_INFO(tag, ...)  dh_log(DH_LOG_INFO,  tag, __VA_ARGS__)
#define DH_WARN(tag, ...)  dh_log(DH_LOG_WARN,  tag, __VA_ARGS__)
#define DH_ERROR(tag, ...) dh_log(DH_LOG_ERROR, tag, __VA_ARGS__)

/* ── filesystem ─────────────────────────────────────────────────────────── */
/* Read a whole file. Returns malloc'd NUL-terminated buffer or NULL. */
char *dh_fs_read_text(const char *path, size_t *out_len);
void *dh_fs_read_bin(const char *path, size_t *out_len);
int   dh_fs_write_text(const char *path, const char *data);
int   dh_fs_write_bin(const char *path, const void *data, size_t len);
int   dh_fs_exists(const char *path);
int   dh_fs_mkdirs(const char *path);
/* Writable user dir (Spec 6.11: saves NEVER live in the install dir). */
const char *dh_fs_user_dir(void);
/* Data dir: where /data JSON + textures_hd live, resolved relative to exe. */
const char *dh_fs_data_dir(void);
void  dh_fs_set_dirs(const char *data_dir, const char *user_dir);
void  dh_fs_join(char *out, int outsz, const char *a, const char *b);
double dh_now_sec(void);                     /* monotonic seconds */
uint64_t dh_now_ms(void);
void  dh_sleep_ms(int ms);

#endif /* DH_LOG_H */
