#include "cow.h"
#include "primitives.h"
#include <math.h>

namespace
{
    const float RAD = 0.0174532925f;
    const float DEG = 57.2957795f;

    // Palette
    const float CREAM[3]     = {0.96f, 0.93f, 0.86f};
    const float PINK[3]      = {0.93f, 0.66f, 0.68f};
    const float DARK_PINK[3] = {0.80f, 0.50f, 0.54f};
    const float NOSTRIL[3]   = {0.40f, 0.14f, 0.17f};
    const float DARK[3]      = {0.07f, 0.07f, 0.08f};
    const float TUFT[3]      = {0.12f, 0.09f, 0.07f};
    const float HORN[3]      = {0.90f, 0.85f, 0.68f};
    const float HORN_TIP[3]  = {0.30f, 0.25f, 0.18f};
    const float HOOF[3]      = {0.16f, 0.12f, 0.09f};
    const float GOLD[3]      = {0.95f, 0.75f, 0.20f};
    const float COLLAR[3]    = {0.65f, 0.12f, 0.12f};
    const float WHITE[3]     = {0.98f, 0.98f, 0.96f};

#define COL(c) c[0], c[1], c[2]

    // ------------------------------------------------------------------
    // Body surface model, used to stick hide patches onto the cow.
    // ------------------------------------------------------------------
    struct Ell { float cx, cy, a, b, c; };   // centre x/y, radii x/y/z (z centre = 0)

    const Ell ELLS[3] = {
        {  0.00f, 0.90f, 0.75f, 0.42f, 0.34f },   // barrel
        { -0.48f, 0.92f, 0.33f, 0.40f, 0.30f },   // rump
        {  0.45f, 0.88f, 0.30f, 0.36f, 0.30f }    // chest
    };

    // Dark patch hugging the body at (px, py) on the given side (+1 / -1).
    void bodyPatch(float px, float py, float sx, float sy, float roll, float side)
    {
        int best = -1;
        float bestZ = 0.0f;
        for (int i = 0; i < 3; i++)
        {
            const Ell& e = ELLS[i];
            float u = (px - e.cx) / e.a;
            float v = (py - e.cy) / e.b;
            float t = 1.0f - u * u - v * v;
            if (t <= 0.0f) continue;
            float z = e.c * sqrtf(t);
            if (z > bestZ) { bestZ = z; best = i; }
        }
        if (best < 0) return;

        const Ell& e = ELLS[best];
        float pz = bestZ * side;

        // Surface normal of the ellipsoid at that point
        float nx = (px - e.cx) / (e.a * e.a);
        float ny = (py - e.cy) / (e.b * e.b);
        float nz = pz / (e.c * e.c);
        float len = sqrtf(nx * nx + ny * ny + nz * nz);
        nx /= len; ny /= len; nz /= len;

        const float depth = 0.05f;   // sink the flattened ellipsoid into the hide

        glPushMatrix();
        glTranslatef(px - nx * depth, py - ny * depth, pz - nz * depth);
        glRotatef(atan2f(nx, nz) * DEG, 0, 1, 0);   // align local Z with the normal
        glRotatef(-asinf(ny) * DEG, 1, 0, 0);
        glRotatef(roll, 0, 0, 1);
        glScalef(sx, sy, 0.07f);
        glColor3f(COL(DARK));
        glutSolidSphere(1.0, 24, 16);
        glPopMatrix();
    }

    // Soft blob shadow on the ground (y = 0 in cow space).
    void drawShadow()
    {
        glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
        glDisable(GL_LIGHTING);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);

        glColor4f(0.0f, 0.0f, 0.0f, 0.10f);
        for (int i = 0; i < 4; i++)
        {
            float s = 1.0f - 0.2f * i;
            glPushMatrix();
            glTranslatef(-0.02f, 0.01f, 0.0f);
            glScalef(1.05f * s, 0.002f, 0.55f * s);
            glutSolidSphere(1.0, 24, 8);
            glPopMatrix();
        }

