/* usumfix: a stand-in SDL2.dll for FullDex's 3DS program (omega-nx, "Pokemon ORAS.exe" and
 * "Pokemon ORAS (Vulkan).exe") that gives Pokemon Ultra Sun and Ultra Moon's battles real
 * single-screen buttons in the one-screen layout.
 *
 * Every SDL function goes on to the program's own SDL2 (renamed SDL2_real.dll). On top of that:
 *  - OpenGL: the program's GL calls are watched (wglGetProcAddress / GetProcAddress hooked),
 *  - Vulkan: vkGetInstanceProcAddr is wrapped (the exe's GetProcAddress hooked, as ash loads it),
 * so that each frame we know where the program draws the game's bottom screen ("Battle Buttons:
 * Whole screen" puts it in a box in the corner) and which texture or image holds it. A small copy
 * of the bottom screen is read back each frame to tell which battle menu it shows; while it shows
 * one we know, the box is held back and the menu is drawn instead from the game's own pictures,
 * cut out of the bottom screen and laid out over the battle. Clicks and taps on the new buttons go
 * to the program as clicks on the matching place in the box. With "Menus: Laid out" the field's X
 * menu, which the program shows in pieces of the bottom screen, is drawn anew in the same places.
 * Other games' folders may hold it too: for them it only passes SDL through. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

/* ================================================================== common */
static char here[MAX_PATH];          /* the program's folder, with a trailing backslash */
static FILE *logf_;
static int debug_on;
static CRITICAL_SECTION log_cs;
static const char *path_here(const char *name)
{
    static char b[4][MAX_PATH + 64];
    static int k;
    char *p = b[k++ & 3];
    snprintf(p, MAX_PATH + 64, "%s%s", here, name);
    return p;
}
static int file_here(const char *name) { return GetFileAttributesA(path_here(name)) != INVALID_FILE_ATTRIBUTES; }
static void lg(const char *fmt, ...)
{
    if (!debug_on) return;
    EnterCriticalSection(&log_cs);
    if (!logf_) logf_ = fopen(path_here("usumfix.log"), "w");
    if (logf_) {
        va_list a; va_start(a, fmt); vfprintf(logf_, fmt, a); va_end(a);
        fputc('\n', logf_); fflush(logf_);
    }
    LeaveCriticalSection(&log_cs);
}
static int game_on;         /* this folder holds Ultra Sun or Ultra Moon */
static int fix_off;         /* usumfix_off.txt: leave the program's own battle screen be */
static unsigned frame;      /* frames shown */

/* ====================================================== the model we draw
 * A quad is drawn over its destination rectangle (window pixels, from the top left) and filled
 * by its kind: the game's bottom screen (src in bottom-screen pixels, 320x240, upright), a solid
 * colour, a glow round a shape, a picture of our own atlas, or a panel's body (a gradient of the
 * game's own colours). A convex polygon (up to 8 points, in src units) cuts it out, rounded by
 * misc.y, with edges smoothed over one window pixel. For our own panels src is the destination
 * itself, so their outlines are in window pixels. The layout of 24 vec4s is the one the shaders
 * read. */
enum { K_ART, K_SOLID, K_GLOW, K_ATLAS, K_COPY, K_BODY, K_ARTBODY };
typedef struct {
    float dst[4];      /* x, y, w, h: window pixels from the top left */
    float src[4];      /* x0, y0, x1, y1: bottom-screen pixels (K_ART), atlas pixels (K_ATLAS) */
    float info[4];     /* kind, number of polygon points, src units per window pixel, glow width (src units) */
    float col[4];      /* the solid colour, or the tint (K_ART, by misc[3]), straight alpha */
    float col2[4];     /* the glow's colour */
    float poly[4][4];  /* 8 points (x, y) in src units, in order round the shape */
    float misc[4];     /* alpha, rounding (src units), unused, tint amount */
    float win[4];      /* the target's width and height, y flipped (1) or not (0), unused */
    float uvm[2][4];   /* bottom-screen pixel to texture: u = uvm[0].x * x + uvm[0].y * y + uvm[0].z */
    float pad[3][4];   /* K_ART: [0] the mask, or the saved picture (atlas x, y of the bottom-screen point x, y), [1] (turned a quarter
                          left, masked, the picture from our saved copy, its colours' spacing), [2] (where the picture is taken from,
                          moved by x, y; held right of column z if w); K_BODY: [0] the panel (x, y, w, h), [1] (gloss, shade) */
    float ext[8][4];   /* K_BODY: its colour at 7 heights, top to bottom; K_ARTBODY: [0] the art's clean rectangle, [1] the rows left out;
                          any kind: [7] the shape from our atlas instead (on, atlas x, y of src 0, 0, steps a src unit) */
} Quad;                /* 384 bytes, 24 vec4 */
#define NU 24
#define MAXQ 200
typedef char quad_is_384[sizeof(Quad) == 384 ? 1 : -1];
static Quad quads[MAXQ];
static int nquads;
static float bot_uvm[2][4] = { { 0, -1.0f / 240, 1, 0 }, { -1.0f / 320, 0, 1, 0 } };   /* the program's texture holds the 3DS's turned picture */
/* our copies of the bottom screen are upright: top row first in Vulkan, last in OpenGL */
static float art_uvm[2][4] = { { 1.0f / 320, 0, 0, 0 }, { 0, 1.0f / 240, 0, 0 } };
static const float art_uvm_gl[2][4] = { { 1.0f / 320, 0, 0, 0 }, { 0, -1.0f / 240, 1, 0 } };
static const float art_uvm_vk[2][4] = { { 1.0f / 320, 0, 0, 0 }, { 0, 1.0f / 240, 0, 0 } };
#define NSLOT 3              /* copies of the bottom screen, each read back; we draw from the one last read */
#define CW 1280              /* a copy's size: four times the 3DS's */
#define CH 960
static int art_slot = -1;

/* what the backends saw this frame */
static float win_w = 1280, win_h = 720;   /* the picture's size */
/* the battle is laid out in the frame of the top screen's picture (the window, less any bars the Aspect ratio setting
   leaves): lay_w x lay_h, its top left at lay_off in the window; the quads and presses are moved there afterwards */
static float lay_w = 1280, lay_h = 720, lay_off[2];
static float box[4];                      /* where the program puts the bottom screen: x, y, w, h (window pixels, from the top) */
static int box_on;                        /* the bottom screen is shown in a box this frame */
static int ui_on;                         /* our menu is drawn this frame (and the box is held back) */
static int seen_bottom;                   /* the bottom screen was drawn (in a box or hidden) this frame */
static int box_n;                         /* how many times this frame (more than one: the program's own pieces, "Buttons only") */
static float top_rect[4];                 /* where the top screen went this frame, window pixels from the top */
static int top_seen;
/* the draws of the bottom screen this frame: where each went (window px from the top: x, y, w, h) and the part of
   the bottom screen it shows (x, y, w, h); the program's "Menu layouts" draw its menus in such pieces */
#define MAXPC 24
typedef struct { float dst[4], src[4]; } Pc;
static Pc pc_cur[MAXPC]; static int pc_n;
static int ui_box;                        /* our battle menu was drawn last frame: the box is held back */
static int xm_hold;                       /* our X menu was drawn last frame: its pieces and the program's L/R strip are held back */
static float xm_zone[4];                  /* where the program's L/R strip is: x0, y0, x1, y1 (window px from the top) */
static int xm_piece_of(const float *src);
/* a draw of the bottom screen: where it went, and whether it is a whole box (not one of the program's cut pieces) */
static void saw_bottom(float x, float ytop, float w, float h, int cut, const float *src)
{
    seen_bottom = 1;
    int vis = w > 2 && h > 2 && ytop < win_h && x < win_w;
    if (!vis) return;
    if (pc_n < MAXPC) {
        Pc *p = &pc_cur[pc_n++];
        p->dst[0] = x; p->dst[1] = ytop; p->dst[2] = w; p->dst[3] = h;
        if (src) memcpy(p->src, src, sizeof p->src);
        else { p->src[0] = 0; p->src[1] = 0; p->src[2] = 320; p->src[3] = 240; }
    }
    box_n++;
    if (cut || box_n > 1) { box_on = 0; return; }
    box_on = 1;
    box[0] = x; box[1] = ytop; box[2] = w; box[3] = h;
}
static void saw_top(float x, float ytop, float w, float h) { top_seen = 1; top_rect[0] = x; top_rect[1] = ytop; top_rect[2] = w; top_rect[3] = h; }
/* the top screen is the big one: across or down the window, or nearly (Aspect ratio "Integer scale" scales it by
   whole pixels, leaving bars: at 1920x1080 it is 1600x960) */
static int top_big(void) { return top_seen && (top_rect[2] >= 0.72f * win_w || top_rect[3] >= 0.72f * win_h); }
/* the one-screen layout: the top screen fills the window and the bottom screen is a box on it */
static int one_screen(void)
{
    if (!box_on || !top_big()) return 0;
    float cx = box[0] + box[2] / 2, cy = box[1] + box[3] / 2;
    return cx >= top_rect[0] && cx <= top_rect[0] + top_rect[2] && cy >= top_rect[1] && cy <= top_rect[1] + top_rect[3]
        && fabsf(box[2] / box[3] - 4.0f / 3.0f) < 0.05f && box[2] < 0.7f * win_w;
}

/* the bottom screen as last read back: 320x240, upright, top row first, RGBA */
static unsigned char rb[240][320][4];
static unsigned rb_frame;                 /* the frame it shows */
static int rb_ok;

static void q_reset(void) { nquads = 0; }
static Quad *q_add(int kind, float x, float y, float w, float h)
{
    if (nquads >= MAXQ) return NULL;
    Quad *q = &quads[nquads++];
    memset(q, 0, sizeof *q);
    q->dst[0] = x; q->dst[1] = y; q->dst[2] = w; q->dst[3] = h;
    q->info[0] = (float)kind;
    q->misc[0] = 1;
    q->col[0] = q->col[1] = q->col[2] = q->col[3] = 1;
    return q;
}
static void q_src(Quad *q, float x0, float y0, float x1, float y1)
{
    q->src[0] = x0; q->src[1] = y0; q->src[2] = x1; q->src[3] = y1;
    q->info[2] = (x1 - x0) / (q->dst[2] > 0 ? q->dst[2] : 1);
}
static void q_poly(Quad *q, int n, const float *pts)
{
    if (n > 8) n = 8;
    q->info[1] = (float)n;
    for (int i = 0; i < n; i++) q->poly[i / 2][(i & 1) * 2] = pts[2 * i], q->poly[i / 2][(i & 1) * 2 + 1] = pts[2 * i + 1];
}
static void q_col(Quad *q, float r, float g, float b, float a) { q->col[0] = r; q->col[1] = g; q->col[2] = b; q->col[3] = a; }

/* ============================================================= the shaders
 * One vertex shader (a strip of 4, grown by the glow) and one fragment shader, written once for
 * OpenGL ES 3.0 here; the Vulkan build of the same (vk_shaders.h) is made from usumfix.vert and
 * usumfix.frag by glslangValidator. */
#define GLSL_BODY \
    "vec2 P(int i){ vec4 v = U[5 + i / 2]; return (i % 2) == 0 ? v.xy : v.zw; }\n" \
    "float poly_d(vec2 p, int n){\n" \
    " float ar = 0.0; for (int i = 0; i < 8; i++) { if (i >= n) break; vec2 a = P(i), b = P((i + 1) % n); ar += a.x * b.y - b.x * a.y; }\n" \
    " float sg = ar >= 0.0 ? 1.0 : -1.0; float dh = -1e9, ds = 1e9;\n" \
    " for (int i = 0; i < 8; i++) { if (i >= n) break; vec2 a = P(i), b = P((i + 1) % n); vec2 e = b - a; float l2 = dot(e, e);\n" \
    "  if (l2 < 1e-8) continue;\n" \
    "  dh = max(dh, dot(p - a, vec2(e.y, -e.x)) * sg * inversesqrt(l2));\n" \
    "  ds = min(ds, length(p - a - e * clamp(dot(p - a, e) / l2, 0.0, 1.0))); }\n" \
    " return dh > 0.0 ? ds : dh; }\n" \
    "void shade(vec2 px){\n" \
    " vec4 dst = U[0], src = U[1], info = U[2], col = U[3], col2 = U[4], misc = U[9];\n" \
    " int kind = int(info.x + 0.5), n = int(info.y + 0.5); float aa = max(info.z, 1e-4);\n" \
    " vec2 t = (px - dst.xy) / dst.zw;\n" \
    " if (kind == 0 && U[14].x > 0.5) t = vec2(1.0 - t.y, t.x);\n" \
    " vec2 s = mix(src.xy, src.zw, t);\n" \
    " if (kind == 4) { vec2 uv = vec2(dot(U[11].xyz, vec3(s, 1.0)), dot(U[12].xyz, vec3(s, 1.0))); o = vec4(texture(u_bot, uv).rgb, 1.0); return; }\n" \
    " float d;\n" \
    " if (U[23].x > 0.5) d = (texture(u_atlas, (U[23].yz + s) / vec2(textureSize(u_atlas, 0))).r * 255.0 - 128.0) / U[23].w - misc.y;\n" \
    " else if (n >= 3) d = poly_d(s, n) - misc.y;\n" \
    " else { vec2 q = max(src.xy - s, s - src.zw); d = max(q.x, q.y); }\n" \
    " float cover = clamp(0.5 - d / aa, 0.0, 1.0);\n" \
    " if (kind == 0) {\n" \
    "  vec2 sa = s + U[15].xy;\n" \
    "  if (U[15].w > 0.5) sa.x = max(sa.x, U[15].z);\n" \
    "  vec2 uv = vec2(dot(U[11].xyz, vec3(sa, 1.0)), dot(U[12].xyz, vec3(sa, 1.0)));\n" \
    "  vec3 c = texture(u_bot, uv).rgb;\n" \
    "  if (U[14].z > 0.5) { vec2 ts = vec2(textureSize(u_atlas, 0)), p = U[13].xy + sa - U[13].zw;\n" \
    "   c = vec3(texture(u_atlas, p / ts).r, texture(u_atlas, (p + vec2(U[14].w, 0.0)) / ts).r, texture(u_atlas, (p + vec2(2.0 * U[14].w, 0.0)) / ts).r); }\n" \
    "  c = mix(c, col.rgb, misc.w);\n" \
    "  float a = cover * misc.x * col.a;\n" \
    "  if (U[14].y > 0.5) a *= texture(u_atlas, (U[13].xy + s - U[13].zw) / vec2(textureSize(u_atlas, 0))).r;\n" \
    "  o = vec4(c, a);\n" \
    " } else if (kind == 1) {\n" \
    "  o = vec4(col.rgb, col.a * cover * misc.x);\n" \
    " } else if (kind == 2) {\n" \
    "  float g = d > 0.0 ? 1.0 - smoothstep(0.0, info.w, d) : clamp(1.0 + d / (aa * 2.0), 0.0, 1.0);\n" \
    "  o = vec4(col2.rgb, col2.a * g * g * misc.x);\n" \
    " } else if (kind == 6) {\n" \
    "  vec4 P = U[13], Q = U[14], R = U[15], rc = U[16], bd = U[17];\n" \
    "  float v = clamp((px.y - P.y) / P.w, 0.0, 1.0), ay;\n" \
    "  if (R.y > -0.5) { ay = (R.y + (px.y - P.y - P.w * 0.5) * R.z - rc.y) / (rc.w - rc.y); ay = rc.y + (1.0 - abs(mod(ay, 2.0) - 1.0)) * (rc.w - rc.y); }\n" \
    "  else { float top = bd.y > bd.x ? bd.x - rc.y : rc.w - rc.y, bot = bd.y > bd.x ? rc.w - bd.y : 0.0, tt = v * (top + bot);\n" \
    "   ay = tt < top ? rc.y + tt : bd.y + (tt - top); }\n" \
    "  float dx = px.x - P.x - P.z * 0.5;\n" \
    "  float ax = abs(Q.z) > 0.5 ? R.x + Q.z * abs(dx) * Q.w : R.x + dx * Q.w;\n" \
    "  ax = (ax - rc.x) / (rc.z - rc.x); ax = rc.x + (1.0 - abs(mod(ax, 2.0) - 1.0)) * (rc.z - rc.x);\n" \
    "  vec2 uv = vec2(dot(U[11].xyz, vec3(ax, ay, 1.0)), dot(U[12].xyz, vec3(ax, ay, 1.0)));\n" \
    "  vec3 c = texture(u_bot, uv).rgb;\n" \
    "  c += (1.0 - c) * Q.x * clamp(1.0 - v / 0.42, 0.0, 1.0);\n" \
    "  c *= 1.0 - Q.y * clamp((v - 0.55) / 0.45, 0.0, 1.0);\n" \
    "  o = vec4(c, cover * misc.x * col.a);\n" \
    " } else if (kind == 5) {\n" \
    "  float v = clamp((px.y - U[13].y) / U[13].w, 0.0, 1.0);\n" \
    "  float k = v * 6.0; int i = int(min(floor(k), 5.0)); float f = k - float(i); f = f * f * (3.0 - 2.0 * f);\n" \
    "  vec3 c = mix(U[16 + i].rgb, U[17 + i].rgb, f);\n" \
    "  c += (1.0 - c) * U[14].x * clamp(1.0 - v / 0.42, 0.0, 1.0);\n" \
    "  c *= 1.0 - U[14].y * clamp((v - 0.55) / 0.45, 0.0, 1.0);\n" \
    "  o = vec4(c, cover * misc.x * col.a);\n" \
    " } else {\n" \
    "  vec4 c = texture(u_atlas, s / vec2(textureSize(u_atlas, 0)));\n" \
    "  o = vec4(col.rgb, col.a * c.r * cover * misc.x);\n" \
    " }\n" \
    "}\n"

static const char *gl_vs =
    "#version 300 es\n"
    "precision highp float;\n"
    "uniform vec4 U[24];\n"
    "out vec2 v_px;\n"
    "void main(){\n"
    " vec4 dst = U[0], src = U[1], info = U[2], win = U[10];\n"
    " float k = dst.z / max(abs(src.z - src.x), 1e-3);\n"
    " float m = int(info.x + 0.5) == 2 ? info.w * k + 2.0 : 1.0;\n"
    " vec2 c = vec2(float(gl_VertexID & 1), float(gl_VertexID >> 1));\n"
    " vec2 px = dst.xy - m + c * (dst.zw + 2.0 * m);\n"
    " v_px = px;\n"
    " gl_Position = vec4(px.x / win.x * 2.0 - 1.0, 1.0 - px.y / win.y * 2.0, 0.0, 1.0);\n"
    "}\n";
static const char *gl_fs =
    "#version 300 es\n"
    "precision highp float;\n"
    "uniform vec4 U[24];\n"
    "uniform sampler2D u_bot; uniform sampler2D u_atlas;\n"
    "in vec2 v_px; out vec4 o;\n"
    GLSL_BODY
    "void main(){ shade(v_px); }\n";

/* ============================================================ our atlas
 * One channel of coverage: the masks of the game's own lettering and symbols (usum_masks.h, made
 * once from pictures of the bottom screen), so they can be cut cleanly out of the live picture, and
 * below them the Pokemon's icons as cut out of the arena while the commands are up. */
#include "usum_masks.h"
#define ATLAS_W 512
#define ATLAS_H 1024
#define MASK_W 256              /* the lettering's masks: the top left */
#define ICON_AY 136             /* the icons' masks: four boxes of ICON_W x ICON_H side by side */
#define ICON_W 46
#define ICON_H 40
/* the panel of a move's details (and of a Z-move's, its top raised): its shape as the distance from its edge
   (bottom-screen px; 8 steps a px, 128 on the edge, less inside), one below the other from row 256 */
#define MI_E 6.0f               /* its left end moved out a little (its rows carried on from column 1, where no text reaches) */
#define MI_M 16                 /* room round it */
typedef struct {
    float raise;                /* its top this much higher than a move's */
    int ay;                     /* its first row in the atlas */
    int sx0, sy0, w, h;         /* the bottom-screen point at the corner of its first texel; its size */
    float ymin, ymax;           /* its top and bottom */
} MiShape;
static MiShape mi_shape[2];
/* the back button as last seen on the moves screen (where the game has it lit): its red, green and blue side by side */
#define BACK_AX 256
#define BACK_SX 0
#define BACK_SY 204
#define BACK_W 44
#define BACK_H 36
#define BACK_STEP 48
static int back_saved;
static unsigned char atlas_px[ATLAS_H][ATLAS_W];
static int atlas_dy0, atlas_dy1;  /* the rows changed since it was last sent to the graphics card */
static void atlas_mark(int y0, int y1)
{
    if (atlas_dy1 <= atlas_dy0) { atlas_dy0 = y0; atlas_dy1 = y1; return; }
    if (y0 < atlas_dy0) atlas_dy0 = y0;
    if (y1 > atlas_dy1) atlas_dy1 = y1;
}
static float mi_edge(const float *e, float x, float shift)
{
    float c = x - 0.5f;
    if (c <= 0) return e[0] - shift;
    if (c >= 319) return e[319] - shift;
    int i = (int)c;
    return e[i] + (e[i + 1] - e[i]) * (c - i) - shift;
}
static float seg_dist(float px, float py, float ax, float ay, float bx, float by)
{
    float ex = bx - ax, ey = by - ay, l2 = ex * ex + ey * ey;
    float t = l2 > 0 ? ((px - ax) * ex + (py - ay) * ey) / l2 : 0;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    float dx = px - ax - t * ex, dy = py - ay - t * ey;
    return sqrtf(dx * dx + dy * dy);
}
static int mi_make(MiShape *m, float raise, int ay)
{
    m->raise = raise; m->ay = ay;
    m->ymin = 1e9f; m->ymax = -1e9f;
    for (int i = 0; i < 320; i++) {
        if (MI_TOP[i] - raise < m->ymin) m->ymin = MI_TOP[i] - raise;
        if (MI_BOT[i] > m->ymax) m->ymax = MI_BOT[i];
    }
    m->sx0 = -(int)MI_E - MI_M; m->sy0 = (int)floorf(m->ymin) - MI_M;
    m->w = 320 + MI_M - m->sx0; m->h = (int)ceilf(m->ymax) + MI_M - m->sy0;
    if (m->w > ATLAS_W || ay + m->h > ATLAS_H) { lg("move details: no room in the atlas"); m->h = 0; return ay; }
    /* round it: the top from the left end to the right, the right end, the bottom back, the left end */
    static float ex[650], ey[650];
    int n = 0;
    ex[n] = -MI_E; ey[n++] = MI_TOP[0] - raise;
    for (int i = 0; i < 320; i++) { ex[n] = i + 0.5f; ey[n++] = MI_TOP[i] - raise; }
    ex[n] = 320; ey[n++] = MI_TOP[319] - raise;
    ex[n] = 320; ey[n++] = MI_BOT[319];
    for (int i = 319; i >= 0; i--) { ex[n] = i + 0.5f; ey[n++] = MI_BOT[i]; }
    ex[n] = -MI_E; ey[n++] = MI_BOT[0];
    for (int j = 0; j < m->h; j++) for (int i = 0; i < m->w; i++) {
        float px = m->sx0 + i + 0.5f, py = m->sy0 + j + 0.5f, best = 1e9f;
        for (int a = 0; a < n; a++) {
            int b = a + 1 < n ? a + 1 : 0;
            float lo = ex[a] < ex[b] ? ex[a] : ex[b], hi = ex[a] < ex[b] ? ex[b] : ex[a];
            if (px < lo - 18 || px > hi + 18) continue;
            float d = seg_dist(px, py, ex[a], ey[a], ex[b], ey[b]);
            if (d < best) best = d;
        }
        int in = px >= -MI_E && px <= 320 && py >= mi_edge(MI_TOP, px, raise) && py <= mi_edge(MI_BOT, px, 0);
        int v = (int)lrintf(128 + (in ? -best : best) * 8);
        atlas_px[ay + j][i] = (unsigned char)(v < 0 ? 0 : v > 255 ? 255 : v);
    }
    return ay + m->h + 2;
}
#include "usum_glyphs.h"
#define XM_AY 690               /* (the X menu's masks, from this atlas row: see there) */
#define XM_CH 92
#define XM_GM 8                 /* the letters' margin of "far outside": wider than a window pixel and the filtering reach */
#define XM_GY (XM_AY + 2 * XM_CH + 2 + XM_GM)
static void atlas_make(void)
{
    memset(atlas_px, 0, sizeof atlas_px);
    for (int y = 0; y < MASK_ROWS; y++) for (int x = 0; x < MASK_W; x++)
        atlas_px[y][x] = (mask_bits[y][x >> 3] >> (x & 7)) & 1 ? 255 : 0;
    int ay = mi_make(&mi_shape[0], 0, 256);
    ay = mi_make(&mi_shape[1], MZ_RAISE, ay);
    if (ay > XM_AY) lg("atlas: the move details reach row %d, past the X menu's (%d)", ay, XM_AY);
    /* the letters L and R, each in a wide margin of "far outside" (a distance field read past its edge must not read
       the atlas's zeros, which would mean "deep inside") */
    for (int y = XM_GY - XM_GM; y < XM_GY + GLYPH_H + XM_GM; y++) for (int x = 0; x < 2 * (GLYPH_W + 2 * XM_GM); x++) {
        int g = x / (GLYPH_W + 2 * XM_GM), sx = x - g * (GLYPH_W + 2 * XM_GM) - XM_GM, sy = y - XM_GY;
        atlas_px[y][x] = sx < 0 || sy < 0 || sx >= GLYPH_W || sy >= GLYPH_H ? 255 : g ? GLYPH_R[sy][sx] : GLYPH_L[sy][sx];
    }
}
/* the back button's picture, while the game has it lit */
static void back_save(void)
{
    int changed = 0;
    for (int y = 0; y < BACK_H; y++) for (int x = 0; x < BACK_W; x++) for (int c = 0; c < 3; c++) {
        unsigned char v = rb[BACK_SY + y][BACK_SX + x][c], *a = &atlas_px[y][BACK_AX + c * BACK_STEP + x];
        if (*a != v) { *a = v; changed = 1; }
    }
    if (changed) atlas_mark(0, BACK_H);
    back_saved = 1;
}

/* ============================================== the menu: read and laid out */
static void dump_rb(const char *why)
{
    char fn[96];
    CreateDirectoryA(path_here("usumfix_dumps"), NULL);
    snprintf(fn, sizeof fn, "usumfix_dumps\\bottom_%06u.ppm", rb_frame);
    FILE *f = fopen(path_here(fn), "wb");
    if (!f) return;
    fprintf(f, "P6\n320 240\n255\n");
    for (int y = 0; y < 240; y++) for (int x = 0; x < 320; x++) fwrite(rb[y][x], 1, 3, f);
    fclose(f);
    lg("frame %u: bottom screen of frame %u written (%s)", frame, rb_frame, why);
}
static unsigned long long rb_sig_last;
static void rb_arrived(void)
{
    if (!debug_on) return;
    /* while collecting: each picture that differs enough from the last one written */
    static unsigned char last_dumped[30][40][3];
    if (file_here("usumfix_collect.txt")) {
        int diff = 0;
        for (int y = 0; y < 30; y++) for (int x = 0; x < 40; x++) for (int c = 0; c < 3; c++)
            if (abs(rb[y * 8 + 4][x * 8 + 4][c] - last_dumped[y][x][c]) > 48) { diff++; break; }
        if (diff > 60) {
            for (int y = 0; y < 30; y++) for (int x = 0; x < 40; x++) memcpy(last_dumped[y][x], rb[y * 8 + 4][x * 8 + 4], 3);
            dump_rb("new picture");
        }
    }
    if (file_here("usumfix_dump_now")) { DeleteFileA(path_here("usumfix_dump_now")); dump_rb("asked"); }
}

/* ---- telling the menus apart: pixels of the game's own pictures, as they are at 320x240 */
typedef struct { short x, y; unsigned char r, g, b; } Pt;
#define NPT(a) ((int)(sizeof a / sizeof a[0]))
static const Pt PT_ARENA[] = {      /* the battle arena's frame in the corners, lit (commands, between turns) */
    { 37, 1, 8, 109, 149 }, { 33, 9, 49, 193, 221 }, { 25, 2, 34, 190, 222 }, { 38, 13, 16, 157, 197 },
    { 286, 9, 49, 193, 221 }, { 280, 0, 8, 106, 149 }, { 293, 3, 65, 205, 230 }, { 281, 23, 126, 219, 255 } };
static const Pt PT_ARENA_DIM[] = {  /* the same, darkened (moves, targets, Z-moves, the yes/no question) */
    { 38, 0, 2, 31, 44 }, { 4, 10, 29, 46, 75 }, { 12, 2, 32, 45, 72 }, { 12, 10, 30, 47, 71 },
    { 289, 3, 2, 33, 44 }, { 280, 3, 2, 32, 46 }, { 296, 3, 63, 75, 75 }, { 317, 20, 75, 74, 75 } };
static const Pt PT_WAIT[] = {       /* the arena drawn back while a turn plays out */
    { 59, 0, 160, 168, 250 }, { 53, 1, 163, 154, 253 }, { 275, 1, 158, 158, 255 }, { 260, 2, 173, 189, 255 },
    { 56, 238, 134, 150, 241 }, { 50, 238, 124, 132, 239 }, { 277, 239, 123, 164, 247 }, { 268, 239, 140, 165, 255 },
    { 131, 135, 222, 246, 197 }, { 186, 101, 231, 239, 222 } };
static const Pt PT_RUN[] = {        /* RUN's tab (its cyan footprints and rim) */
    { 128, 221, 79, 236, 236 }, { 194, 212, 72, 236, 236 }, { 133, 221, 71, 233, 236 }, { 125, 224, 71, 232, 230 },
    { 137, 223, 71, 232, 230 }, { 121, 222, 79, 236, 229 }, { 198, 213, 79, 242, 229 }, { 130, 225, 71, 229, 230 } };
static const Pt PT_BACK[] = {       /* the back button, bottom left */
    { 11, 222, 255, 255, 255 }, { 4, 221, 107, 190, 255 }, { 17, 222, 189, 227, 255 }, { 23, 222, 107, 190, 255 },
    { 32, 221, 254, 247, 240 }, { 35, 230, 255, 255, 255 }, { 15, 231, 231, 247, 255 }, { 0, 231, 74, 182, 255 } };
static const Pt PT_ICIRC[] = {      /* a move's "i" button (the second move's; the others are 50 apart) */
    { 159, 77, 34, 153, 255 }, { 147, 82, 85, 187, 255 }, { 153, 82, 85, 187, 255 }, { 161, 83, 85, 187, 255 } };
