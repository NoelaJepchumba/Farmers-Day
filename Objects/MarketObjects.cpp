#include <GL/freeglut.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include "MarketObjects.h"

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
const Color kWood(0.52f, 0.28f, 0.12f);
const Color kRed(0.75f, 0.16f, 0.16f);
const Color kCream(0.92f, 0.89f, 0.76f);
const Color kBrass(0.85f, 0.65f, 0.20f);

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
// Polygon whose colour is computed per vertex by `col(x, y)`. Filled as a fan from the
// vertex average, so the outline must be star-shaped (all outlines below are).
template <class F>
void polyFn(const std::vector<Vec2>& p, F col) {
    float cx = 0, cy = 0;
    for (const Vec2& v : p) { cx += v.x; cy += v.y; }
    cx /= p.size(); cy /= p.size();
    glBegin(GL_TRIANGLE_FAN);
    setColor(col(cx, cy)); glVertex2f(cx, cy);
    for (size_t i = 0; i <= p.size(); i++) {
        const Vec2& v = p[i % p.size()];
        setColor(col(v.x, v.y)); glVertex2f(v.x, v.y);
    }
    glEnd();
}
void polyGrad(const std::vector<Vec2>& p, const Color& top, const Color& bottom) {
    float lo = 1e9f, hi = -1e9f;
    for (const Vec2& v : p) { lo = std::min(lo, v.y); hi = std::max(hi, v.y); }
    polyFn(p, [&](float, float y) { return mix(bottom, top, (y - lo) / (hi - lo + 1e-6f)); });
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
// Disc with a lighter centre (round, embossed look for coins and knobs).
void radialDisc(float cx, float cy, float r, const Color& centre, const Color& edge) {
    glBegin(GL_TRIANGLE_FAN);
    setColor(centre); glVertex2f(cx - r * 0.25f, cy + r * 0.25f);
    setColor(edge);
    for (int i = 0; i <= 40; i++) {
        float a = 2.0f * kPi * i / 40;
        glVertex2f(cx + std::cos(a) * r, cy + std::sin(a) * r);
    }
    glEnd();
}
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
void softShadow(float cx, float cy, float rx, float ry, float strength) {
    for (int i = 0; i < 4; i++) {
        float k = 1.0f - i * 0.2f;
        ellipse(cx, cy, rx * k, ry * k, Color(0, 0, 0, strength * 0.28f), 32);
    }
}
// Text centred on (cx, cy). Bitmap fonts are a fixed pixel size, so keep strings short.
void text(float cx, float cy, const char* s, void* font, const Color& c) {
    const unsigned char* u = reinterpret_cast<const unsigned char*>(s);
    int px = glutBitmapLength(font, u);
    int w = std::max(1, glutGet(GLUT_WINDOW_WIDTH));
    setColor(c);
    glRasterPos2f(cx - (float)px / w, cy); // NDC spans 2 units over `w` pixels
    glutBitmapString(font, u);
}

/* ------------------------------------------------------------------ */
/*  Scene objects                                                      */
/* ------------------------------------------------------------------ */

// Original poles stopped at y = 0.25 and floated in mid-air. They now run down behind the
// counter to the floor; set this back to 0.25f if you want the old look.
constexpr float kPoleBottom = -0.66f;

void drawCanopy(float time) {
    // Support poles with grain, a highlight and iron bands.
    for (float px : {-0.78f, 0.73f}) {
        hGradRect(px, kPoleBottom, px + 0.05f, 0.76f, shade(kWood, 1.2f), shade(kWood, 0.55f));
        line(px + 0.012f, kPoleBottom, px + 0.012f, 0.76f, Color(1, 0.85f, 0.55f, 0.28f), 1.5f);
        for (int i = 0; i < 6; i++) {
            float gx = px + 0.01f + 0.03f * rnd(i + 400 + (int)(px * 10));
            float y0 = kPoleBottom + (0.76f - kPoleBottom) * rnd(i + 410);
            line(gx, y0, gx, y0 + 0.12f, Color(0.15f, 0.07f, 0.02f, 0.4f), 1.0f);
        }
        for (float by : {0.30f, -0.05f}) {
            hGradRect(px - 0.005f, by, px + 0.055f, by + 0.022f, Color(0.45f, 0.45f, 0.48f), Color(0.18f, 0.18f, 0.2f));
            line(px - 0.005f, by + 0.022f, px + 0.055f, by + 0.022f, Color(1, 1, 1, 0.3f), 1.0f);
        }
    }

    // Shade thrown on the stall by the awning.
    vGradRect(-0.80f, 0.34f, 0.80f, 0.58f, Color(0, 0, 0, 0.30f), Color(0, 0, 0, 0));

    // Sloped roof: ten fabric stripes, lit from the left, each stripe darker on its right side.
    const int N = 10;
    glBegin(GL_QUADS);
    for (int i = 0; i < N; i++) {
        Color c = (i & 1) ? kCream : kRed;
        float t0 = i / (float)N, t1 = (i + 1) / (float)N;
        float tl = lerp(-0.80f, 0.80f, t0), tr = lerp(-0.80f, 0.80f, t1);   // top edge
        float bl = lerp(-0.85f, 0.85f, t0), br = lerp(-0.85f, 0.85f, t1);   // bottom edge (wider)
        setColor(shade(c, 1.10f)); glVertex2f(tl, 0.78f);
        setColor(shade(c, 0.82f)); glVertex2f(tr, 0.78f);
        setColor(shade(c, 0.72f)); glVertex2f(br, 0.68f);
        setColor(shade(c, 0.95f)); glVertex2f(bl, 0.68f);
    }
    glEnd();
    line(-0.80f, 0.78f, 0.80f, 0.78f, Color(0.35f, 0.07f, 0.07f, 0.7f), 2.0f);

    // Front valance with a scalloped, gently swaying hem.
    for (int i = 0; i < N; i++) {
        Color c = (i & 1) ? kCream : kRed;
        float x0 = -0.85f + i * 0.17f, x1 = x0 + 0.17f, xm = 0.5f * (x0 + x1);
        glBegin(GL_QUADS);
        setColor(shade(c, 1.15f)); glVertex2f(x0, 0.68f);
        setColor(shade(c, 0.95f)); glVertex2f(x1, 0.68f);
        setColor(shade(c, 0.82f)); glVertex2f(x1, 0.60f);
        setColor(shade(c, 0.95f)); glVertex2f(x0, 0.60f);
        glEnd();
        float ry = 0.045f + 0.006f * std::sin(time * 1.6f + i * 0.9f);
        glBegin(GL_TRIANGLE_FAN);
        setColor(shade(c, 0.95f)); glVertex2f(xm, 0.60f);
        for (int k = 0; k <= 16; k++) {
            float a = kPi + kPi * k / 16.0f;  // lower half-ellipse
            setColor(shade(c, 0.78f + 0.12f * (float)k / 16.0f));
            glVertex2f(xm + std::cos(a) * 0.085f, 0.60f + std::sin(a) * ry);
        }
        glEnd();
        arc(xm, 0.60f, 0.085f, ry, 180, 360, Color(0.25f, 0.05f, 0.05f, 0.45f), 1.5f);
    }
    line(-0.85f, 0.68f, 0.85f, 0.68f, Color(0.30f, 0.06f, 0.06f, 0.7f), 2.0f);
    line(-0.85f, 0.685f, 0.85f, 0.685f, Color(1, 1, 1, 0.25f), 1.0f);
}

void drawKnob(float cx, float cy) {
    ellipse(cx + 0.003f, cy - 0.004f, 0.016f, 0.016f, Color(0, 0, 0, 0.3f), 14);
    radialDisc(cx, cy, 0.015f, Color(1.0f, 0.88f, 0.5f), Color(0.6f, 0.42f, 0.1f));
}

// A framed cabinet panel: lit frame, recessed middle, vertical grain.
void drawPanel(float x1, float y1, float x2, float y2) {
    vGradRect(x1, y1, x2, y2, shade(kWood, 0.95f), shade(kWood, 0.62f));
    const float m = 0.022f;
    vGradRect(x1 + m, y1 + m, x2 - m, y2 - m, shade(kWood, 0.62f), shade(kWood, 0.46f));
    line(x1 + m, y2 - m, x2 - m, y2 - m, Color(0, 0, 0, 0.55f), 2.0f);   // inner top shadow
    line(x1 + m, y1 + m, x1 + m, y2 - m, Color(0, 0, 0, 0.45f), 2.0f);   // inner left shadow
    line(x1 + m, y1 + m, x2 - m, y1 + m, Color(1, 0.8f, 0.5f, 0.3f), 1.5f); // inner bottom light
    line(x2 - m, y1 + m, x2 - m, y2 - m, Color(1, 0.8f, 0.5f, 0.2f), 1.5f);
    line(x1, y2, x2, y2, Color(1, 0.85f, 0.55f, 0.35f), 1.5f);            // outer top light
    for (int i = 0; i < 7; i++) {
        float gx = x1 + m + (x2 - x1 - 2 * m) * (0.05f + 0.9f * rnd(i + (int)(x1 * 100) + 500));
        line(gx, y1 + m + 0.01f, gx + (rnd(i) - 0.5f) * 0.01f, y2 - m - 0.01f, Color(0.12f, 0.05f, 0.01f, 0.28f), 1.0f);
    }
}

void drawCounter() {
    softShadow(0.0f, -0.70f, 0.93f, 0.035f, 0.6f);

    // Tapered legs with feet.
    for (float lx : {-0.75f, 0.65f}) {
        hGradRect(lx, -0.68f, lx + 0.09f, -0.30f, shade(kWood, 0.80f), shade(kWood, 0.38f));
        line(lx + 0.012f, -0.68f, lx + 0.012f, -0.30f, Color(1, 0.8f, 0.5f, 0.18f), 1.5f);
        rect(lx - 0.008f, -0.69f, lx + 0.098f, -0.665f, shade(kWood, 0.3f));
    }

    // Front cabinet panels, a chalkboard price sign, and knobs.
    drawPanel(-0.72f, -0.60f, -0.05f, -0.39f);
    drawPanel(0.05f, -0.60f, 0.72f, -0.39f);
    {
        const float sx1 = -0.64f, sx2 = -0.20f, sy1 = -0.575f, sy2 = -0.43f;
        rect(sx1 - 0.014f, sy1 - 0.014f, sx2 + 0.014f, sy2 + 0.014f, Color(0.30f, 0.15f, 0.06f));
        vGradRect(sx1, sy1, sx2, sy2, Color(0.15f, 0.21f, 0.18f), Color(0.10f, 0.15f, 0.12f));
        for (int i = 0; i < 5; i++) // chalk dust smudges
            ellipse(sx1 + (sx2 - sx1) * rnd(i + 600), sy1 + (sy2 - sy1) * rnd(i + 610), 0.05f, 0.012f, Color(1, 1, 1, 0.04f), 10);
        text(0.5f * (sx1 + sx2), -0.482f, "FRESH MILK", GLUT_BITMAP_HELVETICA_12, Color(0.95f, 0.95f, 0.9f, 0.95f));
        text(0.5f * (sx1 + sx2), -0.540f, "2.50 a bottle", GLUT_BITMAP_HELVETICA_10, Color(0.95f, 0.85f, 0.5f, 0.9f));
        line(sx1 + 0.04f, -0.505f, sx2 - 0.04f, -0.505f, Color(0.95f, 0.95f, 0.9f, 0.5f), 1.0f);
    }
    drawKnob(-0.10f, -0.50f);
    drawKnob(0.10f, -0.50f);

    // Apron: two boards with seams and joints.
    vGradRect(-0.82f, -0.38f, 0.82f, -0.22f, shade(kWood, 1.05f), shade(kWood, 0.72f));
    line(-0.82f, -0.30f, 0.82f, -0.30f, Color(0.10f, 0.04f, 0.01f, 0.75f), 2.0f);
    line(-0.82f, -0.296f, 0.82f, -0.296f, Color(1, 0.8f, 0.5f, 0.22f), 1.0f);
    for (int i = 0; i < 40; i++) { // grain
        float y = (i & 1) ? -0.35f + 0.04f * rnd(i + 700) : -0.27f + 0.04f * rnd(i + 710);
        float x = -0.80f + 1.5f * rnd(i + 720);
        line(x, y, x + 0.05f + 0.15f * rnd(i + 730), y + (rnd(i + 740) - 0.5f) * 0.006f, Color(0.18f, 0.08f, 0.02f, 0.35f), 1.0f);
    }
    line(-0.15f, -0.38f, -0.15f, -0.30f, Color(0.1f, 0.04f, 0.01f, 0.6f), 1.5f); // butt joints
    line(0.30f, -0.30f, 0.30f, -0.22f, Color(0.1f, 0.04f, 0.01f, 0.6f), 1.5f);
    vGradRect(-0.82f, -0.42f, 0.82f, -0.38f, Color(0, 0, 0, 0.30f), Color(0, 0, 0, 0)); // shadow on panels
    line(-0.82f, -0.38f, 0.82f, -0.38f, Color(0.08f, 0.03f, 0.01f, 0.7f), 2.0f);

    // Top surface: lighter boards that converge slightly toward the back.
    vGradRect(-0.85f, -0.22f, 0.85f, -0.17f, shade(kWood, 1.55f), shade(kWood, 1.28f));
    for (int i = -5; i <= 5; i++) {
        float xb = i * 0.16f, xt = xb * 0.94f;
        line(xb, -0.22f, xt, -0.17f, Color(0.25f, 0.11f, 0.03f, 0.5f), 1.2f);
    }
    ellipse(-0.50f, -0.195f, 0.025f, 0.008f, Color(0.2f, 0.09f, 0.02f, 0.18f), 16); // ring stain
    ellipse(0.26f, -0.20f, 0.012f, 0.005f, Color(0.2f, 0.09f, 0.02f, 0.35f), 12);   // knot
    line(-0.85f, -0.22f, 0.85f, -0.22f, Color(1, 0.88f, 0.6f, 0.6f), 2.0f);          // front edge light
    line(-0.85f, -0.17f, 0.85f, -0.17f, Color(0.2f, 0.09f, 0.03f, 0.6f), 1.5f);
}

// variant 0 = milk, 1 = chocolate, 2 = strawberry. All sizes scale with `s`.
void drawMilkBottle(float x, float y, float s, int variant) {
    auto P = [&](float lx, float ly) { return Vec2{x + lx * s, y + ly * s}; };
    const Color milk[3] = {Color(0.97f, 0.97f, 1.0f), Color(0.46f, 0.28f, 0.15f), Color(0.98f, 0.74f, 0.82f)};
    const Color cap[3] = {Color(0.18f, 0.45f, 0.72f), Color(0.62f, 0.17f, 0.13f), Color(0.85f, 0.33f, 0.55f)};
    const Color lab[3] = {Color(0.35f, 0.70f, 0.42f), Color(0.55f, 0.35f, 0.20f), Color(0.90f, 0.45f, 0.60f)};
    softShadow(x + 0.065f * s, y - 0.003f * s, 0.085f * s, 0.011f * s, 0.55f);

    // Glass outline, then the milk filling it (star-shaped outlines).
    std::vector<Vec2> glass = {P(0.014f, 0), P(0.116f, 0), P(0.130f, 0.014f), P(0.130f, 0.150f), P(0.126f, 0.190f),
                               P(0.118f, 0.210f), P(0.100f, 0.228f), P(0.092f, 0.240f), P(0.091f, 0.280f),
                               P(0.039f, 0.280f), P(0.038f, 0.240f), P(0.030f, 0.228f), P(0.012f, 0.210f),
                               P(0.004f, 0.190f), P(0.0f, 0.150f), P(0.0f, 0.014f)};
    std::vector<Vec2> fill = {P(0.012f, 0.008f), P(0.118f, 0.008f), P(0.123f, 0.016f), P(0.123f, 0.150f), P(0.120f, 0.188f),
                              P(0.112f, 0.206f), P(0.097f, 0.222f), P(0.088f, 0.234f), P(0.086f, 0.264f),
                              P(0.044f, 0.264f), P(0.042f, 0.234f), P(0.033f, 0.222f), P(0.018f, 0.206f),
                              P(0.010f, 0.188f), P(0.007f, 0.150f), P(0.007f, 0.016f)};
    polyFn(glass, [&](float, float) { return Color(0.78f, 0.88f, 0.95f, 0.55f); });
    polyFn(fill, [&](float vx, float vy) {
        float u = clamp01((vx - x) / (0.13f * s));
        float light = 0.78f + 0.30f * std::cos((u - 0.3f) * kPi * 0.95f);
        Color c = milk[variant];
        if (variant == 0 && (vy - y) / s > 0.195f) c = mix(c, Color(1.0f, 0.93f, 0.70f), 0.75f); // cream line
        return shade(c, light);
    });
    setColor(Color(0.6f, 0.75f, 0.88f, 0.7f));
    glLineWidth(1.5f);
    glBegin(GL_LINE_LOOP);
    for (const Vec2& v : glass) glVertex2f(v.x, v.y);
    glEnd();

    // Label: chamfered rectangle with cow spots and a drop.
    std::vector<Vec2> lb = {P(0.016f, 0.08f), P(0.114f, 0.08f), P(0.120f, 0.086f), P(0.120f, 0.144f), P(0.114f, 0.150f),
                            P(0.016f, 0.150f), P(0.010f, 0.144f), P(0.010f, 0.086f)};
    polyFn(lb, [&](float vx, float) {
        float u = clamp01((vx - x) / (0.13f * s));
        return shade(lab[variant], 0.75f + 0.30f * std::cos((u - 0.3f) * kPi * 0.95f));
    });
    ellipse(x + 0.035f * s, y + 0.118f * s, 0.016f * s, 0.011f * s, Color(1, 1, 1, 0.85f), 12);
    ellipse(x + 0.095f * s, y + 0.100f * s, 0.013f * s, 0.009f * s, Color(1, 1, 1, 0.85f), 12);
    ellipse(x + 0.07f * s, y + 0.125f * s, 0.009f * s, 0.012f * s, Color(1, 1, 1, 0.9f), 10);
    ellipse(x + 0.066f * s, y + 0.124f * s, 0.003f * s, 0.005f * s, Color(0.3f, 0.5f, 0.8f, 0.7f), 8);

    // Foil cap with crimped edge, top highlight and a tear line.
    std::vector<Vec2> cp = {P(0.034f, 0.278f), P(0.096f, 0.278f), P(0.094f, 0.306f), P(0.088f, 0.312f), P(0.042f, 0.312f), P(0.036f, 0.306f)};
    polyFn(cp, [&](float vx, float) {
        float u = clamp01((vx - (x + 0.034f * s)) / (0.062f * s));
        return shade(cap[variant], 0.80f + 0.35f * std::cos((u - 0.3f) * kPi * 0.95f));
    });
    for (int i = 0; i < 6; i++) {
        float cx = x + (0.042f + i * 0.0105f) * s;
        line(cx, y + 0.279f * s, cx, y + 0.289f * s, Color(0, 0, 0, 0.35f), 1.0f);
    }
    line(x + 0.040f * s, y + 0.309f * s, x + 0.090f * s, y + 0.309f * s, Color(1, 1, 1, 0.55f), 1.5f);

    // Glass highlights.
    glBegin(GL_QUADS);
    setColor(Color(1, 1, 1, 0.55f)); glVertex2f(x + 0.014f * s, y + 0.02f * s); glVertex2f(x + 0.014f * s, y + 0.175f * s);
    setColor(Color(1, 1, 1, 0.05f)); glVertex2f(x + 0.030f * s, y + 0.17f * s); glVertex2f(x + 0.030f * s, y + 0.02f * s);
    glEnd();
    line(x + 0.044f * s, y + 0.235f * s, x + 0.046f * s, y + 0.262f * s, Color(1, 1, 1, 0.6f), 1.5f);
    line(x + 0.118f * s, y + 0.03f * s, x + 0.118f * s, y + 0.14f * s, Color(1, 1, 1, 0.18f), 1.5f);
}

void drawMoney() {
    // Shadow, then the note: gradient green paper, double border, guilloche waves, medallion.
    rect(-0.294f, -0.088f, -0.044f, 0.072f, Color(0, 0, 0, 0.22f));
    hGradRect(-0.30f, -0.08f, -0.05f, 0.08f, Color(0.26f, 0.64f, 0.34f), Color(0.15f, 0.50f, 0.24f));
    setColor(Color(0.85f, 0.92f, 0.65f));
    glLineWidth(2);
    glBegin(GL_LINE_LOOP);
    glVertex2f(-0.28f, -0.06f); glVertex2f(-0.07f, -0.06f); glVertex2f(-0.07f, 0.06f); glVertex2f(-0.28f, 0.06f);
    glEnd();
    setColor(Color(0.85f, 0.92f, 0.65f, 0.55f));
    glLineWidth(1);
    glBegin(GL_LINE_LOOP);
    glVertex2f(-0.29f, -0.07f); glVertex2f(-0.06f, -0.07f); glVertex2f(-0.06f, 0.07f); glVertex2f(-0.29f, 0.07f);
    glEnd();
    for (int w = 0; w < 3; w++) { // wavy security lines
        setColor(Color(0.85f, 0.95f, 0.7f, 0.35f));
        glLineWidth(1);
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i <= 30; i++) {
            float x = -0.27f + 0.19f * i / 30.0f;
            glVertex2f(x, (w == 1 ? 0.0f : (w == 0 ? 0.042f : -0.042f)) + 0.008f * std::sin(i * 0.9f + w));
        }
        glEnd();
    }
    ellipse(-0.175f, 0.0f, 0.055f, 0.045f, Color(0.88f, 0.93f, 0.72f, 0.5f), 28);       // portrait oval
    radialDisc(-0.175f, 0.0f, 0.035f, Color(1.0f, 0.92f, 0.45f), Color(0.78f, 0.62f, 0.15f)); // medallion
    arc(-0.175f, 0.0f, 0.03f, 0.03f, 0, 360, Color(0.6f, 0.45f, 0.1f, 0.7f), 1.0f);
    text(-0.175f, -0.008f, "$", GLUT_BITMAP_HELVETICA_12, Color(0.45f, 0.30f, 0.05f));
    text(-0.255f, 0.043f, "10", GLUT_BITMAP_HELVETICA_10, Color(0.85f, 0.95f, 0.7f, 0.9f));
    text(-0.095f, -0.052f, "10", GLUT_BITMAP_HELVETICA_10, Color(0.85f, 0.95f, 0.7f, 0.9f));
    // A curled-up corner.
    poly({{-0.05f, -0.08f}, {-0.05f, -0.035f}, {-0.092f, -0.08f}}, Color(0.78f, 0.88f, 0.62f));
    line(-0.05f, -0.035f, -0.092f, -0.08f, Color(0, 0, 0, 0.35f), 1.5f);

    // Coins: each casts a small shadow, has a milled rim, an inner ring and an embossed star.
    struct Coin { float x, y, bright; };
    const Coin coins[3] = {{0.02f, -0.08f, 0.0f}, {0.12f, -0.08f, 0.0f}, {0.07f, -0.01f, 1.0f}};
    for (const Coin& c : coins) {
        const float r = 0.055f;
        ellipse(c.x + 0.007f, c.y - 0.009f, r, r, Color(0, 0, 0, 0.3f), 28);
        radialDisc(c.x, c.y, r, c.bright > 0 ? Color(1.0f, 0.90f, 0.45f) : Color(0.98f, 0.80f, 0.32f),
                   c.bright > 0 ? Color(0.82f, 0.58f, 0.10f) : Color(0.72f, 0.48f, 0.06f));
        arc(c.x, c.y, r, r, 0, 360, Color(0.45f, 0.30f, 0.04f, 0.8f), 1.5f);
        for (int i = 0; i < 36; i++) { // milled edge
            float a = 2 * kPi * i / 36.0f;
            line(c.x + std::cos(a) * r * 0.9f, c.y + std::sin(a) * r * 0.9f,
                 c.x + std::cos(a) * r * 0.99f, c.y + std::sin(a) * r * 0.99f, Color(0.45f, 0.30f, 0.04f, 0.55f), 1.0f);
        }
        arc(c.x, c.y, r * 0.72f, r * 0.72f, 0, 360, Color(0.55f, 0.38f, 0.06f, 0.6f), 1.0f);
        std::vector<Vec2> star;
        for (int i = 0; i < 10; i++) {
            float a = kPi / 2 + i * kPi / 5, rr = (i & 1) ? r * 0.17f : r * 0.40f;
            star.push_back({c.x + std::cos(a) * rr, c.y + std::sin(a) * rr});
        }
        poly(star, Color(0.62f, 0.42f, 0.06f, 0.75f));
        arc(c.x, c.y, r * 0.84f, r * 0.84f, 110, 170, Color(1, 1, 0.9f, 0.8f), 2.0f); // shine
    }
}

