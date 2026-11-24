/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "client.h"

#define INTRO_W 1920.0f
#define INTRO_H 1080.0f
#define INTRO_LOGO 140.0f
#define INTRO_ZOOM 0.7f
#define INTRO_GAP 70.0f
#define INTRO_SIZE 95.0f
#define INTRO_BUILD 300.0f
#define INTRO_TEXTW 715.68f
#define INTRO_CAPH 67.0f
#define INTRO_RX (511.9f / 1024.0f)
#define INTRO_RY (371.6f / 1024.0f)
#define INTRO_RR (298.6f / 1024.0f)
#define INTRO_RI (260.1f / 1024.0f)
#define INTRO_DY (-32.6f / 1024.0f)
#define INTRO_PAD (160.0f / 1024.0f)
#define INTRO_MEDAL 240.0f
#define INTRO_LINE 5.0f
#define INTRO_M_RO 26.98f
#define INTRO_M_WO 3.72f
#define INTRO_M_RI 19.6f
#define INTRO_M_WI 2.55f
#define INTRO_M_RAIL 17.0f
#define INTRO_M_WS 1.98f
#define INTRO_SP_K 2.837f
#define INTRO_SP_B 0.375f
#define INTRO_SP_C 5.65f
#define INTRO_SP_F 4.662f
#define INTRO_G_WR 21.6f
#define INTRO_G_WW 2.2f
#define INTRO_G_WDY 0.8f
#define INTRO_G_FX 31.0f
#define INTRO_G_FY 53.0f
#define INTRO_G_W0 7
#define INTRO_G_N 18
#define INTRO_G_RING 14.0f

#define INTRO_TILEIN 0.1f
#define INTRO_MED0 0.08f
#define INTRO_FRAME 0.03f
#define INTRO_EXIT0 0.92f
#define INTRO_EXIT1 1.02f
#define INTRO_CV0 0.98f
#define INTRO_CV1 1.16f
#define INTRO_G0 1.02f
#define INTRO_GL0 1.78f
#define INTRO_GL1 2.04f
#define INTRO_FIST_OUT 0.18f
#define INTRO_DROP0 1.74f
#define INTRO_HIT 2.04f
#define INTRO_FILL1 2.32f
#define INTRO_MV0 2.4f
#define INTRO_MV1 2.95f
#define INTRO_TXT 2.68f
#define INTRO_STAG 0.03f
#define INTRO_DUR 0.45f
#define INTRO_RISE 0.35f
#define INTRO_SHINE0 2.3f
#define INTRO_SHINE1 2.68f
#define INTRO_OUT0 3.66f
#define INTRO_OUT1 3.98f
#define INTRO_BG0 3.98f
#define INTRO_WARM_FRAMES 12
#define INTRO_WARM_MS 40
#define INTRO_END 4.28f

#define INTRO_MAX_VERTS 3072
#define INTRO_MAX_POLY 192
#define INTRO_MAX_SEGMENTS 128

typedef struct {
    float x, y;
    float c[4];
} introVert_t;

typedef enum { INTRO_IDLE, INTRO_RUNNING, INTRO_DONE } introState_t;

static const char introName[] = "IOQUAKELIVE";
static const float introGlyphX[] = {0.0f,     34.1f,    117.6f,   203.0f,  276.62f, 352.045f,
                                    424.905f, 489.595f, 549.915f, 584.015f, 658.49f};
static const float introBlue[3] = {64.0f / 255.0f, 115.0f / 255.0f, 1.0f};
static const float introWhite[3] = {1.0f, 1.0f, 1.0f};
static const float introFistY[] = {0.108f, 0.243f, 0.54f, 0.838f, 1.0f, 1.027f, 1.027f, 1.027f, 1.0f};
static const float introFistX[] = {0.385f, 0.77f, 1.0f, 1.0f};
static const float introSpanI[] = {0.0f, 18.5f, 39.0f, 63.75f, 90.25f, 117.0f, 141.75f, 161.5f, 175.5f, 180.0f};
static const float introSpanO[] = {0.0f, 5.5f, 19.0f, 39.0f, 64.0f, 90.0f, 117.0f, 141.25f, 161.5f, 175.25f, 180.0f};
static const float introHead[] = {-17.6f, -16.3f, -12.7f, -9.2f, -5.7f, -2.1f, 0.8f, 4.2f, 7.8f, 11.3f, 14.2f, 17.0f};
static const float introBeamW[] = {2.77f, 5.61f, 4.21f, 2.8f};
static const float introPlate[] = {16.0f, 21.5f, 24.0f, 26.0f, 25.0f, 24.5f};

