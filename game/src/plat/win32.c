/* ══════════════════════════════════════════════════════════════════════════
   DIVIDED HORIZON — Win32 platform layer (M1b)
   Window + keyboard/mouse + WGL context for the GL11 backend. Everything the
   game sees is PlatInput, so this file is the ONLY place Windows input quirks
   live (Spec: platform abstraction).

   Input contract (plat.h):
     · look_dx / look_dy are RADIANS per frame — mouse pixels are converted
       here (0.0022 rad/px ≈ 12.6 cm/360° at 96 DPI) so the simulation stays
       device-independent and headless replays stay reproducible.
     · `pressed` carries one-frame edges; `buttons` carries holds.
   ══════════════════════════════════════════════════════════════════════════ */
#ifdef _WIN32

#include <windows.h>
#include <GL/gl.h>

#include "plat.h"
#include "../rend/rend.h"
#include "../core/dh_log.h"

#include <stdlib.h>
#include <string.h>

#define MOUSE_RAD_PER_PX 0.0022f

typedef struct {
    HWND  hwnd;
    HDC   hdc;
    HGLRC hglrc;
    int   quit;
    /* held keys (VK indexed) */
    uint8_t key[256];
    /* mouse */
    long  mx, my;              /* accumulated pixel delta this frame */
    int   last_x, last_y, have_last;
    uint32_t held;             /* abstract buttons held */
    uint32_t edge;             /* abstract buttons pressed this frame */
    int   cursor_locked;
} WinState;

static WinState s_ws;

/* ── timing ────────────────────────────────────────────────────────────── */
uint64_t plat_now_us(void) {
    static LARGE_INTEGER freq;
    static int have_freq = 0;
    LARGE_INTEGER now;
    if (!have_freq) { QueryPerformanceFrequency(&freq); have_freq = 1; }
    QueryPerformanceCounter(&now);
    return (uint64_t)((double)now.QuadPart * 1000000.0 / (double)freq.QuadPart);
}
void plat_sleep_ms(int ms) { if (ms > 0) Sleep((DWORD)ms); }

