#include <GL/freeglut.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include "FarmObjects.h"

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
inline Color mix(const Color& a, const Color& b, float t) {
    return Color(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t,
                 a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t);
}
inline Color shade(const Color& c, float f) {
    return Color(clamp01(c.r * f), clamp01(c.g * f), clamp01(c.b * f), c.a);
}
inline Color alpha(const Color& c, float a) { return Color(c.r, c.g, c.b, a); }
inline void setColor(const Color& c) { glColor4f(c.r, c.g, c.b, c.a); }

// Deterministic "random" 0..1 so the scene looks identical every frame.
inline float rnd(int n) {
    float s = std::sin(n * 127.1f + 311.7f) * 43758.5453f;
    return s - std::floor(s);
}

// Palette
const Color kWood(0.48f, 0.25f, 0.10f);
const Color kWoodDark(0.28f, 0.13f, 0.05f);
const Color kMetal(0.62f, 0.62f, 0.64f);
const Color kStrawLight(0.96f, 0.80f, 0.34f);
const Color kStrawMid(0.80f, 0.60f, 0.18f);
const Color kStrawDark(0.58f, 0.40f, 0.09f);

void rect(float x1, float y1, float x2, float y2, const Color& c) {
    setColor(c);
    glBegin(GL_QUADS);
    glVertex2f(x1, y1); glVertex2f(x2, y1); glVertex2f(x2, y2); glVertex2f(x1, y2);
    glEnd();
}

// Vertical gradient: `top` at y2, `bottom` at y1.
void vGradRect(float x1, float y1, float x2, float y2, const Color& top, const Color& bottom) {
    glBegin(GL_QUADS);
    setColor(bottom); glVertex2f(x1, y1); glVertex2f(x2, y1);
    setColor(top);    glVertex2f(x2, y2); glVertex2f(x1, y2);
    glEnd();
}

// Horizontal gradient: `left` at x1, `right` at x2 (gives round things volume).
void hGradRect(float x1, float y1, float x2, float y2, const Color& left, const Color& right) {
    glBegin(GL_QUADS);
    setColor(left);  glVertex2f(x1, y1); glVertex2f(x1, y2);
    setColor(right); glVertex2f(x2, y2); glVertex2f(x2, y1);
    glEnd();
}

void poly(const std::vector<Vec2>& p, const Color& c) {
    setColor(c);
    glBegin(GL_POLYGON);
    for (const Vec2& v : p) glVertex2f(v.x, v.y);
    glEnd();
}

// Convex polygon with a top-to-bottom colour gradient.
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

void line(float x1, float y1, float x2, float y2, const Color& c, float w = 2.0f) {
    setColor(c);
    glLineWidth(w);
    glBegin(GL_LINES);
    glVertex2f(x1, y1); glVertex2f(x2, y2);
    glEnd();
}

// A thick straight bar (handles, braces) drawn as a quad.
void beam(float x1, float y1, float x2, float y2, float halfW, const Color& c) {
    float dx = x2 - x1, dy = y2 - y1, len = std::sqrt(dx * dx + dy * dy) + 1e-6f;
    float nx = -dy / len * halfW, ny = dx / len * halfW;
    setColor(c);
    glBegin(GL_QUADS);
    glVertex2f(x1 + nx, y1 + ny); glVertex2f(x2 + nx, y2 + ny);
    glVertex2f(x2 - nx, y2 - ny); glVertex2f(x1 - nx, y1 - ny);
    glEnd();
}

// Soft ground shadow: a few stacked translucent ellipses.
void softShadow(float cx, float cy, float rx, float ry, float strength) {
    for (int i = 0; i < 4; i++) {
        float k = 1.0f - i * 0.2f;
        ellipse(cx, cy, rx * k, ry * k, Color(0, 0, 0, strength * 0.28f), 32);
    }
}

void grassTuft(float x, float y, float h, int seed) {
    const Color g1(0.20f, 0.46f, 0.14f), g2(0.30f, 0.60f, 0.20f);
    for (int i = 0; i < 5; i++) {
        float lean = (rnd(seed * 7 + i) - 0.5f) * 0.05f;
        float hh = h * (0.6f + 0.4f * rnd(seed * 13 + i));
        float bx = x + (i - 2) * 0.006f;
        poly({{bx - 0.004f, y}, {bx + 0.004f, y}, {bx + lean, y + hh}}, (i & 1) ? g1 : g2);
    }
}

