// Minimal test harness.  Build (Linux):
//   g++ main_demo.cpp FarmObjects.cpp -o farm -lglut -lGL -lGLU
#include <GL/freeglut.h>
#include "FarmObjects.h"

static void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    // sky and ground so the props have a backdrop
    glBegin(GL_QUADS);
    glColor3f(0.45f, 0.72f, 0.95f); glVertex2f(-1, 1); glVertex2f(1, 1);
    glColor3f(0.80f, 0.92f, 1.00f); glVertex2f(1, 0.1f); glVertex2f(-1, 0.1f);
    glColor3f(0.36f, 0.62f, 0.24f); glVertex2f(-1, 0.1f); glVertex2f(1, 0.1f);
    glColor3f(0.24f, 0.46f, 0.16f); glVertex2f(1, -1); glVertex2f(-1, -1);
    glEnd();
    drawFarmObjects();
    glutSwapBuffers();
}
static void tick(int) { glutPostRedisplay(); glutTimerFunc(16, tick, 0); } // keeps the water shimmering

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_MULTISAMPLE);
    glutInitWindowSize(900, 700);
    glutCreateWindow("Farm objects");
    glutDisplayFunc(display);
    glutTimerFunc(16, tick, 0);
    glutMainLoop();
}