static const Pt PT_HEX[4][5] = {    /* the target buttons' edges */
    { { 68, 42, 255, 255, 255 }, { 74, 42, 255, 255, 255 }, { 106, 43, 255, 255, 255 }, { 46, 42, 255, 255, 255 }, { 84, 43, 255, 255, 255 } },
    { { 233, 42, 255, 255, 255 }, { 239, 42, 255, 255, 255 }, { 252, 43, 255, 255, 255 }, { 218, 43, 255, 255, 255 }, { 246, 42, 255, 255, 255 } },
    { { 84, 128, 25, 127, 93 }, { 124, 128, 25, 127, 93 }, { 118, 128, 25, 127, 93 }, { 76, 128, 25, 127, 93 }, { 99, 128, 25, 127, 93 } },
    { { 274, 130, 13, 115, 81 }, { 268, 130, 13, 115, 81 }, { 209, 130, 15, 117, 83 }, { 186, 130, 15, 117, 83 }, { 180, 130, 15, 114, 81 } } };
static const Pt PT_YESNO[] = {
    { 230, 98, 0, 76, 164 }, { 257, 98, 0, 76, 164 }, { 302, 82, 0, 255, 162 }, { 212, 98, 0, 76, 164 }, { 218, 98, 0, 76, 164 },
    { 74, 165, 120, 232, 255 }, { 182, 165, 120, 232, 255 }, { 165, 165, 120, 232, 255 }, { 80, 165, 120, 232, 255 }, { 148, 165, 120, 232, 255 } };
static float pts_frac(const Pt *p, int n, int dx, int dy, int tol)
{
    int ok = 0;
    for (int i = 0; i < n; i++) {
        int x = p[i].x + dx, y = p[i].y + dy;
        if (x < 0 || y < 0 || x >= 320 || y >= 240) continue;
        const unsigned char *c = rb[y][x];
        if (abs(c[0] - p[i].r) <= tol && abs(c[1] - p[i].g) <= tol && abs(c[2] - p[i].b) <= tol) ok++;
    }
    return n ? (float)ok / n : 0;
}
enum { C_FIGHT, C_FIGHT2, C_GOLD, C_BAG, C_BRIGHT, C_RED, C_RUN, C_TABBLUE, C_PURPLE, C_ORANGE, C_ORANGE2 };
static int px_is(int kind, const unsigned char *c)
{
    int R = c[0], G = c[1], B = c[2];
    switch (kind) {
    case C_FIGHT: return R > 150 && G < R - 60;
    case C_FIGHT2: return R > 150 && G < R - 40;
    case C_GOLD: return R > 200 && G > 160 && B < 120;
    case C_BAG: return R > 220 && G > 120 && G < 235 && B < 100;
    case C_ORANGE: return R > 230 && G > 120 && G < 190 && B < 90;
    case C_ORANGE2: return R > 220 && G > 60 && G < 160 && B < 100;
    case C_BRIGHT: return R + G + B > 450;
    case C_RED: return R > 190 && G < 90 && B < 90;
    case C_RUN: return B > 180 && R < 130 && B > G;
    case C_TABBLUE: return B > 150 && R < 60 && G < 150;
    case C_PURPLE: return R > 110 && B > 140 && G < 100 && R > G + 40;
    }
    return 0;
}
static float region_frac(int x0, int y0, int x1, int y1, int kind)
{
    int n = 0, ok = 0;
    for (int y = y0; y < y1; y += 2) for (int x = x0; x < x1; x += 2) { n++; ok += px_is(kind, rb[y][x]); }
    return n ? (float)ok / n : 0;
}
/* the game's red cursor arrow in a box (it bobs a few pixels) */
static int red_in(int x0, int y0, int x1, int y1)
{
    int n = 0;
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) if (x >= 0 && y >= 0 && x < 320 && y < 240) n += px_is(C_RED, rb[y][x]);
    return n >= 8;
}

typedef struct { int n; float p[8][2]; } Poly;
static Poly poly_hex(int i);
static int poly_has(const Poly *p, float x, float y, float g);
enum { S_NONE, S_IDLE, S_CMD, S_MOVES, S_ZMOVE, S_TARGETS, S_YESNO, S_MOVEINFO };
/* the arena's info buttons (the orange "+" by each Pokemon): the foes' and ours, left and right as in a double battle;
   in a single battle the foe's is where the right-hand one is, ours where the left-hand one is */
static const float INFO_AT[4][2] = { { 130.5f, 52.5f }, { 182.5f, 60.5f }, { 98.5f, 122.5f }, { 150.5f, 130.5f } };
static const char *state_name[] = { "none", "between turns", "commands", "moves", "Z-move", "targets", "yes/no", "move info" };
typedef struct {
    int state;
    int bag;           /* commands: BAG is there, under POKEMON (not in the Battle Tree) */
    int ball;          /* commands: the quick Poke Ball button is there (wild battles) */
    int weather;       /* commands: the weather tab is there (in BAG's place) */
    int run;           /* commands: RUN is there */
    int back;          /* commands: the back button is there (a second Pokemon's turn) */
    int info[4];       /* commands: the arena's info buttons, for the foe on the left and right, our left and right */
    int bars[4];       /* moves: which of the four are there */
    int big;           /* moves: the Z-Power / Mega Evolution / Ultra Burst button is there */
    int big_on;        /* moves: Mega Evolution switched on (its purple aura) */
    int sel;           /* the game's cursor: move 0-3, 4 the big button (moves); target 0-3 */
    int zinfo;         /* move details: a Z-move's */
} Scene;
static Scene read_scene(void)
{
    Scene s; memset(&s, 0, sizeof s); s.sel = -1;
    {
        /* a move's details: the white of its labels (Category, Power, Accuracy, PP); a Z-move's (Category, Power, PP) */
        int ok = 0, n = (int)(sizeof MI_PT / sizeof MI_PT[0]);
        for (int i = 0; i < n; i++) { const unsigned char *c = rb[MI_PT[i][1]][MI_PT[i][0]]; ok += c[0] >= 200 && c[1] >= 200 && c[2] >= 200; }
        if (ok * 8 >= n * 7) { s.state = S_MOVEINFO; return s; }
        ok = 0; n = (int)(sizeof MZ_PT / sizeof MZ_PT[0]);
        for (int i = 0; i < n; i++) { const unsigned char *c = rb[MZ_PT[i][1]][MZ_PT[i][0]]; ok += c[0] >= 200 && c[1] >= 200 && c[2] >= 200; }
        if (ok * 16 >= n * 15) { s.state = S_MOVEINFO; s.zinfo = 1; return s; }
    }
    float lit = pts_frac(PT_ARENA, NPT(PT_ARENA), 0, 0, 36);
    float dim = pts_frac(PT_ARENA_DIM, NPT(PT_ARENA_DIM), 0, 0, 36);
    float back = pts_frac(PT_BACK, NPT(PT_BACK), 0, 0, 36);
    if (dim >= 0.75f && back >= 0.75f) {
        int nb = 0;
        for (int b = 0; b < 4; b++) { s.bars[b] = pts_frac(PT_ICIRC, NPT(PT_ICIRC), 0, 50 * (b - 1), 36) >= 0.75f; nb += s.bars[b]; }
        int hexes = 0;
        for (int i = 0; i < 4; i++) hexes += pts_frac(PT_HEX[i], 5, 0, 0, 36) >= 0.6f;
        if (hexes && !nb) {
            s.state = S_TARGETS;
            /* the arrow left of a target's tip: its red outside the hexagons (inside, the foes' own red would count) */
            int best = 0;
            for (int i = 0; i < 4; i++) {
                float tx = (i & 1) ? 165.0f : 13.0f, ty = i < 2 ? 78.5f : 162.5f;
                int n = 0;
                for (int y = (int)ty - 14; y <= (int)ty + 14; y++) for (int x = (int)tx - 14; x <= (int)tx + 10; x++) {
                    if (x < 0 || y < 0 || x >= 320 || y >= 240 || !px_is(C_RED, rb[y][x])) continue;
                    int in = 0;
                    for (int j = 0; j < 4 && !in; j++) { Poly h = poly_hex(j); in = poly_has(&h, x + 0.5f, y + 0.5f, 0.5f); }
                    if (!in) n++;
                }
                if (n > best) { best = n; s.sel = i; }
            }
        } else if (s.bars[0]) {
            s.state = S_MOVES;
            s.big = region_frac(10, 60, 90, 170, C_BRIGHT) > 0.3f;
            s.big_on = s.big && region_frac(84, 20, 140, 60, C_PURPLE) + region_frac(126, 60, 142, 180, C_PURPLE) + region_frac(84, 180, 140, 215, C_PURPLE) > 0.12f;
            for (int b = 0; b < 4; b++) if (s.bars[b] && red_in(142, 38 + 50 * b, 160, 56 + 50 * b)) s.sel = b;
            if (s.big && red_in(8, 92, 30, 112)) s.sel = 4;
        } else if (region_frac(100, 182, 300, 214, C_GOLD) > 0.5f) {
            s.state = S_ZMOVE;
            for (int b = 0; b < 4; b++) s.bars[b] = 1;   /* darkened behind the Z-move */
        }
        if (s.state == S_NONE) {
            static unsigned told;
            if (rb_frame - told > 60) { told = rb_frame; lg("frame %u: dark arena and back button but no menu: bars %d, targets %d, gold %.2f", rb_frame, nb, hexes, region_frac(100, 182, 300, 214, C_GOLD)); }
        }
    } else if (dim >= 0.75f) {
        if (pts_frac(PT_YESNO, NPT(PT_YESNO), 0, 0, 36) >= 0.8f) s.state = S_YESNO;
        else {
            /* the commands, darkened: on the way in or out (FIGHT's lettering still there, darkened; not so while
               a move's details open over the moves) */
            const MaskInfo *mi = &MASKS[M_FIGHT];
            float d = 0, l = 0;
            for (int i = 0; i < mi->np; i++) { const unsigned char *c = rb[mi->plate[i][1]][mi->plate[i][0]]; d += (0.299f * c[0] + 0.587f * c[1] + 0.114f * c[2]) / 255; }
            for (int i = 0; i < mi->nf; i++) { const unsigned char *c = rb[mi->fill[i][1]][mi->fill[i][0]]; l += (0.299f * c[0] + 0.587f * c[1] + 0.114f * c[2]) / 255; }
            if (mi->np && mi->nf && l / mi->nf - d / mi->np >= 0.12f) s.state = S_CMD;
        }
    } else if (pts_frac(PT_WAIT, NPT(PT_WAIT), 0, 0, 30) >= 0.8f) {
        s.state = S_IDLE;
    } else if (lit >= 0.75f) {
        if (region_frac(262, 36, 316, 76, C_FIGHT) >= 0.2f && region_frac(262, 160, 316, 200, C_FIGHT2) >= 0.2f) s.state = S_CMD;
        else s.state = S_IDLE;
    }
    if (s.state == S_CMD) {
        s.bag = region_frac(5, 135, 55, 175, C_BAG) >= 0.5f;
        s.ball = region_frac(27, 19, 38, 30, C_ORANGE) >= 0.3f;
        s.weather = region_frac(128, 8, 191, 18, C_TABBLUE) >= 0.5f;
        s.run = pts_frac(PT_RUN, NPT(PT_RUN), 0, 0, 36) >= 0.5f;
        s.back = pts_frac(PT_BACK, NPT(PT_BACK), 0, 0, 36) >= 0.75f;
        for (int i = 0; i < 4; i++) {
            int cx = (int)INFO_AT[i][0], cy = (int)INFO_AT[i][1];
            const unsigned char *c = rb[cy][cx];
            s.info[i] = region_frac(cx - 7, cy - 7, cx + 8, cy + 8, C_ORANGE2) >= 0.3f && c[0] > 200 && c[1] > 200 && c[2] > 200;
        }
    }
    return s;
}

/* ---- the pieces we cut out of the bottom screen (bottom-screen pixels), round each shape */
static const Poly POLY_MON = { 5, { { -1, 84 }, { 68, 84 }, { 81, 120 }, { 68, 156 }, { -1, 156 } } };
static const Poly POLY_FIGHT = { 5, { { 321, 84 }, { 251, 84 }, { 239, 120 }, { 251, 156 }, { 321, 156 } } };
static const Poly POLY_RUN = { 4, { { 85, 241 }, { 105, 197 }, { 215, 197 }, { 235, 241 } } };
/* with BAG (wild and other trainers' battles) POKEMON and BAG share the left side, and wild battles add the ball button */
static const Poly POLY_MON_W = { 5, { { -1, 62 }, { 56, 62 }, { 66, 72 }, { 77, 114 }, { -1, 114 } } };
static const Poly POLY_BAG_W = { 5, { { -1, 128 }, { 79, 128 }, { 71, 166 }, { 59, 179 }, { -1, 212 } } };
static const Poly POLY_FIGHT_T = { 7, { { 321, 60 }, { 263, 60 }, { 256, 66 }, { 242, 120 }, { 256, 172 }, { 278, 190 }, { 321, 190 } } };
static const Poly POLY_BALL = { 4, { { -1, -1 }, { 34, -1 }, { 34, 30 }, { -1, 30 } } };
static const Poly POLY_YBADGE = { 8, { { 40, 24.5f }, { 37.8f, 29.8f }, { 32.5f, 32 }, { 27.2f, 29.8f }, { 25, 24.5f }, { 27.2f, 19.2f }, { 32.5f, 17 }, { 37.8f, 19.2f } } };
static const Poly POLY_WEATHER = { 6, { { 121, -1 }, { 199, -1 }, { 196, 21 }, { 185, 33 }, { 135, 33 }, { 124, 21 } } };
static const Poly POLY_BIG = { 5, { { -1, 16 }, { 80, 50 }, { 126, 120 }, { 80, 190 }, { -1, 212 } } };
static const Poly POLY_BACK = { 4, { { -1, 211.5f }, { 35.5f, 211.5f }, { 35.5f, 241 }, { -1, 241 } } };
static const Poly POLY_BADGE = { 8, { { 41.6f, 218 }, { 39.2f, 223.7f }, { 33.5f, 226.1f }, { 27.8f, 223.7f }, { 25.4f, 218 }, { 27.8f, 212.3f }, { 33.5f, 209.9f }, { 39.2f, 212.3f } } };
/* the effectiveness tag under a move, a pill reaching past the move's own bottom edge */
static Poly poly_tag(int b)
{
    float T = 22.0f + 50 * b;
    Poly p = { 4, { { 246, T + 47 }, { 321, T + 47 }, { 321, T + 48 }, { 246, T + 48 } } };
    return p;
}
static const Poly POLY_ZBAR = { 5, { { 53, 173 }, { 321, 173 }, { 321, 226 }, { 68, 226 }, { 53, 204 } } };
static const Poly POLY_YES = { 4, { { 8, 73 }, { 306, 73 }, { 306, 104 }, { 8, 104 } } };
static const Poly POLY_NO = { 4, { { 10, 139 }, { 304, 139 }, { 304, 169 }, { 10, 169 } } };
static Poly poly_bar(int b)
{
    float T = 22.0f + 50 * b;
    Poly p = { 5, { { 144, T }, { 321, T }, { 321, T + 50 }, { 161, T + 50 }, { 145, T + 28 } } };
    return p;
}
/* a target's plate, round the outside of its white rim (measured: tips at x0 and x0 + 142, top and bottom edges
   from x0 + 16 to x0 + 126, 72 tall; the effectiveness pill sits inside its bottom edge) */
static Poly poly_hex(int i)
{
    float x0 = (i & 1) ? 165.0f : 12.0f, y0 = i < 2 ? 42.0f : 126.0f;
    Poly p = { 6, { { x0, y0 + 36 }, { x0 + 16, y0 }, { x0 + 126, y0 }, { x0 + 142, y0 + 36 }, { x0 + 126, y0 + 72 }, { x0 + 16, y0 + 72 } } };
    return p;
}
static void poly_box(const Poly *p, float *b)   /* its bounds, kept inside the bottom screen */
{
    b[0] = b[1] = 1e9f; b[2] = b[3] = -1e9f;
    for (int i = 0; i < p->n; i++) {
        if (p->p[i][0] < b[0]) b[0] = p->p[i][0];
        if (p->p[i][1] < b[1]) b[1] = p->p[i][1];
        if (p->p[i][0] > b[2]) b[2] = p->p[i][0];
        if (p->p[i][1] > b[3]) b[3] = p->p[i][1];
    }
    if (b[0] < 0) b[0] = 0;
    if (b[1] < 0) b[1] = 0;
    if (b[2] > 320) b[2] = 320;
    if (b[3] > 240) b[3] = 240;
}

/* ------------------------------------------------------------- input map */
typedef struct { float dst[4], src[4]; Poly poly; float grow; float fx, fy; } Hit;   /* a piece on screen and the bottom-screen part it shows; fx >= 0: always press there */
#define MAXHIT 40
static struct {
    int active;
    float dw, dh;          /* the picture's size the rectangles are in */
    float box[4];
    int nhit; Hit hit[MAXHIT];
} lay_pub;
static CRITICAL_SECTION lay_cs;
static int nhit_new; static Hit hit_new[MAXHIT];
static float info_box[4], info_box_new[4];   /* where the info column is (picture px), for the wheel: as published, and this frame's */
static void publish(int active)
{
    EnterCriticalSection(&lay_cs);
    memcpy(info_box, info_box_new, sizeof info_box);
    if (!active) info_box[2] = info_box[3] = 0;
    lay_pub.active = active;
    lay_pub.dw = win_w; lay_pub.dh = win_h;
    memcpy(lay_pub.box, box, sizeof box);
    lay_pub.nhit = active ? nhit_new : 0;
    memcpy(lay_pub.hit, hit_new, sizeof hit_new);
    LeaveCriticalSection(&lay_cs);
}
/* inside a convex polygon grown by g (bottom-screen pixels) */
static int poly_has(const Poly *p, float x, float y, float g)
{
    float ar = 0;
    for (int i = 0; i < p->n; i++) { const float *a = p->p[i], *b = p->p[(i + 1) % p->n]; ar += a[0] * b[1] - b[0] * a[1]; }
    float sg = ar >= 0 ? 1.0f : -1.0f;
    for (int i = 0; i < p->n; i++) {
        const float *a = p->p[i], *b = p->p[(i + 1) % p->n];
        float ex = b[0] - a[0], ey = b[1] - a[1], l = sqrtf(ex * ex + ey * ey);
        if (l < 1e-6f) continue;
        float nx = ey / l * sg, ny = -ex / l * sg;
        if ((x - a[0]) * nx + (y - a[1]) * ny > g) return 0;
    }
    return 1;
}

/* ---- drawing a piece: where it goes (window pixels, its bounds' top left) and how big (window px per bottom-screen px) */
static const float DARK[4] = { 0.05f, 0.07f, 0.15f, 1 };
static const float LIGHT[4] = { 0.95f, 0.98f, 1.0f, 1 };
static const float GOLD[4] = { 1.0f, 0.84f, 0.32f, 1 };
enum { F_NONE = 0, F_DARK = 1, F_LIGHT = 2 };
typedef struct { Poly poly; float x, y, k; int frame; int lit; int on; int hit; float alpha, round; float fx, fy; int flat; float adx, ady; } Piece;
#define MAXPIECE 28
static Piece pieces[MAXPIECE];
static int npieces;
static Piece *piece(const Poly *p, float x, float y, float k, int frame, int hit)
{
    if (npieces >= MAXPIECE) return NULL;
    Piece *c = &pieces[npieces++];
    c->poly = *p; c->x = x; c->y = y; c->k = k; c->frame = frame; c->hit = hit; c->lit = 0; c->on = 0; c->alpha = 1; c->round = 0; c->fx = c->fy = -1;
    c->flat = 0; c->adx = c->ady = 0;
    return c;
}
/* a piece that keeps its place beside another, as on the bottom screen */
static Piece *piece_with(const Poly *p, const Piece *by, int frame, int hit)
{
    if (!by) return NULL;
    float a[4], b[4]; poly_box(&by->poly, a); poly_box(p, b);
    return piece(p, by->x + (b[0] - a[0]) * by->k, by->y + (b[1] - a[1]) * by->k, by->k, frame, hit);
}
static void piece_size(const Poly *p, float k, float *w, float *h) { float b[4]; poly_box(p, b); *w = (b[2] - b[0]) * k; *h = (b[3] - b[1]) * k; }
static Quad *piece_quad(const Piece *c, int kind, float grow, float dx, float dy)
{
    float b[4]; poly_box(&c->poly, b);
    float g = grow + c->round + 2;      /* room round the shape for its frame and glow */
    Quad *q = q_add(kind, c->x - g * c->k + dx, c->y - g * c->k + dy, (b[2] - b[0] + 2 * g) * c->k, (b[3] - b[1] + 2 * g) * c->k);
    if (!q) return NULL;
    q_src(q, b[0] - g, b[1] - g, b[2] + g, b[3] + g);
    float pts[16];
    for (int i = 0; i < c->poly.n; i++) { pts[2 * i] = c->poly.p[i][0]; pts[2 * i + 1] = c->poly.p[i][1]; }
    q_poly(q, c->poly.n, pts);
    q->misc[0] = c->alpha;
    return q;
}
static void pieces_draw(void)
{
    /* shadows under everything, then the glow of the chosen one, then each piece in its frame */
    for (int i = 0; i < npieces; i++) {
        Piece *c = &pieces[i];
        if (c->flat) continue;
        Quad *q = piece_quad(c, K_GLOW, 6, 2.0f * c->k, 3.0f * c->k);
        if (q) { q->info[3] = 5; q->col2[0] = q->col2[1] = q->col2[2] = 0; q->col2[3] = 0.5f; q->misc[1] = c->round + (c->frame ? 2.2f : 0.8f); }
    }
    /* switched on (Mega Evolution): the game's purple aura, drawn round the piece, breathing slowly */
    static const float MAGENTA[4] = { 0.86f, 0.25f, 1.0f, 1 };
    float breath = 0.8f + 0.2f * sinf(frame * 0.12f);
    for (int i = 0; i < npieces; i++) if (pieces[i].on) {
        Piece *c = &pieces[i];
        for (int pass = 0; pass < 2; pass++) {
            Quad *q = piece_quad(c, K_GLOW, pass ? 8 : 16, 0, 0);
            if (q) { q->info[3] = pass ? 7 : 15; memcpy(q->col2, MAGENTA, sizeof MAGENTA); q->col2[3] = breath; q->misc[1] = c->round + (c->frame ? 2.4f : 1.2f); }
        }
    }
    for (int i = 0; i < npieces; i++) if (pieces[i].lit && !pieces[i].on) {
        Piece *c = &pieces[i];
        Quad *q = piece_quad(c, K_GLOW, 9, 0, 0);
        if (q) { q->info[3] = 7; memcpy(q->col2, GOLD, sizeof GOLD); q->misc[1] = c->round + (c->frame ? 2.4f : 1.2f); }
    }
    for (int i = 0; i < npieces; i++) {
        Piece *c = &pieces[i];
        Quad *q;
        if (c->frame & F_DARK) { q = piece_quad(c, K_SOLID, 3, 0, 0); if (q) { memcpy(q->col, DARK, sizeof DARK); q->misc[1] = c->round + ((c->frame & F_LIGHT) ? 2.4f : 1.3f); } }
        if (c->frame & F_LIGHT) { q = piece_quad(c, K_SOLID, 3, 0, 0); if (q) { memcpy(q->col, LIGHT, sizeof LIGHT); q->misc[1] = c->round + 1.1f; } }
        q = piece_quad(c, K_ART, 0, 0, 0);
        if (q) { q->misc[1] = c->round; q->pad[2][0] = c->adx; q->pad[2][1] = c->ady; }
        if (c->hit && nhit_new < MAXHIT) {
            float b[4]; poly_box(&c->poly, b);
            Hit *h = &hit_new[nhit_new++];
            h->dst[0] = c->x; h->dst[1] = c->y; h->dst[2] = (b[2] - b[0]) * c->k; h->dst[3] = (b[3] - b[1]) * c->k;
            memcpy(h->src, b, sizeof b);
            h->poly = c->poly;
            h->grow = (c->frame ? 2.5f : 1.0f);
            h->fx = c->fx; h->fy = c->fy;
        }
    }
}

/* ---- the layouts. u: window pixels per pixel of a 1280x720 window */
static void lay_bottom_row(const Scene *s, float k, float gap, float mid, float bottom, float wr, float hr)
{
    if (s->run || !s->back) piece(&POLY_RUN, mid - wr / 2, bottom - hr, k, F_DARK | F_LIGHT, 1);
    else {
        /* a second Pokemon's turn: the back button in RUN's place */
        float kb = 1.35f * k, wk, hk; piece_size(&POLY_BACK, kb, &wk, &hk);
        Piece *bk = piece(&POLY_BACK, mid - wk / 2, bottom - hk, kb, F_DARK | F_LIGHT, 1);
        if (bk) bk->round = 2;
        Piece *bd = piece_with(&POLY_BADGE, bk, F_DARK, 1);
        if (bd) { bd->fx = 16; bd->fy = 226; }
    }
    (void)gap;
}
static void lay_weather(const Scene *s, float k, float mid, float top)
{
    if (!s->weather) return;
    float kw = 0.8f * k, ww, hw; piece_size(&POLY_WEATHER, kw, &ww, &hw);
    Piece *w = piece(&POLY_WEATHER, mid - ww / 2, top - hw, kw, F_DARK | F_LIGHT, 1);
    if (w) w->round = 1;
}
/* the arena's info buttons, each with its Pokemon: a column of chips left of the commands, the foes over ours as in the arena */
static void lay_info(const Scene *s, float k, float right, float bottom)
{
    float gap = 4 * k, y = bottom;
    for (int i = 3; i >= 0; i--) {
        if (!s->info[i]) continue;
        float cx = INFO_AT[i][0], cy = INFO_AT[i][1];
        Poly chip = { 6, { { cx - 9, cy - 3 }, { cx - 3, cy - 9 }, { cx + 36, cy - 9 }, { cx + 40, cy - 5 }, { cx + 40, cy + 36 }, { cx - 9, cy + 36 } } };
        float w, h; piece_size(&chip, k, &w, &h);
        Piece *c = piece(&chip, right - w, y - h, k, F_DARK | F_LIGHT, 1);
        if (c) { c->round = 2; c->fx = cx; c->fy = cy; }
        y -= h + gap;
        if (i == 2 && (s->info[0] || s->info[1])) y -= 2 * gap;   /* a little apart: ours, then the foes */
    }
}
static void lay_commands_old(const Scene *s, float u)
{
    float right = lay_w - 22 * u, bottom = lay_h - 16 * u;
    float wr, hr;
    if (!s->bag) {
        /* the Battle Tree: POKEMON and FIGHT side by side, RUN (or back) under them */
        float k = 1.5f * u, gap = 6 * k;
        float wm, hm, wf, hf;
        piece_size(&POLY_MON, k, &wm, &hm); piece_size(&POLY_FIGHT, k, &wf, &hf); piece_size(&POLY_RUN, k, &wr, &hr);
        float rowy = bottom - hr - gap - hf;
        float mid = right - wf - gap / 2;
        piece(&POLY_MON, right - wf - gap - wm, rowy, k, F_DARK | F_LIGHT, 1);
        piece(&POLY_FIGHT, right - wf, rowy, k, F_DARK | F_LIGHT, 1);
        lay_bottom_row(s, k, gap, mid, bottom, wr, hr);
        lay_weather(s, k, mid, rowy - gap);
        lay_info(s, 0.9f * u, right - wf - gap - wm - 2 * gap, bottom);
        return;
    }
    /* with BAG: POKEMON over BAG as in the game, FIGHT beside them, RUN (or back) under, the ball button by RUN */
    float k = 1.3f * u, gap = 5 * k;
    float wm, hm, wb, hb, wf, hf;
    piece_size(&POLY_MON_W, k, &wm, &hm); piece_size(&POLY_BAG_W, k, &wb, &hb);
    piece_size(&POLY_FIGHT_T, k, &wf, &hf); piece_size(&POLY_RUN, k, &wr, &hr);
    float lh = hm + gap + hb, colh = lh > hf ? lh : hf;
    float top = bottom - hr - gap - colh;
    float lw = wm > wb ? wm : wb;
    float lx = right - wf - gap - lw;
    piece(&POLY_MON_W, lx, top + (colh - lh) / 2, k, F_DARK | F_LIGHT, 1);
    piece(&POLY_BAG_W, lx, top + (colh - lh) / 2 + hm + gap, k, F_DARK | F_LIGHT, 1);
    piece(&POLY_FIGHT_T, right - wf, top + (colh - hf) / 2, k, F_DARK | F_LIGHT, 1);
    float mid = (lx + right) / 2;
    lay_bottom_row(s, k, gap, mid, bottom, wr, hr);
    if (s->ball) {
        float kb = 1.2f * k, wl, hl; piece_size(&POLY_BALL, kb, &wl, &hl);
        Piece *bl = piece(&POLY_BALL, mid - wr / 2 - gap * 2 - wl, bottom - hl, kb, F_DARK | F_LIGHT, 1);
        if (bl) bl->round = 2;
        Piece *yb = piece_with(&POLY_YBADGE, bl, F_DARK, 1);
        if (yb) { yb->fx = 16; yb->fy = 14; }
    }
    lay_weather(s, k, mid, top - gap);
    float left = lx;
    if (s->ball) { float wl, hl; piece_size(&POLY_BALL, 1.2f * k, &wl, &hl); if (mid - wr / 2 - gap * 2 - wl < left) left = mid - wr / 2 - gap * 2 - wl; }
    lay_info(s, 0.9f * u, left - 2 * gap, bottom);
}
/* ================================================ our own panels (window pixels)
 * The new look: clean panels of our own, each filled with its button's own colours (a gradient taken
 * from the live bottom screen) and carrying the game's own lettering, cut out by its mask and centred
 * on a pill of its own plate's colour. Small buttons (back, the quick ball, the weather) show a clean
 * square of their tab. */