// Faint wood-grain streaks inside a rectangle.
void woodGrain(float x1, float y1, float x2, float y2, int count, int seed, const Color& c) {
    for (int i = 0; i < count; i++) {
        float y = y1 + (y2 - y1) * rnd(seed + i * 3);
        float a = x1 + (x2 - x1) * rnd(seed + i * 5) * 0.6f;
        float b = a + (x2 - x1) * (0.15f + 0.35f * rnd(seed + i * 7));
        line(a, y, std::min(b, x2), y + (rnd(seed + i) - 0.5f) * 0.004f, c, 1.0f);
    }
}

/* ------------------------------------------------------------------ */
/*  Scene objects                                                      */
/* ------------------------------------------------------------------ */

void drawFence() {
    const float baseY = -0.03f;
    softShadow(0.0f, baseY - 0.01f, 0.98f, 0.035f, 0.5f);

    // Posts (behind the rails): lit from the left, with grain, bevelled caps and grass at the foot.
    for (int i = 0; i < 9; i++) {
        float x = -0.90f + i * 0.225f;
        float top = 0.34f + (rnd(i + 1) - 0.5f) * 0.02f;
        hGradRect(x, baseY, x + 0.055f, top, shade(kWood, 1.15f), shade(kWood, 0.62f));
        for (int g = 0; g < 2; g++) {
            float gx = x + 0.017f + g * 0.02f;
            line(gx, baseY + 0.02f, gx + (rnd(i * 3 + g) - 0.5f) * 0.006f, top - 0.03f - 0.1f * rnd(i + g),
                 Color(0.15f, 0.07f, 0.02f, 0.45f), 1.0f);
        }
        poly({{x - 0.008f, top}, {x + 0.063f, top}, {x + 0.0275f, top + 0.045f}}, shade(kWood, 1.3f));
        line(x - 0.008f, top, x + 0.0275f, top + 0.045f, Color(0.85f, 0.6f, 0.35f, 0.7f), 1.5f);
        grassTuft(x + 0.0275f, baseY - 0.005f, 0.035f + 0.02f * rnd(i + 20), i + 5);
    }

    // Rails (in front): gradient, highlight on top, shadow below, grain and nails.
    const float rails[2][2] = {{0.18f, 0.24f}, {0.02f, 0.08f}};
    for (int r = 0; r < 2; r++) {
        float y1 = rails[r][0], y2 = rails[r][1];
        rect(-0.95f, y1 - 0.014f, 0.95f, y1, Color(0, 0, 0, 0.18f)); // shadow cast on the posts
        vGradRect(-0.95f, y1, 0.95f, y2, shade(kWood, 1.25f), shade(kWood, 0.80f));
        line(-0.95f, y2, 0.95f, y2, Color(0.80f, 0.55f, 0.30f, 0.65f), 1.5f);
        line(-0.95f, y1, 0.95f, y1, Color(0.10f, 0.04f, 0.01f, 0.6f), 1.5f);
        woodGrain(-0.95f, y1 + 0.008f, 0.95f, y2 - 0.008f, 18, r * 100 + 40, Color(0.2f, 0.09f, 0.03f, 0.4f));
        for (int i = 0; i < 9; i++) {
            float nx = -0.90f + i * 0.225f + 0.0275f;
            ellipse(nx, (y1 + y2) * 0.5f, 0.0065f, 0.0065f, Color(0.18f, 0.18f, 0.2f), 10);
            ellipse(nx - 0.0015f, (y1 + y2) * 0.5f + 0.0015f, 0.002f, 0.002f, Color(0.7f, 0.7f, 0.75f), 6);
        }
    }
}

