
#include <GL/freeglut.h>

#include "characters/farmer.h"
#include "characters/cow.h"
#include "characters/rooster.h"
#include "characters/shopkeeper.h"

void display()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    gluLookAt(4, 3, 9,
              0, 1, 0,
              0, 1, 0);

    // Ground
    glColor3f(0.35, 0.65, 0.25);

    glBegin(GL_QUADS);
        glVertex3f(-10, 0, -10);
        glVertex3f(10, 0, -10);
        glVertex3f(10, 0, 10);
        glVertex3f(-10, 0, 10);
    glEnd();

    // Characters
    drawFarmer(-2.5, 0, 0, 1.0);
    drawCow(0, 0, 0, 1.0);
    drawRooster(2.0, 0, 0, 0.8);
    drawShopkeeper(4.0, 0, 0, 1.0);

    glutSwapBuffers();
}

void init()
{
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);

    glClearColor(0.55, 0.75, 0.95, 1.0);
}

int main(int argc, char** argv)
{
    glutInit(&argc, argv);

    glutInitDisplayMode(
        GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH
    );

    glutInitWindowSize(1100, 700);

    glutCreateWindow("A Day in the Life of a Farmer");

    init();

    glutDisplayFunc(display);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    gluPerspective(45, 1100.0 / 700.0, 0.1, 100);

    glMatrixMode(GL_MODELVIEW);

    glutMainLoop();

    return 0;
}
