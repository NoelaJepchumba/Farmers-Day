
#ifndef PRIMITIVES_H
#define PRIMITIVES_H

#include <GL/freeglut.h>
#include <GL/glu.h>

// ---- Original primitives (unchanged signatures) ----------------------------

void drawSphere(float x, float y, float z,
                float sx, float sy, float sz,
                float r, float g, float b);

void drawCube(float x, float y, float z,
              float sx, float sy, float sz,
              float r, float g, float b);

void drawCone(float x, float y, float z,
              float radius, float height,
              float r, float g, float b);

// ---- New primitives --------------------------------------------------------
// Rotation order: the shape is scaled, then rotated about X, then Y, then Z,
// then translated. Angles are in degrees.

// Ellipsoid with rotation.
void drawSphereRot(float x, float y, float z,
                   float sx, float sy, float sz,
                   float angX, float angY, float angZ,
                   float r, float g, float b);

// Tapered cylinder (capped). Its axis is local +Z before rotation.
// With angX = 90 it points straight down, with angX = -90 straight up.
void drawCylinder(float x, float y, float z,
                  float rBase, float rTop, float height,
                  float angX, float angY, float angZ,
                  float r, float g, float b);

// Torus whose axis is local +Z before rotation.
void drawTorus(float x, float y, float z,
               float tubeRadius, float ringRadius,
               float angX, float angY, float angZ,
               float r, float g, float b);

// Sets up depth test, smooth shading, two lights (key + fill) and
// colour-material so glColor drives the material. Call once after
// creating the window. Lights are placed using the modelview matrix at the
// time of the call (call before gluLookAt for a headlight-style setup, or
// every frame after the camera for world-fixed lights).
void initCowLighting();

#endif
