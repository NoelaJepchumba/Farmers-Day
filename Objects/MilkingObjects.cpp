#include <GL/freeglut.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include "MilkingObjects.h"

namespace {

constexpr float kPi = 3.14159265f;

/* ------------------------------------------------------------------ */
/*  Small toolkit: colours, deterministic noise, drawing primitives    */
/* ------------------------------------------------------------------ */

struct Color {
    float r, g, b, a;
    Color(float R = 0, float G = 0, float B = 0, float A = 1) : r(R), g(G), b(B), a(A) {}
};
struct Vec2 { float x, y; };

inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline Color mix(const Color& a, const Color& b, float t) {
    return Color(lerp(a.r, b.r, t), lerp(a.g, b.g, t), lerp(a.b, b.b, t), lerp(a.a, b.a, t));
}
inline Color shade(const Color& c, float f) {
    return Color(clamp01(c.r * f), clamp01(c.g * f), clamp01(c.b * f), c.a);
}
inline Color alpha(const Color& c, float a) { return Color(c.r, c.g, c.b, a); }
inline void setColor(const Color& c) { glColor4f(c.r, c.g, c.b, c.a); }
inline float rnd(int n) { // deterministic 0..1 noise
    float s = std::sin(n * 127.1f + 311.7f) * 43758.5453f;
    return s - std::floor(s);
}

// Palette
const Color kSteel(0.66f, 0.69f, 0.72f);
const Color kSteelDark(0.36f, 0.39f, 0.42f);
const Color kWood(0.55f, 0.30f, 0.12f);
const Color kMilk(0.96f, 0.97f, 1.00f);

void rect(float x1, float y1, float x2, float y2, const Color& c) {
    setColor(c);
    glBegin(GL_QUADS);
    glVertex2f(x1, y1); glVertex2f(x2, y1); glVertex2f(x2, y2); glVertex2f(x1, y2);
    glEnd();
}
void vGradRect(float x1, float y1, float x2, float y2, const Color& top, const Color& bottom) {
    glBegin(GL_QUADS);
    setColor(bottom); glVertex2f(x1, y1); glVertex2f(x2, y1);
    setColor(top);    glVertex2f(x2, y2); glVertex2f(x1, y2);
    glEnd();
}
void poly(const std::vector<Vec2>& p, const Color& c) {
    setColor(c);
    glBegin(GL_POLYGON);
    for (const Vec2& v : p) glVertex2f(v.x, v.y);
    glEnd();
}
// Convex polygon with a top-to-bottom gradient.
void polyGrad(const std::vector<Vec2>& p, const Color& top, const Color& bottom) {
    float lo = 1e9f, hi = -1e9f, cx = 0, cy = 0;
    for (const Vec2& v : p) { lo = std::min(lo, v.y); hi = std::max(hi, v.y); cx += v.x; cy += v.y; }
    cx /= p.size(); cy /= p.size();
    auto col = [&](float y) { return mix(bottom, top, (y - lo) / (hi - lo + 1e-6f)); };
    glBegin(GL_TRIANGLE_FAN);
    setColor(col(cy)); glVertex2f(cx, cy);
    for (size_t i = 0; i <= p.size(); i++) {
        const Vec2& v = p[i % p.size()];
        setColor(col(v.y)); glVertex2f(v.x, v.y);
    }
    glEnd();
}
void ellipse(float cx, float cy, float rx, float ry, const Color& c, int seg = 40) {
    setColor(c);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= seg; i++) {
        float a = 2.0f * kPi * i / seg;
        glVertex2f(cx + std::cos(a) * rx, cy + std::sin(a) * ry);
    }
    glEnd();
}
// Part of an ellipse outline, angles in degrees (0 = right, 90 = top).
void arc(float cx, float cy, float rx, float ry, float a0, float a1, const Color& c, float w) {
    setColor(c);
    glLineWidth(w);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= 28; i++) {
        float a = (a0 + (a1 - a0) * i / 28.0f) * kPi / 180.0f;
        glVertex2f(cx + std::cos(a) * rx, cy + std::sin(a) * ry);
    }
    glEnd();
}
void line(float x1, float y1, float x2, float y2, const Color& c, float w = 2.0f) {
    setColor(c);
    glLineWidth(w);
    glBegin(GL_LINES);
    glVertex2f(x1, y1); glVertex2f(x2, y2);
    glEnd();
}
void beam(float x1, float y1, float x2, float y2, float halfW, const Color& c) {
    float dx = x2 - x1, dy = y2 - y1, len = std::sqrt(dx * dx + dy * dy) + 1e-6f;
    float nx = -dy / len * halfW, ny = dx / len * halfW;
    setColor(c);
    glBegin(GL_QUADS);
    glVertex2f(x1 + nx, y1 + ny); glVertex2f(x2 + nx, y2 + ny);
    glVertex2f(x2 - nx, y2 - ny); glVertex2f(x1 - nx, y1 - ny);
    glEnd();
}
void softShadow(float cx, float cy, float rx, float ry, float strength) {
    for (int i = 0; i < 4; i++) {
        float k = 1.0f - i * 0.2f;
        ellipse(cx, cy, rx * k, ry * k, Color(0, 0, 0, strength * 0.28f), 32);
    }
}