void drawScale() {
    softShadow(0.56f, -0.155f, 0.19f, 0.016f, 0.5f);

    // Rubber feet and the brushed-metal base with buttons and an LED.
    rect(0.41f, -0.158f, 0.45f, -0.15f, Color(0.1f, 0.1f, 0.11f));
    rect(0.67f, -0.158f, 0.71f, -0.15f, Color(0.1f, 0.1f, 0.11f));
    polyGrad({{0.40f, -0.15f}, {0.72f, -0.15f}, {0.72f, -0.115f}, {0.705f, -0.10f}, {0.415f, -0.10f}, {0.40f, -0.115f}},
             Color(0.52f, 0.54f, 0.57f), Color(0.30f, 0.32f, 0.34f));
    for (int i = 0; i < 6; i++) // brushed texture
        line(0.41f, -0.148f + 0.008f * i, 0.71f, -0.148f + 0.008f * i, Color(1, 1, 1, 0.06f), 1.0f);
    radialDisc(0.44f, -0.126f, 0.011f, Color(0.78f, 0.80f, 0.82f), Color(0.35f, 0.37f, 0.40f));
    radialDisc(0.48f, -0.126f, 0.011f, Color(0.78f, 0.80f, 0.82f), Color(0.35f, 0.37f, 0.40f));
    ellipse(0.69f, -0.126f, 0.005f, 0.005f, Color(0.2f, 0.9f, 0.3f), 10); // power LED
    ellipse(0.69f, -0.126f, 0.012f, 0.012f, Color(0.2f, 0.9f, 0.3f, 0.2f), 12);

    // Stainless platform with a raised lip.
    polyGrad({{0.42f, -0.10f}, {0.70f, -0.10f}, {0.705f, -0.06f}, {0.415f, -0.06f}},
             Color(0.86f, 0.88f, 0.90f), Color(0.58f, 0.60f, 0.63f));
    line(0.415f, -0.06f, 0.705f, -0.06f, Color(1, 1, 1, 0.8f), 2.0f);
    line(0.42f, -0.10f, 0.70f, -0.10f, Color(0, 0, 0, 0.4f), 1.5f);
    line(0.46f, -0.075f, 0.56f, -0.075f, Color(1, 1, 1, 0.35f), 2.0f);

    // Column shaded like a cylinder.
    hGradRect(0.54f, -0.06f, 0.59f, 0.08f, Color(0.62f, 0.64f, 0.67f), Color(0.27f, 0.29f, 0.32f));
    line(0.548f, -0.06f, 0.548f, 0.08f, Color(1, 1, 1, 0.4f), 1.5f);

    // Display housing with a dark bezel, an LCD with ghost segments, and glass glare.
    std::vector<Vec2> housing = {{0.48f, 0.08f}, {0.66f, 0.08f}, {0.67f, 0.09f}, {0.67f, 0.18f}, {0.66f, 0.19f}, {0.48f, 0.19f},
                                 {0.47f, 0.18f}, {0.47f, 0.09f}};
    polyGrad(housing, Color(0.28f, 0.31f, 0.30f), Color(0.10f, 0.13f, 0.11f));
    line(0.48f, 0.19f, 0.66f, 0.19f, Color(1, 1, 1, 0.3f), 1.5f);
    vGradRect(0.49f, 0.10f, 0.65f, 0.17f, Color(0.66f, 0.92f, 0.62f), Color(0.46f, 0.76f, 0.46f));
    line(0.49f, 0.17f, 0.65f, 0.17f, Color(0, 0, 0, 0.45f), 2.0f);   // inner shadow
    line(0.49f, 0.17f, 0.49f, 0.10f, Color(0, 0, 0, 0.35f), 2.0f);
    text(0.565f, 0.124f, "8.88 kg", GLUT_BITMAP_HELVETICA_12, Color(0.2f, 0.4f, 0.2f, 0.18f)); // unlit segments
    text(0.565f, 0.124f, "1.25 kg", GLUT_BITMAP_HELVETICA_12, Color(0.07f, 0.18f, 0.08f));
    poly({{0.49f, 0.17f}, {0.56f, 0.17f}, {0.52f, 0.10f}, {0.49f, 0.10f}}, Color(1, 1, 1, 0.12f));
    ellipse(0.655f, 0.183f, 0.0035f, 0.0035f, Color(0.9f, 0.2f, 0.15f), 8);
}

} // namespace

void drawMarketObjects() {
    float time = glutGet(GLUT_ELAPSED_TIME) * 0.001f;

    // Save only what we touch so the caller's state is untouched.
    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_LINE_BIT | GL_CURRENT_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    drawCanopy(time);
    drawCounter();

    // Milk bottles displayed on the counter (milk, chocolate, strawberry).
    drawMilkBottle(-0.62f, -0.17f, 1.0f, 0);
    drawMilkBottle(-0.40f, -0.17f, 1.0f, 1);
    drawMilkBottle(-0.18f, -0.17f, 1.0f, 2);

    drawMoney();
    drawScale();

    glPopAttrib();
}