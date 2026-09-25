/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — minimal JSON implementation
   ══════════════════════════════════════════════════════════════════════════ */
#include "dh_json.h"
#include "dh_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* ───────────────────────────── parser ─────────────────────────────────── */
typedef struct { const char *p; const char *end; const char *err; } P;

static JsonValue *parse_value(P *s);

static void skip_ws(P *s) {
    while (s->p < s->end) {
        char c = *s->p;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') s->p++;
        else if (c == '/' && s->p + 1 < s->end && s->p[1] == '/') {  /* // comments allowed */
            while (s->p < s->end && *s->p != '\n') s->p++;
        } else break;
    }
}
static JsonValue *new_val(JsonType t) {
    JsonValue *v = (JsonValue*)calloc(1, sizeof(JsonValue));
    if (v) v->type = t;
    return v;
}
static char *parse_string_raw(P *s) {
    if (s->p >= s->end || *s->p != '"') { s->err = "expected string"; return NULL; }
    s->p++;
    size_t cap = 32, len = 0;
    char *out = (char*)malloc(cap);
    if (!out) { s->err = "oom"; return NULL; }
    while (s->p < s->end && *s->p != '"') {
        char c = *s->p++;
        if (c == '\\') {
            if (s->p >= s->end) break;
            char e = *s->p++;
            switch (e) {
                case 'n': c = '\n'; break; case 't': c = '\t'; break;
                case 'r': c = '\r'; break; case 'b': c = '\b'; break;
                case 'f': c = '\f'; break; case '"': c = '"';  break;
                case '\\': c = '\\'; break; case '/': c = '/';  break;
                case 'u': {
                    /* Decode BMP escape to UTF-8 (needed for AR/JA/ZH loc, Spec 34) */
                    if (s->p + 4 > s->end) { s->err = "bad \\u"; free(out); return NULL; }
                    unsigned cp = 0;
                    for (int i = 0; i < 4; i++) {
                        char h = *s->p++; cp <<= 4;
                        if (h >= '0' && h <= '9') cp |= (unsigned)(h - '0');
                        else if (h >= 'a' && h <= 'f') cp |= (unsigned)(h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') cp |= (unsigned)(h - 'A' + 10);
                        else { s->err = "bad hex"; free(out); return NULL; }
                    }
                    char tmp[4]; int n = 0;
                    if (cp < 0x80) { tmp[n++] = (char)cp; }
                    else if (cp < 0x800) { tmp[n++] = (char)(0xC0 | (cp >> 6)); tmp[n++] = (char)(0x80 | (cp & 0x3F)); }
                    else { tmp[n++] = (char)(0xE0 | (cp >> 12)); tmp[n++] = (char)(0x80 | ((cp >> 6) & 0x3F)); tmp[n++] = (char)(0x80 | (cp & 0x3F)); }
                    for (int i = 0; i < n; i++) {
                        if (len + 1 >= cap) { cap *= 2; char *nn = (char*)realloc(out, cap); if (!nn) { free(out); s->err="oom"; return NULL; } out = nn; }
                        out[len++] = tmp[i];
                    }
                    continue;
                }
                default: c = e; break;
            }
        }
        if (len + 1 >= cap) {
            cap *= 2;
            char *nn = (char*)realloc(out, cap);
            if (!nn) { free(out); s->err = "oom"; return NULL; }
            out = nn;
        }
        out[len++] = c;
    }
    if (s->p >= s->end) { free(out); s->err = "unterminated string"; return NULL; }
    s->p++; /* closing quote */
    out[len] = '\0';
    return out;
}
static JsonValue *parse_array(P *s) {
    JsonValue *arr = new_val(JSON_ARR);
    if (!arr) { s->err = "oom"; return NULL; }
    s->p++; /* [ */
    JsonValue *tail = NULL;
    skip_ws(s);
    if (s->p < s->end && *s->p == ']') { s->p++; return arr; }
    for (;;) {
        skip_ws(s);
        JsonValue *it = parse_value(s);
        if (!it) { json_free(arr); return NULL; }
        if (!arr->child) arr->child = it; else tail->next = it;
        tail = it;
        skip_ws(s);
        if (s->p < s->end && *s->p == ',') { s->p++; continue; }
        if (s->p < s->end && *s->p == ']') { s->p++; return arr; }
        s->err = "expected , or ]"; json_free(arr); return NULL;
    }
}
static JsonValue *parse_object(P *s) {
    JsonValue *obj = new_val(JSON_OBJ);
    if (!obj) { s->err = "oom"; return NULL; }
    s->p++; /* { */
    JsonValue *tail = NULL;
    skip_ws(s);
    if (s->p < s->end && *s->p == '}') { s->p++; return obj; }
    for (;;) {
        skip_ws(s);
        char *key = parse_string_raw(s);
        if (!key) { json_free(obj); return NULL; }
        skip_ws(s);
        if (s->p >= s->end || *s->p != ':') { free(key); s->err = "expected :"; json_free(obj); return NULL; }
        s->p++;
        skip_ws(s);
        JsonValue *val = parse_value(s);
        if (!val) { free(key); json_free(obj); return NULL; }
        val->key = key;
        if (!obj->child) obj->child = val; else tail->next = val;
        tail = val;
        skip_ws(s);
        if (s->p < s->end && *s->p == ',') { s->p++; continue; }
        if (s->p < s->end && *s->p == '}') { s->p++; return obj; }
        s->err = "expected , or }"; json_free(obj); return NULL;
    }
}
static JsonValue *parse_value(P *s) {
    skip_ws(s);
    if (s->p >= s->end) { s->err = "unexpected eof"; return NULL; }
    char c = *s->p;
    if (c == '{') return parse_object(s);
    if (c == '[') return parse_array(s);
    if (c == '"') {
        JsonValue *v = new_val(JSON_STR);
        if (!v) { s->err = "oom"; return NULL; }
        v->str = parse_string_raw(s);
        if (!v->str) { free(v); return NULL; }
        return v;
    }
    if ((c == 't' || c == 'f') && s->end - s->p >= 4) {
        if (!strncmp(s->p, "true", 4))  { s->p += 4; JsonValue *v = new_val(JSON_BOOL); if (v) v->num = 1; return v; }
        if (!strncmp(s->p, "false", 5)) { s->p += 5; JsonValue *v = new_val(JSON_BOOL); if (v) v->num = 0; return v; }
    }
    if (!strncmp(s->p, "null", 4)) { s->p += 4; return new_val(JSON_NULL); }
    /* number */
    char *endp = NULL;
    double d = strtod(s->p, &endp);
    if (endp == s->p) { s->err = "unexpected token"; return NULL; }
    s->p = endp;
    JsonValue *v = new_val(JSON_NUM);
    if (!v) { s->err = "oom"; return NULL; }
    v->num = d;
    return v;
}

JsonValue *json_parse(const char *text, const char **err) {
    if (err) *err = NULL;
    if (!text) { if (err) *err = "null input"; return NULL; }
    P s; s.p = text; s.end = text + strlen(text); s.err = NULL;
    JsonValue *root = parse_value(&s);
    if (!root && err) *err = s.err ? s.err : "parse error";
    return root;
}
void json_free(JsonValue *v) {
    while (v) {
        JsonValue *next = v->next;
        if (v->child) json_free(v->child);
        free(v->str); free(v->key); free(v);
        v = next;
    }
}
JsonValue *json_obj_get(JsonValue *o, const char *key) {
    if (!o || !key) return NULL;
    if (o->type == JSON_OBJ) {
        for (JsonValue *c = o->child; c; c = c->next)
            if (c->key && strcmp(c->key, key) == 0) return c;
    }
    return NULL;
}
JsonValue *json_arr_get(JsonValue *a, int idx) {
    if (!a || idx < 0) return NULL;
    int i = 0;
    for (JsonValue *c = a->child; c; c = c->next, i++) if (i == idx) return c;
    return NULL;
}
int json_arr_len(JsonValue *a) {
    if (!a) return 0;
    int n = 0; for (JsonValue *c = a->child; c; c = c->next) n++; return n;
}
const char *json_get_str(JsonValue *o, const char *key, const char *dflt) {
    JsonValue *v = json_obj_get(o, key);
    return (v && v->type == JSON_STR && v->str) ? v->str : dflt;
}
double json_get_num(JsonValue *o, const char *key, double dflt) {
    JsonValue *v = json_obj_get(o, key);
    if (!v) return dflt;
    if (v->type == JSON_NUM) return v->num;
    if (v->type == JSON_BOOL) return v->num;
    return dflt;
}
int   json_get_bool(JsonValue *o, const char *key, int dflt) { JsonValue *v = json_obj_get(o,key); return v ? (v->num != 0.0) : dflt; }
float json_get_flt(JsonValue *o, const char *key, float dflt) { return (float)json_get_num(o, key, dflt); }
int   json_get_int(JsonValue *o, const char *key, int dflt)   { return (int)json_get_num(o, key, dflt); }

JsonValue *json_path(JsonValue *root, ...) {
    va_list ap; va_start(ap, root);
    JsonValue *cur = root;
    for (;;) {
        const char *k = va_arg(ap, const char*);
        if (!k) break;
        cur = json_obj_get(cur, k);
        if (!cur) break;
    }
    va_end(ap);
    return cur;
}

/* ───────────────────────────── writer ─────────────────────────────────── */
static void jw_putc(JsonWriter *w, char c) {
    if (w->err) return;
    if (w->len + 1 >= w->cap) {
        size_t nc = w->cap ? w->cap * 2 : 4096;
        char *nb = (char*)realloc(w->buf, nc);
        if (!nb) { w->err = 1; return; }
        w->buf = nb; w->cap = nc;
    }
    w->buf[w->len++] = c;
    w->buf[w->len] = '\0';
}
static void jw_puts(JsonWriter *w, const char *s) { while (*s) jw_putc(w, *s++); }
static void jw_indent(JsonWriter *w) { for (int i = 0; i < w->depth; i++) jw_puts(w, "  "); }
static void jw_pre_key(JsonWriter *w, const char *key) {
    int d = w->depth < 32 ? w->depth : 31;
    if (w->need_comma[d]) jw_putc(w, ',');
    jw_putc(w, '\n'); jw_indent(w);
    if (key) {
        jw_putc(w, '"');
        for (const char *s = key; *s; s++) {
            if (*s == '"' || *s == '\\') { jw_putc(w, '\\'); jw_putc(w, *s); }
            else jw_putc(w, *s);
        }
        jw_puts(w, "\": ");
    }
    w->need_comma[d] = 1;
}
static void jw_str_escaped(JsonWriter *w, const char *v) {
    jw_putc(w, '"');
    if (v) for (const unsigned char *s = (const unsigned char*)v; *s; s++) {
        switch (*s) {
            case '"':  jw_puts(w, "\\\""); break;
            case '\\': jw_puts(w, "\\\\"); break;
            case '\n': jw_puts(w, "\\n");  break;
            case '\r': jw_puts(w, "\\r");  break;
            case '\t': jw_puts(w, "\\t");  break;
            default:
                if (*s < 0x20) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", *s); jw_puts(w, b); }
                else jw_putc(w, (char)*s);
        }
    }
    jw_putc(w, '"');
}
void jw_init(JsonWriter *w) {
    memset(w, 0, sizeof(*w));
    w->cap = 8192; w->buf = (char*)malloc(w->cap);
    if (w->buf) w->buf[0] = '\0'; else w->err = 1;
}
void jw_free(JsonWriter *w) { free(w->buf); w->buf = NULL; w->len = w->cap = 0; }
char *jw_take(JsonWriter *w) {
    char *b = w->buf;
    w->buf = NULL; w->len = w->cap = 0; w->depth = 0; memset(w->need_comma, 0, sizeof(w->need_comma));
    return b;
}
void jw_obj_begin(JsonWriter *w, const char *key) {
    jw_pre_key(w, key); jw_putc(w, '{');
    if (w->depth < 31) w->depth++;
    w->need_comma[w->depth] = 0;
}
void jw_arr_begin(JsonWriter *w, const char *key) {
    jw_pre_key(w, key); jw_putc(w, '[');
    if (w->depth < 31) w->depth++;
    w->need_comma[w->depth] = 0;
}
static void jw_close(JsonWriter *w, char c) {
    if (w->depth > 0) w->depth--;
    jw_putc(w, '\n'); jw_indent(w); jw_putc(w, c);
}
void jw_obj_end(JsonWriter *w) { jw_close(w, '}'); }
void jw_arr_end(JsonWriter *w) { jw_close(w, ']'); }

void jw_num(JsonWriter *w, const char *key, double v) {
    jw_pre_key(w, key);
    char b[64];
    if (!isfinite(v)) v = 0.0;                      /* Spec 18.4: no NaN in saves */
    if (v == floor(v) && dh_absf(v) < 1e15) snprintf(b, sizeof(b), "%.0f", v);
    else snprintf(b, sizeof(b), "%.6g", v);
    jw_puts(w, b);
}
void jw_int(JsonWriter *w, const char *key, long long v) {
    jw_pre_key(w, key); char b[32]; snprintf(b, sizeof(b), "%lld", v); jw_puts(w, b);
}
void jw_bool(JsonWriter *w, const char *key, int v) { jw_pre_key(w, key); jw_puts(w, v ? "true" : "false"); }
void jw_str(JsonWriter *w, const char *key, const char *v) { jw_pre_key(w, key); jw_str_escaped(w, v); }
void jw_vec3(JsonWriter *w, const char *key, Vec3 v) {
    jw_arr_begin(w, key);
    jw_num(w, NULL, v.x); jw_num(w, NULL, v.y); jw_num(w, NULL, v.z);
    jw_arr_end(w);
}
void jw_raw(JsonWriter *w, const char *fmt, ...) {
    char b[512]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof(b), fmt, ap); va_end(ap);
    jw_pre_key(w, NULL); jw_puts(w, b);
}