Vec2 bezier(Vec2 a, Vec2 b, Vec2 c, Vec2 d, float t) {
    float u = 1 - t;
    float w0 = u * u * u, w1 = 3 * u * u * t, w2 = 3 * u * t * t, w3 = t * t * t;
    return {w0 * a.x + w1 * b.x + w2 * c.x + w3 * d.x, w0 * a.y + w1 * b.y + w2 * c.y + w3 * d.y};
}
Vec2 catmull(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, float t) {
    float t2 = t * t, t3 = t2 * t;
    return {0.5f * (2 * p1.x + (-p0.x + p2.x) * t + (2 * p0.x - 5 * p1.x + 4 * p2.x - p3.x) * t2 + (-p0.x + 3 * p1.x - 3 * p2.x + p3.x) * t3),
            0.5f * (2 * p1.y + (-p0.y + p2.y) * t + (2 * p0.y - 5 * p1.y + 4 * p2.y - p3.y) * t2 + (-p0.y + 3 * p1.y - 3 * p2.y + p3.y) * t3)};
}

// A smooth thick ribbon along a path (wire handles, rope).
void ribbon(const std::vector<Vec2>& pts, float halfW, const Color& c) {
    setColor(c);
    glBegin(GL_QUAD_STRIP);
    for (size_t i = 0; i < pts.size(); i++) {
        Vec2 a = pts[i == 0 ? 0 : i - 1], b = pts[i + 1 < pts.size() ? i + 1 : i];
        float dx = b.x - a.x, dy = b.y - a.y, len = std::sqrt(dx * dx + dy * dy) + 1e-6f;
        float nx = -dy / len * halfW, ny = dx / len * halfW;
        glVertex2f(pts[i].x + nx, pts[i].y + ny);
        glVertex2f(pts[i].x - nx, pts[i].y - ny);
    }
    glEnd();
}
void bezierPath(Vec2 a, Vec2 b, Vec2 c, Vec2 d, std::vector<Vec2>& out, int n = 24) {
    out.clear();
    for (int i = 0; i <= n; i++) out.push_back(bezier(a, b, c, d, i / (float)n));
}

// Brightness across a cylinder (u: 0 = left edge, 1 = right edge):
// soft light from the upper-left plus a narrow specular streak.
float cylLight(float u) {
    float diffuse = 0.55f + 0.45f * std::cos((u - 0.30f) * kPi * 0.95f);
    float spec = std::exp(-(u - 0.24f) * (u - 0.24f) * 220.0f) * 0.5f;
    return diffuse + spec;
}

// A (possibly tapered) metal cylinder seen from the side, shaded across its width.
void cylinder(float xlb, float xrb, float xlt, float xrt, float y1, float y2, const Color& base) {
    const int N = 14;
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= N; i++) {
        float u = i / (float)N;
        setColor(shade(base, cylLight(u)));
        glVertex2f(lerp(xlb, xrb, u), y1);
        glVertex2f(lerp(xlt, xrt, u), y2);
    }
    glEnd();
}

// Cylinder band that hugs a tapered body at height y (used for rolled rims / ribs).
void bandOnTaper(float xlb, float xrb, float xlt, float xrt, float y1b, float y2b, float yA, float yB,
                 float grow, const Color& base) {
    auto at = [&](float y, float a, float b) { return lerp(a, b, (y - y1b) / (y2b - y1b)); };
    cylinder(at(yA, xlb, xlt) - grow, at(yA, xrb, xrt) + grow, at(yB, xlb, xlt) - grow, at(yB, xrb, xrt) + grow, yA, yB, base);
    line(at(yB, xlb, xlt) - grow, yB, at(yB, xrb, xrt) + grow, yB, Color(1, 1, 1, 0.35f), 1.0f);
    line(at(yA, xlb, xlt) - grow, yA, at(yA, xrb, xrt) + grow, yA, Color(0, 0, 0, 0.35f), 1.0f);
}

