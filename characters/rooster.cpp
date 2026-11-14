#include "rooster.h"
#include "primitives.h"
#include <math.h>

namespace
{
    const float RAD = 0.0174532925f;

    // ------------------------------------------------------------------
    // Palette
    // ------------------------------------------------------------------
    const float BODY_COL[3]   = {0.50f, 0.14f, 0.06f};
    const float BREAST_COL[3] = {0.08f, 0.13f, 0.10f};
    const float HACKLE_COL[3] = {0.88f, 0.46f, 0.08f};
    const float SADDLE_COL[3] = {0.80f, 0.30f, 0.06f};
    const float WING_COL[3]   = {0.55f, 0.15f, 0.05f};
    const float FLIGHT_COL[3] = {0.22f, 0.10f, 0.06f};
    const float THIGH_COL[3]  = {0.62f, 0.22f, 0.07f};
    const float HEAD_COL[3]   = {0.86f, 0.44f, 0.09f};
    const float TAIL_DARK[3]  = {0.04f, 0.14f, 0.10f};
    const float TAIL_SHEEN[3] = {0.06f, 0.38f, 0.22f};
    const float TAIL_BRONZE[3]= {0.30f, 0.12f, 0.04f};
    const float COMB_COL[3]   = {0.88f, 0.05f, 0.07f};
    const float SKIN_RED[3]   = {0.85f, 0.10f, 0.10f};
    const float YELLOW[3]     = {0.93f, 0.72f, 0.14f};
    const float YELLOW_DK[3]  = {0.78f, 0.58f, 0.10f};
    const float BEAK_COL[3]   = {0.95f, 0.72f, 0.20f};
    const float EARLOBE[3]    = {0.95f, 0.93f, 0.88f};
    const float IRIS[3]       = {0.92f, 0.55f, 0.08f};
    const float BLACK[3]      = {0.03f, 0.03f, 0.03f};
    const float WHITE[3]      = {0.98f, 0.98f, 0.96f};
    const float CLAW[3]       = {0.30f, 0.24f, 0.16f};

#define COL(c) c[0], c[1], c[2]

    // ------------------------------------------------------------------
    // Tiny vector helpers
    // ------------------------------------------------------------------
    struct V { float x, y, z; };

