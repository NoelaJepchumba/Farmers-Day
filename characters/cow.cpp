
#include "cow.h"
#include "primitives.h"

void drawCow(float x, float y, float z, float scale)
{
    glPushMatrix();

    glTranslatef(x, y, z);
    glScalef(scale, scale, scale);

    // Main body
    drawSphere(0, 0.90, 0, 0.75, 0.42, 0.34,
               0.95, 0.95, 0.90);

    // Black body patches
    drawSphere(-0.35, 1.08, 0.30, 0.22, 0.18, 0.06,
               0.05, 0.05, 0.05);

    drawSphere(0.30, 0.78, 0.31, 0.22, 0.16, 0.05,
               0.05, 0.05, 0.05);

    // Neck
    drawSphere(0.55, 1.13, 0, 0.25, 0.38, 0.25,
               0.95, 0.95, 0.90);

    // Head
    drawSphere(0.76, 1.35, 0, 0.26, 0.28, 0.24,
               0.95, 0.95, 0.90);

    // Muzzle
    drawSphere(0.95, 1.22, 0.08, 0.19, 0.13, 0.18,
               0.85, 0.52, 0.52);

    // Nostrils
    drawSphere(1.04, 1.25, 0.22, 0.025, 0.025, 0.02,
               0.20, 0.08, 0.08);

    drawSphere(0.91, 1.25, 0.22, 0.025, 0.025, 0.02,
               0.20, 0.08, 0.08);

    // Eyes
    drawSphere(0.82, 1.43, 0.22, 0.035, 0.04, 0.02,
               0, 0, 0);

    drawSphere(0.82, 1.43, -0.22, 0.035, 0.04, 0.02,
               0, 0, 0);

    // Horns
    drawCone(0.65, 1.55, 0.08, 0.07, 0.20,
             0.80, 0.72, 0.52);

    drawCone(0.65, 1.55, -0.08, 0.07, 0.20,
             0.80, 0.72, 0.52);

    // Ears
    drawSphere(0.55, 1.48, 0.25, 0.15, 0.07, 0.10,
               0.85, 0.55, 0.55);

    drawSphere(0.55, 1.48, -0.25, 0.15, 0.07, 0.10,
               0.85, 0.55, 0.55);

    // Four legs
    for (int side = -1; side <= 1; side += 2)
    {
        for (int front = 0; front <= 1; front++)
        {
            float lx = front ? 0.43 : -0.45;
            float lz = side * 0.20;

            drawSphere(lx, 0.36, lz,
                       0.11, 0.35, 0.11,
                       0.95, 0.95, 0.90);

            // Hooves
            drawCube(lx, 0.08, lz,
                     0.16, 0.12, 0.16,
                     0.18, 0.13, 0.08);
        }
    }

    // Udder
    drawSphere(-0.05, 0.48, 0.02,
               0.25, 0.15, 0.23,
               0.85, 0.52, 0.55);

    // Teats
    for (int i = 0; i < 4; i++)
    {
        float tx = (i < 2) ? -0.14 : 0.10;
        float tz = (i % 2 == 0) ? 0.12 : -0.12;

        drawSphere(tx, 0.32, tz,
                   0.045, 0.11, 0.045,
                   0.85, 0.52, 0.55);
    }

    // Tail
    drawSphere(-0.78, 1.05, 0,
               0.06, 0.28, 0.06,
               0.95, 0.95, 0.90);

    drawSphere(-0.78, 0.80, 0,
               0.07, 0.12, 0.07,
               0.12, 0.07, 0.04);

    glPopMatrix();
}