void rivet(float x, float y) {
    ellipse(x, y, 0.0055f, 0.0055f, Color(0.30f, 0.32f, 0.35f), 10);
    ellipse(x - 0.0015f, y + 0.0015f, 0.002f, 0.002f, Color(0.9f, 0.92f, 0.95f), 6);
}

/* ------------------------------------------------------------------ */
/*  Scene objects                                                      */
/* ------------------------------------------------------------------ */

void drawBucket(float time) {
    const float cx = -0.605f;
    softShadow(cx + 0.02f, -0.665f, 0.24f, 0.035f, 0.55f);

    // Spilt milk on the ground, with a couple of drops.
    ellipse(-0.40f, -0.69f, 0.085f, 0.014f, Color(0.97f, 0.97f, 1.0f, 0.85f), 28);
    ellipse(-0.425f, -0.687f, 0.03f, 0.004f, Color(1, 1, 1, 0.8f), 14);
    ellipse(-0.29f, -0.682f, 0.011f, 0.006f, Color(0.97f, 0.97f, 1.0f, 0.85f), 10);
    ellipse(-0.265f, -0.696f, 0.007f, 0.004f, Color(0.97f, 0.97f, 1.0f, 0.85f), 10);

    // Tapered galvanised body (wider at the rim), dark rolled base, two ribs.
    cylinder(-0.74f, -0.47f, -0.78f, -0.43f, -0.65f, -0.35f, kSteel);
    bandOnTaper(-0.74f, -0.47f, -0.78f, -0.43f, -0.65f, -0.35f, -0.65f, -0.615f, 0.006f, kSteelDark);
    bandOnTaper(-0.74f, -0.47f, -0.78f, -0.43f, -0.65f, -0.35f, -0.585f, -0.565f, 0.002f, shade(kSteel, 0.85f));
    bandOnTaper(-0.74f, -0.47f, -0.78f, -0.43f, -0.65f, -0.35f, -0.420f, -0.400f, 0.002f, shade(kSteel, 0.85f));
    // Dents and scuffs.
    for (int i = 0; i < 5; i++) {
        float x = -0.72f + 0.25f * rnd(i + 5), y = -0.58f + 0.17f * rnd(i + 9);
        ellipse(x, y, 0.012f + 0.01f * rnd(i), 0.006f, Color(0, 0, 0, 0.07f), 10);
    }

    // Rim lip, dark inside, then the milk.
    ellipse(cx, -0.35f, 0.18f, 0.055f, shade(kSteel, 1.15f));
    ellipse(cx, -0.352f, 0.165f, 0.046f, Color(0.30f, 0.32f, 0.35f));
    ellipse(cx, -0.356f, 0.145f, 0.037f, Color(0.88f, 0.90f, 0.96f));
    ellipse(cx, -0.352f, 0.125f, 0.028f, kMilk);
    float o = std::sin(time * 1.4f) * 0.035f;
    ellipse(cx - 0.04f + o, -0.350f, 0.04f, 0.007f, Color(1, 1, 1, 0.9f), 16);
    for (int i = 0; i < 9; i++) { // foam bubbles
        float a = rnd(i + 70) * 2 * kPi, r = 0.5f + 0.5f * rnd(i + 80);
        ellipse(cx + std::cos(a) * 0.11f * r, -0.352f + std::sin(a) * 0.024f * r, 0.007f, 0.004f, Color(0.85f, 0.88f, 0.95f, 0.8f), 8);
    }
    float rp = std::fmod(time * 0.5f, 1.0f); // slow ripple
    arc(cx + 0.02f, -0.352f, 0.02f + 0.07f * rp, 0.005f + 0.014f * rp, 0, 360, Color(0.7f, 0.75f, 0.9f, 0.35f * (1 - rp)), 1.0f);
    arc(cx, -0.35f, 0.18f, 0.055f, 180, 360, Color(1, 1, 1, 0.55f), 2.0f); // front lip light
    arc(cx, -0.35f, 0.18f, 0.055f, 20, 160, Color(0, 0, 0, 0.3f), 2.0f);   // back lip shade

    // Wire bail with ears.
    std::vector<Vec2> bail;
    bezierPath({-0.77f, -0.375f}, {-0.80f, -0.03f}, {-0.41f, -0.03f}, {-0.44f, -0.375f}, bail, 32);
    ribbon(bail, 0.0075f, Color(0.22f, 0.24f, 0.26f));
    ribbon(bail, 0.003f, Color(0.62f, 0.65f, 0.68f));
    for (float ex : {-0.775f, -0.435f}) {
        ellipse(ex, -0.378f, 0.014f, 0.014f, kSteelDark, 14);
        rivet(ex, -0.378f);
    }
}

