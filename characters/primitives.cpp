
#include "primitives.h"

void drawSphere(float x, float y, float z,
                float sx, float sy, float sz,
                float r, float g, float b)
{
    glPushMatrix();

    glTranslatef(x, y, z);
    glScalef(sx, sy, sz);

    glColor3f(r, g, b);
    glutSolidSphere(1.0, 24, 20);

    glPopMatrix();
}

void drawCube(float x, float y, float z,
              float sx, float sy, float sz,
              float r, float g, float b)
{
    glPushMatrix();

    glTranslatef(x, y, z);
    glScalef(sx, sy, sz);

    glColor3f(r, g, b);
    glutSolidCube(1.0);

    glPopMatrix();
}

void drawCone(float x, float y, float z,
              float radius, float height,
              float r, float g, float b)
{
    glPushMatrix();

    glTranslatef(x, y, z);

    glColor3f(r, g, b);
    glutSolidCone(radius, height, 20, 12);

    glPopMatrix();
}
