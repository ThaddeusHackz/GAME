/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — minimal JSON (Spec 10.4 data-driven rule, 10.5 saves)
   Original implementation. DOM parse + streaming writer. No dependencies.
   ══════════════════════════════════════════════════════════════════════════ */
#ifndef DH_JSON_H
#define DH_JSON_H

#include "dh_types.h"

typedef enum {
    JSON_NULL = 0, JSON_BOOL, JSON_NUM, JSON_STR, JSON_ARR, JSON_OBJ
} JsonType;

typedef struct JsonValue JsonValue;
typedef struct JsonMember JsonMember;

struct JsonValue {
    JsonType type;
    double   num;
    char    *str;            /* owned */
    JsonValue *child;        /* array items / object members' values */
    JsonValue *next;         /* sibling */
    char    *key;            /* when this value is an object member */
};

/* Parse. Returns root or NULL. `err` (optional) gets a static message. */
JsonValue *json_parse(const char *text, const char **err);
void       json_free(JsonValue *v);

/* Lookup — never returns borrowed garbage; NULL when absent. */
JsonValue *json_obj_get(JsonValue *o, const char *key);
JsonValue *json_arr_get(JsonValue *a, int idx);
int        json_arr_len(JsonValue *a);
const char *json_get_str(JsonValue *o, const char *key, const char *dflt);
double     json_get_num(JsonValue *o, const char *key, double dflt);
int        json_get_bool(JsonValue *o, const char *key, int dflt);
float      json_get_flt(JsonValue *o, const char *key, float dflt);
int        json_get_int(JsonValue *o, const char *key, int dflt);
/* Nested path lookup: json_path(root, "display", "fov") */
JsonValue *json_path(JsonValue *root, ...);   /* NULL-terminated list of keys */

/* ── writer (used by SaveManager + settings + feedback.json) ────────────── */
typedef struct {
    char  *buf;
    size_t len, cap;
    int    depth;
    int    need_comma[32];
    int    err;
} JsonWriter;

void jw_init(JsonWriter *w);
void jw_free(JsonWriter *w);
char *jw_take(JsonWriter *w);                /* returns owned buffer, resets */
void jw_obj_begin(JsonWriter *w, const char *key);
void jw_obj_end(JsonWriter *w);
void jw_arr_begin(JsonWriter *w, const char *key);
void jw_arr_end(JsonWriter *w);
void jw_num(JsonWriter *w, const char *key, double v);
void jw_int(JsonWriter *w, const char *key, long long v);
void jw_bool(JsonWriter *w, const char *key, int v);
void jw_str(JsonWriter *w, const char *key, const char *v);
void jw_vec3(JsonWriter *w, const char *key, Vec3 v);
void jw_raw(JsonWriter *w, const char *fmt, ...);

#endif /* DH_JSON_H */