void drawMilkCan() {
    const float cx = 0.04f;
    softShadow(cx, -0.675f, 0.25f, 0.035f, 0.55f);

    // Side handles (behind the body): left and right wire loops.
    for (int side = -1; side <= 1; side += 2) {
        float x0 = side < 0 ? -0.12f : 0.20f, out = side * 0.14f;
        std::vector<Vec2> h;
        bezierPath({x0, -0.31f}, {x0 + out, -0.31f}, {x0 + out, -0.54f}, {x0, -0.54f}, h, 24);
        ribbon(h, 0.0085f, Color(0.22f, 0.24f, 0.27f));
        ribbon(h, 0.0035f, Color(0.60f, 0.63f, 0.67f));
    }

    // Body and shoulders (cone) shaded as cylinders.
    cylinder(-0.12f, 0.20f, -0.12f, 0.20f, -0.67f, -0.25f, kSteel);
    cylinder(-0.12f, 0.20f, -0.07f, 0.15f, -0.25f, -0.13f, kSteel);
    line(-0.12f, -0.25f, 0.20f, -0.25f, Color(0, 0, 0, 0.18f), 1.5f);

    // Painted label, shaded with the same cylinder light so it wraps.
    rect(-0.10f, -0.52f, 0.18f, -0.37f, Color(0.93f, 0.90f, 0.80f));
    rect(-0.10f, -0.405f, 0.18f, -0.395f, Color(0.20f, 0.35f, 0.65f));
    rect(-0.10f, -0.505f, 0.18f, -0.495f, Color(0.20f, 0.35f, 0.65f));
    // milk drop emblem
    {
        float dx = cx + 0.0f, dy = -0.465f, r = 0.026f;
        std::vector<Vec2> drop;
        drop.push_back({dx, dy + 0.065f});
        for (int i = 0; i <= 24; i++) {
            float a = (120.0f + 300.0f * i / 24.0f) * kPi / 180.0f;
            drop.push_back({dx + std::cos(a) * r, dy + std::sin(a) * r});
        }
        polyGrad(drop, Color(0.35f, 0.55f, 0.90f), Color(0.15f, 0.30f, 0.65f));
        ellipse(dx - 0.01f, dy + 0.005f, 0.005f, 0.01f, Color(1, 1, 1, 0.6f), 8);
    }
    glBegin(GL_QUADS); // wrap shading over the label
    for (int i = 0; i < 14; i++) {
        float u0 = i / 14.0f, u1 = (i + 1) / 14.0f;
        float x0 = lerp(-0.12f, 0.20f, u0), x1 = lerp(-0.12f, 0.20f, u1);
        float a0 = clamp01(1.05f - cylLight(u0)) * 0.7f, a1 = clamp01(1.05f - cylLight(u1)) * 0.7f;
        setColor(Color(0, 0, 0, a0)); glVertex2f(x0, -0.52f); glVertex2f(x0, -0.37f);
        setColor(Color(0, 0, 0, a1)); glVertex2f(x1, -0.37f); glVertex2f(x1, -0.52f);
    }
    glEnd();

    // Rolled metal rings: base, lower, upper, and the shoulder seam.
    cylinder(-0.13f, 0.21f, -0.13f, 0.21f, -0.675f, -0.64f, kSteelDark);
    for (float y : {-0.58f, -0.32f}) {
        cylinder(-0.13f, 0.21f, -0.13f, 0.21f, y, y + 0.03f, shade(kSteelDark, 1.05f));
        line(-0.13f, y + 0.03f, 0.21f, y + 0.03f, Color(1, 1, 1, 0.4f), 1.2f);
        line(-0.13f, y, 0.21f, y, Color(0, 0, 0, 0.4f), 1.2f);
    }

    // Neck with a lip, then the lid: short drum, dome, knob.
    cylinder(-0.055f, 0.135f, -0.055f, 0.135f, -0.13f, -0.04f, shade(kSteel, 1.1f));
    cylinder(-0.068f, 0.148f, -0.068f, 0.148f, -0.145f, -0.125f, kSteelDark);
    cylinder(-0.075f, 0.155f, -0.075f, 0.155f, -0.04f, -0.01f, shade(kSteelDark, 1.1f));
    {
        std::vector<Vec2> dome;
        for (int i = 0; i <= 20; i++) {
            float a = kPi * i / 20.0f;
            dome.push_back({cx + std::cos(a) * 0.115f, -0.01f + std::sin(a) * 0.032f});
        }
        // a polygon fan with cylinder-like horizontal shading
        glBegin(GL_TRIANGLE_FAN);
        setColor(shade(kSteelDark, 1.4f)); glVertex2f(cx, -0.01f);
        for (const Vec2& v : dome) {
            float u = (v.x - (cx - 0.115f)) / 0.23f;
            setColor(shade(kSteelDark, cylLight(u) * 1.1f)); glVertex2f(v.x, v.y);
        }
        glEnd();
    }
    ellipse(cx, 0.027f, 0.02f, 0.01f, Color(0.30f, 0.32f, 0.35f), 14);
    arc(cx, 0.03f, 0.03f, 0.022f, 0, 180, Color(0.20f, 0.22f, 0.25f), 3.0f); // lid ring

    // Rivets at the handle plates and a vertical highlight streak.
    rivet(0.195f, -0.31f); rivet(0.195f, -0.54f);
    rivet(-0.115f, -0.31f); rivet(-0.115f, -0.54f);
    line(-0.075f, -0.62f, -0.075f, -0.28f, Color(1, 1, 1, 0.22f), 4.0f);
}

