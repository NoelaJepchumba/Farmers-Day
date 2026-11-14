#include "shopkeeper.h"
#include "primitives.h"

#include <GL/freeglut.h>
#include <algorithm>
#include <cmath>

// Assumes the same conventions as the original code:
//   drawCube  (cx, cy, cz,  width, height, depth,  r, g, b)   - full sizes
//   drawSphere(cx, cy, cz,  rx, ry, rz,            r, g, b)   - radii
// Both draw around the given centre and leave the matrix stack as they found it.

namespace {

constexpr float kPi = 3.14159265f;
constexpr bool kWearHat = true;   // set to false for the original bare head

struct RGB { float r, g, b; };

inline RGB shade(RGB c, float f) {
    return {std::min(1.0f, c.r * f), std::min(1.0f, c.g * f), std::min(1.0f, c.b * f)};
}
inline void cube(float x, float y, float z, float w, float h, float d, RGB c) {
    drawCube(x, y, z, w, h, d, c.r, c.g, c.b);
}
inline void ball(float x, float y, float z, float rx, float ry, float rz, RGB c) {
    drawSphere(x, y, z, rx, ry, rz, c.r, c.g, c.b);
}

// A rectangular bar stretched between two points (straps, collar flaps, pencil).
void bar(float x1, float y1, float z1, float x2, float y2, float z2, float w, float d, RGB c) {
    float dx = x2 - x1, dy = y2 - y1, dz = z2 - z1;
    float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len < 1e-5f) return;
    glPushMatrix();
    glTranslatef((x1 + x2) * 0.5f, (y1 + y2) * 0.5f, (z1 + z2) * 0.5f);
    // rotate +Y onto the direction: axis = Y x dir = (dz, 0, -dx)
    float ang = std::acos(std::max(-1.0f, std::min(1.0f, dy / len))) * 180.0f / kPi;
    if (std::fabs(dx) + std::fabs(dz) > 1e-6f) glRotatef(ang, dz, 0.0f, -dx);
    else if (dy < 0) glRotatef(180.0f, 1, 0, 0);
    cube(0, 0, 0, w, len, d, c);
    glPopMatrix();
}

// Palette
const RGB kSkin      {0.55f, 0.32f, 0.19f};
const RGB kSkinShade {0.45f, 0.25f, 0.14f};
const RGB kSkinLight {0.64f, 0.38f, 0.23f};
const RGB kRosy      {0.68f, 0.34f, 0.22f};
const RGB kShirt     {0.15f, 0.30f, 0.65f};
const RGB kShirtLight{0.30f, 0.45f, 0.80f};
const RGB kCollar    {0.86f, 0.89f, 0.96f};
const RGB kTrousers  {0.20f, 0.20f, 0.28f};
const RGB kApron     {0.50f, 0.27f, 0.12f};
const RGB kApronDark {0.35f, 0.17f, 0.07f};
const RGB kHair      {0.10f, 0.06f, 0.03f};
const RGB kStache    {0.20f, 0.13f, 0.08f};
const RGB kLeather   {0.12f, 0.08f, 0.05f};
const RGB kStraw     {0.86f, 0.72f, 0.38f};

// Where the front of the head ellipsoid is (so facial features sit on the surface).
float faceZ(float x, float y) {
    float u = (y - 1.78f) / 0.27f, v = x / 0.24f, q = 1.0f - u * u - v * v;
    return q > 0 ? 0.22f * std::sqrt(q) : 0.0f;
}

/* ---------------- legs and feet ---------------- */

void drawShoe(float x) {
    const RGB sole{0.05f, 0.04f, 0.03f}, lace{0.86f, 0.80f, 0.68f};
    cube(x, 0.03f, 0.08f, 0.26f, 0.04f, 0.34f, sole);                       // sole
    cube(x, 0.06f, -0.06f, 0.20f, 0.05f, 0.12f, shade(sole, 1.4f));          // heel
    ball(x, 0.10f, 0.07f, 0.115f, 0.075f, 0.16f, kLeather);                  // upper
    ball(x, 0.08f, 0.20f, 0.10f, 0.055f, 0.09f, shade(kLeather, 1.2f));      // toe cap
    ball(x, 0.165f, 0.02f, 0.045f, 0.03f, 0.07f, shade(kLeather, 1.5f));     // tongue
    for (int i = 0; i < 3; i++) cube(x, 0.178f, -0.02f + i * 0.05f, 0.09f, 0.008f, 0.012f, lace); // laces
    ball(x - 0.03f, 0.14f, 0.12f, 0.03f, 0.012f, 0.05f, {0.38f, 0.28f, 0.20f}); // polish shine
}