static float ptr_x = -1e9f, ptr_y = -1e9f;   /* the pointer (picture px), from the program's mouse events */
static int ptr_in(float x, float y, float w, float h)
{
    float px = ptr_x - lay_off[0], py = ptr_y - lay_off[1];
    return px >= x && py >= y && px < x + w && py < y + h;
}
typedef struct { float x, y, w, h, c; int round; } Pan;   /* where, and its corners: cut by c, or rounded by c */
enum { PF_LIGHT = 1, PF_LIT = 2, PF_ON = 4, PF_HOVER = 8 };
/* its outline grown by g: the polygon (window px) and the rounding */
static int pan_poly(const Pan *p, float g, float *pts, float *rnd)
{
    float x = p->x, y = p->y, w = p->w, h = p->h;
    if (p->round) {
        float r = p->c;
        if (r > w / 2 - 0.25f) r = w / 2 - 0.25f;
        if (r > h / 2 - 0.25f) r = h / 2 - 0.25f;
        if (r < 0) r = 0;
        float q[8] = { x + r, y + r, x + w - r, y + r, x + w - r, y + h - r, x + r, y + h - r };
        memcpy(pts, q, sizeof q); *rnd = r + g;
        return 4;
    }
    float c = p->c + g * 0.4142f;
    x -= g; y -= g; w += 2 * g; h += 2 * g;
    float q[16] = { x + c, y, x + w - c, y, x + w, y + c, x + w, y + h - c, x + w - c, y + h, x + c, y + h, x, y + h - c, x, y + c };
    memcpy(pts, q, sizeof q); *rnd = 0;
    return 8;
}
/* a quad over the panel, its outline grown by g, with room m round it (src = the window itself) */
static Quad *pan_quad(int kind, const Pan *p, float g, float m)
{
    float pts[16], rnd; int n = pan_poly(p, g, pts, &rnd);
    float e = g + m + 1.5f;
    Quad *q = q_add(kind, p->x - e, p->y - e, p->w + 2 * e, p->h + 2 * e);
    if (!q) return NULL;
    q_src(q, p->x - e, p->y - e, p->x + p->w + e, p->y + p->h + e);
    q_poly(q, n, pts);
    q->misc[1] = rnd;
    return q;
}
static void pan_shadow(const Pan *p, float u)
{
    Quad *q = pan_quad(K_GLOW, p, 0, 0);
    if (!q) return;
    q->dst[0] += 2 * u; q->dst[1] += 3 * u;
    q->info[3] = 8 * u; q->col2[0] = q->col2[1] = q->col2[2] = 0; q->col2[3] = 0.45f;
}
static const float MAGENTA[4] = { 0.86f, 0.25f, 1.0f, 1 };
static const float *frame_dark = NULL, *frame_light = NULL;   /* (other frame colours for the next frame drawn) */
static void pan_frame(const Pan *p, int f, float u)
{
    Quad *q;
    if (f & PF_ON) {
        float breath = 0.8f + 0.2f * sinf(frame * 0.12f);
        for (int pass = 0; pass < 2; pass++)
            if ((q = pan_quad(K_GLOW, p, 2.5f * u, 0))) { q->info[3] = (pass ? 8 : 16) * u; memcpy(q->col2, MAGENTA, sizeof MAGENTA); q->col2[3] = breath; }
    } else if (f & PF_LIT) {
        if ((q = pan_quad(K_GLOW, p, 2.5f * u, 0))) { q->info[3] = 10 * u; memcpy(q->col2, GOLD, sizeof GOLD); }
    } else if (f & PF_HOVER) {
        if ((q = pan_quad(K_GLOW, p, 2.5f * u, 0))) { q->info[3] = 7 * u; q->col2[0] = q->col2[1] = q->col2[2] = 1; q->col2[3] = 0.55f; }
    }
    if ((q = pan_quad(K_SOLID, p, 2.5f * u, 0))) memcpy(q->col, frame_dark ? frame_dark : DARK, sizeof DARK);
    if ((f & PF_LIGHT) && (q = pan_quad(K_SOLID, p, 1.0f * u, 0))) memcpy(q->col, frame_light ? frame_light : LIGHT, sizeof LIGHT);
    frame_dark = frame_light = NULL;
}
/* the body: a gradient of the button's own colours, top to bottom */
static void pan_body(const Pan *p, float st[7][4], float gloss, float shade)
{
    Quad *q = pan_quad(K_BODY, p, 0, 0);
    if (!q) return;
    q->pad[0][0] = p->x; q->pad[0][1] = p->y; q->pad[0][2] = p->w; q->pad[0][3] = p->h;
    q->pad[1][0] = gloss; q->pad[1][1] = shade;
    for (int i = 0; i < 7; i++) memcpy(q->ext[i], st[i], sizeof st[i]);
}
/* how a button's own picture is refitted to a panel: its clean rectangle (x0, y0)-(x1, y1), folded back at its
   edges; across, sx art px a 1280x720 px from art column acx at the panel's middle (mirror -1 / +1: both halves
   going left / right from there); down, either the rectangle's rows less b0-b1 spread over the panel's height,
   or (acy >= 0) sy art px a 1280x720 px from art row acy at the middle */
typedef struct { float x0, y0, x1, y1, b0, b1, acx, sx, acy, sy; int mirror; } Fit;
static void pan_artbody(const Pan *p, const Fit *f, float u, float gloss, float shade)
{
    Quad *q = pan_quad(K_ARTBODY, p, 0, 0);
    if (!q) return;
    q->pad[0][0] = p->x; q->pad[0][1] = p->y; q->pad[0][2] = p->w; q->pad[0][3] = p->h;
    q->pad[1][0] = gloss; q->pad[1][1] = shade; q->pad[1][2] = (float)f->mirror; q->pad[1][3] = f->sx / u;
    q->pad[2][0] = f->acx; q->pad[2][1] = f->acy; q->pad[2][2] = f->sy / u;
    q->ext[0][0] = f->x0; q->ext[0][1] = f->y0; q->ext[0][2] = f->x1; q->ext[0][3] = f->y1;
    q->ext[1][0] = f->b0; q->ext[1][1] = f->b1;
}
static void pan_solid(const Pan *p, const float *c, float a)
{
    Quad *q = pan_quad(K_SOLID, p, 0, 0);
    if (q) { q_col(q, c[0], c[1], c[2], a); }
}
/* a rectangle (x, y, w, h) showing the bottom screen's (sx0, sy0)-(sx1, sy1), with room m round it */
static int art_planes;      /* (art_quad: the picture from our saved copy of the back button, not the live screen) */
static Quad *art_quad(float x, float y, float w, float h, float sx0, float sy0, float sx1, float sy1, float m)
{
    float kx = (sx1 - sx0) / w, ky = (sy1 - sy0) / h;
    Quad *q = q_add(K_ART, x - m, y - m, w + 2 * m, h + 2 * m);
    if (q) q_src(q, sx0 - m * kx, sy0 - m * ky, sx1 + m * kx, sy1 + m * ky);
    if (q && art_planes) {
        q->pad[0][0] = BACK_AX; q->pad[0][1] = 0; q->pad[0][2] = BACK_SX; q->pad[0][3] = BACK_SY;
        q->pad[1][2] = 1; q->pad[1][3] = BACK_STEP;
    }
    return q;
}
/* the panel's face is that rectangle of the bottom screen, cut to the panel's outline */
static void pan_face(const Pan *p, const float *src)
{
    Quad *q = art_quad(p->x, p->y, p->w, p->h, src[0], src[1], src[2], src[3], 1.5f);
    if (!q) return;
    float kx = (src[2] - src[0]) / p->w, ky = (src[3] - src[1]) / p->h;
    float pts[16], rnd; int n = pan_poly(p, 0, pts, &rnd);
    for (int i = 0; i < n; i++) { pts[2 * i] = src[0] + (pts[2 * i] - p->x) * kx; pts[2 * i + 1] = src[1] + (pts[2 * i + 1] - p->y) * ky; }
    q_poly(q, n, pts);
    q->misc[1] = rnd * kx;
}
static void hit_add(float x, float y, float w, float h, const float *pts, int n, float grow, float fx, float fy)
{
    if (nhit_new >= MAXHIT) return;
    Hit *t = &hit_new[nhit_new++];
    t->dst[0] = x; t->dst[1] = y; t->dst[2] = w; t->dst[3] = h;
    t->src[0] = x; t->src[1] = y; t->src[2] = x + w; t->src[3] = y + h;
    t->poly.n = n;
    for (int i = 0; i < n; i++) { t->poly.p[i][0] = pts[2 * i]; t->poly.p[i][1] = pts[2 * i + 1]; }
    t->grow = grow; t->fx = fx; t->fy = fy;
}
/* a press on the panel is a press on (fx, fy) of the bottom screen */
static void hit_pan(const Pan *p, float fx, float fy)
{
    float pts[16], rnd; int n = pan_poly(p, 0, pts, &rnd);
    hit_add(p->x, p->y, p->w, p->h, pts, n, rnd + 1, fx, fy);
}

/* ---- from the live bottom screen (the read-back that matches the picture we draw from) */
static float rbf(int x, int y, int c) { if (x < 0) x = 0; if (y < 0) y = 0; if (x > 319) x = 319; if (y > 239) y = 239; return rb[y][x][c] / 255.0f; }
static float rb_lum(int x, int y) { return 0.299f * rbf(x, y, 0) + 0.587f * rbf(x, y, 1) + 0.114f * rbf(x, y, 2); }
/* where a button's colours are: art columns x0-x1, rows y0-y1, less the rows b0-b1 (its lettering) */
typedef struct { float x0, x1, y0, y1, b0, b1; } Grad;
static const Grad G_FIGHT = { 256, 318, 60, 182, 103, 136 };
static const Grad G_MON_T = { 2, 56, 58, 182, 109, 132 };
static const Grad G_MON_W = { 2, 50, 64, 92, 0, 0 };
static const Grad G_BAG = { 2, 56, 132, 176, 0, 0 };
static const Grad G_RUN = { 140, 180, 199, 216, 0, 0 };
static const Grad G_BACK = { 1, 5, 212, 239, 0, 0 };
static const Grad G_ZCOL = { 3, 40, 42, 198, 79, 178 };
static const Grad G_MCOL = { 2, 7, 28, 140, 0, 0 };      /* the pale strip left of the symbol, clear of the cursor arrow */
/* the command buttons' own pictures, refitted: FIGHT's flames and POKEMON's and BAG's swirls mirrored about the
   panel's middle (where the burst's centre is); RUN's footprints at the lettering's own size, so the game's own
   RUN stays under our pill */
static const Fit F_FIGHT = { 256, 72, 319.5f, 170, 103, 136, 319.5f, 0.37f, -1, 0, -1 };
static const Fit F_MON_T = { 0.5f, 70, 50, 150, 108, 134, 0.5f, 0.456f, -1, 0, 1 };   /* (the Battle Tree's: half the row) */
static const Fit F_MON_W = { 0.5f, 64, 52, 92, 0, 0, 0.5f, 0.36f, -1, 0, 1 };
static const Fit F_BAG = { 0.5f, 132, 62, 176, 0, 0, 0.5f, 0.45f, -1, 0, 1 };
static const Fit F_RUN = { 112, 205, 208, 240, 0, 0, 159, 1 / 1.45f, 226.5f, 1 / 1.45f, 0 };
/* RUN as half the row (the Battle Tree): the footprints and circles left of the lettering, mirrored to both sides */
static const Fit F_RUN_WIDE = { 113, 205, 206, 240, 0, 0, 159, 1 / 1.45f, 226.5f, 1 / 1.45f, -1 };
static void grad_stops(const Grad *g, float st[7][4])
{
    float top = g->b1 > g->b0 ? g->b0 - g->y0 : g->y1 - g->y0, bot = g->b1 > g->b0 ? g->y1 - g->b1 : 0;
    for (int i = 0; i < 7; i++) {
        float t = i / 6.0f * (top + bot);
        float ay = t < top ? g->y0 + t : g->b1 + (t - top);
        float acc[3] = { 0, 0, 0 }; int n = 0;
        for (int y = (int)ay - 2; y <= (int)ay + 2; y++) {
            if (y < g->y0 || y >= g->y1 || (g->b1 > g->b0 && y >= g->b0 && y < g->b1)) continue;
            for (int x = (int)g->x0; x < (int)g->x1; x++) { for (int c = 0; c < 3; c++) acc[c] += rbf(x, y, c); n++; }
        }
        if (!n) { int y = (int)ay; for (int x = (int)g->x0; x < (int)g->x1; x++) { for (int c = 0; c < 3; c++) acc[c] += rbf(x, y, c); n++; } }
        for (int c = 0; c < 3; c++) st[i][c] = n ? acc[c] / n : 0;
        st[i][3] = 1;
    }
}
static void grad_two(float st[7][4], const float *a, const float *b)
{
    for (int i = 0; i < 7; i++) { float t = i / 6.0f; for (int c = 0; c < 3; c++) st[i][c] = a[c] + (b[c] - a[c]) * t; st[i][3] = 1; }
}
/* the game's lettering is where we expect it: its dark plate darker than its light letters, in the live picture */
static int mask_ok(int m)
{
    const MaskInfo *mi = &MASKS[m];
    float d = 0, l = 0;
    for (int i = 0; i < mi->np; i++) d += rb_lum(mi->plate[i][0], mi->plate[i][1]);
    for (int i = 0; i < mi->nf; i++) l += rb_lum(mi->fill[i][0], mi->fill[i][1]);
    return mi->np && mi->nf && l / mi->nf - d / mi->np > 0.2f;
}
static void plate_col(int m, float *c)
{
    const MaskInfo *mi = &MASKS[m];
    c[0] = c[1] = c[2] = 0;
    for (int i = 0; i < mi->np; i++) for (int k = 0; k < 3; k++) c[k] += rbf(mi->plate[i][0], mi->plate[i][1], k) / mi->np;
}
/* the game's lettering m, centred on (cx, cy) at ks window px a bottom-screen px; turned: reading upwards;
   pill: on a pill of its plate's colour, reaching padx, pady (bottom-screen px) past it */
static void label(int m, float cx, float cy, float ks, int turned, int pill, float padx, float pady)
{
    const MaskInfo *mi = &MASKS[m];
    if (!mask_ok(m)) return;      /* (a moment while the screen changes: no lettering rather than the wrong pixels) */
    float w = (turned ? mi->h : mi->w) * ks, h = (turned ? mi->w : mi->h) * ks;
    float x = cx - w / 2, y = cy - h / 2;
    if (pill) {
        float c[3]; plate_col(m, c);
        float px = (turned ? pady : padx) * ks, py = (turned ? padx : pady) * ks;
        Pan pp = { x - px, y - py, w + 2 * px, h + 2 * py, 1e9f, 1 };
        pan_solid(&pp, c, 1);
    }
    float mg = 1.5f;
    Quad *q = q_add(K_ART, x - mg, y - mg, w + 2 * mg, h + 2 * mg);
    if (!q) return;
    q_src(q, mi->sx - mg / ks, mi->sy - mg / ks, mi->sx + mi->w + mg / ks, mi->sy + mi->h + mg / ks);
    q->info[2] = 1.0f / ks;
    q->pad[0][0] = mi->ax; q->pad[0][1] = mi->ay; q->pad[0][2] = mi->sx; q->pad[0][3] = mi->sy;
    q->pad[1][0] = (float)turned; q->pad[1][1] = 1;
}
/* a disc of the bottom screen (centre ax, ay, radius ar) shown at (cx, cy), radius r, in a dark ring */
static void disc(float ax, float ay, float ar, float cx, float cy, float r, float u)
{
    Pan ring = { cx - r - 1.8f * u, cy - r - 1.8f * u, 2 * (r + 1.8f * u), 2 * (r + 1.8f * u), 1e9f, 1 };
    pan_solid(&ring, DARK, 1);
    Quad *q = art_quad(cx - r, cy - r, 2 * r, 2 * r, ax - ar, ay - ar, ax + ar, ay + ar, 1.5f);
    if (!q) return;
    float e = 0.01f, pts[8] = { ax - e, ay - e, ax + e, ay - e, ax + e, ay + e, ax - e, ay + e };
    q_poly(q, 4, pts);
    q->misc[1] = ar;
}

/* ---- the Pokemon's icons in the arena, cut out by their dark outlines (the commands' info column) */
static int icon_have[4];          /* a Pokemon stands there (foes left, right; ours left, right) */
static float icon_c[4][2];        /* the middle of its icon (bottom-screen px): where a press opens its info */
static unsigned char icon_m[4][ICON_H][ICON_W];
static unsigned char icon_try_m[ICON_H][ICON_W];
static int icon_try_box[4];
/* one try at the icon in its box, with outline pixels darker than ratio times their brightest neighbour: its size */
static int icon_try(int slot, float ratio)
{
    float cx = INFO_AT[slot][0], cy = INFO_AT[slot][1];
    int x0 = (int)cx - 2, y0 = (int)cy - 2;
    static float L[ICON_H][ICON_W];
    static unsigned char dark[ICON_H][ICON_W], wall[ICON_H][ICON_W], reach[ICON_H][ICON_W], disc_[ICON_H][ICON_W];
    unsigned char (*m)[ICON_W] = icon_try_m;
    for (int y = 0; y < ICON_H; y++) for (int x = 0; x < ICON_W; x++) {
        L[y][x] = rb_lum(x0 + x, y0 + y);
        float dx = x0 + x + 0.5f - cx, dy = y0 + y + 0.5f - cy;
        disc_[y][x] = dx * dx + dy * dy < 11.5f * 11.5f;      /* the info button, not the icon */
    }
    /* the outline: clearly darker than the brightest pixel next to it */
    for (int y = 0; y < ICON_H; y++) for (int x = 0; x < ICON_W; x++) {
        float nb = 0;
        for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) {
            if (!i && !j) continue;
            int yy = y + j < 0 ? 0 : y + j >= ICON_H ? ICON_H - 1 : y + j, xx = x + i < 0 ? 0 : x + i >= ICON_W ? ICON_W - 1 : x + i;
            if (L[yy][xx] > nb) nb = L[yy][xx];
        }
        dark[y][x] = L[y][x] < ratio * nb && L[y][x] < 150 / 255.0f && !disc_[y][x];
    }
    /* walls a pixel thicker, so a faded outline with gaps still holds; then everything outside them, from the edges */
    for (int y = 0; y < ICON_H; y++) for (int x = 0; x < ICON_W; x++)
        wall[y][x] = dark[y][x] || (y > 0 && dark[y - 1][x]) || (y < ICON_H - 1 && dark[y + 1][x]) || (x > 0 && dark[y][x - 1]) || (x < ICON_W - 1 && dark[y][x + 1]);
    static short st[ICON_W * ICON_H][2];
    int sp = 0;
    memset(reach, 0, sizeof reach);
    for (int y = 0; y < ICON_H; y++) for (int x = 0; x < ICON_W; x++)
        if ((x == 0 || y == 0 || x == ICON_W - 1 || y == ICON_H - 1 || disc_[y][x]) && !wall[y][x]) { reach[y][x] = 1; st[sp][0] = (short)x; st[sp][1] = (short)y; sp++; }
    while (sp) {
        sp--; int x = st[sp][0], y = st[sp][1];
        static const int d4[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
        for (int k = 0; k < 4; k++) {
            int xx = x + d4[k][0], yy = y + d4[k][1];
            if (xx < 0 || yy < 0 || xx >= ICON_W || yy >= ICON_H || reach[yy][xx] || wall[yy][xx]) continue;
            reach[yy][xx] = 1; st[sp][0] = (short)xx; st[sp][1] = (short)yy; sp++;
        }
    }
    /* what is left, less the walls' extra pixel on the outside, less the info button */
    for (int y = 0; y < ICON_H; y++) for (int x = 0; x < ICON_W; x++) {
        int in = !reach[y][x];
        if (in) in = (y == 0 || !reach[y - 1][x]) && (y == ICON_H - 1 || !reach[y + 1][x]) && (x == 0 || !reach[y][x - 1]) && (x == ICON_W - 1 || !reach[y][x + 1]);
        m[y][x] = in && !disc_[y][x];
    }
    /* thin bits (an arena line running into the icon): cut back from their free ends */
    for (int it = 0; it < 40; it++) {
        int any = 0;
        static unsigned char drop[ICON_H][ICON_W];
        for (int y = 0; y < ICON_H; y++) for (int x = 0; x < ICON_W; x++) {
            drop[y][x] = 0;
            if (!m[y][x]) continue;
            int nb = 0;
            for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) {
                if (!i && !j) continue;
                int yy = y + j, xx = x + i;
                if (yy >= 0 && xx >= 0 && yy < ICON_H && xx < ICON_W) nb += m[yy][xx];
            }
            if (nb <= 1) { drop[y][x] = 1; any = 1; }
        }
        if (!any) break;
        for (int y = 0; y < ICON_H; y++) for (int x = 0; x < ICON_W; x++) if (drop[y][x]) m[y][x] = 0;
    }
    /* small specks: only pieces of 20 pixels, and a fifth of the biggest, or more */
    static unsigned char seen[ICON_H][ICON_W];
    static short lab[ICON_H][ICON_W];
    static int csize[ICON_W * ICON_H];
    memset(seen, 0, sizeof seen);
    int ncomp = 0, big = 0;
    for (int y = 0; y < ICON_H; y++) for (int x = 0; x < ICON_W; x++) {
        if (!m[y][x] || seen[y][x]) continue;
        int nc = 0; sp = 0;
        seen[y][x] = 1; st[sp][0] = (short)x; st[sp][1] = (short)y; sp++;
        while (sp) {
            sp--; int cx_ = st[sp][0], cy_ = st[sp][1];
            lab[cy_][cx_] = (short)ncomp; nc++;
            for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) {
                int xx = cx_ + i, yy = cy_ + j;
                if (xx < 0 || yy < 0 || xx >= ICON_W || yy >= ICON_H || seen[yy][xx] || !m[yy][xx]) continue;
                seen[yy][xx] = 1; st[sp][0] = (short)xx; st[sp][1] = (short)yy; sp++;
            }
        }
        csize[ncomp++] = nc;
        if (nc > big) big = nc;
    }
    int keep_min = big / 5 > 20 ? big / 5 : 20;
    int total = 0; int bx0 = ICON_W, by0 = ICON_H, bx1 = -1, by1 = -1;
    for (int y = 0; y < ICON_H; y++) for (int x = 0; x < ICON_W; x++) {
        if (!m[y][x]) continue;
        if (csize[lab[y][x]] < keep_min) { m[y][x] = 0; continue; }
        total++;
        if (x < bx0) bx0 = x;
        if (x > bx1) bx1 = x;
        if (y < by0) by0 = y;
        if (y > by1) by1 = y;
    }
    icon_try_box[0] = bx0; icon_try_box[1] = by0; icon_try_box[2] = bx1; icon_try_box[3] = by1;
    return total;
}
static void icon_read(int slot)
{
    float cx = INFO_AT[slot][0], cy = INFO_AT[slot][1];
    int x0 = (int)cx - 2, y0 = (int)cy - 2;
    static const float ratios[3] = { 0.6f, 0.66f, 0.72f };   /* a faded icon (its turn done) has a fainter outline */
    int total = 0;
    for (int r = 0; r < 3 && total < 120; r++) total = icon_try(slot, ratios[r]);
    unsigned char (*m)[ICON_W] = icon_try_m;
    int bx0 = icon_try_box[0], by0 = icon_try_box[1], bx1 = icon_try_box[2], by1 = icon_try_box[3];
    icon_have[slot] = total >= 40;
    if (icon_have[slot]) { icon_c[slot][0] = x0 + (bx0 + bx1 + 1) * 0.5f; icon_c[slot][1] = y0 + (by0 + by1 + 1) * 0.5f; }
    else { icon_c[slot][0] = cx + 18; icon_c[slot][1] = cy + 17; }
    int changed = 0;
    for (int y = 0; y < ICON_H; y++) for (int x = 0; x < ICON_W; x++) {
        unsigned char v = (icon_have[slot] && m[y][x]) ? 255 : 0;
        if (icon_m[slot][y][x] != v) { icon_m[slot][y][x] = v; changed = 1; }
    }
    if (changed) {
        for (int y = 0; y < ICON_H; y++) memcpy(&atlas_px[ICON_AY + y][slot * (ICON_W + 2) + 1], icon_m[slot][y], ICON_W);
        atlas_mark(ICON_AY, ICON_AY + ICON_H);
    }
}

/* ---- the info column: the Pokemon on the field, foes over ours, to scroll through and pick one */
static float info_off, info_to;        /* the scroll (px of a 1280x720 window): shown now, and where it is going */
static float info_step = 62;           /* one cell and its gap (px of a 1280x720 window) */
static void lay_info_column(const Scene *s, float x, float bottom, float u)
{
    int slots[4], n = 0;
    for (int i = 0; i < 4; i++) if (icon_have[i]) slots[n++] = i;
    if (!n) return;
    float cell = 58 * u, gap = 4 * u, pad = 5 * u, w = 66 * u;
    int vis = n < 3 ? n : 3;
    float vh = vis * cell + (vis - 1) * gap + 2 * pad + (n > vis ? 18 * u : 0);
    Pan col = { x, bottom - vh, w, vh, 10 * u, 1 };
    float content = n * cell + (n - 1) * gap, view = vh - 2 * pad;
    float maxo = (content > view ? content - view : 0) / u;
    if (info_to > maxo) info_to = maxo;
    if (info_to < 0) info_to = 0;
    info_off += (info_to - info_off) * 0.3f;
    if (fabsf(info_to - info_off) < 0.3f) info_off = info_to;
    float off = info_off * u;
    info_box_new[0] = col.x; info_box_new[1] = col.y; info_box_new[2] = col.w; info_box_new[3] = col.h;
    info_step = (cell + gap) / u;
    pan_shadow(&col, u);
    pan_frame(&col, PF_LIGHT, u);
    static const float NAVY[3] = { 0.07f, 0.11f, 0.21f };
    pan_solid(&col, NAVY, 0.94f);
    float top = col.y + pad, bot = col.y + vh - pad;
    for (int k = 0; k < n; k++) {
        int slot = slots[k], foe = slot < 2;
        float cy = top - off + k * (cell + gap);
        float vt = cy > top ? cy : top, vb = cy + cell < bot ? cy + cell : bot;
        if (vb - vt < 1) continue;
        Pan c = { x + pad, vt, w - 2 * pad, vb - vt, 7 * u, 1 };
        static const float FOE0[3] = { 0.86f, 0.27f, 0.38f }, FOE1[3] = { 0.45f, 0.10f, 0.20f };
        static const float OUR0[3] = { 0.22f, 0.52f, 0.96f }, OUR1[3] = { 0.10f, 0.22f, 0.55f };
        float st[7][4]; grad_two(st, foe ? FOE0 : OUR0, foe ? FOE1 : OUR1);
        /* the gradient follows the whole cell, also while part of it is scrolled out of sight */
        Quad *q = pan_quad(K_BODY, &c, 0, 0);
        if (q) {
            q->pad[0][0] = c.x; q->pad[0][1] = cy; q->pad[0][2] = c.w; q->pad[0][3] = cell;
            q->pad[1][0] = 0.12f; q->pad[1][1] = 0.0f;
            for (int i = 0; i < 7; i++) memcpy(q->ext[i], st[i], sizeof st[i]);
        }
        int hov = ptr_in(c.x, c.y, c.w, c.h);
        if (hov && (q = pan_quad(K_GLOW, &c, 0, 0))) { q->info[3] = 6 * u; memcpy(q->col2, GOLD, sizeof GOLD); }
        /* the icon, cut out, centred on the cell, kept inside what shows of it */
        float ks = 1.45f * u;
        float bx = INFO_AT[slot][0], by = INFO_AT[slot][1];
        float ix0 = (float)((int)bx - 2), iy0 = (float)((int)by - 2);
        float icx = x + w / 2, icy = cy + cell / 2;
        float dx0 = icx - (icon_c[slot][0] - ix0) * ks, dy0 = icy - (icon_c[slot][1] - iy0) * ks;
        Quad *iq = art_quad(dx0, dy0, ICON_W * ks, ICON_H * ks, ix0, iy0, ix0 + ICON_W, iy0 + ICON_H, 1.5f);
        if (iq) {
            iq->pad[0][0] = (float)(slot * (ICON_W + 2) + 1); iq->pad[0][1] = (float)ICON_AY; iq->pad[0][2] = ix0; iq->pad[0][3] = iy0;
            iq->pad[1][1] = 1;
            float ty0 = iy0 + (vt - dy0) / ks, ty1 = iy0 + (vb - dy0) / ks;
            float pts[8] = { ix0 - 2, ty0, ix0 + ICON_W + 2, ty0, ix0 + ICON_W + 2, ty1, ix0 - 2, ty1 };
            q_poly(iq, 4, pts);
        }
        /* the game's own mark that there is more to see (stat changes, effects) */
        if (s->info[slot] && vt <= cy + 2 * u && vb >= cy + 20 * u)
            disc(INFO_AT[slot][0], INFO_AT[slot][1], 8.5f, c.x + 9 * u, cy + 9 * u, 8 * u, u);
        float pts[16], rnd; int pn = pan_poly(&c, 0, pts, &rnd);
        hit_add(c.x, c.y, c.w, c.h, pts, pn, rnd + gap / 2, icon_c[slot][0], icon_c[slot][1]);
    }
    if (n > vis) {
        /* more below or above: a hint at the edge that is not at its end */
        float a = maxo > 0 ? info_off / maxo : 0;
        Pan bar = { col.x + col.w - 5 * u, top + (bot - top - 26 * u) * a, 3 * u, 26 * u, 2 * u, 1 };
        pan_solid(&bar, LIGHT, 0.55f);
    }
}

/* ---- the small square buttons */
static const float BACK_CROP[4] = { 4, 215.5f, 23, 234.5f };   /* the back tab's arrow, on its own blue */
static const float BALL_CROP[4] = { 3.5f, 3.5f, 23.5f, 23.5f };  /* the quick ball tab's ball */
static const float WEATHER_CROP[4] = { 136, 1, 188, 31 };      /* the weather tab's picture */
static void back_button(float x, float y, float sz, float u)
{
    Pan p = { x, y, sz, sz, 9 * u, 1 };
    pan_shadow(&p, u);
    pan_frame(&p, PF_LIGHT | (ptr_in(x, y, sz, sz) ? PF_HOVER : 0), u);
    pan_face(&p, BACK_CROP);
    disc(31.75f, 217.25f, 6.8f, x + sz - 4 * u, y + 4 * u, 12 * u, u);
    hit_pan(&p, 13.5f, 224.5f);
    float r = 14 * u;
    float pts[8] = { x + sz - 4 * u, y + 4 * u, x + sz - 4 * u + 0.01f, y + 4 * u, x + sz - 4 * u + 0.01f, y + 4 * u + 0.01f, x + sz - 4 * u, y + 4 * u + 0.01f };
    hit_add(x + sz - 4 * u - r, y + 4 * u - r, 2 * r, 2 * r, pts, 4, r, 13.5f, 224.5f);
}