void drawStool() {
    softShadow(0.65f, -0.67f, 0.24f, 0.03f, 0.55f);

    // Legs: two front legs splayed outwards, a darker back leg for depth.
    beam(0.65f, -0.27f, 0.65f, -0.655f, 0.022f, shade(kWood, 0.5f));
    poly({{0.52f, -0.27f}, {0.575f, -0.27f}, {0.555f, -0.66f}, {0.50f, -0.66f}}, shade(kWood, 0.85f));
    poly({{0.725f, -0.27f}, {0.78f, -0.27f}, {0.80f, -0.66f}, {0.745f, -0.66f}}, shade(kWood, 0.70f));
    for (float fx : {0.5275f, 0.7725f}) ellipse(fx, -0.663f, 0.03f, 0.008f, Color(0.15f, 0.07f, 0.02f), 12); // worn feet
    ellipse(0.65f, -0.655f, 0.028f, 0.007f, Color(0.12f, 0.05f, 0.02f), 12);
    // Cross rung and grain on legs.
    beam(0.54f, -0.50f, 0.765f, -0.505f, 0.009f, shade(kWood, 0.75f));
    line(0.54f, -0.492f, 0.765f, -0.497f, Color(1, 0.8f, 0.5f, 0.25f), 1.0f);
    for (int i = 0; i < 5; i++) {
        float lx = (i & 1) ? 0.752f : 0.548f;
        float y0 = -0.62f + 0.3f * rnd(i + 130);
        line(lx + (rnd(i) - 0.5f) * 0.01f, y0, lx + (rnd(i + 1) - 0.5f) * 0.01f, y0 + 0.07f, Color(0.15f, 0.07f, 0.02f, 0.4f), 1.0f);
    }

    // Thick rounded seat slab: front face, lit top, grain, a worn patch, edge highlight.
    std::vector<Vec2> slab = {{0.50f, -0.27f}, {0.80f, -0.27f}, {0.82f, -0.255f}, {0.82f, -0.215f}, {0.80f, -0.20f},
                              {0.50f, -0.20f}, {0.48f, -0.215f}, {0.48f, -0.255f}};
    polyGrad(slab, shade(kWood, 1.15f), shade(kWood, 0.68f));
    ellipse(0.65f, -0.20f, 0.165f, 0.026f, Color(0.78f, 0.48f, 0.20f), 36);
    ellipse(0.65f, -0.199f, 0.12f, 0.016f, Color(0.66f, 0.38f, 0.15f, 0.55f), 28); // wear from sitting
    for (int i = 0; i < 6; i++) {
        float x = 0.52f + 0.26f * rnd(i + 150);
        line(x, -0.205f + 0.012f * rnd(i + 160), x + 0.05f, -0.2f + 0.012f * rnd(i + 170), Color(0.35f, 0.17f, 0.05f, 0.45f), 1.0f);
    }
    for (int i = 0; i < 4; i++) {
        float y = -0.26f + 0.045f * rnd(i + 180);
        line(0.50f, y, 0.50f + 0.1f + 0.15f * rnd(i + 190), y, Color(0.25f, 0.12f, 0.03f, 0.4f), 1.0f);
    }
    arc(0.65f, -0.20f, 0.165f, 0.026f, 10, 170, Color(1, 0.85f, 0.55f, 0.7f), 1.5f);
    line(0.50f, -0.27f, 0.80f, -0.27f, Color(0.08f, 0.03f, 0.01f, 0.6f), 1.5f);
}