    inline V mk(float x, float y, float z) { V v = {x, y, z}; return v; }
    inline V add(V a, V b) { return mk(a.x + b.x, a.y + b.y, a.z + b.z); }
    inline V sub(V a, V b) { return mk(a.x - b.x, a.y - b.y, a.z - b.z); }
    inline V mul(V a, float s) { return mk(a.x * s, a.y * s, a.z * s); }
    inline float dot(V a, V b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
    inline V cross(V a, V b)
    {
        return mk(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
    }
    inline V norm(V a)
    {
        float l = sqrtf(dot(a, a));
        if (l < 1e-6f) return mk(1, 0, 0);
        return mul(a, 1.0f / l);
    }
    inline float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

    // Cheap deterministic random number in [0,1] so every feather looks
    // slightly different but the bird never flickers between frames.
    float hash01(int i, int j, int s)
    {
        unsigned h = (unsigned)i * 73856093u ^ (unsigned)j * 19349663u ^ (unsigned)s * 83492791u;
        h ^= h >> 13;
        h *= 0x5bd1e995u;
        h ^= h >> 15;
        return (h & 0xFFFFu) / 65535.0f;
    }

    // ------------------------------------------------------------------
    // One feather: a thin ellipsoid whose root is at `root`, pointing along
    // `dir`. `up` is the direction of the feather's thin axis (its normal).
    // ------------------------------------------------------------------
    void feather(V root, V dir, V up, float len, float wid, float thick,
                 float r, float g, float b)
    {
        V x = norm(dir);
        V z = norm(cross(x, up));
        V y = cross(z, x);
        V c = add(root, mul(x, len * 0.5f));

        GLfloat m[16] = {
            x.x, x.y, x.z, 0,
            y.x, y.y, y.z, 0,
            z.x, z.y, z.z, 0,
            c.x, c.y, c.z, 1
        };

        glPushMatrix();
        glMultMatrixf(m);
        glScalef(len * 0.5f, thick, wid);
        glColor3f(clamp01(r), clamp01(g), clamp01(b));
        glutSolidSphere(1.0, 12, 6);
        glPopMatrix();
    }

    // ------------------------------------------------------------------
    // A long arching tail feather built from a chain of overlapping
    // segments that narrows towards the tip and shifts colour along its length.
    // ------------------------------------------------------------------
    void sickle(V base, float alpha, float zs, float L, float wid,
                const float c0[3], const float c1[3], float g, int seed)
    {
        const int M = 12;
        V dir0 = norm(mk(-cosf(alpha * RAD), sinf(alpha * RAD), zs));
        V prev = base;

        for (int k = 1; k <= M; k++)
        {
            float s = (float)k / M;
            V p = add(base, mul(dir0, L * s));
            p.y -= g * L * s * s;                     // gravity arc

            V seg = sub(p, prev);
            float segLen = sqrtf(dot(seg, seg));
            float w = wid * (1.0f - 0.65f * s);
            float j = 0.92f + 0.16f * hash01(k, seed, 3);

            feather(prev, seg, mk(0, 0, 1), segLen * 2.1f, w * 1.15f, 0.012f * (1.0f - 0.4f * s),
                    (c0[0] * (1 - s) + c1[0] * s) * j,
                    (c0[1] * (1 - s) + c1[1] * s) * j,
                    (c0[2] * (1 - s) + c1[2] * s) * j);
            prev = p;
        }
    }

    // Soft blob shadow on the ground (y = 0 in rooster space).
    void drawShadow()
    {
        glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
        glDisable(GL_LIGHTING);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);

        glColor4f(0.0f, 0.0f, 0.0f, 0.06f);
        for (int i = 0; i < 8; i++)
        {
            float s = 1.0f - 0.1f * i;
            glPushMatrix();
            glTranslatef(-0.08f, 0.01f, 0.0f);
            glScalef(0.62f * s, 0.002f, 0.30f * s);
            glutSolidSphere(1.0, 24, 8);
            glPopMatrix();
        }

        glPopAttrib();
    }

    // ------------------------------------------------------------------
    // Tail: dark base plus a fan of arching sickle feathers.
    // ------------------------------------------------------------------
    void drawTail(float time)
    {
        float flick = 3.0f * sinf(time * 1.7f);

        glPushMatrix();
        glTranslatef(-0.31f, 0.60f, 0.0f);
        glRotatef(flick, 0, 0, 1);

        drawSphereRot(-0.04f, 0.04f, 0.0f, 0.17f, 0.15f, 0.13f, 0, 0, -30, COL(TAIL_DARK));

        V base = mk(0, 0, 0);

        // short bronze coverts first (they sit behind the long ones)
        sickle(base, 85,  0.10f, 0.40f, 0.075f, TAIL_BRONZE, TAIL_SHEEN, 0.35f, 1);
        sickle(base, 85, -0.10f, 0.40f, 0.075f, TAIL_BRONZE, TAIL_SHEEN, 0.35f, 2);
        sickle(base, 40,  0.45f, 0.55f, 0.075f, TAIL_BRONZE, TAIL_DARK,  0.55f, 3);
        sickle(base, 40, -0.45f, 0.55f, 0.075f, TAIL_BRONZE, TAIL_DARK,  0.55f, 4);

        // long, glossy sickles arching up and over
        sickle(base, 72,  0.14f, 0.80f, 0.090f, TAIL_DARK, TAIL_SHEEN, 0.60f, 5);
        sickle(base, 72, -0.14f, 0.80f, 0.090f, TAIL_DARK, TAIL_SHEEN, 0.60f, 6);
        sickle(base, 56,  0.26f, 0.90f, 0.095f, TAIL_DARK, TAIL_SHEEN, 0.60f, 7);
        sickle(base, 56, -0.26f, 0.90f, 0.095f, TAIL_DARK, TAIL_SHEEN, 0.60f, 8);
        sickle(base, 64,  0.00f, 1.00f, 0.100f, TAIL_DARK, TAIL_SHEEN, 0.60f, 9);

        glPopMatrix();
    }

    // ------------------------------------------------------------------
    // Folded wing: smooth covert panel plus fanned flight feathers.
    // ------------------------------------------------------------------
    void drawWing(float side)
    {
        float wz = 0.215f * side;

        // base panel
        drawSphereRot(0.0f, 0.53f, wz, 0.24f, 0.15f, 0.060f, 0, 0, 6, COL(WING_COL));

        // flight feathers fanning out behind the wing
        const float ang[6] = {4, 12, 20, 28, 36, 44};
        const float len[6] = {0.32f, 0.31f, 0.29f, 0.27f, 0.24f, 0.21f};
        for (int k = 0; k < 6; k++)
        {
            float b = ang[k] * RAD;
            V dir = mk(-cosf(b), -sinf(b), 0.0f);
            float jit = 0.92f + 0.16f * hash01(k, (int)side + 3, 11);
            feather(mk(-0.06f, 0.50f - 0.012f * k, 0.250f * side),
                    dir, mk(0, 0, side),
                    len[k], 0.036f, 0.010f,
                    FLIGHT_COL[0] * jit, FLIGHT_COL[1] * jit, FLIGHT_COL[2] * jit);
        }
    }

    // ------------------------------------------------------------------
    // Leg: thigh, scaly yellow shank, spur, four toes with claws.
    // ------------------------------------------------------------------
    void drawLeg(float lx, float side)
    {
        float lz = 0.11f * side;

        // thigh ("trousers")
        drawSphereRot(lx - 0.02f, 0.33f, lz * 1.15f, 0.13f, 0.15f, 0.075f, 0, 0, -10, COL(THIGH_COL));

        // shank with a joint and rows of scales
        drawSphere(lx, 0.24f, lz, 0.034f, 0.034f, 0.034f, COL(YELLOW));
        drawCylinder(lx, 0.24f, lz, 0.028f, 0.020f, 0.21f, 90, 0, 0, COL(YELLOW));
        for (int k = 0; k < 5; k++)
            drawSphere(lx + 0.018f, 0.21f - 0.038f * k, lz, 0.012f, 0.020f, 0.024f, COL(YELLOW_DK));

        // spur
        drawCylinder(lx - 0.022f, 0.12f, lz, 0.016f, 0.003f, 0.05f, -30, -90, 0, COL(CLAW));

        // ankle ball and toes
        drawSphere(lx, 0.030f, lz, 0.034f, 0.030f, 0.034f, COL(YELLOW));

        const float toeAng[4] = {60, 90, 120, -90};     // 3 forward, 1 back
        const float toeLen[4] = {0.14f, 0.16f, 0.14f, 0.08f};
        for (int k = 0; k < 4; k++)
        {
            float phi = toeAng[k] * RAD;
            V dir = mk(sinf(phi), 0.0f, cosf(phi));
            float tx = lx + dir.x * toeLen[k];
            float tz = lz + dir.z * toeLen[k];

            drawCylinder(lx, 0.024f, lz, 0.020f, 0.012f, toeLen[k], 0, toeAng[k], 0, COL(YELLOW));
            drawCylinder(tx, 0.024f, tz, 0.012f, 0.0f, 0.035f, 10, toeAng[k], 0, COL(CLAW));
        }
    }

    // ------------------------------------------------------------------
    // Neck, cape and head (all of it pecks together).
    // ------------------------------------------------------------------
    void drawNeckAndHead()
    {
        // neck
        drawSphereRot(0.25f, 0.86f, 0.0f, 0.20f, 0.15f, 0.13f, 0, 0, 72, COL(HACKLE_COL));

        // smooth golden cape over the shoulders blends neck into body
        drawSphereRot(0.17f, 0.72f, 0.0f, 0.24f, 0.19f, 0.20f, 0, 0, 20, COL(HACKLE_COL));
        drawSphereRot(0.10f, 0.66f, 0.0f, 0.22f, 0.15f, 0.21f, 0, 0, 15, COL(SADDLE_COL));

        // head
        drawSphereRot(0.32f, 1.00f, 0.0f, 0.115f, 0.10f, 0.09f, 0, 0, 0, COL(HEAD_COL));

        // red skin around the eye, white ear lobe
        for (int s = -1; s <= 1; s += 2)
        {
            float side = (float)s;

            drawSphere(0.37f, 1.01f, 0.060f * side, 0.050f, 0.060f, 0.035f, COL(SKIN_RED));
            drawSphere(0.30f, 0.97f, 0.080f * side, 0.030f, 0.040f, 0.012f, COL(EARLOBE));

            // eye: skin ring, iris, pupil, highlight
            drawSphere(0.36f, 1.035f, 0.072f * side, 0.040f, 0.045f, 0.020f, COL(SKIN_RED));
            drawSphere(0.36f, 1.035f, 0.078f * side, 0.026f, 0.030f, 0.018f, COL(IRIS));
            drawSphere(0.366f, 1.035f, 0.090f * side, 0.013f, 0.016f, 0.010f, COL(BLACK));
            drawSphere(0.372f, 1.045f, 0.097f * side, 0.005f, 0.005f, 0.005f, COL(WHITE));

            // wattle hanging under the beak
            drawSphereRot(0.385f, 0.915f, 0.035f * side, 0.025f, 0.065f, 0.014f, 0, 0, -8, COL(COMB_COL));

            // nostril
            drawSphere(0.445f, 0.998f, 0.028f * side, 0.008f, 0.005f, 0.006f, COL(BLACK));
        }

        // beak
        drawCylinder(0.41f, 0.995f, 0.0f, 0.042f, 0.006f, 0.130f, 0, 90, -8, COL(BEAK_COL));
        drawCylinder(0.41f, 0.965f, 0.0f, 0.030f, 0.006f, 0.100f, 0, 90, -4, COL(BEAK_COL));

        // serrated comb: base ridge plus five points
        drawSphereRot(0.325f, 1.085f, 0.0f, 0.095f, 0.030f, 0.018f, 0, 0, -5, COL(COMB_COL));
        const float cx[5] = {0.255f, 0.290f, 0.325f, 0.360f, 0.395f};
        const float ch[5] = {0.040f, 0.058f, 0.070f, 0.058f, 0.040f};
        for (int k = 0; k < 5; k++)
            drawSphereRot(cx[k], 1.09f + ch[k] * 0.6f, 0.0f, 0.036f, ch[k], 0.018f, 0, 0, -8, COL(COMB_COL));
    }
}

void drawRooster(float x, float y, float z, float scale, float time)
{
    glPushMatrix();

    glTranslatef(x, y, z);
    glScalef(scale, scale, scale);

    drawShadow();

    // ---------------- Body (front tilted up) ----------------
    drawSphereRot(0.0f, 0.50f, 0.0f, 0.36f, 0.26f, 0.22f, 0, 0, 12, COL(BODY_COL));

    // ---------------- Breast (dark, glossy green-black) ----------------
    drawSphereRot(0.21f, 0.58f, 0.0f, 0.20f, 0.24f, 0.20f, 0, 0, 0, COL(BREAST_COL));

    // ---------------- Tail, wings, legs ----------------
    drawTail(time);

    drawWing(1.0f);
    drawWing(-1.0f);

    drawLeg( 0.05f,  1.0f);
    drawLeg(-0.03f, -1.0f);

    // ---------------- Neck + head (pecks around the base of the neck) ----------------
    {
        float peck = powf(fmaxf(0.0f, sinf(time * 2.4f)), 8.0f);
        float angle = -45.0f * peck + 2.0f * sinf(time * 1.1f);

        glPushMatrix();
        glTranslatef(0.18f, 0.60f, 0.0f);
        glRotatef(angle, 0, 0, 1);
        glTranslatef(-0.18f, -0.60f, 0.0f);

        drawNeckAndHead();

        glPopMatrix();
    }

    glPopMatrix();
}