#include <GL/freeglut.h>
#include <GL/glu.h>

#include "characters/primitives.h"   // initCowLighting()
#include "characters/farmer.h"
#include "characters/cow.h"
#include "characters/rooster.h"
#include "characters/shopkeeper.h"

// Older gl.h headers (e.g. on Windows) only cover OpenGL 1.1 and lack this
#ifndef GL_MULTISAMPLE
#define GL_MULTISAMPLE 0x809D
#endif

static const int WIN_W = 1100;
static const int WIN_H = 700;

void display()
{
    // Seconds since start, drives the cow and rooster animation
    float t = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    gluLookAt(4, 3, 9,
              0, 1, 0,
              0, 1, 0);

    // Lights are positioned after the camera so they stay fixed in the world
    const GLfloat keyPos[]  = { 0.5f,  1.0f,  0.8f, 0.0f };
    const GLfloat fillPos[] = { -0.8f, 0.3f, -0.5f, 0.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, keyPos);
    glLightfv(GL_LIGHT1, GL_POSITION, fillPos);

    // Ground (needs a normal now that lighting is on)
    glColor3f(0.35f, 0.65f, 0.25f);
    glNormal3f(0, 1, 0);

    glBegin(GL_QUADS);
        glVertex3f(-10, 0, -10);
        glVertex3f( 10, 0, -10);
        glVertex3f( 10, 0,  10);
        glVertex3f(-10, 0,  10);
    glEnd();

    // Characters
    drawFarmer(-2.5f, 0, 0, 1.0f);
    drawCow(0, 0, 0, 1.0f, t);
    drawRooster(2.0f, 0, 0, 0.8f, t);
    drawShopkeeper(4.0f, 0, 0, 1.0f);

    glutSwapBuffers();
}

void reshape(int w, int h)
{
    if (h == 0) h = 1;

    glViewport(0, 0, w, h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45, (double)w / h, 0.1, 100);

    glMatrixMode(GL_MODELVIEW);
}

void idle()
{
    glutPostRedisplay();   // keep redrawing so the characters animate
}

void init()
{
    glClearColor(0.55f, 0.75f, 0.95f, 1.0f);

    // Depth test, normalised normals, smooth shading, colour-material
    // and the key + fill lights
    initCowLighting();
}

int main(int argc, char** argv)
{
    glutInit(&argc, argv);

    glutInitDisplayMode(
        GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_MULTISAMPLE
    );

    glutInitWindowSize(WIN_W, WIN_H);

    glutCreateWindow("A Day in the Life of a Farmer");

    init();

    glEnable(GL_MULTISAMPLE);   // smoother edges

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutIdleFunc(idle);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    gluPerspective(45, 1100.0 / 700.0, 0.1, 100);

    glMatrixMode(GL_MODELVIEW);
#include "Scenes/SceneManager.h"

int main(int argc, char** argv) {

    glutInit(&argc, argv);

    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);

    glutInitWindowSize(1000, 700);

    glutInitWindowPosition(100, 100);

    glutCreateWindow("A Day in the Life of a Farmer");

    initializeScene();

    glClearColor(0.65f, 0.85f, 0.95f, 1.0f);

    glutDisplayFunc(display);

    glutKeyboardFunc(keyboard);

    glutMainLoop();

    return 0;
}