void drawRope(float time) {
    // Hangs from an iron nail; the free end sways gently and frays.
    const Vec2 key[5] = {{0.42f, 0.25f}, {0.48f, 0.12f}, {0.44f, 0.00f}, {0.50f, -0.12f}, {0.46f, -0.22f}};
    std::vector<Vec2> pts;
    for (int s = 0; s < 4; s++) {
        Vec2 p0 = key[s == 0 ? 0 : s - 1], p1 = key[s], p2 = key[s + 1], p3 = key[s == 3 ? 4 : s + 2];
        for (int i = 0; i < 12; i++) pts.push_back(catmull(p0, p1, p2, p3, i / 12.0f));
    }
    pts.push_back(key[4]);
    for (Vec2& p : pts) { // sway grows toward the free end
        float f = (0.25f - p.y) / 0.47f;
        p.x += std::sin(time * 1.3f + p.y * 3.0f) * 0.008f * f * f;
    }

    // Nail and the knot that holds the rope.
    beam(0.42f, 0.27f, 0.42f, 0.30f, 0.004f, Color(0.2f, 0.2f, 0.22f));
    ellipse(0.42f, 0.275f, 0.012f, 0.012f, Color(0.22f, 0.23f, 0.25f), 12);
    ellipse(0.4175f, 0.279f, 0.004f, 0.004f, Color(0.8f, 0.8f, 0.85f), 8);

    ribbon(pts, 0.0135f, Color(0.38f, 0.26f, 0.10f));   // shadow edge
    ribbon(pts, 0.0105f, Color(0.74f, 0.58f, 0.30f));   // rope body
    // Twisted strands: short diagonal strokes along the rope.
    float acc = 0;
    for (size_t i = 1; i < pts.size(); i++) {
        float dx = pts[i].x - pts[i - 1].x, dy = pts[i].y - pts[i - 1].y;
        float seg = std::sqrt(dx * dx + dy * dy);
        acc += seg;
        if (acc > 0.0115f) {
            acc = 0;
            float len = seg + 1e-6f, nx = -dy / len, ny = dx / len, tx = dx / len, ty = dy / len;
            const float w = 0.0105f;
            bool even = (i / 3) & 1;
            Color c = even ? Color(0.40f, 0.27f, 0.10f, 0.8f) : Color(0.92f, 0.76f, 0.45f, 0.7f);
            line(pts[i].x - nx * w - tx * 0.004f, pts[i].y - ny * w - ty * 0.004f,
                 pts[i].x + nx * w + tx * 0.004f, pts[i].y + ny * w + ty * 0.004f, c, 1.4f);
        }
    }
    ellipse(pts.front().x, pts.front().y - 0.004f, 0.0155f, 0.012f, Color(0.60f, 0.45f, 0.20f), 14); // knot
    ellipse(pts.front().x - 0.004f, pts.front().y, 0.005f, 0.004f, Color(0.95f, 0.8f, 0.5f, 0.6f), 8);

    // Frayed end.
    Vec2 e = pts.back();
    for (int i = 0; i < 6; i++) {
        float a = (-100.0f + i * 40.0f / 5.0f * 2.0f) * kPi / 180.0f;
        float len = 0.02f + 0.015f * rnd(i + 300);
        line(e.x + (i - 2.5f) * 0.0035f, e.y, e.x + (i - 2.5f) * 0.0035f + std::cos(a) * len * 0.5f, e.y + std::sin(a) * len,
             (i & 1) ? Color(0.85f, 0.7f, 0.4f) : Color(0.6f, 0.45f, 0.2f), 1.3f);
    }
}

} // namespace

void drawMilkingObjects() {
    float time = glutGet(GLUT_ELAPSED_TIME) * 0.001f;

    // Save only what we touch so the caller's state is untouched.
    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_LINE_BIT | GL_CURRENT_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    drawStool();
    drawMilkCan();
    drawBucket(time);
    drawRope(time);

    glPopAttrib();
}