
#ifndef PRIMITIVES_H
#define PRIMITIVES_H

#include <GL/freeglut.h>

void drawSphere(float x, float y, float z,
                float sx, float sy, float sz,
                float r, float g, float b);

void drawCube(float x, float y, float z,
              float sx, float sy, float sz,
              float r, float g, float b);

void drawCone(float x, float y, float z,
              float radius, float height,
              float r, float g, float b);

#endif