static struct {
    introState_t state;
    float time;
    int last;
    int warm;
    int swallowKey;
    qhandle_t tile, arc, nail, nailGlow, line, fist;
    qhandle_t wreath[INTRO_G_N];
    int font;
    float scale, ox, oy, cam, sx, sy;
    qhandle_t batch;
    int numVerts;
    polyVert_t verts[INTRO_MAX_VERTS];
} intro = {INTRO_IDLE, 0.0f, 0, 0, -1};

static float Intro_Clamp(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

static float Intro_Lerp(float a, float b, float p) {
    return a + (b - a) * p;
}

static float Intro_Span(float t, float a, float b) {
    return Intro_Clamp((t - a) / (b - a));
}

static float Intro_EaseOut(float p) {
    float q = 1.0f - p;

    return 1.0f - q * q * q;
}

static float Intro_EaseInOut(float p) {
    float q;

    if (p < 0.5f) {
        return 8.0f * p * p * p * p;
    }
    q = -2.0f * p + 2.0f;
    return 1.0f - q * q * q * q * 0.5f;
}

static float Intro_EaseExpo(float p) {
    return p >= 1.0f ? 1.0f : 1.0f - powf(2.0f, -10.0f * p);
}

static void Intro_Point(float lx, float ly, float* x, float* y) {
    *x = intro.ox + intro.scale * (intro.cam * lx + INTRO_W * 0.5f * (1.0f - intro.cam) + intro.sx);
    *y = intro.oy + intro.scale * (intro.cam * ly + INTRO_H * 0.5f * (1.0f - intro.cam) + intro.sy);
}

static void Intro_Color(float r, float g, float b, float a) {
    vec4_t c;

    c[0] = r;
    c[1] = g;
    c[2] = b;
    c[3] = a;
    re.SetColor(c);
}

static void Intro_Pic(float lx, float ly, float lw, float lh, qhandle_t shader) {
    float x, y, u = intro.scale * intro.cam;

    Intro_Point(lx, ly, &x, &y);
    re.DrawStretchPic(x, y, lw * u, lh * u, 0.0f, 0.0f, 1.0f, 1.0f, shader);
}

static void Intro_Flush(void) {
    if (intro.numVerts && re.DrawTriangles2D) {
        re.DrawTriangles2D(intro.numVerts, intro.verts, intro.batch);
    }
    intro.numVerts = 0;
}

static void Intro_Begin(qhandle_t shader) {
    Intro_Flush();
    intro.batch = shader;
}

static byte Intro_Byte(float v) {
    return (byte)(Intro_Clamp(v) * 255.0f + 0.5f);
}

static void Intro_Set(introVert_t* v, float x, float y, const float* c, float value, qboolean add) {
    v->x = x;
    v->y = y;
    if (add) {
        v->c[0] = c[0] * value;
        v->c[1] = c[1] * value;
        v->c[2] = c[2] * value;
        v->c[3] = 1.0f;
    } else {
        v->c[0] = c[0];
        v->c[1] = c[1];
        v->c[2] = c[2];
        v->c[3] = value;
    }
}

static void Intro_Vertex(const introVert_t* v, const float* rect) {
    polyVert_t* p = &intro.verts[intro.numVerts++];

    Intro_Point(v->x, v->y, &p->xyz[0], &p->xyz[1]);
    p->xyz[2] = 0.0f;
    p->st[0] = (v->x - rect[0]) / (rect[2] - rect[0]);
    p->st[1] = (v->y - rect[1]) / (rect[3] - rect[1]);
    p->modulate[0] = Intro_Byte(v->c[0]);
    p->modulate[1] = Intro_Byte(v->c[1]);
    p->modulate[2] = Intro_Byte(v->c[2]);
    p->modulate[3] = Intro_Byte(v->c[3]);
}

static int Intro_ClipEdge(const introVert_t* in, int n, introVert_t* out, int axis, float limit, float sign) {
    int i, k, m = 0;

    for (i = 0; i < n; i++) {
        const introVert_t* a = &in[i];
        const introVert_t* b = &in[(i + 1) % n];
        float da = sign * ((axis ? a->y : a->x) - limit);
        float db = sign * ((axis ? b->y : b->x) - limit);

        if (da >= 0.0f) {
            out[m++] = *a;
        }
        if ((da >= 0.0f) != (db >= 0.0f)) {
            float f = da / (da - db);

            out[m].x = a->x + (b->x - a->x) * f;
            out[m].y = a->y + (b->y - a->y) * f;
            for (k = 0; k < 4; k++) {
                out[m].c[k] = a->c[k] + (b->c[k] - a->c[k]) * f;
            }
            m++;
        }
    }
    return m;
}

static void Intro_Poly(const introVert_t* in, int n, const float* rect) {
    introVert_t a[INTRO_MAX_POLY], b[INTRO_MAX_POLY];
    int i;

    if (n < 3 || n > INTRO_MAX_POLY - 4) {
        return;
    }
    n = Intro_ClipEdge(in, n, a, 0, rect[0], 1.0f);
    n = Intro_ClipEdge(a, n, b, 0, rect[2], -1.0f);
    n = Intro_ClipEdge(b, n, a, 1, rect[1], 1.0f);
    n = Intro_ClipEdge(a, n, b, 1, rect[3], -1.0f);
    for (i = 1; i + 1 < n; i++) {
        if (intro.numVerts + 3 > INTRO_MAX_VERTS) {
            Intro_Flush();
        }
        Intro_Vertex(&b[0], rect);
        Intro_Vertex(&b[i], rect);
        Intro_Vertex(&b[i + 1], rect);
    }
}

static int Intro_Segments(float r) {
    float rs = r * intro.scale * intro.cam;
    int n;

    if (rs <= 1.0f) {
        return 16;
    }
    n = (int)ceilf(M_PI / acosf(1.0f - 0.25f / rs));
    return n < 16 ? 16 : (n > INTRO_MAX_SEGMENTS ? INTRO_MAX_SEGMENTS : n);
}

static float Intro_Table(const float* v, int n, float f) {
    float a, b, c, d, p;
    int i;

    if (f <= 0.0f) {
        return v[0];
    }
    if (f >= n - 1) {
        return v[n - 1];
    }
    i = (int)f;
    p = f - i;
    a = v[i > 0 ? i - 1 : 0];
    b = v[i];
    c = v[i + 1];
    d = v[i + 2 < n ? i + 2 : n - 1];
    return b + 0.5f * p * (c - a + p * (2.0f * a - 5.0f * b + 4.0f * c - d + p * (3.0f * (b - c) + d - a)));
}

static void Intro_Stroke(const float* p0, const float* n0, const float* p1, const float* n1, float h, const float* c,
                         float alpha, const float* rect) {
    float e = 0.5f / (intro.scale * intro.cam), o[4], a[4];
    introVert_t q[4];
    int b;

    if (alpha <= 0.0f || h <= 0.0f) {
        return;
    }
    if (h < e) {
        alpha *= h / e;
        h = e;
    }
    o[0] = -h - e;
    o[1] = -h + e;
    o[2] = h - e;
    o[3] = h + e;
    a[0] = 0.0f;
    a[1] = alpha;
    a[2] = alpha;
    a[3] = 0.0f;
    for (b = 0; b < 3; b++) {
        if (o[b + 1] <= o[b]) {
            continue;
        }
        Intro_Set(&q[0], p0[0] + n0[0] * o[b], p0[1] + n0[1] * o[b], c, a[b], qfalse);
        Intro_Set(&q[1], p0[0] + n0[0] * o[b + 1], p0[1] + n0[1] * o[b + 1], c, a[b + 1], qfalse);
        Intro_Set(&q[2], p1[0] + n1[0] * o[b + 1], p1[1] + n1[1] * o[b + 1], c, a[b + 1], qfalse);
        Intro_Set(&q[3], p1[0] + n1[0] * o[b], p1[1] + n1[1] * o[b], c, a[b], qfalse);
        Intro_Poly(q, 4, rect);
    }
}

static float Intro_Reach(float x, float y, float rx, float ry) {
    return atan2f(fabsf(x - rx), y - ry);
}

static void Intro_Ring(float cx, float cy, float r, float h, float th0, float th1, const float* c, float alpha,
                       float reach, float rx, float ry, const float* rect) {
    float step = 2.0f * M_PI / Intro_Segments(r), p[2][2], n[2][2], th[2], al[2];
    int steps, j, k, i;

    if (th1 <= th0 || alpha <= 0.0f || h <= 0.0f || r <= 0.0f) {
        return;
    }
    steps = (int)ceilf((th1 - th0) / step);
    for (j = 0; j < steps; j++) {
        th[0] = th0 + (th1 - th0) * j / steps;
        th[1] = th0 + (th1 - th0) * (j + 1) / steps;
        for (k = 0; k < 2; k++) {
            al[k] = Intro_Reach(cx + r * cosf(th[k]), cy + r * sinf(th[k]), rx, ry);
        }
        if (al[0] < reach && al[1] < reach) {
            continue;
        }
        if (al[0] < reach || al[1] < reach) {
            float lo = th[0], hi = th[1];
            qboolean in0 = al[0] >= reach;

            for (i = 0; i < 12; i++) {
                float mid = (lo + hi) * 0.5f;

                if ((Intro_Reach(cx + r * cosf(mid), cy + r * sinf(mid), rx, ry) >= reach) == in0) {
                    lo = mid;
                } else {
                    hi = mid;
                }
            }
            if (in0) {
                th[1] = lo;
            } else {
                th[0] = hi;
            }
        }
        for (k = 0; k < 2; k++) {
            n[k][0] = cosf(th[k]);
            n[k][1] = sinf(th[k]);
            p[k][0] = cx + r * n[k][0];
            p[k][1] = cy + r * n[k][1];
        }
        Intro_Stroke(p[0], n[0], p[1], n[1], h, c, alpha, rect);
    }
}

static void Intro_Rail(float mx, float my, float k, float u0, float u1, float w, const float* c, float alpha,
                       const float* rect) {
    float p0[2], p1[2], nv[2];

    if (u1 <= u0) {
        return;
    }
    nv[0] = 0.70710678f;
    nv[1] = 0.70710678f;
    p0[0] = mx + u0 * k * 0.70710678f;
    p0[1] = my - u0 * k * 0.70710678f;
    p1[0] = mx + u1 * k * 0.70710678f;
    p1[1] = my - u1 * k * 0.70710678f;
    Intro_Stroke(p0, nv, p1, nv, w * k * 0.5f, c, alpha, rect);
}

static float Intro_SpiralTheta(float u) {
    float th = u / INTRO_SP_K;
    int i;

    for (i = 0; i < 4; i++) {
        th -= (INTRO_SP_K * th + INTRO_SP_B * sinf(th + INTRO_SP_F) - u) /
              (INTRO_SP_K + INTRO_SP_B * cosf(th + INTRO_SP_F));
    }
    return th;
}

static void Intro_Spiral(float mx, float my, float k, float u0, float u1, const float* c, float alpha,
                         const float* rect) {
    float th0, th1, p[2][2], n[2][2];
    int steps, j, m;

    if (u1 <= u0) {
        return;
    }
    th0 = Intro_SpiralTheta(u0);
    th1 = Intro_SpiralTheta(u1);
    steps = (int)ceilf((th1 - th0) / 0.09f);
    if (steps < 1) {
        steps = 1;
    }
    for (j = 0; j <= steps; j++) {
        float th = th0 + (th1 - th0) * j / steps, ph = th + INTRO_SP_F;
        float u = INTRO_SP_K * th + INTRO_SP_B * sinf(ph), v = INTRO_SP_C * cosf(ph);
        float du = INTRO_SP_K + INTRO_SP_B * cosf(ph), dv = -INTRO_SP_C * sinf(ph);
        float tx = (du + dv) * 0.70710678f, ty = (dv - du) * 0.70710678f, l = sqrtf(tx * tx + ty * ty);

        m = j & 1;
        p[m][0] = mx + (u + v) * k * 0.70710678f;
        p[m][1] = my + (v - u) * k * 0.70710678f;
        n[m][0] = -ty / l;
        n[m][1] = tx / l;
        if (j > 0) {
            Intro_Stroke(p[m ^ 1], n[m ^ 1], p[m], n[m], INTRO_M_WS * k * 0.5f, c, alpha, rect);
        }
    }
}

static void Intro_Fan(float cx, float cy, float r, float from, float to, float alpha, const float* rect) {
    float step = 2.0f * M_PI / Intro_Segments(r);
    introVert_t q[3];
    int side, j, steps;

    if (to <= from || alpha <= 0.0f) {
        return;
    }
    steps = (int)ceilf((to - from) / step);
    for (side = -1; side <= 1; side += 2) {
        for (j = 0; j < steps; j++) {
            float a0 = from + (to - from) * j / steps, a1 = from + (to - from) * (j + 1) / steps;

            Intro_Set(&q[0], cx, cy, introWhite, alpha, qfalse);
            Intro_Set(&q[1], cx + side * sinf(a0) * r, cy + cosf(a0) * r, introWhite, alpha, qfalse);
            Intro_Set(&q[2], cx + side * sinf(a1) * r, cy + cosf(a1) * r, introWhite, alpha, qfalse);
            Intro_Poly(q, 3, rect);
        }
    }
}

static void Intro_Plate(float cx, float cy, float hw, float rh, const float* c, float alpha, const float* rect) {
    float e = 0.5f / (intro.scale * intro.cam);
    introVert_t v[4];
    int q, j, k, n;

    if (alpha <= 0.0f || hw * 1.41421356f <= rh) {
        return;
    }
    n = Intro_Segments(rh) / 4 + 1;
    for (q = 0; q < 4; q++) {
        for (j = 0; j < n; j++) {
            float ux[2], uy[2], rb[2], ro[2];

            for (k = 0; k < 2; k++) {
                float ph = (-0.25f + 0.5f * (j + k) / n) * M_PI, th = q * 0.5f * M_PI + ph, sec = 1.0f / cosf(ph);

                ux[k] = cosf(th);
                uy[k] = sinf(th);
                rb[k] = hw * sec - e * sec > rh ? hw * sec - e * sec : rh;
                ro[k] = hw * sec + e * sec > rh ? hw * sec + e * sec : rh;
            }
            if (ro[0] <= rh && ro[1] <= rh) {
                continue;
            }
            Intro_Set(&v[0], cx + ux[0] * rh, cy + uy[0] * rh, c, alpha, qfalse);
            Intro_Set(&v[1], cx + ux[0] * rb[0], cy + uy[0] * rb[0], c, alpha, qfalse);
            Intro_Set(&v[2], cx + ux[1] * rb[1], cy + uy[1] * rb[1], c, alpha, qfalse);
            Intro_Set(&v[3], cx + ux[1] * rh, cy + uy[1] * rh, c, alpha, qfalse);
            Intro_Poly(v, 4, rect);
            Intro_Set(&v[0], cx + ux[0] * rb[0], cy + uy[0] * rb[0], c, alpha, qfalse);
            Intro_Set(&v[1], cx + ux[0] * ro[0], cy + uy[0] * ro[0], c, 0.0f, qfalse);
            Intro_Set(&v[2], cx + ux[1] * ro[1], cy + uy[1] * ro[1], c, 0.0f, qfalse);
            Intro_Set(&v[3], cx + ux[1] * rb[1], cy + uy[1] * rb[1], c, alpha, qfalse);
            Intro_Poly(v, 4, rect);
        }
    }
}

static float Intro_WreathFrame(float t) {
    float fw = 1.0f + (t - INTRO_G0) / INTRO_FRAME;

    return fw > 24.0f ? 24.0f : fw;
}

static void Intro_Gauntlet(float t, float mx, float my, float cx, float cy, float s, float reach, float fade) {
    float k = INTRO_MEDAL / 64.0f, fg = 1.0f + (t - INTRO_G0) / INTRO_FRAME, fw = Intro_WreathFrame(t);
    float gl = Intro_EaseInOut(Intro_Span(t, INTRO_GL0, INTRO_GL1)), wy = my + INTRO_G_WDY * k;
    float sc = Intro_Lerp(1.0f, (INTRO_RR + INTRO_RI) * 0.5f * s / (INTRO_G_WR * k), gl);
    float rx = Intro_Lerp(mx, cx, gl), ry = Intro_Lerp(wy, cy, gl);
    float l = rx - 32.0f * k * sc, top = ry + (my - 32.0f * k - wy) * sc, w = 64.0f * k * sc;
    int i;

    if (fg < 0.6f || fade <= 0.0f || reach >= M_PI) {
        return;
    }
    if (fw > 7.0f) {
        i = (int)fw;
        i = i > 24 ? 24 : i;
        if (reach > 0.0f) {
            float wr[4];

            wr[0] = l;
            wr[1] = top;
            wr[2] = l + w;
            wr[3] = top + w;
            Intro_Begin(intro.wreath[i - INTRO_G_W0]);
            Intro_Fan(cx, cy, INTRO_RR * s * 1.6f, reach, M_PI, fade, wr);
            Intro_Flush();
        } else {
            Intro_Color(1.0f, 1.0f, 1.0f, fade);
            Intro_Pic(l, top, w, w, intro.wreath[i - INTRO_G_W0]);
        }
    }
    if (t >= INTRO_GL0) {
        fg = 9.0f - 8.4f * Intro_Span(t, INTRO_GL0, INTRO_GL0 + INTRO_FIST_OUT);
    }
    if (fg >= 1.0f) {
        float sx = Intro_Table(introFistX, ARRAY_LEN(introFistX), fg - 1.0f) * sc;
        float sy = Intro_Table(introFistY, ARRAY_LEN(introFistY), fg - 1.0f) * sc;
        float ax = rx + (INTRO_G_FX - 32.0f) * k * sc, ay = ry + (my + (INTRO_G_FY - 32.0f) * k - wy) * sc;

        Intro_Color(1.0f, 1.0f, 1.0f, fade);
        Intro_Pic(ax + (l - ax) * sx / sc, ay + (top - ay) * sy / sc, w * sx / sc, w * sy / sc, intro.fist);
    }
}

static void Intro_Medal(float t, float mx, float my, float cx, float cy, float s, float reach, float fade) {
    float f = 1.0f + (t - INTRO_MED0) / INTRO_FRAME, k = INTRO_MEDAL / 64.0f;
    float ex = Intro_Span(t, INTRO_EXIT0, INTRO_EXIT1);
    float cv = Intro_EaseInOut(Intro_Span(t, INTRO_CV0, INTRO_CV1));
    float wy = my + INTRO_G_WDY * k, big[4], lo, ho, hn, hd;

    if (f < 0.6f || reach >= M_PI || fade <= 0.0f) {
        return;
    }
    big[0] = -INTRO_W;
    big[1] = -INTRO_H;
    big[2] = INTRO_W * 2.0f;
    big[3] = INTRO_H * 2.0f;
    ho = DEG2RAD(Intro_Table(introSpanO, ARRAY_LEN(introSpanO), f - 3.0f));
    hn = DEG2RAD(Intro_Table(introSpanI, ARRAY_LEN(introSpanI), f - 3.0f));
    ho = ho > M_PI ? M_PI : ho;
    hn = hn > M_PI ? M_PI : hn;
    fade *= Intro_Clamp((M_PI - reach) / 0.5f);

    Intro_Begin(intro.line);
    if (f >= 19.0f && t < INTRO_EXIT1) {
        Intro_Plate(mx, my, Intro_Lerp(Intro_Table(introPlate, ARRAY_LEN(introPlate), f - 19.0f), 18.0f, ex * ex) * k,
                    INTRO_M_RO * k, introWhite, fade, big);
    }
    if (1.0f + (t - INTRO_G0) / INTRO_FRAME < INTRO_G_RING) {
        Intro_Ring(mx, Intro_Lerp(my, wy, cv), Intro_Lerp(INTRO_M_RO, INTRO_G_WR, cv) * k,
                   Intro_Lerp(INTRO_M_WO, INTRO_G_WW, cv) * k * 0.5f, 0.75f * M_PI - ho, 0.75f * M_PI + ho, introWhite,
                   fade, 0.0f, mx, wy, big);
        Intro_Ring(mx, Intro_Lerp(my, wy, cv), Intro_Lerp(INTRO_M_RI, INTRO_G_WR, cv) * k,
                   Intro_Lerp(INTRO_M_WI, INTRO_G_WW, cv) * k * 0.5f, 0.75f * M_PI - hn, 0.75f * M_PI + hn, introWhite,
                   fade, 0.0f, mx, wy, big);
    }

    lo = INTRO_M_RAIL - 2.0f * INTRO_M_RAIL * Intro_Clamp(f - 0.62f) + 2.0f * INTRO_M_RAIL * ex * ex;
    if (lo < INTRO_M_RAIL) {
        Intro_Rail(mx, my, k, lo, INTRO_M_RAIL, Intro_Table(introBeamW, ARRAY_LEN(introBeamW), f - 1.0f), introWhite,
                   fade, big);
        hd = Intro_Table(introHead, ARRAY_LEN(introHead), f - 9.0f);
        Intro_Spiral(mx, my, k, lo > -INTRO_M_RAIL ? lo : -INTRO_M_RAIL, hd < INTRO_M_RAIL ? hd : INTRO_M_RAIL,
                     introWhite, fade, big);
    }
    Intro_Flush();
}

static void Intro_Shine(float x, float y, float s, float bx, float alpha, const float* rect) {
    float cx = x + s * 0.5f, cy = y + s * 0.5f, co = cosf(-0.38f), si = sinf(-0.38f);
    float u[3], al[3];
    introVert_t q[4];
    int i, k;

    u[0] = bx - s * 0.3f;
    u[1] = bx;
    u[2] = bx + s * 0.3f;
    al[0] = 0.0f;
    al[1] = alpha;
    al[2] = 0.0f;
    for (i = 0; i < 2; i++) {
        float cu[4], cv[4], ca[4];

        cu[0] = u[i];
        cv[0] = -s;
        ca[0] = al[i];
        cu[1] = u[i + 1];
        cv[1] = -s;
        ca[1] = al[i + 1];
        cu[2] = u[i + 1];
        cv[2] = s;
        ca[2] = al[i + 1];
        cu[3] = u[i];
        cv[3] = s;
        ca[3] = al[i];
        for (k = 0; k < 4; k++) {
            Intro_Set(&q[k], cx + cu[k] * co - cv[k] * si, cy + cu[k] * si + cv[k] * co, introWhite, ca[k], qfalse);
        }
        Intro_Poly(q, 4, rect);
    }
}

static void Intro_Text(float t, float finalX, float fade) {
    float lt = t - INTRO_TXT, base = INTRO_H * 0.5f + INTRO_CAPH * 0.5f, tx = finalX + INTRO_LOGO + INTRO_GAP;
    char ch[2];
    int n;

    if (lt <= 0.0f) {
        return;
    }
    ch[1] = '\0';
    for (n = 0; n < (int)ARRAY_LEN(introGlyphX); n++) {
        float p = Intro_Clamp((lt - n * INTRO_STAG) / INTRO_DUR), e, x, y;

        if (p <= 0.0f) {
            continue;
        }
        e = Intro_EaseExpo(Intro_Clamp(p / INTRO_RISE));
        x = intro.ox + intro.scale * (tx + introGlyphX[n]);
        y = intro.oy + intro.scale * (base + (1.0f - e) * 18.0f);
        Intro_Color(0.949f, 0.949f, 0.949f, Intro_EaseOut(Intro_Clamp(p * 1.4f)) * fade);
        ch[0] = introName[n];
        re.Font_DrawString((int)floorf(x + 0.5f), (int)floorf(y + 0.5f), ch, intro.font, INTRO_SIZE * intro.scale, 0,
                           NULL, 0);
    }
}

static void Intro_Logo(float t, float x, float y, float s, float fade) {
    float h = t - INTRO_HIT;
    float cx = x + INTRO_RX * s, cy = y + INTRO_RY * s;
    float reach = h > 0.0f ? M_PI * Intro_EaseOut(Intro_Span(t, INTRO_HIT, INTRO_FILL1)) : 0.0f;
    float sp = Intro_Span(t, INTRO_SHINE0, INTRO_SHINE1);
    float pad = s * INTRO_PAD, rect[4];

    rect[0] = x;
    rect[1] = y;
    rect[2] = x + s;
    rect[3] = y + s;

    Intro_Color(introBlue[0], introBlue[1], introBlue[2], Intro_Clamp(t / INTRO_TILEIN) * fade);
    Intro_Pic(x, y, s, s, intro.tile);

    if (reach >= M_PI) {
        Intro_Color(1.0f, 1.0f, 1.0f, fade);
        Intro_Pic(x, y, s, s, intro.arc);
    } else if (reach > 0.0f) {
        Intro_Begin(intro.arc);
        Intro_Fan(cx, cy, INTRO_RR * s * 1.35f, 0.0f, reach, fade, rect);
        Intro_Flush();
    }

    Intro_Gauntlet(t, x + s * 0.5f, y + s * 0.5f, cx, cy, s, reach, fade);
    Intro_Medal(t, x + s * 0.5f, y + s * 0.5f, cx, cy, s, reach, fade);

    if (t >= INTRO_DROP0) {
        float dd = INTRO_HIT - INTRO_DROP0, p = Intro_Span(t, INTRO_DROP0, INTRO_HIT), l = s * 2.35f;
        float off = -l * (1.0f - powf(p, 2.6f));
        float vel = l * 2.6f * powf(p, 1.6f) / dd;
        float trail = t < INTRO_HIT ? (s * 0.6f < vel * 0.035f ? s * 0.6f : vel * 0.035f) : 0.0f;
        float na = Intro_Clamp((t - INTRO_DROP0) / 0.03f) * fade;
        int i;

        if (trail > 1.0f) {
            for (i = 18; i >= 1; i--) {
                Intro_Color(1.0f, 1.0f, 1.0f, na * 0.22f * (1.0f - i / 19.0f));
                Intro_Pic(x, y + off - trail * i / 18.0f, s, s, intro.nail);
            }
        }
        Intro_Color(1.0f, 1.0f, 1.0f, na);
        Intro_Pic(x, y + off, s, s, intro.nail);
    }

    if (h >= 0.0f && h < 1.2f) {
        float g = 0.7f * expf(-h * 6.0f) * fade;

        if (g > 0.002f) {
            Intro_Color(g, g, g, 1.0f);
            Intro_Pic(x - pad, y - pad, s + pad * 2.0f, s + pad * 2.0f, intro.nailGlow);
        }
    }

    if (sp > 0.0f && sp < 1.0f) {
        Intro_Begin(intro.tile);
        Intro_Shine(x, y, s, Intro_Lerp(-s * 1.15f, s * 1.15f, Intro_EaseInOut(sp)), 0.32f * fade, rect);
        Intro_Flush();
    }
}

static void Intro_Render(float t) {
    float vw = cls.glconfig.vidWidth, vh = cls.glconfig.vidHeight;
    float fade = 1.0f - Intro_Span(t, INTRO_OUT0, INTRO_OUT1), bg = 1.0f - Intro_Span(t, INTRO_BG0, INTRO_END);
    float k, m, s, x, finalX;

    intro.scale = (vh / INTRO_H < vw / 1200.0f ? vh / INTRO_H : vw / 1200.0f) * INTRO_ZOOM;
    intro.ox = (vw - INTRO_W * intro.scale) * 0.5f;
    intro.oy = (vh - INTRO_H * intro.scale) * 0.5f;

    Intro_Color(0.0f, 0.0f, 0.0f, bg);
    re.DrawStretchPic(0.0f, 0.0f, vw, vh, 0.0f, 0.0f, 0.0f, 0.0f, cls.whiteShader);

    if (fade > 0.0f) {
        intro.cam = 1.0f;
        k = t >= INTRO_HIT ? expf(-(t - INTRO_HIT) * 13.0f) : 0.0f;
        intro.sx = k * 8.0f * sinf(t * 97.3f);
        intro.sy = k * 8.0f * cosf(t * 71.9f);

        m = Intro_EaseInOut(Intro_Span(t, INTRO_MV0, INTRO_MV1));
        finalX = (INTRO_W - (INTRO_LOGO + INTRO_GAP + INTRO_TEXTW)) * 0.5f;
        s = Intro_Lerp(INTRO_BUILD, INTRO_LOGO, m);
        x = Intro_Lerp(INTRO_W * 0.5f, finalX + INTRO_LOGO * 0.5f, m) - s * 0.5f;

        Intro_Text(t, finalX, 1.0f);
        Intro_Logo(t, x, INTRO_H * 0.5f - s * 0.5f, s, 1.0f);
        Intro_Flush();
        if (fade < 1.0f) {
            Intro_Color(0.0f, 0.0f, 0.0f, 1.0f - fade);
            re.DrawStretchPic(0.0f, 0.0f, vw, vh, 0.0f, 0.0f, 0.0f, 0.0f, cls.whiteShader);
        }
    }
    re.SetColor(NULL);
}

static qboolean Intro_Register(void) {
    int i;

    if (!re.DrawTriangles2D || !re.Font_Register || !re.Font_DrawString) {
        return qfalse;
    }
    for (i = 0; i < INTRO_G_N; i++) {
        intro.wreath[i] = re.RegisterShader(va("gfx/intro/gauntlet/wreath%02d", INTRO_G_W0 + i));
        if (!intro.wreath[i]) {
            return qfalse;
        }
    }
    intro.fist = re.RegisterShader("gfx/intro/gauntlet/fist");
    intro.tile = re.RegisterShader("gfx/intro/tile");
    intro.line = re.RegisterShader("gfx/intro/line");
    intro.arc = re.RegisterShader("gfx/intro/arc");
    intro.nail = re.RegisterShader("gfx/intro/nail");
    intro.nailGlow = re.RegisterShader("gfx/intro/nail_glow");
    intro.font = re.Font_Register("Outfit-SemiBold");
    return intro.tile && intro.arc && intro.nail && intro.nailGlow && intro.line && intro.fist && intro.font >= 0;
}

static void Intro_Start(void) {
    if (intro.state != INTRO_IDLE) {
        return;
    }
    if (com_dedicated->integer || clc.state != CA_DISCONNECTED || !Intro_Register()) {
        intro.state = INTRO_DONE;
        return;
    }
    intro.state = INTRO_RUNNING;
    intro.time = 0.0f;
    intro.last = cls.realtime;
    intro.warm = INTRO_WARM_FRAMES;
}

qboolean CL_IntroActive(void) {
    return intro.state == INTRO_RUNNING;
}

qboolean CL_IntroCoversUI(void) {
    Intro_Start();
    return intro.state == INTRO_RUNNING && clc.state == CA_DISCONNECTED && !intro.warm && intro.time < INTRO_BG0;
}

void CL_IntroDraw(void) {
    int now, dt;

    Intro_Start();
    if (intro.state != INTRO_RUNNING) {
        return;
    }
    if (clc.state != CA_DISCONNECTED || !re.DrawTriangles2D) {
        intro.state = INTRO_DONE;
        return;
    }
    now = cls.realtime;
    dt = now - intro.last;
    intro.last = now;
    if (intro.warm) {
        if (dt > 0 && dt <= INTRO_WARM_MS) {
            intro.warm = 0;
        } else {
            intro.warm--;
            dt = 0;
        }
    }
    if (dt < 0) {
        dt = 0;
    } else if (dt > 50) {
        dt = 50;
    }
    intro.time += dt * 0.001f;
    if (intro.time >= INTRO_END) {
        intro.state = INTRO_DONE;
        return;
    }
    Intro_Render(intro.time);
}

qboolean CL_IntroKey(int key, qboolean down) {
    if (intro.state == INTRO_RUNNING) {
        if (down) {
            intro.swallowKey = key;
        } else if (key == intro.swallowKey) {
            intro.swallowKey = -1;
        }
        return qtrue;
    }
    if (intro.swallowKey >= 0 && key == intro.swallowKey) {
        if (!down) {
            intro.swallowKey = -1;
        }
        return qtrue;
    }
    return qfalse;
}

qboolean CL_IntroChar(void) {
    return intro.state == INTRO_RUNNING;
}

void CL_IntroShutdown(void) {
    if (intro.state == INTRO_RUNNING) {
        intro.state = INTRO_DONE;
    }
    intro.numVerts = 0;
}