void drawTrough() {
    softShadow(-0.50f, -0.69f, 0.42f, 0.035f, 0.55f);

    // Splayed legs with an X brace.
    for (float lx : {-0.75f, -0.25f}) {
        poly({{lx - 0.035f, -0.42f}, {lx + 0.035f, -0.42f}, {lx + 0.048f, -0.68f}, {lx - 0.048f, -0.68f}}, shade(kWood, 0.55f));
    }
    beam(-0.75f, -0.64f, -0.25f, -0.46f, 0.008f, shade(kWood, 0.5f));
    beam(-0.75f, -0.46f, -0.25f, -0.64f, 0.008f, shade(kWood, 0.5f));

    // Dark inside wall at the back, then the feed heap on top of it.
    vGradRect(-0.80f, -0.25f, -0.20f, -0.20f, Color(0.20f, 0.11f, 0.04f), Color(0.10f, 0.05f, 0.02f));
    std::vector<Vec2> heap;
    heap.push_back({-0.80f, -0.25f});
    for (int i = 0; i <= 12; i++) {
        float x = -0.80f + i * 0.05f;
        float bump = std::sin(i / 12.0f * kPi);
        heap.push_back({x, -0.25f + 0.045f * bump + 0.01f * rnd(i + 50)});
    }
    heap.push_back({-0.20f, -0.25f});
    polyGrad(heap, Color(0.88f, 0.70f, 0.28f), Color(0.62f, 0.45f, 0.14f));
    for (int i = 0; i < 26; i++) { // loose grain
        float x = -0.78f + 0.56f * rnd(i * 2 + 60);
        float bump = std::sin((x + 0.80f) / 0.60f * kPi);
        float y = -0.245f + 0.035f * bump * rnd(i * 3 + 61);
        Color c = (i % 3 == 0) ? Color(0.95f, 0.80f, 0.40f) : (i % 3 == 1 ? Color(0.70f, 0.50f, 0.16f) : Color(0.82f, 0.64f, 0.24f));
        ellipse(x, y, 0.011f, 0.008f, c, 8);
    }

    // Front of the trough: wider at the top, darker at the bottom.
    std::vector<Vec2> body = {{-0.80f, -0.43f}, {-0.20f, -0.43f}, {-0.15f, -0.25f}, {-0.85f, -0.25f}};
    polyGrad(body, shade(kWood, 1.2f), shade(kWood, 0.65f));
    for (int p = 1; p <= 2; p++) { // plank seams
        float y = -0.43f + p * 0.06f;
        float inset = 0.05f * (y + 0.43f) / 0.18f;
        line(-0.80f - inset + 0.04f, y, -0.20f + inset - 0.04f, y, Color(0.12f, 0.05f, 0.01f, 0.85f), 2.0f);
        line(-0.80f - inset + 0.04f, y + 0.004f, -0.20f + inset - 0.04f, y + 0.004f, Color(0.8f, 0.55f, 0.3f, 0.25f), 1.0f);
    }
    woodGrain(-0.78f, -0.42f, -0.22f, -0.27f, 14, 120, Color(0.2f, 0.09f, 0.03f, 0.35f));

    // Metal straps at each end with rivets.
    for (float sx : {-0.78f, -0.22f}) {
        hGradRect(sx - 0.012f, -0.43f, sx + 0.012f, -0.25f, shade(kMetal, 1.1f), shade(kMetal, 0.55f));
        ellipse(sx, -0.30f, 0.004f, 0.004f, Color(0.25f, 0.25f, 0.28f), 8);
        ellipse(sx, -0.38f, 0.004f, 0.004f, Color(0.25f, 0.25f, 0.28f), 8);
    }

    // Rim.
    vGradRect(-0.86f, -0.262f, -0.14f, -0.238f, shade(kWood, 1.45f), shade(kWood, 1.0f));
    line(-0.86f, -0.238f, -0.14f, -0.238f, Color(0.95f, 0.75f, 0.5f, 0.6f), 1.5f);
}

void drawHayBale(float x, float y, int seed) {
    const float w = 0.25f, h = 0.17f, c = 0.014f;
    softShadow(x + w * 0.5f, y - 0.004f, w * 0.65f, 0.02f, 0.45f);

    // Slightly rounded body.
    std::vector<Vec2> body = {{x + c, y}, {x + w - c, y}, {x + w, y + c}, {x + w, y + h - c},
                              {x + w - c, y + h}, {x + c, y + h}, {x, y + h - c}, {x, y + c}};
    polyGrad(body, Color(0.92f, 0.72f, 0.24f), Color(0.62f, 0.43f, 0.11f));

    // Straw strands in three tones.
    for (int i = 0; i < 34; i++) {
        float sx = x + 0.012f + (w - 0.024f) * rnd(seed * 100 + i);
        float sy = y + 0.012f + (h - 0.03f) * rnd(seed * 100 + i + 40);
        float len = 0.03f + 0.04f * rnd(seed * 100 + i + 80);
        float sl = (rnd(seed * 100 + i + 120) - 0.5f) * 0.015f;
        Color col = (i % 3 == 0) ? kStrawLight : (i % 3 == 1 ? kStrawMid : kStrawDark);
        line(sx, sy, std::min(sx + len, x + w - 0.008f), sy + sl, alpha(col, 0.9f), 1.2f);
    }

    // Twine ties.
    for (float tx : {x + 0.07f, x + 0.18f}) {
        line(tx, y, tx, y + h, Color(0.30f, 0.19f, 0.05f), 2.4f);
        line(tx + 0.004f, y, tx + 0.004f, y + h, Color(0.75f, 0.58f, 0.25f, 0.8f), 1.0f);
    }
    // Edge light / shade.
    line(x + c, y + h, x + w - c, y + h, Color(1.0f, 0.88f, 0.5f, 0.8f), 2.0f);
    line(x + c, y, x + w - c, y, Color(0.25f, 0.15f, 0.03f, 0.8f), 2.0f);
    // Stray straws poking out of the sides.
    for (int i = 0; i < 4; i++) {
        float sy = y + 0.02f + (h - 0.04f) * rnd(seed * 31 + i);
        float dir = (i & 1) ? 1.0f : -1.0f;
        float ex = (dir > 0 ? x + w : x);
        line(ex, sy, ex + dir * 0.02f * (0.5f + rnd(seed + i)), sy + 0.006f * dir, alpha(kStrawMid, 0.9f), 1.2f);
    }
}