/* ---- the commands, laid out like the ORAS and Black/White menus: FIGHT across the top, the rest under it */
static int cmd_labels_ok(const Scene *s)
{
    if (!mask_ok(M_FIGHT)) return 0;
    if (s->bag && (!mask_ok(M_BAG) || !mask_ok(M_MON_W))) return 0;
    if (!s->bag && !mask_ok(M_MON_T)) return 0;
    if (s->run && !mask_ok(M_RUN)) return 0;
    return 1;
}
static void lay_commands_old(const Scene *s, float u);
static void lay_commands(const Scene *s, float u)
{
    static unsigned icons_of;
    if (icons_of != rb_frame) { icons_of = rb_frame; for (int i = 0; i < 4; i++) icon_read(i); }
    static int bad;
    bad = cmd_labels_ok(s) ? 0 : bad + 1;
    if (bad > 40) { lay_commands_old(s, u); return; }
    float R = lay_w - 20 * u, B = lay_h - 16 * u;
    float W = 412 * u, FH = 84 * u, RH = 60 * u, G = 8 * u;
    Pan fight = { R - W, B - RH - G - FH, W, FH, 12 * u, 0 };
    /* the row: BAG (wild battles and other trainers'), RUN (or back, for a second Pokemon), POKEMON */
    enum { B_BAG, B_RUN, B_BACK, B_MON };
    int kind[3], nr = 0;
    if (s->bag) kind[nr++] = B_BAG;
    kind[nr++] = (s->run || !s->back) ? B_RUN : B_BACK;
    kind[nr++] = B_MON;
    Pan row[3];
    float rw = (W - (nr - 1) * G) / nr;     /* the row lined up with FIGHT's edges: three in wild battles, two in the Battle Tree */
    for (int i = 0; i < nr; i++) { Pan p = { fight.x + i * (rw + G), B - RH, rw, RH, 9 * u, 0 }; row[i] = p; }
    Pan ball = { fight.x - 10 * u - 60 * u, B - 60 * u, 60 * u, 60 * u, 9 * u, 1 };
    Pan wthr = { 20 * u, 20 * u, 83 * u, 48 * u, 10 * u, 1 };
    pan_shadow(&fight, u);
    for (int i = 0; i < nr; i++) pan_shadow(&row[i], u);
    if (s->ball) pan_shadow(&ball, u);
    if (s->weather) pan_shadow(&wthr, u);
    float st[7][4];
    /* FIGHT */
    pan_frame(&fight, PF_LIGHT | (ptr_in(fight.x, fight.y, fight.w, fight.h) ? PF_HOVER : 0), u);
    pan_artbody(&fight, &F_FIGHT, u, 0.12f, 0.08f);
    label(M_FIGHT, fight.x + fight.w / 2, fight.y + fight.h / 2, 1.75f * u, 0, 1, 3, 1.5f);
    hit_pan(&fight, 282, 150);
    for (int i = 0; i < nr; i++) {
        Pan *p = &row[i];
        pan_frame(p, PF_LIGHT | (ptr_in(p->x, p->y, p->w, p->h) ? PF_HOVER : 0), u);
        float cx = p->x + p->w / 2, cy = p->y + p->h / 2;
        switch (kind[i]) {
        case B_BAG:
            pan_artbody(p, &F_BAG, u, 0.12f, 0.08f);
            label(M_BAG, cx, cy, 1.45f * u, 0, 1, 3, 1.5f);
            hit_pan(p, 30, 150);
            break;
        case B_RUN:
            pan_artbody(p, nr == 2 ? &F_RUN_WIDE : &F_RUN, u, 0.12f, 0.08f);
            label(M_RUN, cx, cy, 1.45f * u, 0, 1, 3, 1.5f);
            hit_pan(p, 160, 226);
            break;
        case B_BACK: {
            grad_stops(&G_BACK, st); pan_body(p, st, 0.10f, 0.0f);
            float a = p->h * 0.8f;
            Pan ar = { cx - a / 2, cy - a / 2, a, a, 5 * u, 1 };
            pan_face(&ar, BACK_CROP);
            disc(31.75f, 217.25f, 6.8f, ar.x + ar.w + 3 * u, ar.y + 3 * u, 11 * u, u);
            hit_pan(p, 13.5f, 224.5f);
            break; }
        case B_MON:
            if (s->bag) { pan_artbody(p, &F_MON_W, u, 0.12f, 0.08f); label(M_MON_W, cx, cy, 1.45f * u, 0, 1, 3, 1.5f); hit_pan(p, 30, 75); }
            else { pan_artbody(p, &F_MON_T, u, 0.12f, 0.08f); label(M_MON_T, cx, cy, 1.45f * u, 0, 1, 3, 1.5f); hit_pan(p, 30, 150); }
            break;
        }
    }
    if (s->ball) {
        /* the quick ball (Y): the last ball used */
        pan_frame(&ball, PF_LIGHT | (ptr_in(ball.x, ball.y, ball.w, ball.h) ? PF_HOVER : 0), u);
        pan_face(&ball, BALL_CROP);
        disc(31.75f, 22.5f, 6.8f, ball.x + ball.w - 4 * u, ball.y + 4 * u, 12 * u, u);
        hit_pan(&ball, 13.5f, 13.5f);
    }
    if (s->weather) {
        /* the weather, in the corner of the screen */
        pan_frame(&wthr, PF_LIGHT | (ptr_in(wthr.x, wthr.y, wthr.w, wthr.h) ? PF_HOVER : 0), u);
        pan_face(&wthr, WEATHER_CROP);
        hit_pan(&wthr, 160, 16);
    }
    lay_info_column(s, R - 66 * u, fight.y - 12 * u, u);
}

/* ---- the moves: the game's four bars, the Z-Power / Mega Evolution button as a column beside them, back */
static void lay_moves(const Scene *s, float u)
{
    float k = 1.25f * u, gap = 3 * k;
    float bottom = lay_h - 14 * u;
    float wbar, hbar; Poly bp = poly_bar(0); piece_size(&bp, k, &wbar, &hbar);
    int n = 0; for (int b = 0; b < 4; b++) n += s->bars[b];
    float top = bottom - n * hbar - (n - 1) * gap;
    float stack_left = lay_w - wbar, y = top, left_edge = stack_left;
    for (int b = 0; b < 4; b++) {
        if (!s->bars[b]) continue;
        Poly p = poly_bar(b), t = poly_tag(b);
        int zb = s->state == S_ZMOVE;           /* behind the Z-move: the moves greyed by the game, and faded here too */
        if (zb && b == 3) {
            /* the Z-move in the last move's place */
            float wz, hz; piece_size(&POLY_ZBAR, k, &wz, &hz);
            Piece *zp = piece(&POLY_ZBAR, lay_w - wz, y + (173 - (22 + 150)) * k, k, F_DARK, 1);
            if (zp) zp->lit = 1;
            left_edge = lay_w - wz;
            y += hbar + gap;
            continue;
        }
        Piece *c = piece(&p, lay_w - wbar, y, k, F_DARK, !zb);
        if (c && s->sel == b) c->lit = 1;
        if (c && zb) c->alpha = 0.55f;
        Piece *tag = piece_with(&t, c, F_NONE, !zb);
        if (tag) { tag->round = 5.5f; if (zb) tag->alpha = 0.55f; tag->fx = 240; tag->fy = 22.0f + 50 * b + 25; }   /* presses the move */
        y += hbar + gap;
    }
    if (s->big && s->state == S_MOVES) {
        /* a column the moves' height, as a move bar stood on end, wide enough for the lettering to read across */
        float cw = 108 * u;
        Pan c = { stack_left - 12 * u - cw, top, cw, bottom - top, 12 * u, 0 };
        static int kind; static unsigned kind_at;
        int k2 = mask_ok(M_ZEMB) && mask_ok(M_ZPOW) ? 1 : mask_ok(M_MSYM) && mask_ok(M_MEGA2) ? 2 : 0;
        if (k2) { kind = k2; kind_at = rb_frame; }
        else if (rb_frame - kind_at > 30) kind = 0;
        int f = PF_LIGHT | (s->big_on ? PF_ON : s->sel == 4 ? PF_LIT : ptr_in(c.x, c.y, c.w, c.h) ? PF_HOVER : 0);
        pan_shadow(&c, u);
        pan_frame(&c, f, u);
        float st[7][4];
        if (kind == 1) {
            grad_stops(&G_ZCOL, st); pan_body(&c, st, 0.12f, 0.08f);
            float ks = (cw - 14 * u) / MASKS[M_ZEMB].w;
            label(M_ZEMB, c.x + cw / 2, c.y + c.h * 0.36f, ks, 0, 0, 0, 0);
            label(M_ZPOW, c.x + cw / 2, c.y + c.h * 0.76f, (cw - 22 * u) / MASKS[M_ZPOW].w, 0, 1, 3, 1.5f);
        } else if (kind == 2) {
            /* Mega Evolution in the rainbow it is known for (the same whether on or off; on, it also gets the
               aura's glowing outline), its symbol, and MEGA over EVOLUTION, each line centred on its own pill */
            static const float RAINBOW[7][4] = {
                { 1.00f, 0.52f, 0.60f, 1 }, { 1.00f, 0.71f, 0.45f, 1 }, { 1.00f, 0.90f, 0.47f, 1 }, { 0.64f, 0.93f, 0.52f, 1 },
                { 0.44f, 0.86f, 0.95f, 1 }, { 0.50f, 0.63f, 1.00f, 1 }, { 0.75f, 0.52f, 1.00f, 1 } };
            memcpy(st, RAINBOW, sizeof st);
            pan_body(&c, st, 0.18f, 0.0f);
            float ks = (cw * 0.62f) / MASKS[M_MSYM].w;
            label(M_MSYM, c.x + cw / 2, c.y + c.h * 0.33f, ks, 0, 0, 0, 0);
            float kl = (cw - 26 * u) / MASKS[M_MEGA2].w;
            float h1 = MASKS[M_MEGA1].h * kl, h2 = MASKS[M_MEGA2].h * kl, g = 3 * kl + 3 * u;
            float top1 = c.y + c.h * 0.78f - (h1 + g + h2) / 2;
            label(M_MEGA1, c.x + cw / 2, top1 + h1 / 2, kl, 0, 1, 3, 1.5f);
            label(M_MEGA2, c.x + cw / 2, top1 + h1 + g + h2 / 2, kl, 0, 1, 3, 1.5f);
        } else {
            /* another big button (Ultra Burst): its own colours, and its middle */
            grad_stops(&G_ZCOL, st); pan_body(&c, st, 0.12f, 0.08f);
            float a = cw - 8 * u;
            Pan m = { c.x + 4 * u, c.y + (c.h - a) / 2, a, a, 6 * u, 1 };
            static const float MID[4] = { 14, 70, 104, 160 };
            pan_face(&m, MID);
        }
        hit_pan(&c, 50, 110);
        left_edge = c.x;
    }
    float sz = 60 * u;
    back_button(left_edge - 12 * u - sz, bottom - sz, sz, u);
}
/* ---- the targets: the game's four plates, whole and on their own */
static void lay_targets(const Scene *s, float u)
{
    float k = 1.1f * u, gx = 10 * u, gy = 8 * u;
    float right = lay_w - 20 * u, bottom = lay_h - 16 * u;
    Poly h0 = poly_hex(0); float wh, hh; piece_size(&h0, k, &wh, &hh);
    float gl = right - 2 * wh - gx, gt = bottom - 2 * hh - gy;
    Piece *sel = NULL;
    for (int i = 0; i < 4; i++) {
        Poly p = poly_hex(i);
        Piece *c = piece(&p, gl + (i & 1) * (wh + gx), gt + (i >> 1) * (hh + gy), k, F_DARK, 1);
        if (c) { c->round = 0.6f; if (s->sel == i) { c->lit = 1; sel = c; } }
    }
    if (sel && s->sel >= 0) {
        /* the game's arrow lies on the chosen plate's tip: that tip again, from the plate beside it */
        int i = s->sel;
        float x0 = (i & 1) ? 165.0f : 12.0f, y0 = i < 2 ? 42.0f : 126.0f;
        Poly tip = { 5, { { x0, y0 + 36 }, { x0 + 16, y0 }, { x0 + 19, y0 }, { x0 + 19, y0 + 72 }, { x0 + 16, y0 + 72 } } };
        Piece *pt = piece_with(&tip, sel, F_NONE, 0);
        if (pt) { pt->flat = 1; pt->adx = (i & 1) ? -153.0f : 153.0f; pt->round = 0.6f; }
    }
    float sz = 60 * u;
    back_button(gl - 14 * u - sz, bottom - sz, sz, u);
}
/* ---- a move's details: the game's wavy panel on its own (its colours are the move's type), and back as everywhere else */
static Quad *mi_quad(const MiShape *m_, int kind, float x0, float y0, float k, float m)
{
    float sx0 = -MI_E - m, sy0 = m_->ymin - m, sx1 = 320 + m, sy1 = m_->ymax + m;
    Quad *q = q_add(kind, x0 + sx0 * k, y0 + (sy0 - m_->ymin) * k, (sx1 - sx0) * k, (sy1 - sy0) * k);
    if (!q) return NULL;
    q_src(q, sx0, sy0, sx1, sy1);
    q->ext[7][0] = 1; q->ext[7][1] = (float)-m_->sx0; q->ext[7][2] = (float)(m_->ay - m_->sy0); q->ext[7][3] = 8;
    return q;
}
static void lay_moveinfo(const Scene *s, float u)
{
    const MiShape *m = &mi_shape[s->zinfo ? 1 : 0];
    if (m->h <= 0) return;
    float k = 1.25f * u, g = 2.2f;                                   /* its size; the dark line round it (bottom-screen px) */
    float x0 = lay_w - 320 * k;                                       /* where bottom-screen x 0 is: its right end at the window's edge */
    float y0 = lay_h - 14 * u - (m->ymax + g - m->ymin) * k;          /* where its top is */
    Quad *q;
    /* its shadow; the dark line round it (as its own along the waves); the panel, its left end carried on from column 1 */
    if ((q = mi_quad(m, K_GLOW, x0, y0, k, 4))) {
        q->dst[0] += 2 * u; q->dst[1] += 3 * u;
        q->misc[1] = g; q->info[3] = 8 * u / k;
        q->col2[0] = q->col2[1] = q->col2[2] = 0; q->col2[3] = 0.45f;
    }
    if ((q = mi_quad(m, K_SOLID, x0, y0, k, 4))) { q->misc[1] = g; memcpy(q->col, DARK, sizeof DARK); }
    if ((q = mi_quad(m, K_ART, x0, y0, k, 2))) { q->pad[2][2] = 1.5f; q->pad[2][3] = 1; }
    /* back: lit, as on the moves screen (the game darkens it behind the panel) */
    float sz = 60 * u;
    art_planes = back_saved;
    back_button(x0 - (MI_E + g) * k - 14 * u - sz, lay_h - 14 * u - sz, sz, u);
    art_planes = 0;
}
static void lay_yesno(float u)
{
    /* above the message box, at the right, like a choice box */
    float k = 1.0f * u, gap = 8 * u;
    float right = lay_w - 22 * u, bottom = lay_h - 142 * u;
    float wy, hy, wn, hn; piece_size(&POLY_YES, k, &wy, &hy); piece_size(&POLY_NO, k, &wn, &hn);
    piece(&POLY_NO, right - wn, bottom - hn, k, F_NONE, 1);
    piece(&POLY_YES, right - wy, bottom - hn - gap - hy, k, F_NONE, 1);
}


/* ======================================================== the X menu
 * With the program's "Menu layouts" on, the one-screen layout shows the field's X menu as pieces of the bottom screen
 * at the right of the window: the six options, back, SAVE and the page dots, with the program's own L and R keys
 * drawn over the dots. We draw the menu anew at the same places: each option's pill and icon cut out of the yellow
 * background (a flood from the background that stops at the pills' dark outlines), its label in white with a dark
 * edge, back and SAVE as clean panels, and L and R side by side over the middle of the six. The dots are left out
 * (two pages; the lit one is the page shown): the green "!" a dot carries when the other page has news moves onto the
 * key that goes there. Presses go to the program as they are, as everything is where its piece is. */
/* the options' masks: cells of XM_CW x XM_CH, three a row, from atlas row XM_AY; below them the letters L and R
   (distance fields, usum_glyphs.h) side by side from row XM_GY, each in a border of "far outside" */
#define XM_CW 148
#define XM_LAB 46               /* in a cell: the pill and its icon from row 1, the label from row XM_LAB + 1, its edge from XM_EDGE + 1 */
#define XM_EDGE 68
static const float XM_SRC[9][4] = {    /* the program's pieces: the part of the bottom screen each shows (x, y, w, h) */
    { 27, 28, 121, 60 }, { 176, 28, 116, 60 }, { 27, 90, 121, 58 }, { 176, 90, 116, 58 }, { 27, 150, 121, 58 }, { 176, 150, 116, 58 },
    { 136, 7, 62, 19 }, { 0, 208, 36, 32 }, { 183, 208, 137, 32 } };
enum { XM_DOTS = 6, XM_BACK = 7, XM_SAVE = 8 };
static const int XM_ROWS[3][4] = { { 27, 69, 69, 88 }, { 88, 131, 131, 148 }, { 148, 191, 191, 206 } };   /* each row's pills (y0, y1) and labels */
static const int XM_COLS[2][2] = { { 14, 159 }, { 161, 306 } };
static const float SAVE_CROP[4] = { 196, 210, 320, 240 };       /* SAVE and its notebook, right of the Y badge */
static int xm_piece_of(const float *src)
{
    for (int i = 0; i < 9; i++)
        if (fabsf(src[0] - XM_SRC[i][0]) < 0.6f && fabsf(src[1] - XM_SRC[i][1]) < 0.6f && fabsf(src[2] - XM_SRC[i][2]) < 0.6f && fabsf(src[3] - XM_SRC[i][3]) < 0.6f)
            return i;
    return -1;
}
static float xm_dst[9][4];              /* where the program puts each piece this frame (window px from the top) */
static int xm_slot_ok[6], xm_lab_ok[6]; /* each option's pill and label, as cut out of the last read-back */
static int xm_page;                     /* the page shown: 0 or 1 */
static int xm_news;                     /* the other page's dot carries the "!" */
static float xm_news_c[2];              /* where (bottom-screen px) */
static unsigned xm_since;               /* the frame the pieces came (0: not up) */
static unsigned xm_good_frame;          /* the last frame a read-back showed the menu */
static unsigned xm_read_of;             /* the read-back the masks are from */
static int xm_good;
/* the program's X menu pieces this frame, in the one-screen layout */
static int xm_find(void)
{
    int have = 0;
    for (int i = 0; i < pc_n; i++) {
        int k = xm_piece_of(pc_cur[i].src);
        if (k >= 0) { memcpy(xm_dst[k], pc_cur[i].dst, sizeof xm_dst[k]); have |= 1 << k; }
    }
    if (have != 0x1FF) return 0;
    if (!top_big()) return 0;
    /* (on the top screen: the options' middle inside it) */
    float cx = xm_dst[0][0] + xm_dst[0][2], cy = xm_dst[2][1] + xm_dst[2][3] / 2;
    return cx >= top_rect[0] && cx <= top_rect[0] + top_rect[2] && cy >= top_rect[1] && cy <= top_rect[1] + top_rect[3];
}
static float xm_lum(int x, int y) { const unsigned char *c = rb[y][x]; return 0.299f * c[0] + 0.587f * c[1] + 0.114f * c[2]; }
/* the menu's yellow background */
static int xm_bg(int x, int y) { const unsigned char *c = rb[y][x]; return c[0] >= 200 && c[1] >= 160 && c[0] - c[2] >= 25; }
/* an option's pill and icon in the band (x0, y0)-(x1, y1): everything a flood from the bright, coloured background on the
   band's top and bottom rows cannot reach through bright pixels (the pills are ringed by dark outlines; no pill reaches
   those rows, while on a change of page the pills slide through the band's ends); its mask to the atlas at ax, ay */
static int xm_pill(int x0, int y0, int x1, int y1, int ax, int ay)
{
    static unsigned char pass[48][148], reach[48][148];
    static int q[48 * 148];
    int W = x1 - x0, H = y1 - y0, qh = 0, qt = 0;
    if (W > 148 || H > 48) return 0;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) { pass[y][x] = xm_lum(x0 + x, y0 + y) > 110; reach[y][x] = 0; }
    for (int y = 0; y < H; y += H - 1) for (int x = 0; x < W; x++) {
        if (!pass[y][x]) continue;
        const unsigned char *c = rb[y0 + y][x0 + x];
        int mx = c[0] > c[1] ? (c[0] > c[2] ? c[0] : c[2]) : (c[1] > c[2] ? c[1] : c[2]);
        int mn = c[0] < c[1] ? (c[0] < c[2] ? c[0] : c[2]) : (c[1] < c[2] ? c[1] : c[2]);
        if (mx > 1 && (float)(mx - mn) / mx > 0.25f) { reach[y][x] = 1; q[qt++] = y * W + x; }
    }
    while (qh < qt) {
        int i = q[qh++], y = i / W, x = i % W;
        static const int D[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
        for (int k = 0; k < 4; k++) {
            int xx = x + D[k][0], yy = y + D[k][1];
            if (xx < 0 || yy < 0 || xx >= W || yy >= H || reach[yy][xx] || !pass[yy][xx]) continue;
            reach[yy][xx] = 1; q[qt++] = yy * W + xx;
        }
    }
    int n = 0;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) { int m = !reach[y][x]; atlas_px[ay + y][ax + x] = m ? 255 : 0; n += m; }
    return n > W * H / 30 && n < W * H * 85 / 100;
}
/* an option's label: its dark letters as coverage, and that grown by 1.25 px for a dark edge round them */
static int xm_label(int x0, int y0, int x1, int y1, int ax, int ay, int ey)
{
    static float a[24][148];
    int W = x1 - x0, H = y1 - y0;
    if (W > 148 || H > 24) return 0;
    float ink = 0;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        float v = (125.0f - xm_lum(x0 + x, y0 + y)) / 65.0f;
        v = v < 0 ? 0 : v > 1 ? 1 : v;
        a[y][x] = v; ink += v;
        atlas_px[ay + y][ax + x] = (unsigned char)(v * 255 + 0.5f);
    }
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        float e = 0;
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
            int xx = x + dx, yy = y + dy;
            if (xx < 0 || yy < 0 || xx >= W || yy >= H) continue;
            float w = dx && dy ? 0.336f : dx || dy ? 0.75f : 1.0f, v = a[yy][xx] * w;
            if (v > e) e = v;
        }
        atlas_px[ey + y][ax + x] = (unsigned char)(e * 255 + 0.5f);
    }
    return ink > 15 && ink < W * H * 0.6f;
}
/* from the last read-back: is the menu up (its yellow at the top and bottom, where nothing moves), and its options' masks */
static int xm_read(void)
{
    static const int BG[10][2] = { { 60, 12 }, { 100, 12 }, { 220, 12 }, { 250, 12 }, { 100, 22 }, { 250, 22 }, { 80, 225 }, { 150, 225 }, { 60, 232 }, { 120, 212 } };
    int n = 0;
    for (int i = 0; i < 10; i++) n += xm_bg(BG[i][0], BG[i][1]);
    if (n < 9) return 0;
    for (int s = 0; s < 6; s++) {
        int r = s / 2, c = s % 2, ax = (s % 3) * XM_CW + 1, ay = XM_AY + (s / 3) * XM_CH + 1;
        xm_slot_ok[s] = xm_pill(XM_COLS[c][0], XM_ROWS[r][0], XM_COLS[c][1], XM_ROWS[r][1], ax, ay);
        xm_lab_ok[s] = xm_label(XM_COLS[c][0], XM_ROWS[r][2], XM_COLS[c][1], XM_ROWS[r][3], ax, ay + XM_LAB, ay + XM_EDGE);
    }
    atlas_mark(XM_AY, XM_AY + 2 * XM_CH);
    /* the page shown: its dot is lit (white); the other's olive, with a green "!" at its right when that page has news */
    xm_page = xm_lum(144, 15) > xm_lum(172, 15) ? 0 : 1;
    int oc = xm_page ? 144 : 172, g = 0;
    float sx = 0, sy = 0;
    for (int y = 5; y < 23; y++) for (int x = oc + 4; x <= oc + 18; x++) {
        const unsigned char *p = rb[y][x];
        if (p[1] > 120 && p[0] < 90 && p[2] < 120) { sx += x + 0.5f; sy += y + 0.5f; g++; }
    }
    xm_news = g >= 6;
    if (xm_news) { xm_news_c[0] = sx / g; xm_news_c[1] = sy / g; }
    return 1;
}
/* a key as the program draws its own (at 720: 40 x 34 px, a grey edge 3 px wide and 6 at the bottom, a light grey
   cap, the letter 16 px high), its letter 0: L, 1: R */
static void xm_key(int which, float x, float y, float w, float h, float u)
{
    static const float EDGE_C[4] = { 104 / 255.0f, 106 / 255.0f, 108 / 255.0f, 1 }, CAP[4] = { 213 / 255.0f, 217 / 255.0f, 220 / 255.0f, 1 };
    Pan p = { x, y, w, h, 5 * u, 1 };
    Quad *q = pan_quad(K_GLOW, &p, 0, 0);
    if (q) { q->dst[0] += 1 * u; q->dst[1] += 2 * u; q->info[3] = 4 * u; q->col2[0] = q->col2[1] = q->col2[2] = 0; q->col2[3] = 0.3f; }
    pan_solid(&p, EDGE_C, 1);
    Pan f = { x + 3 * u, y + 3 * u, w - 6 * u, h - 9 * u, 3.5f * u, 1 };
    pan_solid(&f, CAP, 1);
    float sc = 16.0f * u / 19.0f;                 /* window px a texel (the letters' capitals are 19 texels high) */
    float gw = GLYPH_W * sc, gh = GLYPH_H * sc, cx = f.x + f.w / 2, cy = f.y + f.h / 2;
    if ((q = q_add(K_SOLID, cx - gw / 2, cy - gh / 2, gw, gh))) {
        q_src(q, 0, 0, GLYPH_W, GLYPH_H);
        q_col(q, 40 / 255.0f, 42 / 255.0f, 47 / 255.0f, 1);
        q->ext[7][0] = 1; q->ext[7][1] = (float)(which * (GLYPH_W + 2 * XM_GM) + XM_GM); q->ext[7][2] = XM_GY; q->ext[7][3] = 8;
    }
}
static void xm_draw(float u)
{
    /* the options: the pills' shadows, the pills, then the labels' shadows, edges and letters */
    for (int pass = 0; pass < 5; pass++)
        for (int s = 0; s < 6; s++) {
            int r = s / 2, c = s % 2;
            const float *d = xm_dst[s], *src = XM_SRC[s];
            float kx = d[2] / src[2], ky = d[3] / src[3];
            int x0 = XM_COLS[c][0], x1 = XM_COLS[c][1], py0 = XM_ROWS[r][0], py1 = XM_ROWS[r][1], ly0 = XM_ROWS[r][2], ly1 = XM_ROWS[r][3];
            float ax = (float)((s % 3) * XM_CW + 1), ay = (float)(XM_AY + (s / 3) * XM_CH + 1);
            float px = d[0] + (x0 - src[0]) * kx, py = d[1] + (py0 - src[1]) * ky, pw = (x1 - x0) * kx, ph = (py1 - py0) * ky;
            float ly = d[1] + (ly0 - src[1]) * ky, lh = (ly1 - ly0) * ky;
            Quad *q;
            if (pass < 2 && !xm_slot_ok[s]) continue;
            if (pass >= 2 && !xm_lab_ok[s]) continue;
            switch (pass) {
            case 0:
                if ((q = q_add(K_ATLAS, px + 2 * u, py + 3 * u, pw, ph))) { q_src(q, ax, ay, ax + (x1 - x0), ay + (py1 - py0)); q_col(q, 0, 0, 0, 0.35f); }
                break;
            case 1: {
                int hover = ptr_in(d[0], d[1], d[2], d[3]);
                /* under the pointer: a white rim (its mask a little larger) and the pill a little lighter */
                float g = 2.5f * u;
                if (hover && (q = q_add(K_ATLAS, px - g, py - g, pw + 2 * g, ph + 2 * g))) {
                    q_src(q, ax, ay, ax + (x1 - x0), ay + (py1 - py0)); q_col(q, 1, 1, 1, 0.8f);
                }
                if ((q = q_add(K_ART, px, py, pw, ph))) {
                    q_src(q, (float)x0, (float)py0, (float)x1, (float)py1);
                    q->pad[0][0] = ax; q->pad[0][1] = ay; q->pad[0][2] = (float)x0; q->pad[0][3] = (float)py0; q->pad[1][1] = 1;
                    if (hover) { q_col(q, 1, 1, 1, 1); q->misc[3] = 0.12f; }
                }
                break;
            }
            case 2: case 3: case 4: {
                float e = pass == 2 ? 1.0f : 0.0f, my = ay + (pass == 4 ? XM_LAB : XM_EDGE);
                if ((q = q_add(K_ATLAS, px + 1.2f * u * e, ly + 1.8f * u * e, pw, lh))) {
                    q_src(q, ax, my, ax + (x1 - x0), my + (ly1 - ly0));
                    if (pass == 2) q_col(q, 0, 0, 0, 0.45f);
                    else if (pass == 3) q_col(q, DARK[0], DARK[1], DARK[2], 1);
                    else q_col(q, 1, 1, 1, 1);
                }
                break;
            }
            }
        }
    /* L and R over the middle of the six, at the dots' height; the "!" of news on the key to that page */
    float gx0 = xm_dst[0][0], gx1 = xm_dst[1][0] + xm_dst[1][2], mid = (gx0 + gx1) / 2;
    float kw = 40 * u, kh = 34 * u, gap = 40 * u;
    float ky = xm_dst[XM_DOTS][1] + (xm_dst[XM_DOTS][3] - kh) / 2;
    float lx = mid - gap / 2 - kw, rx = mid + gap / 2;
    xm_key(0, lx, ky, kw, kh, u);
    xm_key(1, rx, ky, kw, kh, u);
    if (xm_news) disc(xm_news_c[0], xm_news_c[1], 4.6f, (xm_page ? lx : rx) + kw - 3 * u, ky + 3 * u, 8 * u, u);
    /* back: the clean square of its tab, as in battle, at the left of its piece (its B clear of SAVE's Y) */
    const float *bd = xm_dst[XM_BACK];
    Pan b = { bd[0], bd[1], bd[3], bd[3], 8 * u, 1 };
    pan_shadow(&b, u);
    pan_frame(&b, PF_LIGHT | (ptr_in(bd[0], bd[1], bd[2], bd[3]) ? PF_HOVER : 0), u);
    pan_face(&b, BACK_CROP);
    disc(31.75f, 217.25f, 6.8f, b.x + b.w - 4 * u, b.y + 4 * u, 11 * u, u);
    /* SAVE: a clean panel of its own picture, its Y at the top left */
    const float *sd = xm_dst[XM_SAVE];
    Pan v = { sd[0] + 8 * u, sd[1], sd[2] - 8 * u, sd[3], 9 * u, 1 };
    pan_shadow(&v, u);
    pan_frame(&v, PF_LIGHT | (ptr_in(sd[0], sd[1], sd[2], sd[3]) ? PF_HOVER : 0), u);
    pan_face(&v, SAVE_CROP);
    disc(185.5f, 217.0f, 6.8f, v.x + 4 * u, v.y + 4 * u, 11 * u, u);
}
/* the frame's X menu: 1 if the program's pieces are held back (drawn by us, or for a moment nothing while the
   read-back catches up) */
