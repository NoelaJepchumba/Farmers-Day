
#include "primitives.h"

namespace
{
    GLUquadric* quad()
    {
        static GLUquadric* q = nullptr;
        if (!q)
        {
            q = gluNewQuadric();
            gluQuadricNormals(q, GLU_SMOOTH);
        }
        return q;
    }

    const int SPHERE_SLICES = 32;
    const int SPHERE_STACKS = 24;
}

void drawSphere(float x, float y, float z,
                float sx, float sy, float sz,
                float r, float g, float b)
{
    glPushMatrix();

    glTranslatef(x, y, z);
    glScalef(sx, sy, sz);

    glColor3f(r, g, b);
    glutSolidSphere(1.0, SPHERE_SLICES, SPHERE_STACKS);

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
    glutSolidCone(radius, height, 24, 12);

    glPopMatrix();
}

void drawSphereRot(float x, float y, float z,
                   float sx, float sy, float sz,
                   float angX, float angY, float angZ,
                   float r, float g, float b)
{
    glPushMatrix();

    glTranslatef(x, y, z);
    glRotatef(angZ, 0, 0, 1);
    glRotatef(angY, 0, 1, 0);
    glRotatef(angX, 1, 0, 0);
    glScalef(sx, sy, sz);

    glColor3f(r, g, b);
    glutSolidSphere(1.0, SPHERE_SLICES, SPHERE_STACKS);

    glPopMatrix();
}

void drawCylinder(float x, float y, float z,
                  float rBase, float rTop, float height,
                  float angX, float angY, float angZ,
                  float r, float g, float b)
{
    glPushMatrix();

    glTranslatef(x, y, z);
    glRotatef(angZ, 0, 0, 1);
    glRotatef(angY, 0, 1, 0);
    glRotatef(angX, 1, 0, 0);

    glColor3f(r, g, b);
    gluCylinder(quad(), rBase, rTop, height, 24, 6);

    // Base cap (faces -Z)
    glPushMatrix();
    glRotatef(180.0f, 1, 0, 0);
    gluDisk(quad(), 0.0, rBase, 24, 1);
    glPopMatrix();

    // Top cap (faces +Z)
    glPushMatrix();
    glTranslatef(0, 0, height);
    gluDisk(quad(), 0.0, rTop, 24, 1);
    glPopMatrix();

    glPopMatrix();
}

void drawTorus(float x, float y, float z,
               float tubeRadius, float ringRadius,
               float angX, float angY, float angZ,
               float r, float g, float b)
{
    glPushMatrix();

    glTranslatef(x, y, z);
    glRotatef(angZ, 0, 0, 1);
    glRotatef(angY, 0, 1, 0);
    glRotatef(angX, 1, 0, 0);

    glColor3f(r, g, b);
    glutSolidTorus(tubeRadius, ringRadius, 16, 40);

    glPopMatrix();
}

void initCowLighting()
{
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);          // needed because we scale spheres
    glShadeModel(GL_SMOOTH);

    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    const GLfloat spec[] = {0.22f, 0.22f, 0.22f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 28.0f);

    const GLfloat globalAmb[] = {0.30f, 0.30f, 0.34f, 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmb);

    glEnable(GL_LIGHTING);

    // Key light: warm sun from upper right-front
    const GLfloat keyPos[]  = {0.5f, 1.0f, 0.8f, 0.0f};
    const GLfloat keyDiff[] = {0.95f, 0.92f, 0.84f, 1.0f};
    const GLfloat keySpec[] = {0.60f, 0.60f, 0.55f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, keyPos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  keyDiff);
    glLightfv(GL_LIGHT0, GL_SPECULAR, keySpec);
    glEnable(GL_LIGHT0);

    // Fill light: cool, weak, from the opposite side
    const GLfloat fillPos[]  = {-0.8f, 0.3f, -0.5f, 0.0f};
    const GLfloat fillDiff[] = {0.25f, 0.30f, 0.40f, 1.0f};
    const GLfloat fillSpec[] = {0.0f, 0.0f, 0.0f, 1.0f};
    glLightfv(GL_LIGHT1, GL_POSITION, fillPos);
    glLightfv(GL_LIGHT1, GL_DIFFUSE,  fillDiff);
    glLightfv(GL_LIGHT1, GL_SPECULAR, fillSpec);
    glEnable(GL_LIGHT1);
}