void drawHayStack() {
    drawHayBale(-0.10f, -0.65f, 1);
    drawHayBale(0.17f, -0.65f, 2);
    drawHayBale(0.035f, -0.48f, 3);  // the third one sits on top of the two below
    for (int i = 0; i < 6; i++) {   // loose straw on the ground
        float sx = -0.14f + 0.62f * rnd(i + 200);
        float sy = -0.675f - 0.015f * rnd(i + 210);
        line(sx, sy, sx + 0.03f, sy + 0.004f, alpha(kStrawMid, 0.9f), 1.2f);
    }
    grassTuft(-0.13f, -0.655f, 0.04f, 31);
    grassTuft(0.44f, -0.665f, 0.045f, 32);
}

float barrelHalfWidth(float t) { return 0.105f + 0.02f * std::sin(t * kPi); } // bulges in the middle

void drawWaterBarrel(float time) {
    const float cx = 0.70f, yb = -0.65f, yt = -0.30f;
    const int kStaves = 8, kSeg = 10;
    softShadow(cx + 0.02f, yb - 0.01f, 0.17f, 0.03f, 0.55f);

    // Curved wooden staves, shaded like a cylinder lit from the left.
    for (int i = 0; i < kStaves; i++) {
        float u0 = i / (float)kStaves, u1 = (i + 1) / (float)kStaves, um = 0.5f * (u0 + u1);
        float f = std::max(0.45f, 0.62f + 0.5f * std::cos((um - 0.32f) * kPi * 0.9f) + (rnd(i + 300) - 0.5f) * 0.1f);
        setColor(shade(kWood, f));
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= kSeg; j++) {
            float t = j / (float)kSeg, y = yb + (yt - yb) * t, hw = barrelHalfWidth(t);
            glVertex2f(cx + hw * (2 * u0 - 1), y);
            glVertex2f(cx + hw * (2 * u1 - 1), y);
        }
        glEnd();
        // seam between staves
        setColor(Color(0.08f, 0.03f, 0.01f, 0.55f));
        glLineWidth(1.2f);
        glBegin(GL_LINE_STRIP);
        for (int j = 0; j <= kSeg; j++) {
            float t = j / (float)kSeg;
            glVertex2f(cx + barrelHalfWidth(t) * (2 * u0 - 1), yb + (yt - yb) * t);
        }
        glEnd();
    }
    // Soft highlight stripe.
    line(cx - 0.065f, yb + 0.03f, cx - 0.072f, yt - 0.03f, Color(1, 1, 1, 0.14f), 5.0f);

    // Metal hoops that follow the bulge.
    for (float t : {0.12f, 0.30f, 0.70f, 0.88f}) {
        float y = yb + (yt - yb) * t, hw = barrelHalfWidth(t) + 0.004f;
        hGradRect(cx - hw, y - 0.011f, cx + hw, y + 0.011f, shade(kMetal, 1.2f), shade(kMetal, 0.5f));
        line(cx - hw, y + 0.011f, cx + hw, y + 0.011f, Color(1, 1, 1, 0.35f), 1.0f);
        ellipse(cx - hw + 0.012f, y, 0.003f, 0.003f, Color(0.2f, 0.2f, 0.22f), 6);
        ellipse(cx + hw - 0.012f, y, 0.003f, 0.003f, Color(0.2f, 0.2f, 0.22f), 6);
    }

    // Open top: rim, dark inside, and animated water.
    const float topY = yt;
    ellipse(cx, topY, 0.112f, 0.034f, shade(kWood, 0.85f));
    ellipse(cx, topY, 0.098f, 0.027f, Color(0.12f, 0.07f, 0.03f));
    ellipse(cx, topY - 0.003f, 0.090f, 0.022f, Color(0.16f, 0.44f, 0.70f));
    ellipse(cx, topY - 0.001f, 0.075f, 0.015f, Color(0.30f, 0.62f, 0.86f));
    float o = std::sin(time * 1.7f) * 0.03f;
    ellipse(cx + o, topY - 0.002f, 0.022f, 0.0045f, Color(1, 1, 1, 0.45f), 16);
    ellipse(cx - o * 0.7f + 0.02f, topY + 0.004f, 0.012f, 0.003f, Color(1, 1, 1, 0.35f), 12);
    float rp = 0.03f + 0.012f * std::fmod(time * 0.6f, 1.0f); // slow ripple ring
    glLineWidth(1.0f);
    setColor(Color(1, 1, 1, 0.25f * (1.0f - std::fmod(time * 0.6f, 1.0f))));
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 24; i++) {
        float a = 2 * kPi * i / 24;
        glVertex2f(cx - 0.025f + std::cos(a) * rp, topY - 0.002f + std::sin(a) * rp * 0.25f);
    }
    glEnd();
    // Light rim line
    setColor(Color(0.9f, 0.7f, 0.45f, 0.6f));
    glLineWidth(1.5f);
    glBegin(GL_LINE_STRIP);
    for (int i = 10; i <= 30; i++) {
        float a = 2 * kPi * i / 40;
        glVertex2f(cx + std::cos(a) * 0.112f, topY + std::sin(a) * 0.034f);
    }
    glEnd();
}