static int xm_layout(void)
{
    if (!xm_find()) {
        if (xm_since) lg("frame %u: X menu pieces gone", frame);
        xm_since = 0; xm_good = 0;
        return 0;
    }
    if (!xm_since) { xm_since = frame ? frame : 1; xm_read_of = 0; lg("frame %u: X menu pieces", frame); }
    if (rb_ok && xm_read_of != rb_frame) {
        xm_read_of = rb_frame;
        int g = xm_read();
        if (g != xm_good) lg("frame %u: X menu %s", frame, g ? "drawn by us" : "not in the read-back");
        xm_good = g;
        if (g) xm_good_frame = frame;
    }
    float u = xm_dst[0][3] / XM_SRC[0][3] / 1.3f;
    xm_zone[0] = xm_dst[0][0] - 50 * u; xm_zone[2] = xm_dst[1][0] + xm_dst[1][2] + 50 * u;
    xm_zone[1] = xm_dst[XM_DOTS][1] - 12 * u; xm_zone[3] = xm_dst[XM_DOTS][1] + xm_dst[XM_DOTS][3] + 12 * u;
    if (!xm_good || art_slot < 0)
        /* just opened (the read-back a few frames behind), or the picture not as expected: wait a moment, then leave
           the program's own pieces be */
        return frame - xm_since < 12 || (xm_good_frame && frame - xm_good_frame < 12 && xm_good_frame >= xm_since);
    xm_draw(u);
    return 1;
}

static void read_battle_size(void);
static float ui_scale;
/* the frame's decision: from the last read-back, which menu, and our quads for it */
static Scene scene;
static unsigned last_known_frame;   /* the last read-back that showed a battle screen we know */
static void layout_frame(void)
{
    info_box_new[2] = info_box_new[3] = 0;
    q_reset();
    nhit_new = 0;
    npieces = 0;
    ui_on = 0; ui_box = 0; xm_hold = 0; lay_off[0] = lay_off[1] = 0;
    /* the X menu, in pieces: drawn anew, presses going to the program as they are */
    if (game_on && !fix_off && xm_layout()) {
        xm_hold = 1; ui_on = 1;
        for (int i = 0; i < nquads; i++) {
            quads[i].win[0] = win_w; quads[i].win[1] = win_h;
            memcpy(quads[i].uvm, art_uvm, sizeof art_uvm);
        }
        publish(0);
        return;
    }
    /* only with the bottom screen in a box on the top screen (one screen); in the other layouts the program's own picture is right */
    {
        /* (for the log: why the box is or is not taken over, when that changes) */
        static int last_why = -1;
        int why = !game_on ? 1 : fix_off ? 2 : !rb_ok ? 3 : !seen_bottom ? 4 : box_n > 1 ? 5 : !box_on ? 6 : !top_seen ? 7 : !one_screen() ? 8 : 0;
        if (why != last_why) {
            lg("frame %u: %s (boxes %d, box %.0f,%.0f %.0fx%.0f, top %d %.0f,%.0f %.0fx%.0f)", frame,
               why == 0 ? "one screen, the box is ours" : why == 4 ? "no bottom screen drawn" : why == 5 ? "the bottom screen drawn in pieces" :
               why == 6 ? "the bottom screen not in a box" : why == 7 ? "no top screen" : why == 8 ? "not the one-screen layout" : "off",
               box_n, box[0], box[1], box[2], box[3], top_seen, top_rect[0], top_rect[1], top_rect[2], top_rect[3]);
            last_why = why;
        }
    }
    if (!game_on || fix_off || !rb_ok || !one_screen()) { publish(0); return; }
    static unsigned read_of;
    if (read_of != rb_frame) {
        read_of = rb_frame;
        Scene s = read_scene();
        if (s.sel < 0 && s.state == scene.state) s.sel = scene.sel;   /* the arrow bobs: keep the last place seen */
        if (s.state != scene.state || s.zinfo != scene.zinfo || s.sel != scene.sel || s.big != scene.big || s.big_on != scene.big_on || s.bag != scene.bag || s.ball != scene.ball || s.run != scene.run || s.back != scene.back || s.weather != scene.weather)
            lg("frame %u: %s, bars %d%d%d%d, big %d%s, bag %d, ball %d, weather %d, run %d, back %d, cursor %d", rb_frame, state_name[s.state],
               s.bars[0], s.bars[1], s.bars[2], s.bars[3], s.big, s.big_on ? " (on)" : "", s.bag, s.ball, s.weather, s.run, s.back, s.sel);
        scene = s;
        if (s.state != S_NONE) last_known_frame = rb_frame;
        if (s.state == S_MOVES || s.state == S_ZMOVE || s.state == S_TARGETS) back_save();
    }
    if (frame % 120 == 0) read_battle_size();
    /* the frame: the top screen's picture, inside the window */
    {
        float x0 = top_rect[0], y0 = top_rect[1], x1 = top_rect[0] + top_rect[2], y1 = top_rect[1] + top_rect[3];
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 > win_w) x1 = win_w;
        if (y1 > win_h) y1 = win_h;
        if (x1 - x0 < 0.5f * win_w || y1 - y0 < 0.5f * win_h) { x0 = 0; y0 = 0; x1 = win_w; y1 = win_h; }
        lay_off[0] = x0; lay_off[1] = y0; lay_w = x1 - x0; lay_h = y1 - y0;
        static float last[4];
        if (fabsf(last[0] - x0) + fabsf(last[1] - y0) + fabsf(last[2] - lay_w) + fabsf(last[3] - lay_h) > 1) {
            lg("frame %u: battle laid out in %.0f,%.0f %.0fx%.0f of the window's %.0fx%.0f", frame, x0, y0, lay_w, lay_h, win_w, win_h);
            last[0] = x0; last[1] = y0; last[2] = lay_w; last[3] = lay_h;
        }
    }
    float u = lay_h / 720.0f * ui_scale;
    if (file_here("usumfix_hide.txt")) ui_on = 1;   /* (for pictures of the battle: the box held back, nothing drawn) */
    else switch (scene.state) {
    case S_CMD: lay_commands(&scene, u); ui_on = 1; break;
    case S_MOVES: case S_ZMOVE: lay_moves(&scene, u); ui_on = 1; break;
    case S_TARGETS: lay_targets(&scene, u); ui_on = 1; break;
    case S_YESNO: lay_yesno(u); ui_on = 1; break;
    case S_MOVEINFO: lay_moveinfo(&scene, u); ui_on = 1; break;
    case S_IDLE: ui_on = 1; break;                   /* between turns: nothing on screen */
    default:
        /* something else: shortly after a battle screen we know it is most likely a change of screens,
           so keep the box back a moment; then show the program's own box */
        ui_on = last_known_frame && rb_frame - last_known_frame < 10;
        break;
    }
    if (ui_on) pieces_draw();
    /* to the frame's place in the window (the panels' own rectangles too, which their gradients are measured in) */
    for (int i = 0; i < nquads; i++) {
        Quad *q = &quads[i];
        int kind = (int)(q->info[0] + 0.5f);
        q->dst[0] += lay_off[0]; q->dst[1] += lay_off[1];
        if (kind == K_BODY || kind == K_ARTBODY) { q->pad[0][0] += lay_off[0]; q->pad[0][1] += lay_off[1]; }
        q->win[0] = win_w; q->win[1] = win_h;
        memcpy(q->uvm, art_uvm, sizeof art_uvm);
    }
    for (int i = 0; i < nhit_new; i++) { hit_new[i].dst[0] += lay_off[0]; hit_new[i].dst[1] += lay_off[1]; }
    if (info_box_new[2] > 0) { info_box_new[0] += lay_off[0]; info_box_new[1] += lay_off[1]; }
    lay_off[0] = lay_off[1] = 0;
    ui_box = ui_on;
    publish(ui_on);
}

/* ======================================================== OpenGL backend */
typedef unsigned int GLenum, GLuint, GLbitfield;
typedef int GLint, GLsizei;
typedef float GLfloat;
typedef unsigned char GLboolean;
typedef ptrdiff_t GLsizeiptr, GLintptr;
typedef char GLchar;
typedef struct __GLsync *GLsync;
typedef uint64_t GLuint64;
#define GL_TRIANGLE_STRIP 0x0005
#define GL_SCISSOR_TEST 0x0C11
#define GL_BLEND 0x0BE2
#define GL_CULL_FACE 0x0B44
#define GL_DEPTH_TEST 0x0B71
#define GL_STENCIL_TEST 0x0B90
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE0 0x84C0
#define GL_FRAMEBUFFER 0x8D40
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_RGBA 0x1908
#define GL_RED 0x1903
#define GL_R8 0x8229
#define GL_RGBA8 0x8058
#define GL_UNSIGNED_BYTE 0x1401
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_LINEAR 0x2601
#define GL_NEAREST 0x2600
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_ONE 1
#define GL_FUNC_ADD 0x8006
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_UNPACK_ROW_LENGTH 0x0CF2
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#define GL_PIXEL_UNPACK_BUFFER_BINDING 0x88EF
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_PIXEL_PACK_BUFFER 0x88EB
#define GL_PIXEL_PACK_BUFFER_BINDING 0x88ED
#define GL_MAP_READ_BIT 0x0001
#define GL_STREAM_READ 0x88E1
#define GL_SYNC_GPU_COMMANDS_COMPLETE 0x9117
#define GL_ALREADY_SIGNALED 0x911A
#define GL_CONDITION_SATISFIED 0x911C
#define GL_CURRENT_PROGRAM 0x8B8D
#define GL_DRAW_FRAMEBUFFER_BINDING 0x8CA6
#define GL_READ_FRAMEBUFFER_BINDING 0x8CAA
#define GL_VERTEX_ARRAY_BINDING 0x85B5
#define GL_ARRAY_BUFFER_BINDING 0x8894
#define GL_ACTIVE_TEXTURE 0x84E0
#define GL_TEXTURE_BINDING_2D 0x8069
#define GL_SAMPLER_BINDING 0x8919
#define GL_VIEWPORT 0x0BA2
#define GL_SCISSOR_BOX 0x0C10
#define GL_BLEND_SRC_RGB 0x80C9
#define GL_BLEND_DST_RGB 0x80C8
#define GL_BLEND_SRC_ALPHA 0x80CB
#define GL_BLEND_DST_ALPHA 0x80CA
#define GL_BLEND_EQUATION_RGB 0x8009
#define GL_BLEND_EQUATION_ALPHA 0x883D
#define GL_COLOR_WRITEMASK 0x0C23

#define GLFN(ret, name, args) typedef ret (*t_##name) args; static t_##name r_##name;
GLFN(void, glBindFramebuffer, (GLenum, GLuint))
GLFN(void, glBindTexture, (GLenum, GLuint))
GLFN(void, glActiveTexture, (GLenum))
GLFN(void, glUseProgram, (GLuint))
GLFN(GLint, glGetUniformLocation, (GLuint, const GLchar *))
GLFN(void, glUniform1i, (GLint, GLint))
GLFN(void, glUniform2i, (GLint, GLint, GLint))
GLFN(void, glUniform4i, (GLint, GLint, GLint, GLint, GLint))
GLFN(void, glUniform1f, (GLint, GLfloat))
GLFN(void, glUniform2f, (GLint, GLfloat, GLfloat))
GLFN(void, glUniform4f, (GLint, GLfloat, GLfloat, GLfloat, GLfloat))
GLFN(void, glUniform1fv, (GLint, GLsizei, const GLfloat *))
GLFN(void, glUniform2fv, (GLint, GLsizei, const GLfloat *))
GLFN(void, glUniform4fv, (GLint, GLsizei, const GLfloat *))
GLFN(void, glUniform2iv, (GLint, GLsizei, const GLint *))
GLFN(void, glUniform4iv, (GLint, GLsizei, const GLint *))
GLFN(void, glViewport, (GLint, GLint, GLsizei, GLsizei))
GLFN(void, glScissor, (GLint, GLint, GLsizei, GLsizei))
GLFN(void, glEnable, (GLenum))
GLFN(void, glDisable, (GLenum))
GLFN(void, glBindVertexArray, (GLuint))
GLFN(void, glBindBuffer, (GLenum, GLuint))
GLFN(void, glBlendFunc, (GLenum, GLenum))
GLFN(void, glBlendFuncSeparate, (GLenum, GLenum, GLenum, GLenum))
GLFN(void, glBlendEquation, (GLenum))
GLFN(void, glBlendEquationSeparate, (GLenum, GLenum))
GLFN(void, glBindSampler, (GLuint, GLuint))
GLFN(void, glColorMask, (GLboolean, GLboolean, GLboolean, GLboolean))
GLFN(void, glDrawArrays, (GLenum, GLint, GLsizei))
GLFN(void, glDrawElements, (GLenum, GLsizei, GLenum, const void *))
GLFN(void, glClear, (GLbitfield))
GLFN(void, glGenTextures, (GLsizei, GLuint *))
GLFN(void, glTexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *))
GLFN(void, glTexSubImage2D, (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *))
GLFN(void, glTexParameteri, (GLenum, GLenum, GLint))
GLFN(void, glPixelStorei, (GLenum, GLint))
GLFN(GLuint, glCreateShader, (GLenum))
GLFN(void, glShaderSource, (GLuint, GLsizei, const GLchar *const *, const GLint *))
GLFN(void, glCompileShader, (GLuint))
GLFN(void, glGetShaderiv, (GLuint, GLenum, GLint *))
GLFN(void, glGetShaderInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *))
GLFN(GLuint, glCreateProgram, (void))
GLFN(void, glAttachShader, (GLuint, GLuint))
GLFN(void, glLinkProgram, (GLuint))
GLFN(void, glGetProgramiv, (GLuint, GLenum, GLint *))
GLFN(void, glGetProgramInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *))
GLFN(void, glGenVertexArrays, (GLsizei, GLuint *))
GLFN(void, glGenSamplers, (GLsizei, GLuint *))
GLFN(void, glSamplerParameteri, (GLuint, GLenum, GLint))
GLFN(void, glGetIntegerv, (GLenum, GLint *))
GLFN(GLboolean, glIsEnabled, (GLenum))
GLFN(void, glGetBooleanv, (GLenum, GLboolean *))
GLFN(void, glGenFramebuffers, (GLsizei, GLuint *))
GLFN(void, glFramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint))
GLFN(void, glReadPixels, (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *))
GLFN(void, glGenBuffers, (GLsizei, GLuint *))
GLFN(void, glBufferData, (GLenum, GLsizeiptr, const void *, GLenum))
GLFN(void *, glMapBufferRange, (GLenum, GLintptr, GLsizeiptr, GLbitfield))
GLFN(GLboolean, glUnmapBuffer, (GLenum))
GLFN(void, glBufferSubData, (GLenum, GLintptr, GLsizeiptr, const void *))
GLFN(void, glVertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void *))
GLFN(GLsync, glFenceSync, (GLenum, GLbitfield))
GLFN(GLenum, glClientWaitSync, (GLsync, GLbitfield, GLuint64))
GLFN(void, glDeleteSync, (GLsync))
GLFN(void, glDrawArraysInstanced, (GLenum, GLint, GLsizei, GLsizei))
GLFN(void, glDrawElementsInstanced, (GLenum, GLsizei, GLenum, const void *, GLsizei))

typedef PROC (WINAPI *wglGPA_t)(LPCSTR);
typedef FARPROC (WINAPI *GPA_t)(HMODULE, LPCSTR);
typedef BOOL (WINAPI *Swap_t)(HDC);
static wglGPA_t real_wglGPA;
static GPA_t real_GPA;
static Swap_t real_Swap;
static void *gp(const char *n)
{
    void *p = real_wglGPA ? (void *)real_wglGPA(n) : NULL;
    if ((uintptr_t)p <= 3 || p == (void *)-1) {
        HMODULE gl = GetModuleHandleA("opengl32.dll");
        p = gl ? (void *)real_GPA(gl, n) : NULL;
    }
    return p;
}

static DWORD gl_thread;
#define ON_GL (GetCurrentThreadId() == gl_thread)
static int gl_drawlog_on;
static GLuint draw_fb, read_fb, cur_prog, active_unit, bound_tex[32], sampler_of[32];
static GLint vp[4], sc[4];
static int scissor_on;

/* the program's composite programs (uniforms "area" and "screen") */
#define NCOMP 16
enum { U_AREA, U_SCREEN, U_UPRIGHT, U_SMOOTH, U_SOURCE, U_WINDOW, U_N };
static const char *u_names[U_N] = { "area", "screen", "upright", "smooth_scale", "source", "window" };
/* the program's UI programs: their "size" uniform (the UI's own pixels), and the vertices last uploaded for the window */
#define NUIP 8
static struct { GLuint prog; GLint loc; float size[2]; } gl_uip[NUIP];
static unsigned char gl_vshadow[65536]; static int gl_vshadow_n;
typedef struct { GLint loc; int kind; float f[4]; } UVal;
static struct { GLuint prog; UVal u[U_N]; DWORD thread; } comp[NCOMP];
static int comp_index(GLuint p) { for (int i = 0; i < NCOMP; i++) if (p && comp[i].prog == p) return i; return -1; }
static int is_comp(GLuint p)
{
    int i = comp_index(p);
    return i >= 0 && comp[i].u[U_AREA].loc >= 0 && comp[i].u[U_SCREEN].loc >= 0 && comp[i].u[U_AREA].loc != comp[i].u[U_SCREEN].loc;
}
/* "screen": the screen's size in the program's pixels (400x240 or 428x240 top, 320x240 bottom, times the resolution) */
static int scr_is(const UVal *s, float w) { return s->kind && s->f[1] >= 120 && fabsf(s->f[0] * 240.0f - s->f[1] * w) <= s->f[1] * 2.4f; }
static UVal *comp_uval(GLint loc)
{
    int i = comp_index(cur_prog);
    if (i < 0 || loc < 0) return NULL;
    for (int k = 0; k < U_N; k++) if (comp[i].u[k].loc == loc) return &comp[i].u[k];
    return NULL;
}
static void set_uval(GLint loc, int n, const float *v) { UVal *u = comp_uval(loc); if (u) { u->kind = n; for (int k = 0; k < n && k < 4; k++) u->f[k] = v[k]; } }

static GLuint gl_bottom_tex;               /* the bottom screen's texture, as last drawn */
static GLuint g_prog, g_vao, g_samp, g_atlas, g_rbtex, g_rbfb, g_pbo[NSLOT], g_copy[NSLOT], g_copyfb[NSLOT];
static GLsync g_fence[NSLOT];
static unsigned g_pbo_frame[NSLOT];
static int g_busy[NSLOT];
static GLint g_uU, g_ubot, g_uatlas;
static int g_ready, g_failed;

static GLuint gl_compile(GLenum type, const char *src)
{
    GLuint s = r_glCreateShader(type);
    r_glShaderSource(s, 1, &src, NULL);
    r_glCompileShader(s);
    GLint ok = 0; r_glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char b[2048]; r_glGetShaderInfoLog(s, sizeof b, NULL, b); lg("GL shader: %s", b); return 0; }
    return s;
}
static int gl_load(void)
{
#define L(n) if (!r_##n) r_##n = (t_##n)gp(#n); if (!r_##n) { lg("GL: no %s", #n); return 0; }
    L(glGenTextures) L(glTexImage2D) L(glTexSubImage2D) L(glTexParameteri) L(glPixelStorei) L(glCreateShader) L(glShaderSource)
    L(glCompileShader) L(glGetShaderiv) L(glGetShaderInfoLog) L(glCreateProgram) L(glAttachShader) L(glLinkProgram)
    L(glGetProgramiv) L(glGetProgramInfoLog) L(glGenVertexArrays) L(glGenSamplers) L(glSamplerParameteri)
    L(glGetIntegerv) L(glIsEnabled) L(glGetBooleanv) L(glGenFramebuffers) L(glFramebufferTexture2D) L(glReadPixels)
    L(glGenBuffers) L(glBufferData) L(glMapBufferRange) L(glUnmapBuffer) L(glFenceSync) L(glClientWaitSync) L(glDeleteSync)
    L(glUseProgram) L(glBindVertexArray) L(glBindTexture) L(glActiveTexture) L(glBindSampler) L(glUniform4fv) L(glUniform1i)
    L(glViewport) L(glScissor) L(glEnable) L(glDisable) L(glBlendFuncSeparate) L(glBlendEquationSeparate) L(glColorMask)
    L(glDrawArrays) L(glBindFramebuffer) L(glBindBuffer) L(glGetUniformLocation)
#undef L
    return 1;
}
static int gl_init(void)
{
    if (g_ready) return 1;
    if (g_failed) return 0;
    g_failed = 1;
    if (!gl_load()) return 0;
    GLuint vs = gl_compile(GL_VERTEX_SHADER, gl_vs), fs = gl_compile(GL_FRAGMENT_SHADER, gl_fs);
    if (!vs || !fs) return 0;
    g_prog = r_glCreateProgram();
    r_glAttachShader(g_prog, vs); r_glAttachShader(g_prog, fs);
    r_glLinkProgram(g_prog);
    GLint ok = 0; r_glGetProgramiv(g_prog, GL_LINK_STATUS, &ok);
    if (!ok) { char b[2048]; r_glGetProgramInfoLog(g_prog, sizeof b, NULL, b); lg("GL link: %s", b); return 0; }
    g_uU = r_glGetUniformLocation(g_prog, "U");
    g_ubot = r_glGetUniformLocation(g_prog, "u_bot");
    g_uatlas = r_glGetUniformLocation(g_prog, "u_atlas");
    r_glGenVertexArrays(1, &g_vao);
    r_glGenSamplers(1, &g_samp);
    r_glSamplerParameteri(g_samp, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    r_glSamplerParameteri(g_samp, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    r_glSamplerParameteri(g_samp, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    r_glSamplerParameteri(g_samp, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GLint ub = 0, ua = 4, rl = 0;
    r_glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &ub);
    r_glGetIntegerv(GL_UNPACK_ALIGNMENT, &ua);
    r_glGetIntegerv(GL_UNPACK_ROW_LENGTH, &rl);
    r_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    r_glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    r_glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    r_glGenTextures(1, &g_atlas);
    r_glBindTexture(GL_TEXTURE_2D, g_atlas);
    r_glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, ATLAS_W, ATLAS_H, 0, GL_RED, GL_UNSIGNED_BYTE, atlas_px);
    r_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    r_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    r_glGenTextures(1, &g_rbtex);
    r_glBindTexture(GL_TEXTURE_2D, g_rbtex);
    r_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 320, 240, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    r_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    r_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    r_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, (GLuint)ub);
    r_glPixelStorei(GL_UNPACK_ALIGNMENT, ua);
    r_glPixelStorei(GL_UNPACK_ROW_LENGTH, rl);
    r_glGenFramebuffers(1, &g_rbfb);
    r_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_rbfb);
    r_glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_rbtex, 0);
    r_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_fb);
    r_glGenBuffers(NSLOT, g_pbo);
    for (int i = 0; i < NSLOT; i++) {
        r_glBindBuffer(GL_PIXEL_PACK_BUFFER, g_pbo[i]);
        r_glBufferData(GL_PIXEL_PACK_BUFFER, 320 * 240 * 4, NULL, GL_STREAM_READ);
    }
    r_glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    r_glGenTextures(NSLOT, g_copy);
    r_glGenFramebuffers(NSLOT, g_copyfb);
    r_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    for (int i = 0; i < NSLOT; i++) {
        r_glBindTexture(GL_TEXTURE_2D, g_copy[i]);
        r_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, CW, CH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        r_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        r_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        r_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_copyfb[i]);
        r_glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_copy[i], 0);
    }
    r_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, (GLuint)ub);
    r_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_fb);
    g_failed = 0; g_ready = 1;
    lg("GL: our drawing is ready");
    return 1;
}

/* the program's state, kept round our drawing */
static struct {
    GLint prog, vao, unit, tex0, tex1, samp0, samp1, dfb, rfb, vp[4], sc[4], pack, packalign;
    GLint bsrc, bdst, bsrca, bdsta, beq, beqa;
    GLboolean blend, scissor, cull, depth, stencil, mask[4];
} gs;
static void gl_save(void)
{
    r_glGetIntegerv(GL_CURRENT_PROGRAM, &gs.prog);
    r_glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &gs.vao);
    r_glGetIntegerv(GL_ACTIVE_TEXTURE, &gs.unit);
    r_glActiveTexture(GL_TEXTURE0); r_glGetIntegerv(GL_TEXTURE_BINDING_2D, &gs.tex0); r_glGetIntegerv(GL_SAMPLER_BINDING, &gs.samp0);
    r_glActiveTexture(GL_TEXTURE0 + 1); r_glGetIntegerv(GL_TEXTURE_BINDING_2D, &gs.tex1); r_glGetIntegerv(GL_SAMPLER_BINDING, &gs.samp1);
    r_glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &gs.dfb);
    r_glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &gs.rfb);
    r_glGetIntegerv(GL_VIEWPORT, gs.vp);
    r_glGetIntegerv(GL_SCISSOR_BOX, gs.sc);
    r_glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &gs.pack);
    r_glGetIntegerv(GL_PACK_ALIGNMENT, &gs.packalign);
    r_glGetIntegerv(GL_BLEND_SRC_RGB, &gs.bsrc); r_glGetIntegerv(GL_BLEND_DST_RGB, &gs.bdst);
    r_glGetIntegerv(GL_BLEND_SRC_ALPHA, &gs.bsrca); r_glGetIntegerv(GL_BLEND_DST_ALPHA, &gs.bdsta);
    r_glGetIntegerv(GL_BLEND_EQUATION_RGB, &gs.beq); r_glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &gs.beqa);
    gs.blend = r_glIsEnabled(GL_BLEND); gs.scissor = r_glIsEnabled(GL_SCISSOR_TEST);
    gs.cull = r_glIsEnabled(GL_CULL_FACE); gs.depth = r_glIsEnabled(GL_DEPTH_TEST); gs.stencil = r_glIsEnabled(GL_STENCIL_TEST);
    r_glGetBooleanv(GL_COLOR_WRITEMASK, gs.mask);
}
static void gl_restore(void)
{
    r_glUseProgram((GLuint)gs.prog);
    r_glBindVertexArray((GLuint)gs.vao);
    r_glActiveTexture(GL_TEXTURE0 + 1); r_glBindTexture(GL_TEXTURE_2D, (GLuint)gs.tex1); r_glBindSampler(1, (GLuint)gs.samp1);
    r_glActiveTexture(GL_TEXTURE0); r_glBindTexture(GL_TEXTURE_2D, (GLuint)gs.tex0); r_glBindSampler(0, (GLuint)gs.samp0);
    r_glActiveTexture((GLenum)gs.unit);
    r_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)gs.dfb);
    r_glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)gs.rfb);
    r_glViewport(gs.vp[0], gs.vp[1], gs.vp[2], gs.vp[3]);
    r_glScissor(gs.sc[0], gs.sc[1], gs.sc[2], gs.sc[3]);
    r_glBindBuffer(GL_PIXEL_PACK_BUFFER, (GLuint)gs.pack);
    r_glPixelStorei(GL_PACK_ALIGNMENT, gs.packalign);
    r_glBlendFuncSeparate((GLenum)gs.bsrc, (GLenum)gs.bdst, (GLenum)gs.bsrca, (GLenum)gs.bdsta);
    r_glBlendEquationSeparate((GLenum)gs.beq, (GLenum)gs.beqa);
    if (gs.blend) r_glEnable(GL_BLEND); else r_glDisable(GL_BLEND);
    if (gs.scissor) r_glEnable(GL_SCISSOR_TEST); else r_glDisable(GL_SCISSOR_TEST);
    if (gs.cull) r_glEnable(GL_CULL_FACE); else r_glDisable(GL_CULL_FACE);
    if (gs.depth) r_glEnable(GL_DEPTH_TEST); else r_glDisable(GL_DEPTH_TEST);
    if (gs.stencil) r_glEnable(GL_STENCIL_TEST); else r_glDisable(GL_STENCIL_TEST);
    r_glColorMask(gs.mask[0], gs.mask[1], gs.mask[2], gs.mask[3]);
}
static void gl_quad(const Quad *q)
{
    r_glUniform4fv(g_uU, NU, (const GLfloat *)q);
    r_glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}
/* the copy and the read-back: the bottom screen drawn upright into a copy of ours (CWxCH), and that
   drawn small (320x240) and read through a pixel buffer, picked up a frame or two later without
   making the program wait; we draw from the copy whose read-back we have, so what we show always
   matches what we read */
