// Test harness for the market stall. Build (Linux):
//   g++ market_demo.cpp MarketObjects.cpp -o market -lglut -lGL -lGLU
#include <GL/freeglut.h>
#include "Objects/MarketObjects.h"

static void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    glBegin(GL_QUADS); // sky above, cobbled street below
    glColor3f(0.55f, 0.78f, 0.95f); glVertex2f(-1, 1); glVertex2f(1, 1);
    glColor3f(0.85f, 0.93f, 1.00f); glVertex2f(1, -0.70f); glVertex2f(-1, -0.70f);
    glColor3f(0.55f, 0.50f, 0.45f); glVertex2f(-1, -0.70f); glVertex2f(1, -0.70f);
    glColor3f(0.40f, 0.36f, 0.32f); glVertex2f(1, -1); glVertex2f(-1, -1);
    glEnd();
    drawMarketObjects();
    glutSwapBuffers();
}
static void tick(int) { glutPostRedisplay(); glutTimerFunc(16, tick, 0); } // keeps the awning swaying

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_MULTISAMPLE);
    glutInitWindowSize(900, 700);
    glutCreateWindow("Market stall");
    glutDisplayFunc(display);
    glutTimerFunc(16, tick, 0);
    glutMainLoop();
}