void drawRake() {
    // Wooden handle, then the metal head with tines.
    beam(0.42f, -0.68f, 0.50f, -0.28f, 0.007f, shade(kWood, 1.05f));
    line(0.4175f, -0.68f, 0.4975f, -0.28f, Color(1, 0.8f, 0.5f, 0.25f), 1.0f);
    beam(0.40f, -0.345f, 0.56f, -0.335f, 0.009f, Color(0.42f, 0.42f, 0.45f));
    line(0.40f, -0.335f, 0.56f, -0.325f, Color(0.85f, 0.85f, 0.9f, 0.7f), 1.5f);
    beam(0.478f, -0.352f, 0.500f, -0.322f, 0.008f, Color(0.30f, 0.30f, 0.33f)); // socket
    for (int i = 0; i < 6; i++) {
        float x = 0.405f + i * 0.030f;
        poly({{x - 0.0055f, -0.35f}, {x + 0.0055f, -0.35f}, {x + 0.001f, -0.425f}}, Color(0.38f, 0.38f, 0.42f));
    }
}

void drawPitchfork() {
    const float hx1 = 0.885f, hy1 = -0.68f, hx2 = 0.925f, hy2 = -0.20f;
    softShadow(0.89f, -0.685f, 0.05f, 0.01f, 0.5f);
    beam(hx1, hy1, hx2, hy2, 0.0065f, shade(kWood, 1.05f));
    line(hx1 - 0.0035f, hy1, hx2 - 0.0035f, hy2, Color(1, 0.8f, 0.5f, 0.25f), 1.0f);
    beam(hx2 - 0.004f, hy2 - 0.04f, hx2 + 0.004f, hy2, 0.011f, Color(0.35f, 0.35f, 0.38f)); // socket
    beam(hx2 - 0.032f, hy2 + 0.004f, hx2 + 0.034f, hy2 + 0.004f, 0.0055f, Color(0.45f, 0.45f, 0.48f)); // crossbar
    for (int i = 0; i < 3; i++) {
        float px = hx2 - 0.028f + i * 0.028f;
        beam(px, hy2 + 0.004f, px, hy2 + 0.105f, 0.0035f, Color(0.5f, 0.5f, 0.54f));
        poly({{px - 0.0045f, hy2 + 0.105f}, {px + 0.0045f, hy2 + 0.105f}, {px, hy2 + 0.135f}}, Color(0.7f, 0.7f, 0.75f));
    }
}

} // namespace

void drawFarmObjects() {
    float time = glutGet(GLUT_ELAPSED_TIME) * 0.001f;

    // Save only what we touch so the caller's state is untouched.
    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_LINE_BIT | GL_CURRENT_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    drawFence();       // back
    drawTrough();
    drawHayStack();
    drawRake();
    drawWaterBarrel(time);
    drawPitchfork();   // front

    glPopAttrib();
}