static void gl_copy_issue(void)
{
    int slot = -1;
    for (int i = 0; i < NSLOT; i++) if (!g_busy[i] && i != art_slot) { slot = i; break; }
    if (slot < 0 || !gl_bottom_tex) return;
    r_glDisable(GL_BLEND);
    Quad q; memset(&q, 0, sizeof q);
    q.info[0] = K_COPY; q.info[2] = 1;
    q.src[2] = 320; q.src[3] = 240;
    /* the program's texture into our copy */
    r_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_copyfb[slot]);
    r_glViewport(0, 0, CW, CH);
    r_glActiveTexture(GL_TEXTURE0); r_glBindTexture(GL_TEXTURE_2D, gl_bottom_tex);
    q.dst[2] = CW; q.dst[3] = CH; q.win[0] = CW; q.win[1] = CH;
    memcpy(q.uvm, bot_uvm, sizeof bot_uvm);
    gl_quad(&q);
    /* the copy, small, to read */
    r_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_rbfb);
    r_glBindFramebuffer(GL_READ_FRAMEBUFFER, g_rbfb);
    r_glViewport(0, 0, 320, 240);
    r_glBindTexture(GL_TEXTURE_2D, g_copy[slot]);
    q.dst[2] = 320; q.dst[3] = 240; q.win[0] = 320; q.win[1] = 240;
    memcpy(q.uvm, art_uvm_gl, sizeof art_uvm_gl);
    gl_quad(&q);
    r_glBindBuffer(GL_PIXEL_PACK_BUFFER, g_pbo[slot]);
    r_glPixelStorei(GL_PACK_ALIGNMENT, 4);
    r_glReadPixels(0, 0, 320, 240, GL_RGBA, GL_UNSIGNED_BYTE, (void *)0);
    g_fence[slot] = r_glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    g_busy[slot] = 1; g_pbo_frame[slot] = frame;
}
static void gl_copy_collect(void)
{
    int done[NSLOT] = { 0 }, best = -1;
    for (int i = 0; i < NSLOT; i++) {
        if (!g_busy[i]) continue;
        GLenum r = r_glClientWaitSync(g_fence[i], 0, 0);
        if (r != GL_ALREADY_SIGNALED && r != GL_CONDITION_SATISFIED) continue;
        done[i] = 1;
        if (best < 0 || g_pbo_frame[i] > g_pbo_frame[best]) best = i;
    }
    if (best >= 0) {
        r_glBindBuffer(GL_PIXEL_PACK_BUFFER, g_pbo[best]);
        const unsigned char *m = (const unsigned char *)r_glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, 320 * 240 * 4, GL_MAP_READ_BIT);
        if (m) {
            for (int y = 0; y < 240; y++) memcpy(rb[y], m + (size_t)(239 - y) * 320 * 4, 320 * 4);   /* GL rows go up */
            r_glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
            rb_frame = g_pbo_frame[best]; rb_ok = 1; art_slot = best;
            rb_arrived();
        }
        r_glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    }
    for (int i = 0; i < NSLOT; i++) if (done[i]) { r_glDeleteSync(g_fence[i]); g_fence[i] = 0; g_busy[i] = 0; }
}

/* the program's calls we watch */
static void W_glBindFramebuffer(GLenum t, GLuint f)
{
    if (ON_GL) {
        if (t == GL_FRAMEBUFFER || t == GL_DRAW_FRAMEBUFFER) draw_fb = f;
        if (t == GL_FRAMEBUFFER || t == GL_READ_FRAMEBUFFER) read_fb = f;
    }
    r_glBindFramebuffer(t, f);
}
static void W_glBindTexture(GLenum t, GLuint tex) { if (ON_GL && t == GL_TEXTURE_2D && active_unit < 32) bound_tex[active_unit] = tex; r_glBindTexture(t, tex); }
static void W_glActiveTexture(GLenum u) { if (ON_GL) active_unit = u - GL_TEXTURE0; r_glActiveTexture(u); }
static void W_glBindSampler(GLuint u, GLuint s) { if (ON_GL && u < 32) sampler_of[u] = s; r_glBindSampler(u, s); }
static void W_glUseProgram(GLuint p) { if (ON_GL) cur_prog = p; r_glUseProgram(p); }
static GLint W_glGetUniformLocation(GLuint p, const GLchar *n)
{
    GLint l = r_glGetUniformLocation(p, n);
    if (n && !strcmp(n, "size") && l >= 0) {
        int i;
        for (i = 0; i < NUIP && gl_uip[i].prog && gl_uip[i].prog != p; i++) {}
        if (i < NUIP) { gl_uip[i].prog = p; gl_uip[i].loc = l; gl_uip[i].size[0] = gl_uip[i].size[1] = 0; }
    }
    for (int k = 0; k < U_N; k++) if (n && !strcmp(n, u_names[k])) {
        int i = comp_index(p);
        if (i >= 0 && comp[i].thread != GetCurrentThreadId())
            for (int j = 0; j < U_N; j++) { comp[i].u[j].loc = -1; comp[i].u[j].kind = 0; }
        if (i < 0) for (i = 0; i < NCOMP; i++) if (!comp[i].prog) {
            comp[i].prog = p;
            for (int j = 0; j < U_N; j++) { comp[i].u[j].loc = -1; comp[i].u[j].kind = 0; }
            break;
        }
        if (i >= 0 && i < NCOMP) { comp[i].u[k].loc = l; comp[i].thread = GetCurrentThreadId(); }
    }
    return l;
}
static void W_glUniform1i(GLint l, GLint a) { if (ON_GL) { float f = (float)a; set_uval(l, 1, &f); } r_glUniform1i(l, a); }
static void gl_ui_size(GLint l, float a, float b);
static void W_glUniform2i(GLint l, GLint a, GLint b) { if (ON_GL) { float v[2] = { (float)a, (float)b }; set_uval(l, 2, v); gl_ui_size(l, v[0], v[1]); } r_glUniform2i(l, a, b); }
static void W_glUniform4i(GLint l, GLint a, GLint b, GLint c, GLint d) { if (ON_GL) { float v[4] = { (float)a, (float)b, (float)c, (float)d }; set_uval(l, 4, v); } r_glUniform4i(l, a, b, c, d); }
static void W_glUniform1f(GLint l, GLfloat a) { if (ON_GL) set_uval(l, 1, &a); r_glUniform1f(l, a); }
static void gl_ui_size(GLint l, float a, float b)
{
    for (int i = 0; i < NUIP && gl_uip[i].prog; i++) if (gl_uip[i].prog == cur_prog && gl_uip[i].loc == l) { gl_uip[i].size[0] = a; gl_uip[i].size[1] = b; }
}
static void W_glUniform2f(GLint l, GLfloat a, GLfloat b) { if (ON_GL) { float v[2] = { a, b }; set_uval(l, 2, v); gl_ui_size(l, a, b); } r_glUniform2f(l, a, b); }
static void W_glUniform4f(GLint l, GLfloat a, GLfloat b, GLfloat c, GLfloat d) { if (ON_GL) { float v[4] = { a, b, c, d }; set_uval(l, 4, v); } r_glUniform4f(l, a, b, c, d); }
static void W_glUniform1fv(GLint l, GLsizei n, const GLfloat *v) { if (ON_GL && n == 1) set_uval(l, 1, v); r_glUniform1fv(l, n, v); }
static void W_glUniform2fv(GLint l, GLsizei n, const GLfloat *v) { if (ON_GL && n == 1) { set_uval(l, 2, v); gl_ui_size(l, v[0], v[1]); } r_glUniform2fv(l, n, v); }
static void W_glUniform4fv(GLint l, GLsizei n, const GLfloat *v) { if (ON_GL && n == 1) set_uval(l, 4, v); r_glUniform4fv(l, n, v); }
static void W_glUniform2iv(GLint l, GLsizei n, const GLint *v) { if (ON_GL && n == 1) { float f[2] = { (float)v[0], (float)v[1] }; set_uval(l, 2, f); gl_ui_size(l, f[0], f[1]); } r_glUniform2iv(l, n, v); }
static void W_glUniform4iv(GLint l, GLsizei n, const GLint *v) { if (ON_GL && n == 1) { float f[4] = { (float)v[0], (float)v[1], (float)v[2], (float)v[3] }; set_uval(l, 4, f); } r_glUniform4iv(l, n, v); }
static void W_glViewport(GLint x, GLint y, GLsizei w, GLsizei h) { if (ON_GL) { vp[0] = x; vp[1] = y; vp[2] = w; vp[3] = h; } r_glViewport(x, y, w, h); }
static void W_glScissor(GLint x, GLint y, GLsizei w, GLsizei h) { if (ON_GL) { sc[0] = x; sc[1] = y; sc[2] = w; sc[3] = h; } r_glScissor(x, y, w, h); }
static void W_glEnable(GLenum c) { if (ON_GL && c == GL_SCISSOR_TEST) scissor_on = 1; r_glEnable(c); }
static void W_glDisable(GLenum c) { if (ON_GL && c == GL_SCISSOR_TEST) scissor_on = 0; r_glDisable(c); }
static void W_glClear(GLbitfield m)
{
    if (ON_GL && draw_fb == 0) {
        /* the window cleared whole: its size */
        if (!scissor_on && vp[0] == 0 && vp[1] == 0 && vp[2] > 0 && vp[3] > 0) { win_w = (float)vp[2]; win_h = (float)vp[3]; }
        /* the little square the program marks a touch with, on the box */
        if (scissor_on && sc[2] <= 16 && sc[3] <= 16 && ui_on) return;
    }
    r_glClear(m);
}
static void W_glDrawArrays(GLenum mode, GLint f, GLsizei c)
{
    if (ON_GL && gl_drawlog_on) lg("  gl draw arrays mode %x first %d count %d | prog %u fb %u tex0 %u | viewport %d,%d %dx%d", mode, f, c, cur_prog, draw_fb, bound_tex[0], vp[0], vp[1], vp[2], vp[3]);
    if (ON_GL && draw_fb == 0 && is_comp(cur_prog)) {
        int ci = comp_index(cur_prog);
        UVal *scr = &comp[ci].u[U_SCREEN];
        if (scr_is(scr, 320)) {
            UVal *src = &comp[ci].u[U_SOURCE];
            int unit = (src->loc >= 0 && src->kind) ? (int)src->f[0] : 0;
            if (unit < 0 || unit > 31) unit = 0;
            gl_bottom_tex = bound_tex[unit];
            int cut = scissor_on && (sc[2] < vp[2] - 2 || sc[3] < vp[3] - 2);
            /* the part shown ("window"), in the screen's own pixels as the Resolution setting scales them: in the 3DS's */
            UVal *w = &comp[ci].u[U_WINDOW];
            float part_n[4]; const float *part = NULL;
            if (w->loc >= 0 && w->kind == 4 && w->f[2] > 0.5f && scr->f[1] > 1) {
                for (int j = 0; j < 4; j++) part_n[j] = w->f[j] * 240.0f / scr->f[1];
                part = part_n;
            }
            saw_bottom((float)vp[0], win_h - (vp[1] + vp[3]), (float)vp[2], (float)vp[3], cut, part);
            if (box_on && ui_box) return;             /* held back: our menu is drawn instead */
            if (xm_hold && part && xm_piece_of(part) >= 0) return;
        } else if (scr->kind && scr->f[1] >= 120 && scr->f[0] * 240.0f >= scr->f[1] * 398.0f && scr->f[0] <= scr->f[1] * 4.0f) {
            /* the top screen: 400 x 240, or wider (428 for 16:9, more with "Fill screen") */
            saw_top((float)vp[0], win_h - (vp[1] + vp[3]), (float)vp[2], (float)vp[3]);
        }
    }
    r_glDrawArrays(mode, f, c);
}
static void W_glBindBuffer(GLenum t, GLuint b) { if (ON_GL && gl_drawlog_on) lg("  gl bind buffer %x %u", t, b); r_glBindBuffer(t, b); }
static void W_glBufferData(GLenum t, GLsizeiptr n, const void *d, GLenum u)
{
    if (ON_GL && t == 0x8892 && xm_hold) {   /* GL_ARRAY_BUFFER: the UI's vertices, uploaded before each of its draws */
        if (draw_fb == 0 && d && n > 0 && n <= (GLsizeiptr)sizeof gl_vshadow) { memcpy(gl_vshadow, d, (size_t)n); gl_vshadow_n = (int)n; }
        else gl_vshadow_n = 0;
    }
    if (ON_GL && gl_drawlog_on) {
        char b[400]; int k = 0;
        if (d && t == 0x8892) { const float *f = (const float *)d; for (int i = 0; i < 10 && (i + 1) * 4 <= n; i++) k += snprintf(b + k, sizeof b - k, " %.4g", f[i]); }
        lg("  gl buffer data %x size %lld usage %x:%s", t, (long long)n, u, b);
    }
    r_glBufferData(t, n, d, u);
}
static void W_glBufferSubData(GLenum t, GLintptr o, GLsizeiptr n, const void *d) { if (ON_GL && gl_drawlog_on) lg("  gl buffer sub data %x off %lld size %lld", t, (long long)o, (long long)n); r_glBufferSubData(t, o, n, d); }
static void W_glVertexAttribPointer(GLuint i, GLint sz, GLenum ty, GLboolean nm, GLsizei st, const void *p)
{
    if (ON_GL && gl_drawlog_on) lg("  gl attrib %u size %d type %x stride %d offset %lld", i, sz, ty, st, (long long)(intptr_t)p);
    r_glVertexAttribPointer(i, sz, ty, nm, st, p);
}
static void W_glBindVertexArray(GLuint v) { if (ON_GL && gl_drawlog_on) lg("  gl bind vertex array %u", v); r_glBindVertexArray(v); }
/* is a UI mesh (window px from the top) all inside the program's L/R strip, which we hold back? */
static int xm_in_zone(float x0, float y0, float x1, float y1)
{
    return x1 > x0 && x0 >= xm_zone[0] && y0 >= xm_zone[1] && x1 <= xm_zone[2] && y1 <= xm_zone[3];
}
static void W_glDrawElements(GLenum m, GLsizei c, GLenum t, const void *i)
{
    if (ON_GL && xm_hold && draw_fb == 0 && gl_vshadow_n >= 20) {
        int k;
        for (k = 0; k < NUIP && gl_uip[k].prog && gl_uip[k].prog != cur_prog; k++) {}
        if (k < NUIP && gl_uip[k].prog == cur_prog && gl_uip[k].size[0] > 0 && gl_uip[k].size[1] > 0) {
            /* the UI's vertices: x, y, u, v and a colour, 20 bytes each */
            float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
            for (int v = 0; v + 20 <= gl_vshadow_n; v += 20) {
                const float *p = (const float *)(gl_vshadow + v);
                if (p[0] < x0) x0 = p[0];
                if (p[0] > x1) x1 = p[0];
                if (p[1] < y0) y0 = p[1];
                if (p[1] > y1) y1 = p[1];
            }
            /* (the UI over the whole window: its own pixels from the window's top left) */
            float kx = win_w / gl_uip[k].size[0], ky = win_h / gl_uip[k].size[1];
            if (vp[0] == 0 && vp[1] == 0 && fabsf(vp[2] - win_w) < 2 && fabsf(vp[3] - win_h) < 2 && xm_in_zone(x0 * kx, y0 * ky, x1 * kx, y1 * ky)) return;
        }
    }
    if (ON_GL && gl_drawlog_on) lg("  gl draw elements mode %x count %d type %x offset %lld | prog %u fb %u tex0 %u | viewport %d,%d %dx%d | scissor %s %d,%d %dx%d",
                                   m, c, t, (long long)(intptr_t)i, cur_prog, draw_fb, bound_tex[0], vp[0], vp[1], vp[2], vp[3], scissor_on ? "on" : "off", sc[0], sc[1], sc[2], sc[3]);
    r_glDrawElements(m, c, t, i);
}
static void W_glDrawArraysInstanced(GLenum m, GLint f, GLsizei c, GLsizei n) { r_glDrawArraysInstanced(m, f, c, n); }
static void W_glDrawElementsInstanced(GLenum m, GLsizei c, GLenum t, const void *i, GLsizei n) { r_glDrawElementsInstanced(m, c, t, i, n); }

/* the end of a frame on OpenGL: what was read, the decision, our drawing, the next copy */
static void gl_frame_end(void)
{
    if (!game_on || !gl_init()) return;
    gl_save();
    gl_copy_collect();
    memcpy(art_uvm, art_uvm_gl, sizeof art_uvm);
    layout_frame();
    if (atlas_dy1 > atlas_dy0) {
        GLint ub = 0, ua = 4, rl = 0;
        r_glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &ub);
        r_glGetIntegerv(GL_UNPACK_ALIGNMENT, &ua);
        r_glGetIntegerv(GL_UNPACK_ROW_LENGTH, &rl);
        r_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        r_glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        r_glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        r_glActiveTexture(GL_TEXTURE0 + 1);
        r_glBindTexture(GL_TEXTURE_2D, g_atlas);
        r_glTexSubImage2D(GL_TEXTURE_2D, 0, 0, atlas_dy0, ATLAS_W, atlas_dy1 - atlas_dy0, GL_RED, GL_UNSIGNED_BYTE, atlas_px[atlas_dy0]);
        r_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, (GLuint)ub);
        r_glPixelStorei(GL_UNPACK_ALIGNMENT, ua);
        r_glPixelStorei(GL_UNPACK_ROW_LENGTH, rl);
        atlas_dy0 = atlas_dy1 = 0;
    }
    r_glUseProgram(g_prog);
    r_glBindVertexArray(g_vao);
    r_glUniform1i(g_ubot, 0);
    r_glUniform1i(g_uatlas, 1);
    r_glActiveTexture(GL_TEXTURE0 + 1); r_glBindTexture(GL_TEXTURE_2D, g_atlas); r_glBindSampler(1, g_samp);
    r_glActiveTexture(GL_TEXTURE0); r_glBindSampler(0, g_samp);
    r_glDisable(GL_SCISSOR_TEST); r_glDisable(GL_CULL_FACE); r_glDisable(GL_DEPTH_TEST); r_glDisable(GL_STENCIL_TEST);
    r_glColorMask(1, 1, 1, 1);
    if (nquads && ui_on && art_slot >= 0) {
        r_glBindTexture(GL_TEXTURE_2D, g_copy[art_slot]);
        r_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        r_glViewport(0, 0, (GLsizei)win_w, (GLsizei)win_h);
        r_glEnable(GL_BLEND);
        r_glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        r_glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
        for (int i = 0; i < nquads; i++) gl_quad(&quads[i]);
    }
    if (seen_bottom && gl_bottom_tex) gl_copy_issue();
    gl_restore();
}

static const struct { const char *name; void **real; void *wrap; } gl_wraps[] = {
#define WR(n) { #n, (void **)&r_##n, (void *)W_##n }
    WR(glBindFramebuffer), WR(glBindTexture), WR(glActiveTexture), WR(glUseProgram), WR(glGetUniformLocation),
    WR(glUniform1i), WR(glUniform2i), WR(glUniform4i), WR(glUniform1f), WR(glUniform2f), WR(glUniform4f),
    WR(glUniform1fv), WR(glUniform2fv), WR(glUniform4fv), WR(glUniform2iv), WR(glUniform4iv),
    WR(glViewport), WR(glScissor), WR(glEnable), WR(glDisable), WR(glBindSampler), WR(glClear),
    WR(glDrawArrays), WR(glDrawElements), WR(glDrawArraysInstanced), WR(glDrawElementsInstanced),
    WR(glBindBuffer), WR(glBufferData), WR(glBufferSubData), WR(glVertexAttribPointer), WR(glBindVertexArray),
#undef WR
};
static void *gl_wrap_proc(const char *name, void *real)
{
    if (!real || !game_on || (uintptr_t)name <= 0xFFFF) return real;
    for (size_t i = 0; i < sizeof gl_wraps / sizeof gl_wraps[0]; i++)
        if (!strcmp(name, gl_wraps[i].name)) {
            if (!*gl_wraps[i].real) *gl_wraps[i].real = real;
            return gl_wraps[i].wrap;
        }
    return real;
}
static PROC WINAPI my_wglGetProcAddress(LPCSTR n) { return (PROC)gl_wrap_proc(n, (void *)real_wglGPA(n)); }
static BOOL WINAPI my_SwapBuffers(HDC dc)
{
    if (!gl_thread) gl_thread = GetCurrentThreadId();
    if (ON_GL && game_on) {
        fix_off = file_here("usumfix_off.txt");
        gl_frame_end();
        pc_n = 0;
        if (gl_drawlog_on) { gl_drawlog_on = 0; lg("(gl draw log ends)"); }
        if (debug_on && file_here("usumfix_drawlog.txt")) { DeleteFileA(path_here("usumfix_drawlog.txt")); gl_drawlog_on = 1; lg("gl draw log, frame %u:", frame + 1); }
        frame++;
        box_on = 0; seen_bottom = 0; box_n = 0; top_seen = 0;
    }
    return real_Swap(dc);
}

/* ======================================================== Vulkan backend */
static PFN_vkGetInstanceProcAddr r_GIPA;
static PFN_vkGetDeviceProcAddr r_GDPA;
static VkInstance vk_inst;
static VkPhysicalDevice vk_phys;
static VkDevice vk_dev;
#define VKF(n) static PFN_##n r_##n;
VKF(vkCreateInstance) VKF(vkCreateDevice) VKF(vkGetPhysicalDeviceMemoryProperties)
VKF(vkCreateImage) VKF(vkCreateImageView) VKF(vkCreateSwapchainKHR) VKF(vkGetSwapchainImagesKHR)
VKF(vkQueuePresentKHR) VKF(vkQueueSubmit) VKF(vkCmdBeginRendering) VKF(vkCmdEndRendering)
VKF(vkCmdBindPipeline) VKF(vkCmdPushDescriptorSetKHR) VKF(vkCmdSetViewport) VKF(vkCmdSetScissor)
VKF(vkCmdDraw) VKF(vkCmdBlitImage) VKF(vkCmdClearAttachments) VKF(vkCmdDrawIndexed) VKF(vkCmdUpdateBuffer) VKF(vkCmdBindDescriptorSets)
VKF(vkMapMemory2KHR) VKF(vkCmdBindVertexBuffers) VKF(vkCmdBindIndexBuffer)
/* ours */
VKF(vkGetImageMemoryRequirements) VKF(vkAllocateMemory) VKF(vkBindImageMemory) VKF(vkCreateBuffer)
VKF(vkGetBufferMemoryRequirements) VKF(vkBindBufferMemory) VKF(vkMapMemory) VKF(vkCreateSampler)
VKF(vkCreateDescriptorSetLayout) VKF(vkCreatePipelineLayout) VKF(vkCreateShaderModule) VKF(vkCreateGraphicsPipelines)
VKF(vkCmdPipelineBarrier) VKF(vkCmdCopyImageToBuffer) VKF(vkCmdCopyBufferToImage) VKF(vkCmdPushConstants)
VKF(vkCreateFence) VKF(vkGetFenceStatus) VKF(vkResetFences) VKF(vkDestroyImageView)

#define VK_MAXI 4096
static CRITICAL_SECTION vk_cs;
static struct { VkImage img; VkExtent3D ext; VkFormat fmt; VkImageUsageFlags use; } vk_imgs[VK_MAXI];
static int vk_nimg;
static struct { VkImageView view; VkImage img; } vk_views[VK_MAXI * 2];
static int vk_nview;
static VkImage vk_swap_imgs[16]; static int vk_nswap;
static VkExtent2D vk_swap_ext;
static int vk_img_find(VkImage im) { for (int i = vk_nimg - 1; i >= 0; i--) if (vk_imgs[i].img == im) return i; return -1; }
static int vk_view_img(VkImageView v)
{
    for (int i = vk_nview - 1; i >= 0; i--) if (vk_views[i].view == v) return vk_img_find(vk_views[i].img);
    return -1;
}
static int vk_is_swap(VkImage im) { for (int i = 0; i < vk_nswap; i++) if (vk_swap_imgs[i] == im) return 1; return 0; }

/* the command buffer being recorded: what it draws into and with */
static struct {
    VkCommandBuffer cb;
    int target_win;              /* rendering into the window's picture */
    int last_b0_img;             /* the image bound at binding 0 by the last push */
    VkImageView last_b0_view;
    VkImageLayout last_b0_layout;
    VkViewport viewport;
    VkRect2D scissor;
    VkImage win_img;             /* the window's picture (rendered into, then blitted to the swapchain) */
    VkImageView win_view;        /* the program's view of it */
    VkImageLayout win_layout;
    int pc_valid; int pc_area[4]; float pc_src[4];   /* the last piece's rectangles (its "area", and the part shown) */
    float ui_size[2];            /* the program's UI: its own pixels */
} rec;
static VkImageView vk_bottom_view; static VkImageLayout vk_bottom_layout;
static int vk_bottom_img = -1;

/* ours */
static int vk_ready, vk_failed;
static VkPhysicalDeviceMemoryProperties vk_memp;
static VkDescriptorSetLayout v_dsl;
static VkPipelineLayout v_pl;
static VkPipeline v_pipe, v_pipe_copy;
static VkFormat v_pipe_fmt;
static VkSampler v_samp;
static VkImage v_atlas; static VkImageView v_atlas_view; static int v_atlas_ready;
static VkBuffer v_ubo; static unsigned char *v_ubo_map; static VkDeviceSize v_ubo_stride = 512;
static VkBuffer v_stage; static unsigned char *v_stage_map;
static VkImage v_rbimg; static VkImageView v_rbview;
static VkBuffer v_rbbuf; static unsigned char *v_rbmap;
static VkFence v_rbfence[NSLOT]; static int v_rbbusy[NSLOT]; static unsigned v_rbframe[NSLOT]; static int v_rbqueued = -1;
static VkCommandBuffer v_rbcb;      /* the program's command buffer our read-back went into */
static VkImage v_copy[NSLOT]; static VkImageView v_copyview[NSLOT];
static int v_flip = 1;    /* the window's picture is stored bottom row first (the blit turns it over) */
#define V_UBO_SLOTS (3 * (MAXQ + 2))
static unsigned v_ubo_next;

#include "vk_shaders.h"   /* usumfix_vert_spv, usumfix_frag_spv */