/* ── key → abstract button ─────────────────────────────────────────────── */
static uint32_t key_buttons(const WinState *w) {
    uint32_t b = w->held;
    if (w->key[VK_SPACE])    b |= BTN_JUMP;
    if (w->key[VK_SHIFT])    b |= BTN_SPRINT;
    if (w->key[VK_CONTROL])  b |= BTN_CROUCH;
    if (w->key['V'])         b |= BTN_ROLL;      /* M2: R became reload (genre standard) */
    if (w->key['R'])         b |= BTN_RELOAD;
    if (w->key['E'])         b |= BTN_USE;
    if (w->key['F'])         b |= BTN_FIRE;
    if (w->key['C'])         b |= BTN_AIM;
    if (w->key['1'])         b |= BTN_SLOT1;
    if (w->key['2'])         b |= BTN_SLOT2;
    if (w->key['3'])         b |= BTN_SLOT3;
    if (w->key['G'])         b |= BTN_ARENA;
    if (w->key['T'])         b |= BTN_MELEE;     /* M3: takedown / melee */
    if (w->key['B'])         b |= BTN_BINOC;     /* M3: binoculars (hold) */
    if (w->key['Q'])         b |= BTN_RADIO;     /* M4: cycle radio (in cars) */
    if (w->key['K'])         b |= BTN_FERRY;     /* M4: ferry island <-> city */
    if (w->key['P'])         b |= BTN_PHOTO;
    if (w->key['H'])         b |= BTN_HEAL;      /* M5 */
    if (w->key['I'])         b |= BTN_CHAR;
    if (w->key['L'])         b |= BTN_LOAD;
    if (w->key[VK_F3])       b |= BTN_DEBUG;
    if (w->key[VK_TAB])      b |= BTN_MAP;
    if (w->key[VK_ESCAPE])   b |= BTN_MENU;
    if (w->key[VK_UP])       b |= BTN_UP;
    if (w->key[VK_DOWN])     b |= BTN_DOWN;
    if (w->key[VK_LEFT])     b |= BTN_LEFT;
    if (w->key[VK_RIGHT])    b |= BTN_RIGHT;
    return b;
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    WinState *w = &s_ws;
    switch (msg) {
    case WM_CLOSE:
        w->quit = 1;
        return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (wp < 256) {
            if (!w->key[wp]) {
                /* one-frame edges for the verbs that are presses, not holds */
                if (wp == VK_SPACE) w->edge |= BTN_JUMP;
                if (wp == 'V')      w->edge |= BTN_ROLL;
                if (wp == 'R')      w->edge |= BTN_RELOAD;
                if (wp == 'E')      w->edge |= BTN_USE;
                if (wp == 'P')      w->edge |= BTN_PHOTO;
                if (wp == '1')      w->edge |= BTN_SLOT1;
                if (wp == '2')      w->edge |= BTN_SLOT2;
                if (wp == '3')      w->edge |= BTN_SLOT3;
                if (wp == 'G')      w->edge |= BTN_ARENA;
                if (wp == 'T')      w->edge |= BTN_MELEE;
                if (wp == 'Q')      w->edge |= BTN_RADIO;
                if (wp == 'K')      w->edge |= BTN_FERRY;
                if (wp == VK_ESCAPE) w->edge |= BTN_MENU;
                if (wp == VK_TAB)   w->edge |= BTN_MAP;     /* M5: menus need edges */
                if (wp == VK_UP)    w->edge |= BTN_UP;
                if (wp == VK_DOWN)  w->edge |= BTN_DOWN;
                if (wp == VK_LEFT)  w->edge |= BTN_LEFT;
                if (wp == VK_RIGHT) w->edge |= BTN_RIGHT;
                if (wp == 'H')      w->edge |= BTN_HEAL;
                if (wp == 'I')      w->edge |= BTN_CHAR;
                if (wp == 'L')      w->edge |= BTN_LOAD;
                if (wp == VK_F3)    w->edge |= BTN_DEBUG;
            }
            w->key[wp] = 1;
        }
        return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (wp < 256) w->key[wp] = 0;
        return 0;
    case WM_LBUTTONDOWN: w->held |= BTN_FIRE; w->edge |= BTN_FIRE; return 0;
    case WM_LBUTTONUP:   w->held &= ~BTN_FIRE; return 0;
    case WM_RBUTTONDOWN: w->held |= BTN_AIM;  w->edge |= BTN_AIM;  return 0;
    case WM_RBUTTONUP:   w->held &= ~BTN_AIM; return 0;
    case WM_MOUSEMOVE: {
        int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
        if (w->have_last && w->cursor_locked) {
            w->mx += x - w->last_x;
            w->my += y - w->last_y;
        }
        w->last_x = x; w->last_y = y; w->have_last = 1;
        return 0;
    }
    case WM_DESTROY:
        w->quit = 1;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

/* ── poll: pump messages, publish PlatInput ────────────────────────────── */
static int win_poll(Plat *p, PlatInput *in) {
    WinState *w = &s_ws;
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) { w->quit = 1; break; }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    memset(in, 0, sizeof(*in));
    if (w->quit) { in->quit = 1; return 0; }

    uint32_t b = key_buttons(w);
    in->buttons = b;
    in->pressed = w->edge;
    w->edge = 0;

    /* move axes from held keys (W/S forward, A/D strafe) */
    float my = 0.0f, mx = 0.0f;
    if (w->key['W'] || (b & BTN_UP))    my += 1.0f;
    if (w->key['S'] || (b & BTN_DOWN))  my -= 1.0f;
    if (w->key['D'] || (b & BTN_RIGHT)) mx += 1.0f;
    if (w->key['A'] || (b & BTN_LEFT))  mx -= 1.0f;
    in->mx = mx; in->my = my;

    /* mouse pixels → radians (platform contract) */
    in->look_dx = (float)w->mx * MOUSE_RAD_PER_PX;
    in->look_dy = (float)-w->my * MOUSE_RAD_PER_PX;   /* mouse up = look up */
    w->mx = w->my = 0;

    /* FPS-style cursor lock while the window has focus */
    if (!w->cursor_locked && GetFocus() == w->hwnd) {
        RECT rc; GetClientRect(w->hwnd, &rc);
        POINT c = { (rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2 };
        ClientToScreen(w->hwnd, &c);
        SetCursorPos(c.x, c.y);
        ShowCursor(FALSE);
        w->cursor_locked = 1;
        w->have_last = 0;
    } else if (w->cursor_locked && GetFocus() != w->hwnd) {
        ShowCursor(TRUE);
        w->cursor_locked = 0;
        w->have_last = 0;
    }
    (void)p;
    return 1;
}

static void win_present(Plat *p, const uint32_t *fb, int w, int h) {
    (void)fb; (void)w; (void)h; (void)p;
    rend_gl_swap();                      /* GL owns the backbuffer */
}

static void win_destroy(Plat *p) {
    WinState *w = &s_ws;
    if (w->cursor_locked) ShowCursor(TRUE);
    rend_gl_destroy();
    if (w->hglrc) { wglMakeCurrent(NULL, NULL); wglDeleteContext(w->hglrc); w->hglrc = NULL; }
    if (w->hdc && w->hwnd) ReleaseDC(w->hwnd, w->hdc);
    if (w->hwnd) DestroyWindow(w->hwnd);
    w->hdc = NULL; w->hwnd = NULL;
    free(p);
}

/* ── create: window + pixel format + WGL context ───────────────────────── */
Plat *plat_win32_create(int w, int h, const char *title) {
    HINSTANCE inst = GetModuleHandleA(NULL);
    WNDCLASSA wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.lpszClassName = "DividedHorizonWindow";
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    if (!RegisterClassA(&wc)) { DH_ERROR("plat", "RegisterClass failed"); return NULL; }

    RECT rc = { 0, 0, w, h };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName,
                                title ? title : "DIVIDED HORIZON",
                                WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT,
                                rc.right - rc.left, rc.bottom - rc.top,
                                NULL, NULL, inst, NULL);
    if (!hwnd) { DH_ERROR("plat", "CreateWindow failed"); return NULL; }

    HDC hdc = GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;
    int pf = ChoosePixelFormat(hdc, &pfd);
    if (!pf || !SetPixelFormat(hdc, pf, &pfd)) {
        DH_ERROR("plat", "SetPixelFormat failed — no OpenGL-capable visual");
        DestroyWindow(hwnd);
        return NULL;
    }
    HGLRC gl = wglCreateContext(hdc);
    if (!gl) {
        DH_ERROR("plat", "wglCreateContext failed — no OpenGL 1.1 driver?");
        DestroyWindow(hwnd);
        return NULL;
    }
    wglMakeCurrent(hdc, gl);

    /* v-sync on by default when the driver exposes the swap-interval ext */
    typedef BOOL (WINAPI *SwapInt)(int);
    SwapInt si = (SwapInt)wglGetProcAddress("wglSwapIntervalEXT");
    if (si) si(1);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    WinState *ws = &s_ws;
    memset(ws, 0, sizeof(*ws));
    ws->hwnd = hwnd; ws->hdc = hdc; ws->hglrc = gl;

    if (!rend_gl_create(hdc)) {
        DH_ERROR("plat", "rend_gl_create rejected the HDC");
        wglDeleteContext(gl); DestroyWindow(hwnd);
        return NULL;
    }

    Plat *p = (Plat *)calloc(1, sizeof(Plat));
    if (!p) return NULL;
    p->name = "win32";
    p->width = w; p->height = h;
    p->running = 1;
    p->poll = win_poll;
    p->present = win_present;
    p->destroy = win_destroy;
    DH_INFO("plat", "win32 window %dx%d + WGL context online", w, h);
    return p;
}

#endif /* _WIN32 */

/* ── M7: waveOut stream (winmm, already linked). 6 × 1024-sample buffers
   ≈ 280 ms of queue at 22.05 kHz; any finished buffer is refilled from the
   mixer each frame. If the device fails to open we log it and stay silent —
   the game never depends on audio (Honesty Contract / P5). ── */
#include <mmsystem.h>
#include "../audio/audio.h"
#define AQ_N 6
#define AQ_LEN 1024
static HWAVEOUT g_wo; static int g_wo_state;     /* 0 untried, 1 ok, -1 failed */
static WAVEHDR g_wh[AQ_N]; static int16_t g_wbuf[AQ_N][AQ_LEN];
static int g_audio_null;
void plat_audio_set_null(int on) { g_audio_null = on; }
int plat_audio_pump(void) {
    if (g_audio_null) return plat_audio_null_pump();
    if (g_wo_state == 0) {
        WAVEFORMATEX f; memset(&f, 0, sizeof f);
        f.wFormatTag = WAVE_FORMAT_PCM; f.nChannels = 1; f.nSamplesPerSec = AUDIO_RATE;
        f.wBitsPerSample = 16; f.nBlockAlign = 2; f.nAvgBytesPerSec = AUDIO_RATE * 2;
        if (waveOutOpen(&g_wo, WAVE_MAPPER, &f, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
            g_wo_state = -1; DH_WARN("audio", "waveOut unavailable — running silent");
            return 0;
        }
        g_wo_state = 1;
        for (int i = 0; i < AQ_N; i++) {
            memset(&g_wh[i], 0, sizeof g_wh[i]);
            g_wh[i].lpData = (LPSTR)g_wbuf[i]; g_wh[i].dwBufferLength = AQ_LEN * 2;
            waveOutPrepareHeader(g_wo, &g_wh[i], sizeof(WAVEHDR));
            audio_mix(g_wbuf[i], AQ_LEN);
            waveOutWrite(g_wo, &g_wh[i], sizeof(WAVEHDR));
        }
        DH_INFO("audio", "waveOut open: %d Hz mono, %d x %d buffers", AUDIO_RATE, AQ_N, AQ_LEN);
    }
    if (g_wo_state != 1) return 0;
    int pk = 0;
    for (int i = 0; i < AQ_N; i++) if (g_wh[i].dwFlags & WHDR_DONE) {
        audio_mix(g_wbuf[i], AQ_LEN);
        for (int k = 0; k < AQ_LEN; k += 8) { int a = abs(g_wbuf[i][k]); if (a > pk) pk = a; }
        g_wh[i].dwFlags &= ~WHDR_DONE;
        waveOutWrite(g_wo, &g_wh[i], sizeof(WAVEHDR));
    }
    return pk;
}
void plat_audio_close(void) {
    if (g_wo_state != 1) return;
    waveOutReset(g_wo);
    for (int i = 0; i < AQ_N; i++) waveOutUnprepareHeader(g_wo, &g_wh[i], sizeof(WAVEHDR));
    waveOutClose(g_wo); g_wo_state = 0;
}