        glPopAttrib();
    }

    // Curved two-segment horn.
    void drawHorn(float side)
    {
        const float bx = 0.78f, by = 1.465f, bz = 0.12f * side;
        const float a1 = -35.0f, a2 = -78.0f;     // tilt about X (negative = up)
        const float len1 = 0.16f, len2 = 0.15f;
        const float angY = (side > 0.0f) ? 0.0f : 180.0f;

        float j1y = by - sinf(a1 * RAD) * len1;
        float j1z = bz + side * cosf(a1 * RAD) * len1;
        float j2y = j1y - sinf(a2 * RAD) * len2;
        float j2z = j1z + side * cosf(a2 * RAD) * len2;

        drawCylinder(bx, by, bz, 0.050f, 0.036f, len1, a1, angY, 0, COL(HORN));
        drawSphere(bx, j1y, j1z, 0.036f, 0.036f, 0.036f, COL(HORN));
        drawCylinder(bx, j1y, j1z, 0.036f, 0.014f, len2, a2, angY, 0, COL(HORN_TIP));
        drawSphere(bx, j2y, j2z, 0.015f, 0.015f, 0.015f, COL(HORN_TIP));
    }

    // Drooping ear with pink inside, plus an occasional flick.
    void drawEar(float side, float time)
    {
        float flick = 5.0f * sinf(time * 3.0f + side * 1.5f) * sinf(time * 0.9f);
        float ax = side * (20.0f + flick);
        float ay = -side * 20.0f;

        drawSphereRot(0.660f, 1.40f, 0.29f * side, 0.040f, 0.085f, 0.160f, ax, ay, 0, COL(DARK));
        drawSphereRot(0.685f, 1.40f, 0.29f * side, 0.025f, 0.065f, 0.125f, ax, ay, 0, COL(PINK));
    }

    // One leg: tapered upper leg, knee, lower leg, fetlock, hoof.
    void drawLeg(float lx, float lz, bool hind)
    {
        float topR = hind ? 0.130f : 0.120f;

        drawCylinder(lx, 0.68f, lz, topR, 0.085f, 0.32f, 90, 0, 0, COL(CREAM));
        drawSphere(lx, 0.36f, lz, 0.088f, 0.088f, 0.088f, COL(CREAM));
        drawCylinder(lx, 0.36f, lz, 0.078f, 0.062f, 0.25f, 90, 0, 0, COL(CREAM));
        drawSphere(lx, 0.12f, lz, 0.070f, 0.070f, 0.070f, COL(CREAM));
        drawCylinder(lx, 0.125f, lz, 0.070f, 0.090f, 0.125f, 90, 0, 0, COL(HOOF));
    }
}