static int vk_memtype(uint32_t bits, VkMemoryPropertyFlags want)
{
    for (uint32_t i = 0; i < vk_memp.memoryTypeCount; i++)
        if ((bits & (1u << i)) && (vk_memp.memoryTypes[i].propertyFlags & want) == want) return (int)i;
    return -1;
}
static int vk_buffer(VkDeviceSize size, VkBufferUsageFlags use, VkBuffer *b, unsigned char **map)
{
    VkBufferCreateInfo bi = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bi.size = size; bi.usage = use; bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (r_vkCreateBuffer(vk_dev, &bi, NULL, b) != VK_SUCCESS) return 0;
    VkMemoryRequirements mr; r_vkGetBufferMemoryRequirements(vk_dev, *b, &mr);
    int t = vk_memtype(mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (t < 0) return 0;
    VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    ai.allocationSize = mr.size; ai.memoryTypeIndex = (uint32_t)t;
    VkDeviceMemory m;
    if (r_vkAllocateMemory(vk_dev, &ai, NULL, &m) != VK_SUCCESS) return 0;
    if (r_vkBindBufferMemory(vk_dev, *b, m, 0) != VK_SUCCESS) return 0;
    if (r_vkMapMemory(vk_dev, m, 0, VK_WHOLE_SIZE, 0, (void **)map) != VK_SUCCESS) return 0;
    return 1;
}
static int vk_image(uint32_t w, uint32_t h, VkFormat fmt, VkImageUsageFlags use, VkImage *img, VkImageView *view)
{
    VkImageCreateInfo ii = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    ii.imageType = VK_IMAGE_TYPE_2D; ii.format = fmt; ii.extent.width = w; ii.extent.height = h; ii.extent.depth = 1;
    ii.mipLevels = 1; ii.arrayLayers = 1; ii.samples = VK_SAMPLE_COUNT_1_BIT; ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = use; ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE; ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (r_vkCreateImage(vk_dev, &ii, NULL, img) != VK_SUCCESS) return 0;
    VkMemoryRequirements mr; r_vkGetImageMemoryRequirements(vk_dev, *img, &mr);
    int t = vk_memtype(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (t < 0) t = vk_memtype(mr.memoryTypeBits, 0);
    VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    ai.allocationSize = mr.size; ai.memoryTypeIndex = (uint32_t)t;
    VkDeviceMemory m;
    if (r_vkAllocateMemory(vk_dev, &ai, NULL, &m) != VK_SUCCESS) return 0;
    if (r_vkBindImageMemory(vk_dev, *img, m, 0) != VK_SUCCESS) return 0;
    VkImageViewCreateInfo vi = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    vi.image = *img; vi.viewType = VK_IMAGE_VIEW_TYPE_2D; vi.format = fmt;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; vi.subresourceRange.levelCount = 1; vi.subresourceRange.layerCount = 1;
    return r_vkCreateImageView(vk_dev, &vi, NULL, view) == VK_SUCCESS;
}
static VkPipeline vk_make_pipeline(VkFormat fmt, int blend)
{
    VkShaderModule mv, mf;
    VkShaderModuleCreateInfo si = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    si.codeSize = sizeof usumfix_vert_spv; si.pCode = usumfix_vert_spv;
    if (r_vkCreateShaderModule(vk_dev, &si, NULL, &mv) != VK_SUCCESS) return VK_NULL_HANDLE;
    si.codeSize = sizeof usumfix_frag_spv; si.pCode = usumfix_frag_spv;
    if (r_vkCreateShaderModule(vk_dev, &si, NULL, &mf) != VK_SUCCESS) return VK_NULL_HANDLE;
    VkPipelineShaderStageCreateInfo st[2] = { { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO }, { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
    st[0].stage = VK_SHADER_STAGE_VERTEX_BIT; st[0].module = mv; st[0].pName = "main";
    st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; st[1].module = mf; st[1].pName = "main";
    VkPipelineVertexInputStateCreateInfo vin = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    VkPipelineInputAssemblyStateCreateInfo ia = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    VkPipelineViewportStateCreateInfo vps = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    vps.viewportCount = 1; vps.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rs = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rs.polygonMode = VK_POLYGON_MODE_FILL; rs.cullMode = VK_CULL_MODE_NONE; rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; rs.lineWidth = 1;
    VkPipelineMultisampleStateCreateInfo ms = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState ba = { 0 };
    ba.blendEnable = blend ? VK_TRUE : VK_FALSE;
    ba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA; ba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; ba.colorBlendOp = VK_BLEND_OP_ADD;
    ba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; ba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; ba.alphaBlendOp = VK_BLEND_OP_ADD;
    ba.colorWriteMask = 0xF;
    VkPipelineColorBlendStateCreateInfo cb = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    cb.attachmentCount = 1; cb.pAttachments = &ba;
    VkDynamicState dyn[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo ds = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    ds.dynamicStateCount = 2; ds.pDynamicStates = dyn;
    VkPipelineRenderingCreateInfo pr = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    pr.colorAttachmentCount = 1; pr.pColorAttachmentFormats = &fmt;
    VkGraphicsPipelineCreateInfo gi = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    gi.pNext = &pr; gi.stageCount = 2; gi.pStages = st; gi.pVertexInputState = &vin; gi.pInputAssemblyState = &ia;
    gi.pViewportState = &vps; gi.pRasterizationState = &rs; gi.pMultisampleState = &ms; gi.pColorBlendState = &cb;
    gi.pDynamicState = &ds; gi.layout = v_pl;
    VkPipeline p = VK_NULL_HANDLE;
    if (r_vkCreateGraphicsPipelines(vk_dev, VK_NULL_HANDLE, 1, &gi, NULL, &p) != VK_SUCCESS) return VK_NULL_HANDLE;
    return p;
}
static int vk_init(VkFormat win_fmt)
{
    if (vk_ready && win_fmt == v_pipe_fmt) return 1;
    if (vk_failed) return 0;
    vk_failed = 1;
#define D(n) if (!r_##n) r_##n = (PFN_##n)r_GDPA(vk_dev, #n); if (!r_##n) { lg("Vulkan: no %s", #n); return 0; }
    D(vkGetImageMemoryRequirements) D(vkAllocateMemory) D(vkBindImageMemory) D(vkCreateBuffer) D(vkGetBufferMemoryRequirements)
    D(vkBindBufferMemory) D(vkMapMemory) D(vkCreateSampler) D(vkCreateDescriptorSetLayout) D(vkCreatePipelineLayout)
    D(vkCreateShaderModule) D(vkCreateGraphicsPipelines) D(vkCmdPipelineBarrier) D(vkCmdCopyImageToBuffer) D(vkCmdCopyBufferToImage)
    D(vkCmdPushConstants) D(vkCreateFence) D(vkGetFenceStatus) D(vkResetFences) D(vkCmdBindPipeline) D(vkCmdSetViewport) D(vkCmdSetScissor)
    D(vkCmdDraw) D(vkCreateImage) D(vkCreateImageView) D(vkDestroyImageView)
#undef D
    if (!r_vkCmdBeginRendering) { r_vkCmdBeginRendering = (PFN_vkCmdBeginRendering)r_GDPA(vk_dev, "vkCmdBeginRendering"); }
    if (!r_vkCmdBeginRendering) { r_vkCmdBeginRendering = (PFN_vkCmdBeginRendering)r_GDPA(vk_dev, "vkCmdBeginRenderingKHR"); }
    if (!r_vkCmdEndRendering) { r_vkCmdEndRendering = (PFN_vkCmdEndRendering)r_GDPA(vk_dev, "vkCmdEndRendering"); }
    if (!r_vkCmdEndRendering) { r_vkCmdEndRendering = (PFN_vkCmdEndRendering)r_GDPA(vk_dev, "vkCmdEndRenderingKHR"); }
    if (!r_vkCmdPushDescriptorSetKHR) r_vkCmdPushDescriptorSetKHR = (PFN_vkCmdPushDescriptorSetKHR)r_GDPA(vk_dev, "vkCmdPushDescriptorSetKHR");
    if (!r_vkCmdBeginRendering || !r_vkCmdEndRendering || !r_vkCmdPushDescriptorSetKHR) { lg("Vulkan: no dynamic rendering or push descriptors"); return 0; }
    if (!vk_ready) {
        PFN_vkGetPhysicalDeviceMemoryProperties gmp = (PFN_vkGetPhysicalDeviceMemoryProperties)r_GIPA(vk_inst, "vkGetPhysicalDeviceMemoryProperties");
        if (!gmp || !vk_phys) { lg("Vulkan: no memory properties"); return 0; }
        gmp(vk_phys, &vk_memp);
        VkDescriptorSetLayoutBinding b[3] = { { 0 } };
        b[0].binding = 0; b[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; b[0].descriptorCount = 1; b[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        b[1].binding = 1; b[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; b[1].descriptorCount = 1; b[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        b[2].binding = 2; b[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; b[2].descriptorCount = 1; b[2].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo dl = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        dl.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR; dl.bindingCount = 3; dl.pBindings = b;
        if (r_vkCreateDescriptorSetLayout(vk_dev, &dl, NULL, &v_dsl) != VK_SUCCESS) { lg("Vulkan: descriptor set layout"); return 0; }
        VkPipelineLayoutCreateInfo pli = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        pli.setLayoutCount = 1; pli.pSetLayouts = &v_dsl;
        if (r_vkCreatePipelineLayout(vk_dev, &pli, NULL, &v_pl) != VK_SUCCESS) { lg("Vulkan: pipeline layout"); return 0; }
        VkSamplerCreateInfo sci = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        sci.magFilter = sci.minFilter = VK_FILTER_LINEAR; sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sci.addressModeU = sci.addressModeV = sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; sci.maxLod = 0;
        if (r_vkCreateSampler(vk_dev, &sci, NULL, &v_samp) != VK_SUCCESS) { lg("Vulkan: sampler"); return 0; }
        if (!vk_buffer(V_UBO_SLOTS * v_ubo_stride, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &v_ubo, &v_ubo_map)) { lg("Vulkan: uniform buffer"); return 0; }
        if (!vk_buffer(3 * ATLAS_W * ATLAS_H, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &v_stage, &v_stage_map)) { lg("Vulkan: staging buffer"); return 0; }
        if (!vk_image(ATLAS_W, ATLAS_H, VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, &v_atlas, &v_atlas_view)) { lg("Vulkan: atlas"); return 0; }
        if (!vk_image(320, 240, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, &v_rbimg, &v_rbview)) { lg("Vulkan: read-back image"); return 0; }
        if (!vk_buffer(NSLOT * 320 * 240 * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, &v_rbbuf, &v_rbmap)) { lg("Vulkan: read-back buffer"); return 0; }
        for (int i = 0; i < NSLOT; i++)
            if (!vk_image(CW, CH, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, &v_copy[i], &v_copyview[i])) { lg("Vulkan: copy image"); return 0; }
        VkFenceCreateInfo fi = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        for (int i = 0; i < NSLOT; i++) if (r_vkCreateFence(vk_dev, &fi, NULL, &v_rbfence[i]) != VK_SUCCESS) { lg("Vulkan: fence"); return 0; }
    }
    v_pipe = vk_make_pipeline(win_fmt, 1);
    v_pipe_copy = vk_make_pipeline(VK_FORMAT_R8G8B8A8_UNORM, 0);
    if (!v_pipe || !v_pipe_copy) { lg("Vulkan: pipelines"); return 0; }
    v_pipe_fmt = win_fmt;
    vk_failed = 0; vk_ready = 1;
    lg("Vulkan: our drawing is ready (window format %d)", win_fmt);
    return 1;
}
static void vk_barrier_img(VkCommandBuffer c, VkImage img, VkImageLayout from, VkImageLayout to, VkAccessFlags sa, VkAccessFlags da,
                           VkPipelineStageFlags ss, VkPipelineStageFlags ds)
{
    VkImageMemoryBarrier b = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    b.srcAccessMask = sa; b.dstAccessMask = da; b.oldLayout = from; b.newLayout = to;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; b.image = img;
    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; b.subresourceRange.levelCount = 1; b.subresourceRange.layerCount = 1;
    r_vkCmdPipelineBarrier(c, ss, ds, 0, 0, NULL, 0, NULL, 1, &b);
}
static void vk_push_and_draw(VkCommandBuffer c, const Quad *q, VkImageView bottom, VkImageLayout bl)
{
    unsigned slot = v_ubo_next++ % V_UBO_SLOTS;
    memcpy(v_ubo_map + slot * v_ubo_stride, q, sizeof *q);
    VkDescriptorImageInfo ii[2] = { { v_samp, bottom, bl }, { v_samp, v_atlas_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL } };
    VkDescriptorBufferInfo bi = { v_ubo, slot * v_ubo_stride, sizeof(Quad) };
    VkWriteDescriptorSet w[3] = { { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET }, { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET }, { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET } };
    w[0].dstBinding = 0; w[0].descriptorCount = 1; w[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; w[0].pImageInfo = &ii[0];
    w[1].dstBinding = 1; w[1].descriptorCount = 1; w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; w[1].pImageInfo = &ii[1];
    w[2].dstBinding = 2; w[2].descriptorCount = 1; w[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; w[2].pBufferInfo = &bi;
    r_vkCmdPushDescriptorSetKHR(c, VK_PIPELINE_BIND_POINT_GRAPHICS, v_pl, 0, 3, w);
    r_vkCmdDraw(c, 4, 1, 0, 0);
}
/* a pass of ours drawing one quad into an image of ours (no blending) */
static void vk_pass(VkCommandBuffer c, VkImageView target, uint32_t w, uint32_t h, const Quad *q, VkImageView src, VkImageLayout sl)
{
    VkRenderingAttachmentInfo ca = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    ca.imageView = target; ca.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; ca.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; ca.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    VkRenderingInfo ri = { VK_STRUCTURE_TYPE_RENDERING_INFO };
    ri.renderArea.extent.width = w; ri.renderArea.extent.height = h; ri.layerCount = 1; ri.colorAttachmentCount = 1; ri.pColorAttachments = &ca;
    r_vkCmdBeginRendering(c, &ri);
    r_vkCmdBindPipeline(c, VK_PIPELINE_BIND_POINT_GRAPHICS, v_pipe_copy);
    VkViewport v = { 0, 0, (float)w, (float)h, 0, 1 };
    VkRect2D s = { { 0, 0 }, { w, h } };
    r_vkCmdSetViewport(c, 0, 1, &v);
    r_vkCmdSetScissor(c, 0, 1, &s);
    vk_push_and_draw(c, q, src, sl);
    r_vkCmdEndRendering(c);
}
/* our drawing in the program's command buffer, just before it copies the window's picture to the screen:
   what was read, the decision, our drawing (from the copy that was read), the next copy and read-back */
static void vk_frame_end(VkCommandBuffer c, VkImage win, VkImageLayout win_layout)
{
    int wi = vk_img_find(win);
    if (wi < 0 || !vk_init(vk_imgs[wi].fmt)) return;
    fix_off = file_here("usumfix_off.txt");
    {
        int best = -1, done[NSLOT] = { 0 };
        for (int i = 0; i < NSLOT; i++) if (v_rbbusy[i] && r_vkGetFenceStatus(vk_dev, v_rbfence[i]) == VK_SUCCESS) {
            done[i] = 1;
            if (best < 0 || v_rbframe[i] > v_rbframe[best]) best = i;
        }
        if (best >= 0) {
            const unsigned char *m = v_rbmap + (size_t)best * 320 * 240 * 4;
            for (int y = 0; y < 240; y++) memcpy(rb[y], m + (size_t)y * 320 * 4, 320 * 4);
            rb_frame = v_rbframe[best]; rb_ok = 1; art_slot = best;
            rb_arrived();
        }
        for (int i = 0; i < NSLOT; i++) if (done[i]) { r_vkResetFences(vk_dev, 1, &v_rbfence[i]); v_rbbusy[i] = 0; }
    }
    win_w = (float)vk_imgs[wi].ext.width; win_h = (float)vk_imgs[wi].ext.height;
    memcpy(art_uvm, art_uvm_vk, sizeof art_uvm);
    layout_frame();
    /* a view of the window's picture to draw into (one per picture the program uses; a new one after a resize) */
    static struct { VkImage img; VkImageView view; } wv[8];
    static int nwv;
    VkImageView our_win_view = VK_NULL_HANDLE;
    for (int i = 0; i < nwv; i++) if (wv[i].img == win) our_win_view = wv[i].view;
    if (!our_win_view) {
        VkImageViewCreateInfo vi = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        vi.image = win; vi.viewType = VK_IMAGE_VIEW_TYPE_2D; vi.format = vk_imgs[wi].fmt;
        vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; vi.subresourceRange.levelCount = 1; vi.subresourceRange.layerCount = 1;
        if (r_vkCreateImageView(vk_dev, &vi, NULL, &our_win_view) != VK_SUCCESS) return;
        int k = nwv < 8 ? nwv++ : (int)(frame % 8);
        wv[k].img = win; wv[k].view = our_win_view;
    }
    if (!v_atlas_ready || atlas_dy1 > atlas_dy0) {
        static unsigned ring;
        unsigned slot = ring++ % 3;
        int y0 = v_atlas_ready ? atlas_dy0 : 0, y1 = v_atlas_ready ? atlas_dy1 : ATLAS_H;
        memcpy(v_stage_map + (size_t)slot * ATLAS_W * ATLAS_H + (size_t)y0 * ATLAS_W, atlas_px[y0], (size_t)(y1 - y0) * ATLAS_W);
        atlas_dy0 = atlas_dy1 = 0;
        vk_barrier_img(c, v_atlas, v_atlas_ready ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       v_atlas_ready ? VK_ACCESS_SHADER_READ_BIT : 0, VK_ACCESS_TRANSFER_WRITE_BIT,
                       v_atlas_ready ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkBufferImageCopy r = { 0 };
        r.bufferOffset = (VkDeviceSize)slot * ATLAS_W * ATLAS_H + (VkDeviceSize)y0 * ATLAS_W;
        r.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; r.imageSubresource.layerCount = 1;
        r.imageOffset.y = y0;
        r.imageExtent.width = ATLAS_W; r.imageExtent.height = (uint32_t)(y1 - y0); r.imageExtent.depth = 1;
        r_vkCmdCopyBufferToImage(c, v_stage, v_atlas, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &r);
        vk_barrier_img(c, v_atlas, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        v_atlas_ready = 1;
    }
    VkImageLayout draw_layout = win_layout == VK_IMAGE_LAYOUT_GENERAL ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    if (nquads && ui_on && art_slot >= 0) {
        vk_barrier_img(c, win, win_layout, draw_layout, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT,
                       VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        VkRenderingAttachmentInfo ca = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        ca.imageView = our_win_view; ca.imageLayout = draw_layout; ca.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; ca.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        VkRenderingInfo ri = { VK_STRUCTURE_TYPE_RENDERING_INFO };
        ri.renderArea.extent.width = (uint32_t)win_w; ri.renderArea.extent.height = (uint32_t)win_h;
        ri.layerCount = 1; ri.colorAttachmentCount = 1; ri.pColorAttachments = &ca;
        r_vkCmdBeginRendering(c, &ri);
        r_vkCmdBindPipeline(c, VK_PIPELINE_BIND_POINT_GRAPHICS, v_pipe);
        VkViewport v = { 0, 0, win_w, win_h, 0, 1 };
        VkRect2D s = { { 0, 0 }, { (uint32_t)win_w, (uint32_t)win_h } };
        r_vkCmdSetViewport(c, 0, 1, &v);
        r_vkCmdSetScissor(c, 0, 1, &s);
        for (int i = 0; i < nquads; i++) { quads[i].win[2] = (float)v_flip; vk_push_and_draw(c, &quads[i], v_copyview[art_slot], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL); }
        r_vkCmdEndRendering(c);
        vk_barrier_img(c, win, draw_layout, win_layout, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    }
    /* this frame's bottom screen: into a free copy, and small into the read-back */
    if (seen_bottom && vk_bottom_view && v_rbqueued < 0) {
        int slot = -1;
        for (int i = 0; i < NSLOT; i++) if (!v_rbbusy[i] && i != art_slot) { slot = i; break; }
        if (slot >= 0) {
            Quad q; memset(&q, 0, sizeof q);
            q.info[0] = K_COPY; q.info[2] = 1; q.src[2] = 320; q.src[3] = 240;
            vk_barrier_img(c, v_copy[slot], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
                           VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
            q.dst[2] = CW; q.dst[3] = CH; q.win[0] = CW; q.win[1] = CH; q.win[2] = 0;
            memcpy(q.uvm, bot_uvm, sizeof bot_uvm);
            vk_pass(c, v_copyview[slot], CW, CH, &q, vk_bottom_view, vk_bottom_layout);
            vk_barrier_img(c, v_copy[slot], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            vk_barrier_img(c, v_rbimg, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT,
                           VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
            q.dst[2] = 320; q.dst[3] = 240; q.win[0] = 320; q.win[1] = 240;
            memcpy(q.uvm, art_uvm_vk, sizeof art_uvm_vk);
            vk_pass(c, v_rbview, 320, 240, &q, v_copyview[slot], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            vk_barrier_img(c, v_rbimg, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                           VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
            VkBufferImageCopy r = { 0 };
            r.bufferOffset = (VkDeviceSize)slot * 320 * 240 * 4;
            r.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; r.imageSubresource.layerCount = 1;
            r.imageExtent.width = 320; r.imageExtent.height = 240; r.imageExtent.depth = 1;
            r_vkCmdCopyImageToBuffer(c, v_rbimg, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, v_rbbuf, 1, &r);
            v_rbqueued = slot; v_rbframe[slot] = frame; v_rbcb = c;
        }
    }
}

/* the program's calls we watch */
static VkResult VKAPI_CALL W_vkCreateDevice(VkPhysicalDevice pd, const VkDeviceCreateInfo *ci, const VkAllocationCallbacks *a, VkDevice *d)
{
    VkResult r = r_vkCreateDevice(pd, ci, a, d);
    if (r == VK_SUCCESS) { vk_phys = pd; vk_dev = *d; lg("Vulkan: device made"); }
    return r;
}
static VkResult VKAPI_CALL W_vkCreateImage(VkDevice d, const VkImageCreateInfo *ci, const VkAllocationCallbacks *a, VkImage *out)
{
    VkResult r = r_vkCreateImage(d, ci, a, out);
    if (r == VK_SUCCESS) {
        EnterCriticalSection(&vk_cs);
        if (vk_nimg >= VK_MAXI) { memmove(vk_imgs, vk_imgs + VK_MAXI / 2, sizeof vk_imgs[0] * (VK_MAXI / 2)); vk_nimg = VK_MAXI / 2; }
        vk_imgs[vk_nimg].img = *out; vk_imgs[vk_nimg].ext = ci->extent; vk_imgs[vk_nimg].fmt = ci->format; vk_imgs[vk_nimg].use = ci->usage; vk_nimg++;
        LeaveCriticalSection(&vk_cs);
    }
    return r;
}
static VkResult VKAPI_CALL W_vkCreateImageView(VkDevice d, const VkImageViewCreateInfo *ci, const VkAllocationCallbacks *a, VkImageView *out)
{
    VkResult r = r_vkCreateImageView(d, ci, a, out);
    if (r == VK_SUCCESS) {
        EnterCriticalSection(&vk_cs);
        if (vk_nview >= VK_MAXI * 2) { memmove(vk_views, vk_views + VK_MAXI, sizeof vk_views[0] * VK_MAXI); vk_nview = VK_MAXI; }
        vk_views[vk_nview].view = *out; vk_views[vk_nview].img = ci->image; vk_nview++;
        LeaveCriticalSection(&vk_cs);
    }
    return r;
}
static VkResult VKAPI_CALL W_vkCreateSwapchainKHR(VkDevice d, const VkSwapchainCreateInfoKHR *ci, const VkAllocationCallbacks *a, VkSwapchainKHR *s)
{
    vk_swap_ext = ci->imageExtent;
    lg("Vulkan: swapchain %ux%u format %d", ci->imageExtent.width, ci->imageExtent.height, ci->imageFormat);
    return r_vkCreateSwapchainKHR(d, ci, a, s);
}
static VkResult VKAPI_CALL W_vkGetSwapchainImagesKHR(VkDevice d, VkSwapchainKHR s, uint32_t *n, VkImage *im)
{
    VkResult r = r_vkGetSwapchainImagesKHR(d, s, n, im);
    if (im && (r == VK_SUCCESS || r == VK_INCOMPLETE)) {
        EnterCriticalSection(&vk_cs);
        vk_nswap = 0;
        for (uint32_t i = 0; i < *n && i < 16; i++) vk_swap_imgs[vk_nswap++] = im[i];
        LeaveCriticalSection(&vk_cs);
    }
    return r;
}
static void VKAPI_CALL W_vkCmdBeginRendering(VkCommandBuffer c, const VkRenderingInfo *ri)
{
    rec.cb = c;
    rec.target_win = 0;
    if (ri->colorAttachmentCount == 1) {
        int k = vk_view_img(ri->pColorAttachments[0].imageView);
        if (k >= 0 && vk_swap_ext.width && vk_imgs[k].ext.width == vk_swap_ext.width && vk_imgs[k].ext.height == vk_swap_ext.height) {
            rec.target_win = 1;
            rec.win_img = vk_imgs[k].img;
            rec.win_view = ri->pColorAttachments[0].imageView;
            rec.win_layout = ri->pColorAttachments[0].imageLayout;
        }
    }
    r_vkCmdBeginRendering(c, ri);
}
static void VKAPI_CALL W_vkCmdEndRendering(VkCommandBuffer c) { rec.target_win = 0; r_vkCmdEndRendering(c); }
/* (debug: usumfix_drawlog.txt logs every draw into the window for one frame) */
static int drawlog_on;
static void drawlog(const char *what, uint32_t n, uint32_t first)
{
    if (!drawlog_on || !rec.target_win) return;
    VkExtent3D e = { 0, 0, 0 }; int fmt = 0;
    if (rec.last_b0_img >= 0) { e = vk_imgs[rec.last_b0_img].ext; fmt = vk_imgs[rec.last_b0_img].fmt; }
    lg("  draw %s n %u first %u | b0 %ux%u fmt %d | viewport %.1f,%.1f %.1fx%.1f | scissor %d,%d %ux%u", what, n, first, e.width, e.height, fmt,
       rec.viewport.x, rec.viewport.y, rec.viewport.width, rec.viewport.height, rec.scissor.offset.x, rec.scissor.offset.y, rec.scissor.extent.width, rec.scissor.extent.height);
}
/* the program's buffers: which memory each is in, and where that memory is mapped, so what it hands its shaders
   (a menu piece's rectangles, its UI's vertices) can be read. Tables by handle, entries gone with the handle. */
#define HM_N 16384
typedef struct { uint64_t k, a, b, c; } HmEnt;
static HmEnt hm_buf[HM_N];    /* buffer: its memory, its offset in it */
static HmEnt hm_mem[HM_N];    /* memory, while mapped: where, from which offset, how many bytes */
static HmEnt hm_alloc[HM_N];  /* memory: its size */
static CRITICAL_SECTION dl_cs; static int dl_cs_ok;
static uint32_t hm_hash(uint64_t k) { k ^= k >> 33; k *= 0xff51afd7ed558ccdULL; k ^= k >> 33; return (uint32_t)k & (HM_N - 1); }
static void hm_put(HmEnt *t, uint64_t k, uint64_t a, uint64_t b, uint64_t c)
{
    if (!k) return;
    uint32_t i = hm_hash(k);
    for (int n = 0; n < HM_N; n++, i = (i + 1) & (HM_N - 1))
        if (!t[i].k || t[i].k == k) { t[i].k = k; t[i].a = a; t[i].b = b; t[i].c = c; return; }
}
static HmEnt *hm_get(HmEnt *t, uint64_t k)
{
    if (!k) return NULL;
    uint32_t i = hm_hash(k);
    for (int n = 0; n < HM_N; n++, i = (i + 1) & (HM_N - 1)) { if (!t[i].k) return NULL; if (t[i].k == k) return &t[i]; }
    return NULL;
}
static void hm_del(HmEnt *t, uint64_t k)
{
    if (!k) return;
    uint32_t i = hm_hash(k); int n;
    for (n = 0; n < HM_N; n++, i = (i + 1) & (HM_N - 1)) { if (!t[i].k) return; if (t[i].k == k) break; }
    if (n == HM_N) return;
    /* the entries after it that belong further back move up */
    for (uint32_t j = i;;) {
        j = (j + 1) & (HM_N - 1);
        if (!t[j].k) break;
        uint32_t h = hm_hash(t[j].k);
        int move = j > i ? (h <= i || h > j) : (h <= i && h > j);
        if (move) { t[i] = t[j]; i = j; }
    }
    t[i].k = 0;
}
static PFN_vkMapMemory r2_vkMapMemory; static PFN_vkUnmapMemory r2_vkUnmapMemory; static PFN_vkFreeMemory r2_vkFreeMemory;
static PFN_vkAllocateMemory r2_vkAllocateMemory;
static VkResult VKAPI_CALL W_vkAllocateMemory(VkDevice d, const VkMemoryAllocateInfo *ai, const VkAllocationCallbacks *a, VkDeviceMemory *m)
{
    VkResult r = r2_vkAllocateMemory(d, ai, a, m);
    if (r == VK_SUCCESS && m && ai && dl_cs_ok) { EnterCriticalSection(&dl_cs); hm_put(hm_alloc, (uint64_t)(uintptr_t)*m, ai->allocationSize, 0, 0); LeaveCriticalSection(&dl_cs); }
    return r;
}
static PFN_vkBindBufferMemory r2_vkBindBufferMemory; static PFN_vkBindBufferMemory2 r2_vkBindBufferMemory2; static PFN_vkDestroyBuffer r2_vkDestroyBuffer;
static VkResult VKAPI_CALL W_vkMapMemory(VkDevice d, VkDeviceMemory m, VkDeviceSize off, VkDeviceSize size, VkMemoryMapFlags f, void **pp)
{
    VkResult r = r2_vkMapMemory(d, m, off, size, f, pp);
    if (r == VK_SUCCESS && pp && *pp && dl_cs_ok) {
        EnterCriticalSection(&dl_cs);
        /* the bytes mapped (the rest of the allocation when the whole is asked for; none known: nothing is read there) */
        HmEnt *al = hm_get(hm_alloc, (uint64_t)(uintptr_t)m);
        uint64_t n = size != VK_WHOLE_SIZE ? size : al && al->a > off ? al->a - off : 0;
        hm_put(hm_mem, (uint64_t)(uintptr_t)m, (uint64_t)(uintptr_t)*pp, off, n);
        LeaveCriticalSection(&dl_cs);
    }
    return r;
}
static void VKAPI_CALL W_vkUnmapMemory(VkDevice d, VkDeviceMemory m)
{
    if (dl_cs_ok) { EnterCriticalSection(&dl_cs); hm_del(hm_mem, (uint64_t)(uintptr_t)m); LeaveCriticalSection(&dl_cs); }
    r2_vkUnmapMemory(d, m);
}
static void VKAPI_CALL W_vkFreeMemory(VkDevice d, VkDeviceMemory m, const VkAllocationCallbacks *a)
{
    if (dl_cs_ok) { EnterCriticalSection(&dl_cs); hm_del(hm_mem, (uint64_t)(uintptr_t)m); hm_del(hm_alloc, (uint64_t)(uintptr_t)m); LeaveCriticalSection(&dl_cs); }
    r2_vkFreeMemory(d, m, a);
}
static VkResult VKAPI_CALL W_vkBindBufferMemory(VkDevice d, VkBuffer b, VkDeviceMemory m, VkDeviceSize off)
{
    if (dl_cs_ok) { EnterCriticalSection(&dl_cs); hm_put(hm_buf, (uint64_t)(uintptr_t)b, (uint64_t)(uintptr_t)m, off, 0); LeaveCriticalSection(&dl_cs); }
    return r2_vkBindBufferMemory(d, b, m, off);
}
static VkResult VKAPI_CALL W_vkBindBufferMemory2(VkDevice d, uint32_t n, const VkBindBufferMemoryInfo *bi)
{
    if (dl_cs_ok) {
        EnterCriticalSection(&dl_cs);
        for (uint32_t i = 0; i < n; i++) hm_put(hm_buf, (uint64_t)(uintptr_t)bi[i].buffer, (uint64_t)(uintptr_t)bi[i].memory, bi[i].memoryOffset, 0);
        LeaveCriticalSection(&dl_cs);
    }
    return r2_vkBindBufferMemory2(d, n, bi);
}
static void VKAPI_CALL W_vkDestroyBuffer(VkDevice d, VkBuffer b, const VkAllocationCallbacks *a)
{
    if (dl_cs_ok) { EnterCriticalSection(&dl_cs); hm_del(hm_buf, (uint64_t)(uintptr_t)b); LeaveCriticalSection(&dl_cs); }
    r2_vkDestroyBuffer(d, b, a);
}
/* where the program sees a buffer's byte off, if that is in mapped memory, and how many bytes from there are mapped */
static const float *dl_buf_ptr_n(VkBuffer b, VkDeviceSize off, uint64_t *avail)
{
    *avail = 0;
    if (!dl_cs_ok || !b) return NULL;
    const float *res = NULL;
    EnterCriticalSection(&dl_cs);
    HmEnt *e = hm_get(hm_buf, (uint64_t)(uintptr_t)b);
    if (e) {
        HmEnt *m = hm_get(hm_mem, e->a);
        uint64_t at = e->b + off;
        if (m && at >= m->b && at < m->b + m->c && !((at - m->b) & 3)) {
            res = (const float *)((char *)(uintptr_t)m->a + (at - m->b));
            *avail = m->b + m->c - at;
        }
    }
    LeaveCriticalSection(&dl_cs);
    return res;
}
/* the same, when at least need bytes are there */
static const float *dl_buf_ptr(VkBuffer b, VkDeviceSize off, uint64_t need)
{
    uint64_t n;
    const float *p = dl_buf_ptr_n(b, off, &n);
    return p && n >= need ? p : NULL;
}
static void VKAPI_CALL W_vkCmdPushDescriptorSetKHR(VkCommandBuffer c, VkPipelineBindPoint bp, VkPipelineLayout l, uint32_t set, uint32_t n, const VkWriteDescriptorSet *w)
{
    if (drawlog_on && rec.target_win)
        for (uint32_t i = 0; i < n; i++) {
            const VkWriteDescriptorSet *x = &w[i];
            if (x->pBufferInfo && (x->descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || x->descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)) {
                VkDeviceSize range = x->pBufferInfo[0].range == VK_WHOLE_SIZE ? 64 : x->pBufferInfo[0].range;
                const float *f = dl_buf_ptr(x->pBufferInfo[0].buffer, x->pBufferInfo[0].offset, range < 256 ? range : 256);
                char t[900]; int k = 0;
                if (f) for (VkDeviceSize j = 0; j < range / 4 && j < 64 && k < 660; j++) {
                    uint32_t u; memcpy(&u, &f[j], 4);
                    if ((u & 0x7F800000u) == 0 && u) k += snprintf(t + k, sizeof t - k, " i%d", (int)u);
                    else k += snprintf(t + k, sizeof t - k, " %.4g", f[j]);
                }
                lg("  push descriptor binding %u type %d buffer offset %llu range %llu:%s", x->dstBinding, x->descriptorType,
                   (unsigned long long)x->pBufferInfo[0].offset, (unsigned long long)x->pBufferInfo[0].range, f ? t : " (not mapped)");
            } else lg("  push descriptor binding %u type %d", x->dstBinding, x->descriptorType);
        }
    if (rec.target_win)
        for (uint32_t i = 0; i < n; i++) {
            const VkWriteDescriptorSet *x = &w[i];
            if (!x->pBufferInfo || x->descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) continue;
            VkDeviceSize range = x->pBufferInfo[0].range;
            const float *f = x->dstBinding == 65 || x->dstBinding == 64 ? dl_buf_ptr(x->pBufferInfo[0].buffer, x->pBufferInfo[0].offset, x->dstBinding == 65 ? 192 : 8) : NULL;
            if (x->dstBinding == 65) {
                /* a screen's draw: area (ints), screen, ..., and at the end the part of it shown (window) */
                rec.pc_valid = 0;
                if (f && (range == VK_WHOLE_SIZE || range >= 192)) {
                    memcpy(rec.pc_area, f, sizeof rec.pc_area);
                    /* the part shown is in the screen's own pixels, which the Resolution setting scales (words 4-6:
                       the screen's width and height there): in the 3DS's pixels, 240 high */
                    int sh; memcpy(&sh, f + 5, sizeof sh);
                    float k = sh > 0 ? 240.0f / sh : 1.0f;
                    for (int j = 0; j < 4; j++) rec.pc_src[j] = f[44 + j] * k;
                    rec.pc_valid = 1;
                }
            } else if (x->dstBinding == 64 && f && (range == VK_WHOLE_SIZE || range >= 8)) {
                rec.ui_size[0] = f[0]; rec.ui_size[1] = f[1];
            }
        }
    if (rec.target_win) {
        rec.last_b0_img = -1;
        for (uint32_t i = 0; i < n; i++)
            if (w[i].dstBinding == 0 && w[i].pImageInfo && w[i].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
                rec.last_b0_view = w[i].pImageInfo[0].imageView;
                rec.last_b0_layout = w[i].pImageInfo[0].imageLayout;
                rec.last_b0_img = vk_view_img(rec.last_b0_view);
            }
    }
    r_vkCmdPushDescriptorSetKHR(c, bp, l, set, n, w);
}
static void VKAPI_CALL W_vkCmdPushConstants(VkCommandBuffer c, VkPipelineLayout l, VkShaderStageFlags st, uint32_t off, uint32_t size, const void *v)
{
    if (drawlog_on && rec.target_win) {
        char b[600]; int k = 0;
        const float *f = (const float *)v;
        for (uint32_t i = 0; i < size / 4 && i < 32 && k < 560; i++) k += snprintf(b + k, sizeof b - k, " %.4g", f[i]);
        lg("  push constants off %u size %u stages %x:%s", off, size, st, b);
    }
    r_vkCmdPushConstants(c, l, st, off, size, v);
}
static struct { VkBuffer vb; VkDeviceSize vbo; VkBuffer ib; VkDeviceSize ibo; VkIndexType it; } dl_geo;
static void VKAPI_CALL W_vkCmdBindVertexBuffers(VkCommandBuffer c, uint32_t first, uint32_t n, const VkBuffer *b, const VkDeviceSize *o)
{
    if (first == 0 && n) { dl_geo.vb = b[0]; dl_geo.vbo = o[0]; }
    r_vkCmdBindVertexBuffers(c, first, n, b, o);
}
static void VKAPI_CALL W_vkCmdBindIndexBuffer(VkCommandBuffer c, VkBuffer b, VkDeviceSize o, VkIndexType t)
{
    dl_geo.ib = b; dl_geo.ibo = o; dl_geo.it = t;
    r_vkCmdBindIndexBuffer(c, b, o, t);
}
static void VKAPI_CALL W_vkCmdUpdateBuffer(VkCommandBuffer c, VkBuffer b, VkDeviceSize off, VkDeviceSize size, const void *v)
{
    if (drawlog_on && rec.target_win) {
        char t[600]; int k = 0; const float *f = (const float *)v;
        for (VkDeviceSize i = 0; i < size / 4 && i < 32 && k < 560; i++) k += snprintf(t + k, sizeof t - k, " %.4g", f[i]);
        lg("  update buffer off %llu size %llu:%s", (unsigned long long)off, (unsigned long long)size, t);
    }
    r_vkCmdUpdateBuffer(c, b, off, size, v);
}
static void VKAPI_CALL W_vkCmdBindDescriptorSets(VkCommandBuffer c, VkPipelineBindPoint bp, VkPipelineLayout l, uint32_t first, uint32_t n, const VkDescriptorSet *ds, uint32_t nd, const uint32_t *dyn)
{
    if (drawlog_on && rec.target_win) lg("  bind descriptor sets first %u n %u dynamic %u (%u)", first, n, nd, nd ? dyn[0] : 0);
    r_vkCmdBindDescriptorSets(c, bp, l, first, n, ds, nd, dyn);
}
static void VKAPI_CALL W_vkCmdDrawIndexed(VkCommandBuffer c, uint32_t ic, uint32_t inst, uint32_t fi, int32_t vo, uint32_t finst)
{
    if (xm_hold && rec.target_win && rec.ui_size[0] > 0 && rec.ui_size[1] > 0 && ic && ic <= 65536) {
        /* the program's UI: its vertices (x, y, u, v and a colour, 20 bytes each), named by the indices */
        uint64_t vn = 0, in_ = 0;
        const float *v = dl_buf_ptr_n(dl_geo.vb, dl_geo.vbo, &vn);
        const char *ix = (const char *)dl_buf_ptr_n(dl_geo.ib, dl_geo.ibo, &in_);
        uint64_t isz = dl_geo.it == VK_INDEX_TYPE_UINT16 ? 2 : 4;
        if (v && ix && ((uint64_t)fi + ic) * isz <= in_) {
            float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
            int bad = 0;
            for (uint32_t i = 0; i < ic && !bad; i++) {
                uint32_t id = isz == 2 ? ((const uint16_t *)ix)[fi + i] : ((const uint32_t *)ix)[fi + i];
                int64_t at = (int64_t)id + vo;
                if (at < 0 || (uint64_t)at * 20 + 8 > vn) { bad = 1; break; }
                const float *p = v + (size_t)at * 5;
                if (p[0] < x0) x0 = p[0];
                if (p[0] > x1) x1 = p[0];
                if (p[1] < y0) y0 = p[1];
                if (p[1] > y1) y1 = p[1];
            }
            if (!bad) {
            /* (the UI over the whole window: its own pixels from the window's top left) */
            VkViewport vp_ = rec.viewport;
            float kx = win_w / rec.ui_size[0], ky = win_h / rec.ui_size[1];
            if (fabsf(vp_.x) < 1 && fabsf(vp_.width - win_w) < 2 && fabsf(fabsf(vp_.height) - win_h) < 2 && xm_in_zone(x0 * kx, y0 * ky, x1 * kx, y1 * ky)) return;
            }
        }
    }
    drawlog("indexed", ic, fi);
    if (drawlog_on && rec.target_win) {
        uint64_t vn = 0, in_ = 0, isz = dl_geo.it == VK_INDEX_TYPE_UINT16 ? 2 : 4;
        const float *v = dl_buf_ptr_n(dl_geo.vb, dl_geo.vbo, &vn);
        const char *ix = (const char *)dl_buf_ptr_n(dl_geo.ib, dl_geo.ibo, &in_);
        lg("    vertex offset %d, index type %d, vertex buffer %s, index buffer %s", vo, dl_geo.it, v ? "mapped" : "not mapped", ix ? "mapped" : "not mapped");
        if (v && ix) {
            char t[900]; int k = 0;
            /* the first 6 indices, and the vertices they name: x, y, u, v and a colour (20 bytes) */
            for (uint32_t i = 0; i < 6 && i < ic && k < 800 && ((uint64_t)fi + i + 1) * isz <= in_; i++) {
                uint32_t id = isz == 2 ? ((const uint16_t *)ix)[fi + i] : ((const uint32_t *)ix)[fi + i];
                int64_t at = (int64_t)id + vo;
                if (at < 0 || (uint64_t)at * 20 + 20 > vn) break;
                const float *p = v + (size_t)at * 5;
                k += snprintf(t + k, sizeof t - k, " [%lld: %.1f %.1f %.3f %.3f %08x]", (long long)at, p[0], p[1], p[2], p[3], *(const uint32_t *)&p[4]);
            }
            lg("    %s", t);
        }
    }
    r_vkCmdDrawIndexed(c, ic, inst, fi, vo, finst);
}
static void VKAPI_CALL W_vkCmdSetViewport(VkCommandBuffer c, uint32_t f, uint32_t n, const VkViewport *v) { if (n) rec.viewport = v[0]; r_vkCmdSetViewport(c, f, n, v); }
static void VKAPI_CALL W_vkCmdSetScissor(VkCommandBuffer c, uint32_t f, uint32_t n, const VkRect2D *r) { if (n) rec.scissor = r[0]; r_vkCmdSetScissor(c, f, n, r); }
static void VKAPI_CALL W_vkCmdDraw(VkCommandBuffer c, uint32_t vc, uint32_t ic, uint32_t fv, uint32_t fi)
{
    drawlog("plain", vc, fv);
    if (game_on && rec.target_win && vc == 3 && rec.last_b0_img >= 0) {
        VkExtent3D e = vk_imgs[rec.last_b0_img].ext;
        /* the bottom screen: stored turned, 240 wide and 320 tall (times the resolution) */
        if (e.height && fabsf((float)e.width / e.height - 0.75f) < 0.01f && e.width >= 200) {
            vk_bottom_view = rec.last_b0_view; vk_bottom_layout = rec.last_b0_layout; vk_bottom_img = rec.last_b0_img;
            VkViewport v = rec.viewport;
            VkRect2D r = rec.scissor;
            int cut = (float)r.extent.width < v.width - 2 || (float)r.extent.height < v.height - 2;
            /* the part shown, when the pushed rectangles are this draw's */
            const float *part = NULL;
            if (rec.pc_valid && rec.pc_area[0] == (int)lrintf(v.x) && rec.pc_area[2] == (int)lrintf(v.width) && rec.pc_area[3] == (int)lrintf(v.height)
                && rec.pc_src[2] > 0.5f && rec.pc_src[3] > 0.5f) part = rec.pc_src;
            saw_bottom(v.x, win_h - (v.y + v.height), v.width, v.height, cut, part);
            if (box_on && ui_box) return;
            if (xm_hold && part && xm_piece_of(part) >= 0) return;
        } else if (e.height && (float)e.width / e.height > 0.25f && (float)e.width / e.height < 0.61f && e.width >= 200) {
            /* the top screen: stored turned, 240 wide and 400 tall, or more (428 for 16:9, more with "Fill screen") */
            VkViewport v = rec.viewport;
            saw_top(v.x, win_h - (v.y + v.height), v.width, v.height);
        }
    }
    r_vkCmdDraw(c, vc, ic, fv, fi);
}
static void VKAPI_CALL W_vkCmdClearAttachments(VkCommandBuffer c, uint32_t n, const VkClearAttachment *a, uint32_t nr, const VkClearRect *r)
{
    /* the little square the program marks a touch with, on the box */
    if (game_on && rec.target_win && ui_on && nr == 1 && r->rect.extent.width <= 16 && r->rect.extent.height <= 16) return;
    r_vkCmdClearAttachments(c, n, a, nr, r);
}
static void VKAPI_CALL W_vkCmdBlitImage(VkCommandBuffer c, VkImage s, VkImageLayout sl, VkImage d, VkImageLayout dl, uint32_t n, const VkImageBlit *r, VkFilter f)
{
    if (game_on && vk_is_swap(d) && vk_img_find(s) >= 0) {
        static int told;
        if (!told++) lg("Vulkan: the window's picture goes to the screen by a blit, %d,%d-%d,%d to %d,%d-%d,%d, layout %d",
                        r->srcOffsets[0].x, r->srcOffsets[0].y, r->srcOffsets[1].x, r->srcOffsets[1].y,
                        r->dstOffsets[0].x, r->dstOffsets[0].y, r->dstOffsets[1].x, r->dstOffsets[1].y, sl);
        v_flip = (r->srcOffsets[1].y - r->srcOffsets[0].y) * (r->dstOffsets[1].y - r->dstOffsets[0].y) < 0;
        vk_frame_end(c, s, sl);
    }
    r_vkCmdBlitImage(c, s, sl, d, dl, n, r, f);
}
/* our read-back went into a command buffer of the program's: once that is sent, a fence of ours after it on the same queue */
static void rb_sent(VkQueue q)
{
    int slot = v_rbqueued;
    if (slot >= 0 && r_vkQueueSubmit(q, 0, NULL, v_rbfence[slot]) == VK_SUCCESS) v_rbbusy[slot] = 1;
    v_rbqueued = -1;
}
static VkResult VKAPI_CALL W_vkQueueSubmit(VkQueue q, uint32_t n, const VkSubmitInfo *si, VkFence fence)
{
    VkResult r = r_vkQueueSubmit(q, n, si, fence);
    if (v_rbqueued >= 0 && r == VK_SUCCESS)
        for (uint32_t i = 0; i < n; i++) for (uint32_t j = 0; j < si[i].commandBufferCount; j++)
            if (si[i].pCommandBuffers[j] == v_rbcb) { rb_sent(q); return r; }
    return r;
}
static PFN_vkQueueSubmit2 r_vkQueueSubmit2;
static VkResult VKAPI_CALL W_vkQueueSubmit2(VkQueue q, uint32_t n, const VkSubmitInfo2 *si, VkFence fence)
{
    VkResult r = r_vkQueueSubmit2(q, n, si, fence);
    if (v_rbqueued >= 0 && r == VK_SUCCESS)
        for (uint32_t i = 0; i < n; i++) for (uint32_t j = 0; j < si[i].commandBufferInfoCount; j++)
            if (si[i].pCommandBufferInfos[j].commandBuffer == v_rbcb) { rb_sent(q); return r; }
    return r;
}
static VkResult VKAPI_CALL W_vkQueuePresentKHR(VkQueue q, const VkPresentInfoKHR *pi)
{
    if (v_rbqueued >= 0) rb_sent(q);   /* (sent some other way: the present queue, as a last resort) */
    if (drawlog_on) { drawlog_on = 0; lg("(draw log ends)"); }
    pc_n = 0;
    if (debug_on && file_here("usumfix_drawlog.txt")) { DeleteFileA(path_here("usumfix_drawlog.txt")); drawlog_on = 1; lg("draw log, frame %u:", frame + 1); }
    frame++;
    box_on = 0; seen_bottom = 0; box_n = 0; top_seen = 0;
    return r_vkQueuePresentKHR(q, pi);
}

#define VW(n) if (!strcmp(name, #n)) { if (!r_##n) r_##n = (PFN_##n)real; return (PFN_vkVoidFunction)W_##n; }
static PFN_vkVoidFunction vk_wrap(const char *name, PFN_vkVoidFunction real)
{
    if (!real || !game_on) return real;
    VW(vkCreateDevice) VW(vkCreateImage) VW(vkCreateImageView) VW(vkCreateSwapchainKHR) VW(vkGetSwapchainImagesKHR)
    VW(vkCmdBeginRendering) VW(vkCmdEndRendering) VW(vkCmdPushDescriptorSetKHR) VW(vkCmdSetViewport) VW(vkCmdSetScissor) VW(vkCmdDraw)
    VW(vkCmdClearAttachments) VW(vkCmdBlitImage) VW(vkQueueSubmit) VW(vkQueuePresentKHR) VW(vkCmdDrawIndexed) VW(vkCmdPushConstants)
    VW(vkCmdUpdateBuffer) VW(vkCmdBindDescriptorSets) VW(vkCmdBindVertexBuffers) VW(vkCmdBindIndexBuffer)
#define VW2(n) if (!strcmp(name, #n)) { if (!r2_##n) r2_##n = (PFN_##n)real; return (PFN_vkVoidFunction)W_##n; }
    VW2(vkAllocateMemory) VW2(vkMapMemory) VW2(vkUnmapMemory) VW2(vkFreeMemory) VW2(vkBindBufferMemory) VW2(vkBindBufferMemory2) VW2(vkDestroyBuffer)
    if (!strcmp(name, "vkBindBufferMemory2KHR")) { if (!r2_vkBindBufferMemory2) r2_vkBindBufferMemory2 = (PFN_vkBindBufferMemory2)real; return (PFN_vkVoidFunction)W_vkBindBufferMemory2; }
#undef VW2
    VW(vkQueueSubmit2)
    if (!strcmp(name, "vkQueueSubmit2KHR")) { if (!r_vkQueueSubmit2) r_vkQueueSubmit2 = (PFN_vkQueueSubmit2)real; return (PFN_vkVoidFunction)W_vkQueueSubmit2; }
    if (!strcmp(name, "vkCmdBeginRenderingKHR")) { if (!r_vkCmdBeginRendering) r_vkCmdBeginRendering = (PFN_vkCmdBeginRendering)real; return (PFN_vkVoidFunction)W_vkCmdBeginRendering; }
    if (!strcmp(name, "vkCmdEndRenderingKHR")) { if (!r_vkCmdEndRendering) r_vkCmdEndRendering = (PFN_vkCmdEndRendering)real; return (PFN_vkVoidFunction)W_vkCmdEndRendering; }
    return real;
}
static PFN_vkVoidFunction VKAPI_CALL my_GDPA(VkDevice d, const char *name) { return vk_wrap(name, r_GDPA(d, name)); }
static PFN_vkVoidFunction VKAPI_CALL my_GIPA(VkInstance i, const char *name)
{
    if (!strcmp(name, "vkGetInstanceProcAddr")) return (PFN_vkVoidFunction)my_GIPA;
    PFN_vkVoidFunction f = r_GIPA(i, name);
    if (i) vk_inst = i;
    if (!strcmp(name, "vkGetDeviceProcAddr")) { if (f) r_GDPA = (PFN_vkGetDeviceProcAddr)f; return f ? (PFN_vkVoidFunction)my_GDPA : f; }
    return vk_wrap(name, f);
}

/* ========================================================== GetProcAddress */
static FARPROC WINAPI my_GetProcAddress(HMODULE m, LPCSTR n)
{
    FARPROC f = real_GPA(m, n);
    if (!f || (ULONG_PTR)n <= 0xFFFF || !game_on) return f;
    if (!strcmp(n, "vkGetInstanceProcAddr")) {
        r_GIPA = (PFN_vkGetInstanceProcAddr)f;
        lg("Vulkan: vkGetInstanceProcAddr wrapped");
        return (FARPROC)my_GIPA;
    }
    if (n[0] == 'g' && n[1] == 'l') return (FARPROC)gl_wrap_proc(n, (void *)f);
    return f;
}

/* =================================================================== input */
typedef int (*PollEvent_t)(void *);
typedef void *(*GetWindowFromID_t)(uint32_t);
typedef void (*GetWindowSize_t)(void *, int *, int *);
static PollEvent_t real_PollEvent;
static GetWindowFromID_t real_GetWindowFromID;
static GetWindowSize_t real_GetWindowSize;
static int press_hit = -1;
static float press_to[2];
static int swallowing;
static int in_rect(const float *r, float x, float y) { return x >= r[0] && y >= r[1] && x < r[0] + r[2] && y < r[1] + r[3]; }
/* where (x, y), in picture pixels, should go: 1 moved, 2 dropped, 0 left */
static int remap(float *x, float *y, int is_down, int is_up)
{
    int res = 0;
    float ox = *x, oy = *y;
    EnterCriticalSection(&lay_cs);
    ptr_x = ox; ptr_y = oy;
    if (is_down) swallowing = 0;
    if (press_hit >= 0 && !is_down) { *x = press_to[0]; *y = press_to[1]; res = 1; }
    else if (lay_pub.active) {
        /* the topmost piece under the press, and the bottom-screen point it shows there */
        int h = -1; float sx = 0, sy = 0;
        for (int i = lay_pub.nhit - 1; i >= 0 && h < 0; i--) {
            const Hit *t = &lay_pub.hit[i];
            if (t->dst[2] <= 0 || t->dst[3] <= 0) continue;
            float px = t->src[0] + (*x - t->dst[0]) / t->dst[2] * (t->src[2] - t->src[0]);
            float py = t->src[1] + (*y - t->dst[1]) / t->dst[3] * (t->src[3] - t->src[1]);
            if (poly_has(&t->poly, px, py, t->grow)) {
                h = i;
                /* kept on the piece, and inside the bottom screen */
                sx = px < t->src[0] + 1 ? t->src[0] + 1 : px > t->src[2] - 1 ? t->src[2] - 1 : px;
                sy = py < t->src[1] + 1 ? t->src[1] + 1 : py > t->src[3] - 1 ? t->src[3] - 1 : py;
                if (t->fx >= 0) { sx = t->fx; sy = t->fy; }
            }
        }
        if (is_down) press_hit = h;
        if (h >= 0) {
            *x = lay_pub.box[0] + sx / 320.0f * lay_pub.box[2];
            *y = lay_pub.box[1] + sy / 240.0f * lay_pub.box[3];
            res = 1;
            if (is_down) lg("  piece %d, bottom screen %.0f,%.0f, box %.0f,%.0f %.0fx%.0f", h, sx, sy, lay_pub.box[0], lay_pub.box[1], lay_pub.box[2], lay_pub.box[3]);
            if (is_down) { press_to[0] = *x; press_to[1] = *y; }
        } else if (is_down || swallowing) {
            if (in_rect(lay_pub.box, *x, *y)) res = 2;   /* the held-back box: not a touch */
            if (swallowing) res = 2;
            if (is_down && res == 2) swallowing = 1;
        }
    } else if (swallowing) res = 2;
    if (is_up) { press_hit = -1; swallowing = 0; }
    LeaveCriticalSection(&lay_cs);
    if ((is_down || is_up) && (res || lay_pub.active))
        lg("%s at %.0f,%.0f -> %s %.0f,%.0f", is_down ? "press" : "release", ox, oy, res == 2 ? "dropped" : res ? "moved to" : "kept", *x, *y);
    return res;
}
__declspec(dllexport) int SDL_PollEvent(void *ev)
{
    if (!real_PollEvent) {
        HMODULE s = GetModuleHandleA("SDL2_real.dll");
        if (!s) s = LoadLibraryA("SDL2_real.dll");
        real_PollEvent = (PollEvent_t)GetProcAddress(s, "SDL_PollEvent");
        real_GetWindowFromID = (GetWindowFromID_t)GetProcAddress(s, "SDL_GetWindowFromID");
        real_GetWindowSize = (GetWindowSize_t)GetProcAddress(s, "SDL_GetWindowSize");
    }
    for (;;) {
        int r = real_PollEvent(ev);
        if (!r || !ev || !game_on) return r;
        uint32_t type = *(uint32_t *)ev;
        unsigned char *e = (unsigned char *)ev;
        int res = 0;
        if (type == 0x400 || type == 0x401 || type == 0x402) {           /* mouse motion / down / up */
            uint32_t wid = *(uint32_t *)(e + 8);
            int32_t *px = (int32_t *)(e + 20), *py = px + 1;
            int ww = 0, wh = 0;
            void *w = real_GetWindowFromID ? real_GetWindowFromID(wid) : NULL;
            if (w && real_GetWindowSize) real_GetWindowSize(w, &ww, &wh);
            if (ww > 0 && wh > 0 && lay_pub.dw > 0) {
                float kx = lay_pub.dw / ww, ky = lay_pub.dh / wh;
                float x = *px * kx, y = *py * ky;
                res = remap(&x, &y, type == 0x401, type == 0x402);
                if (res == 1) { *px = (int32_t)(x / kx); *py = (int32_t)(y / ky); }
            }
        } else if (type == 0x403) {                                       /* the wheel: over the info column, it scrolls it */
            int32_t wy = *(int32_t *)(e + 20);
            uint32_t dir = *(uint32_t *)(e + 24);
            if (dir == 1) wy = -wy;
            lg("wheel %d (direction %u) at %.0f,%.0f; column %.0f,%.0f %.0fx%.0f", wy, dir, ptr_x, ptr_y, info_box[0], info_box[1], info_box[2], info_box[3]);
            EnterCriticalSection(&lay_cs);
            if (lay_pub.active && info_box[2] > 0 && ptr_x >= info_box[0] && ptr_y >= info_box[1] && ptr_x < info_box[0] + info_box[2] && ptr_y < info_box[1] + info_box[3]) {
                info_to -= wy * info_step;
                res = 2;
            }
            LeaveCriticalSection(&lay_cs);
        } else if (type == 0x700 || type == 0x701 || type == 0x702) {    /* finger down / up / motion */
            float *fx = (float *)(e + 24), *fy = (float *)(e + 28);
            if (lay_pub.dw > 0) {
                float x = *fx * lay_pub.dw, y = *fy * lay_pub.dh;
                res = remap(&x, &y, type == 0x700, type == 0x701);
                if (res == 1) { *fx = x / lay_pub.dw; *fy = y / lay_pub.dh; }
            }
        }
        if (res != 2) return r;
    }
}

/* ================================================================== set up */
static void patch_iat(HMODULE mod, const char *dll, const char *fn, void *hook, void **orig)
{
    BYTE *base = (BYTE *)mod;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_DATA_DIRECTORY dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress) return;
    for (IMAGE_IMPORT_DESCRIPTOR *d = (IMAGE_IMPORT_DESCRIPTOR *)(base + dir.VirtualAddress); d->Name; d++) {
        if (_stricmp((char *)(base + d->Name), dll)) continue;
        IMAGE_THUNK_DATA *names = (IMAGE_THUNK_DATA *)(base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        IMAGE_THUNK_DATA *iat = (IMAGE_THUNK_DATA *)(base + d->FirstThunk);
        for (; names->u1.AddressOfData; names++, iat++) {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
            IMAGE_IMPORT_BY_NAME *ibn = (IMAGE_IMPORT_BY_NAME *)(base + names->u1.AddressOfData);
            if (strcmp((char *)ibn->Name, fn)) continue;
            DWORD old;
            VirtualProtect(&iat->u1.Function, sizeof(void *), PAGE_READWRITE, &old);
            if (!*orig) *orig = (void *)iat->u1.Function;
            iat->u1.Function = (ULONG_PTR)hook;
            VirtualProtect(&iat->u1.Function, sizeof(void *), old, &old);
        }
    }
}
/* which game: this folder's name is its title id (FullDex makes 3ds\<title id>); else the dump rom.txt names */
static int is_usum_id(unsigned long long id) { return id == 0x00040000001B5000ULL || id == 0x00040000001B5100ULL; }
static int detect_game(void)
{
    char d[MAX_PATH]; strcpy(d, here);
    size_t n = strlen(d);
    if (n && (d[n - 1] == '\\' || d[n - 1] == '/')) d[--n] = 0;
    const char *base = strrchr(d, '\\'); const char *b2 = strrchr(d, '/');
    if (b2 > base) base = b2;
    base = base ? base + 1 : d;
    if (strlen(base) == 16) { unsigned long long id = strtoull(base, NULL, 16); if (is_usum_id(id)) return 1; }
    FILE *f = fopen(path_here("rom.txt"), "r");
    char p[1024] = "";
    if (f) { if (!fgets(p, sizeof p, f)) p[0] = 0; fclose(f); }
    p[strcspn(p, "\r\n")] = 0;
    FILE *g = p[0] ? fopen(p, "rb") : NULL;
    if (!g) g = fopen(path_here("game.3ds"), "rb");
    if (g) {
        unsigned char h[0x110];
        size_t got = fread(h, 1, sizeof h, g);
        fclose(g);
        if (got == sizeof h && !memcmp(h + 0x100, "NCSD", 4)) {
            unsigned long long id = 0; for (int i = 7; i >= 0; i--) id = id << 8 | h[0x108 + i];
            if (is_usum_id(id)) return 1;
        }
    }
    return 0;
}
/* the size the player chose for the battle's buttons (Battle Button Size, 100 = as it comes), read again when the settings change */
static float ui_scale = 1;
static void read_battle_size(void)
{
    static FILETIME seen;
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (!GetFileAttributesExA(path_here("config\\settings.toml"), GetFileExInfoStandard, &fa)) return;
    if (!CompareFileTime(&fa.ftLastWriteTime, &seen)) return;
    seen = fa.ftLastWriteTime;
    FILE *f = fopen(path_here("config\\settings.toml"), "rb");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof line, f)) {
        int v;
        if (sscanf(line, "battle_size = %d", &v) == 1) {
            float k = v / 100.0f;
            ui_scale = k < 0.5f ? 0.5f : k > 2.0f ? 2.0f : k;
            lg("Battle Button Size %d%%", v);
        }
    }
    fclose(f);
}
/* the add-on works with the whole bottom screen in a box (Battle Buttons: Whole screen) */
static void want_whole_screen(void)
{
    const char *p = path_here("config\\settings.toml");
    FILE *f = fopen(p, "rb");
    if (!f) return;
    char buf[16384]; size_t n = fread(buf, 1, sizeof buf - 1, f); fclose(f); buf[n] = 0;
    char *k = strstr(buf, "battle_buttons = true");
    if (!k || n + 1 >= sizeof buf) return;
    memmove(k + strlen("battle_buttons = false"), k + strlen("battle_buttons = true"), strlen(k + strlen("battle_buttons = true")) + 1);
    memcpy(k, "battle_buttons = false", strlen("battle_buttons = false"));
    f = fopen(p, "wb");
    if (f) { fwrite(buf, 1, strlen(buf), f); fclose(f); lg("Battle Buttons set to Whole screen for the add-on"); }
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    (void)h; (void)r;
    if (reason == DLL_PROCESS_ATTACH) {
        InitializeCriticalSection(&log_cs);
        InitializeCriticalSection(&lay_cs);
        InitializeCriticalSection(&vk_cs);
        InitializeCriticalSection(&dl_cs); dl_cs_ok = 1;
        GetModuleFileNameA(NULL, here, MAX_PATH);
        char *s = strrchr(here, '\\'); if (s) s[1] = 0; else here[0] = 0;
        debug_on = file_here("usumfix_debug.txt");
        game_on = detect_game();
        for (int i = 0; i < NCOMP; i++) for (int k = 0; k < U_N; k++) comp[i].u[k].loc = -1;
        atlas_make();
        HMODULE exe = GetModuleHandleA(NULL);
        patch_iat(exe, "OPENGL32.dll", "wglGetProcAddress", (void *)my_wglGetProcAddress, (void **)&real_wglGPA);
        patch_iat(exe, "GDI32.dll", "SwapBuffers", (void *)my_SwapBuffers, (void **)&real_Swap);
        patch_iat(exe, "kernel32.dll", "GetProcAddress", (void *)my_GetProcAddress, (void **)&real_GPA);
        patch_iat(exe, "KERNEL32.dll", "GetProcAddress", (void *)my_GetProcAddress, (void **)&real_GPA);
        if (!real_GPA) real_GPA = (GPA_t)GetProcAddress;
        if (game_on && !file_here("usumfix_off.txt")) want_whole_screen();
        lg("usumfix loaded in %s: %s; OpenGL %s, Vulkan %s", here, game_on ? "Ultra Sun / Ultra Moon" : "another game (left alone)",
           real_wglGPA ? "watched" : "not used", real_GPA ? "watched" : "not used");
    }
    return TRUE;
}