void drawLegs() {
    for (float x : {-0.13f, 0.13f}) {
        cube(x, 0.43f, 0.0f, 0.20f, 0.55f, 0.20f, kTrousers);
        cube(x, 0.45f, 0.1005f, 0.018f, 0.48f, 0.004f, shade(kTrousers, 0.7f)); // crease
        cube(x, 0.19f, 0.0f, 0.215f, 0.05f, 0.215f, shade(kTrousers, 1.25f));   // turned-up cuff
        drawShoe(x);
    }
    ball(0.0f, 0.76f, 0.0f, 0.27f, 0.13f, 0.18f, kTrousers);                    // hips close the gap
    cube(0.0f, 0.83f, 0.0f, 0.50f, 0.05f, 0.35f, {0.30f, 0.17f, 0.08f});          // belt
}

/* ---------------- torso and apron ---------------- */

void drawTorso(float breathe) {
    glPushMatrix();
    glTranslatef(0.0f, 1.17f, 0.0f);
    glScalef(1.0f + 0.01f * breathe, 1.0f + 0.015f * breathe, 1.0f + 0.015f * breathe);
    ball(0, 0, 0, 0.34f, 0.40f, 0.22f, kShirt);
    glPopMatrix();
    ball(0.0f, 1.43f, 0.0f, 0.37f, 0.11f, 0.20f, kShirt);                       // shoulder line
    bar(-0.015f, 1.49f, 0.16f, -0.13f, 1.51f, 0.10f, 0.045f, 0.012f, kCollar);   // collar flaps
    bar(0.015f, 1.49f, 0.16f, 0.13f, 1.51f, 0.10f, 0.045f, 0.012f, kCollar);
}

void drawApron() {
    cube(0.0f, 1.02f, 0.232f, 0.38f, 0.48f, 0.04f, kApron);                      // skirt
    cube(0.0f, 1.36f, 0.222f, 0.26f, 0.22f, 0.035f, kApron);                     // bib
    cube(0.0f, 1.26f, 0.240f, 0.30f, 0.016f, 0.04f, kApronDark);                 // seam
    bar(-0.11f, 1.46f, 0.215f, -0.075f, 1.57f, 0.04f, 0.035f, 0.012f, kApron);   // neck straps
    bar(0.11f, 1.46f, 0.215f, 0.075f, 1.57f, 0.04f, 0.035f, 0.012f, kApron);
    for (float s : {-1.0f, 1.0f}) {                                              // waist ties and bows
        bar(s * 0.19f, 0.93f, 0.22f, s * 0.26f, 0.93f, 0.06f, 0.03f, 0.012f, kApronDark);
        ball(s * 0.24f, 0.915f, 0.12f, 0.035f, 0.025f, 0.018f, kApronDark);
        ball(s * 0.27f, 0.90f, 0.11f, 0.025f, 0.04f, 0.015f, kApronDark);
    }
    // pocket with stitching and a pencil
    cube(0.0f, 1.08f, 0.262f, 0.16f, 0.12f, 0.025f, kApronDark);
    cube(0.0f, 1.138f, 0.2765f, 0.16f, 0.006f, 0.004f, {0.78f, 0.58f, 0.32f});
    bar(0.04f, 1.12f, 0.272f, 0.065f, 1.19f, 0.268f, 0.014f, 0.014f, {0.95f, 0.78f, 0.15f});
    ball(0.0665f, 1.196f, 0.268f, 0.009f, 0.009f, 0.009f, {0.85f, 0.45f, 0.45f}); // eraser
    // name tag
    cube(-0.08f, 1.33f, 0.2395f, 0.09f, 0.04f, 0.008f, {0.95f, 0.92f, 0.82f});
    cube(-0.105f, 1.33f, 0.2445f, 0.012f, 0.012f, 0.003f, {0.75f, 0.15f, 0.12f});
    cube(-0.07f, 1.33f, 0.2445f, 0.04f, 0.006f, 0.003f, {0.30f, 0.25f, 0.2f});
}

/* ---------------- arms ---------------- */