void drawCow(float x, float y, float z, float scale, float time)
{
    glPushMatrix();

    glTranslatef(x, y, z);
    glScalef(scale, scale, scale);

    drawShadow();

    // ---------------- Body ----------------
    drawSphere( 0.00f, 0.90f, 0.0f, 0.75f, 0.42f, 0.34f, COL(CREAM));   // barrel
    drawSphere(-0.48f, 0.92f, 0.0f, 0.33f, 0.40f, 0.30f, COL(CREAM));   // rump
    drawSphere( 0.45f, 0.88f, 0.0f, 0.30f, 0.36f, 0.30f, COL(CREAM));   // chest

    // Shoulders and thighs give the silhouette some muscle
    for (int s = -1; s <= 1; s += 2)
    {
        drawSphere( 0.40f, 0.80f, 0.24f * s, 0.19f, 0.27f, 0.14f, COL(CREAM));
        drawSphere(-0.44f, 0.78f, 0.22f * s, 0.24f, 0.30f, 0.16f, COL(CREAM));
    }

    // ---------------- Hide patches (different on each side) ----------------
    // Right side
    bodyPatch(-0.35f, 1.14f, 0.18f, 0.12f,  25.0f,  1.0f);
    bodyPatch( 0.15f, 1.16f, 0.17f, 0.11f, -20.0f,  1.0f);
    bodyPatch(-0.62f, 1.06f, 0.10f, 0.12f,   0.0f,  1.0f);
    // Left side
    bodyPatch(-0.15f, 1.10f, 0.20f, 0.13f, -10.0f, -1.0f);
    bodyPatch( 0.38f, 1.17f, 0.12f, 0.09f,  30.0f, -1.0f);
    bodyPatch(-0.55f, 1.02f, 0.12f, 0.14f,   0.0f, -1.0f);

    // ---------------- Legs ----------------
    for (int side = -1; side <= 1; side += 2)
    {
        drawLeg( 0.42f, 0.19f * side, false);
        drawLeg(-0.46f, 0.19f * side, true);
    }

    // ---------------- Udder ----------------
    drawSphere(-0.02f, 0.50f, 0.0f, 0.22f, 0.13f, 0.20f, COL(PINK));
    for (int i = 0; i < 4; i++)
    {
        float tx = (i < 2) ? -0.12f : 0.08f;
        float tz = (i % 2 == 0) ? 0.09f : -0.09f;
        drawCylinder(tx, 0.42f, tz, 0.035f, 0.028f, 0.10f, 90, 0, 0, COL(DARK_PINK));
        drawSphere(tx, 0.32f, tz, 0.028f, 0.028f, 0.028f, COL(DARK_PINK));
    }

    // ---------------- Tail (swishes with time) ----------------
    {
        float sway = 18.0f * sinf(time * 2.2f);
        glPushMatrix();
        glTranslatef(-0.79f, 1.02f, 0.0f);
        glRotatef(-12.0f, 0, 0, 1);
        glRotatef(sway, 1, 0, 0);

        drawSphere(0, 0, 0, 0.05f, 0.05f, 0.05f, COL(CREAM));
        drawCylinder(0, 0, 0, 0.040f, 0.022f, 0.60f, 90, 0, 0, COL(CREAM));
        drawSphere(0, -0.66f, 0, 0.065f, 0.14f, 0.065f, COL(TUFT));
        glPopMatrix();
    }

    // ---------------- Neck ----------------
    drawSphereRot(0.58f, 1.10f, 0.0f, 0.28f, 0.36f, 0.27f, 0, 0, -35, COL(CREAM));

    // Collar and bell
    drawTorus(0.58f, 1.10f, 0.0f, 0.035f, 0.285f, 0, 90, 55, COL(COLLAR));
    drawSphere(0.825f, 0.865f, 0.0f, 0.065f, 0.075f, 0.065f, COL(GOLD));
    drawSphere(0.825f, 0.790f, 0.0f, 0.020f, 0.020f, 0.020f, COL(TUFT));

    // ---------------- Head (bobs gently around the base of the neck) ----------------
    {
        float bob = 2.5f * sinf(time * 1.3f);
        glPushMatrix();
        glTranslatef(0.50f, 1.00f, 0.0f);
        glRotatef(bob, 0, 0, 1);
        glTranslatef(-0.50f, -1.00f, 0.0f);

        // Skull and muzzle (tilted down 25 degrees)
        drawSphereRot(0.800f, 1.280f, 0.0f, 0.25f, 0.24f, 0.21f, 0, 0, -25, COL(CREAM));
        drawSphereRot(0.956f, 1.141f, 0.0f, 0.15f, 0.12f, 0.15f, 0, 0, -25, COL(PINK));

        // Forelock tuft
        drawSphereRot(0.880f, 1.475f, 0.0f, 0.07f, 0.04f, 0.09f, 0, 0, -25, COL(TUFT));

        for (int s = -1; s <= 1; s += 2)
        {
            float side = (float)s;

            // Face patch
            drawSphereRot(0.736f, 1.332f, 0.165f * side, 0.10f, 0.09f, 0.05f, 0, 0, -25, COL(DARK));

            // Nostril
            drawSphereRot(1.082f, 1.104f, 0.060f * side, 0.030f, 0.020f, 0.020f, 0, 0, -25, COL(NOSTRIL));

            // Eye: white, pupil, highlight
            drawSphere(0.880f, 1.330f, 0.175f * side, 0.050f, 0.055f, 0.035f, COL(WHITE));
            drawSphere(0.892f, 1.330f, 0.200f * side, 0.032f, 0.040f, 0.020f, COL(DARK));
            drawSphere(0.902f, 1.345f, 0.215f * side, 0.008f, 0.008f, 0.008f, COL(WHITE));

            drawEar(side, time);
            drawHorn(side);
        }

        glPopMatrix();
    }

    glPopMatrix();
}