// side: -1 = the arm at -x, +1 = the arm at +x. The +x arm is the one that waves.
void drawArm(float side, float t, float wave) {
    float sway = std::sin(t * 1.3f + (side > 0 ? 0.0f : 1.7f));
    float rest = 7.0f + 2.0f * sway;
    float w = side > 0 ? wave : 0.0f;
    float shoulder = rest + w * (140.0f - rest);

    glPushMatrix();
    glTranslatef(side * 0.35f, 1.42f, 0.0f);
    glRotatef(side * shoulder, 0, 0, 1);
    ball(0, 0, 0, 0.115f, 0.115f, 0.115f, kShirt);                             // shoulder joint
    ball(0, -0.135f, 0, 0.11f, 0.17f, 0.11f, kShirt);                           // upper arm
    glTranslatef(0.0f, -0.27f, 0.0f);                                           // elbow
    glRotatef(-12.0f - w * 20.0f, 1, 0, 0);                                     // slight forward bend
    glRotatef(side * w * (-25.0f + 22.0f * std::sin(t * 9.0f)), 0, 0, 1);       // hand wags while waving
    cube(0.0f, -0.01f, 0.0f, 0.19f, 0.05f, 0.19f, kShirtLight);                 // rolled sleeve
    ball(0.0f, -0.13f, 0.0f, 0.072f, 0.15f, 0.072f, kSkin);                     // forearm

    glTranslatef(0.0f, -0.26f, 0.0f);                                           // wrist
    ball(0.0f, -0.06f, 0.0f, 0.065f, 0.07f, 0.038f, kSkin);                     // palm
    for (int i = 0; i < 4; i++) {
        float fx = (i - 1.5f) * 0.03f, len = (i == 1 || i == 2) ? 0.05f : 0.042f;
        ball(fx, -0.135f - len * 0.3f, 0.008f, 0.014f, len, 0.016f, kSkinShade); // fingers
    }
    ball(-side * 0.065f, -0.07f, 0.02f, 0.02f, 0.042f, 0.02f, kSkin);           // thumb
    glPopMatrix();
}

/* ---------------- head ---------------- */

void drawHat() {
    glPushMatrix();
    glTranslatef(0.0f, 2.0f, 0.0f);
    glRotatef(5.0f, 1, 0, 0);
    glRotatef(3.0f, 0, 0, 1);
    ball(0, -0.005f, 0, 0.45f, 0.015f, 0.43f, shade(kStraw, 0.6f));            // brim underside
    ball(0, 0.0f, 0, 0.46f, 0.03f, 0.44f, kStraw);                              // brim
    ball(0, 0.10f, 0, 0.27f, 0.15f, 0.25f, kStraw);                             // crown
    ball(0, 0.04f, 0, 0.28f, 0.035f, 0.26f, {0.55f, 0.15f, 0.12f});            // hat band
    ball(0, 0.085f, 0, 0.273f, 0.008f, 0.253f, shade(kStraw, 0.75f));          // weave line
    glPopMatrix();
}

void drawHead(float t, float wave) {
    float yaw = 7.0f * std::sin(t * 0.55f) + wave * 10.0f;
    float pitch = 2.0f * std::sin(t * 0.9f) - wave * 3.0f;
    float ph = std::fmod(t, 4.2f);                                               // blink every ~4 s
    float open = ph > 4.0f ? 1.0f - std::sin((ph - 4.0f) / 0.2f * kPi) : 1.0f;

    glPushMatrix();
    glTranslatef(0.0f, 1.56f, 0.0f);                                             // pivot at the neck
    glRotatef(yaw, 0, 1, 0);
    glRotatef(pitch, 1, 0, 0);
    glTranslatef(0.0f, -1.56f, 0.0f);

    ball(0, 1.78f, 0, 0.24f, 0.27f, 0.22f, kSkin);                               // head
    ball(0, 1.60f, 0.07f, 0.12f, 0.085f, 0.12f, kSkin);                          // chin
    for (float s : {-1.0f, 1.0f}) {                                              // ears
        ball(s * 0.24f, 1.78f, 0, 0.05f, 0.075f, 0.04f, kSkin);
        ball(s * 0.245f, 1.78f, 0.012f, 0.025f, 0.045f, 0.02f, kSkinShade);
    }
    for (float s : {-1.0f, 1.0f}) {                                              // cheeks
        float x = s * 0.15f;
        ball(x, 1.70f, faceZ(x, 1.70f), 0.05f, 0.032f, 0.02f, kRosy);
    }

    // Eyes: white, brown iris, pupil, glint, and an eyelid that closes for the blink.
    for (float s : {-1.0f, 1.0f}) {
        float x = s * 0.09f, y = 1.83f, z = faceZ(x, y);
        ball(x, y, z - 0.005f, 0.04f, 0.042f, 0.022f, {0.96f, 0.95f, 0.92f});
        ball(x, y - 0.002f, z + 0.012f, 0.022f, 0.024f, 0.012f, {0.25f, 0.14f, 0.06f});
        ball(x, y - 0.002f, z + 0.019f, 0.012f, 0.013f, 0.006f, {0, 0, 0});
        ball(x + 0.007f, y + 0.008f, z + 0.021f, 0.005f, 0.005f, 0.003f, {1, 1, 1});
        if (open < 0.97f) ball(x, y, z + 0.012f, 0.046f, 0.05f * (1.0f - open), 0.026f, kSkin);
    }
    // Eyebrows, lifted a little during the greeting.
    for (float s : {-1.0f, 1.0f}) {
        float x = s * 0.095f, y = 1.905f + 0.008f * wave;
        glPushMatrix();
        glTranslatef(x, y, faceZ(x, 1.905f) + 0.004f);
        glRotatef(s * 6.0f, 0, 0, 1);
        cube(0, 0, 0, 0.085f, 0.016f, 0.018f, kHair);
        glPopMatrix();
    }
    // Nose.
    ball(0, 1.78f, faceZ(0, 1.78f) + 0.012f, 0.02f, 0.05f, 0.02f, kSkin);
    ball(0, 1.735f, faceZ(0, 1.735f) + 0.022f, 0.04f, 0.032f, 0.03f, kSkinLight);
    for (float s : {-1.0f, 1.0f}) ball(s * 0.017f, 1.722f, faceZ(0, 1.722f) + 0.045f, 0.009f, 0.007f, 0.006f, kSkinShade);

    // Smile: dark opening, teeth, then lips along a curve.
    ball(0, 1.652f, faceZ(0, 1.652f) + 0.004f, 0.04f, 0.015f, 0.01f, {0.25f, 0.05f, 0.05f});
    ball(0, 1.656f, faceZ(0, 1.656f) + 0.008f, 0.032f, 0.008f, 0.008f, {0.95f, 0.93f, 0.85f});
    for (int i = -3; i <= 3; i++) {
        float x = i * 0.022f, u = x / 0.07f;
        float y = 1.66f - 0.018f * (1.0f - u * u);
        ball(x, y, faceZ(x, y) + 0.006f, 0.013f, 0.009f, 0.009f, {0.52f, 0.18f, 0.14f});
    }

    // Handlebar moustache.
    ball(0, 1.693f, faceZ(0, 1.693f) + 0.012f, 0.06f, 0.022f, 0.03f, kStache);
    for (float s : {-1.0f, 1.0f}) {
        ball(s * 0.075f, 1.688f, faceZ(s * 0.075f, 1.688f) + 0.01f, 0.06f, 0.02f, 0.024f, kStache);
        ball(s * 0.135f, 1.70f, faceZ(s * 0.135f, 1.70f) + 0.008f, 0.026f, 0.02f, 0.02f, kStache);
        ball(s * 0.158f, 1.725f, faceZ(s * 0.158f, 1.725f) + 0.006f, 0.015f, 0.022f, 0.015f, kStache); // curled tip
    }

    // Hair showing under the hat, or the original cap of hair without it.
    if (kWearHat) {
        ball(0, 1.84f, -0.08f, 0.235f, 0.22f, 0.18f, kHair);
        for (float s : {-1.0f, 1.0f}) ball(s * 0.215f, 1.88f, 0.0f, 0.05f, 0.09f, 0.11f, kHair);
        drawHat();
    } else {
        ball(0, 1.98f, 0, 0.23f, 0.09f, 0.21f, kHair);
        ball(0, 1.84f, -0.08f, 0.235f, 0.22f, 0.18f, kHair);
    }
    glPopMatrix();
}

} // namespace

void drawShopkeeperPosed(float x, float y, float z, float scale, float time, float wave) {
    float breathe = 0.5f + 0.5f * std::sin(time * 1.8f);

    glPushMatrix();
    glTranslatef(x, y, z);
    glScalef(scale, scale, scale);

    drawLegs();
    drawTorso(breathe);
    drawApron();
    drawArm(-1.0f, time, wave);
    drawArm(1.0f, time, wave);
    ball(0.0f, 1.53f, 0.0f, 0.12f, 0.13f, 0.12f, kSkin);                         // neck
    drawHead(time, wave);

    glPopMatrix();
}

void drawShopkeeper(float x, float y, float z, float scale) {
    float t = glutGet(GLUT_ELAPSED_TIME) * 0.001f;
    float c = std::fmod(t, 10.0f);                                               // wave for 3 s every 10 s
    float wave = (c >= 6.0f && c < 9.0f) ? std::min(1.0f, 2.0f * std::sin((c - 6.0f) / 3.0f * kPi)) : 0.0f;
    drawShopkeeperPosed(x, y, z, scale, t, wave);
}