// Test harness for the milking props. Build (Linux):
//   g++ milking_demo.cpp MilkingObjects.cpp -o milking -lglut -lGL -lGLU
#include <GL/freeglut.h>
#include "Objects/MilkingObjects.h"

static void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    // barn wall and floor as a backdrop
    glBegin(GL_QUADS);
    glColor3f(0.62f, 0.45f, 0.30f); glVertex2f(-1, 1); glVertex2f(1, 1);
    glColor3f(0.52f, 0.36f, 0.22f); glVertex2f(1, -0.6f); glVertex2f(-1, -0.6f);
    glColor3f(0.42f, 0.33f, 0.24f); glVertex2f(-1, -0.6f); glVertex2f(1, -0.6f);
    glColor3f(0.30f, 0.23f, 0.17f); glVertex2f(1, -1); glVertex2f(-1, -1);
    glEnd();
    drawMilkingObjects();
    glutSwapBuffers();
}
static void tick(int) { glutPostRedisplay(); glutTimerFunc(16, tick, 0); } // keeps milk and rope moving

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_MULTISAMPLE);
    glutInitWindowSize(900, 700);
    glutCreateWindow("Milking objects");
    glutDisplayFunc(display);
    glutTimerFunc(16, tick, 0);
    glutMainLoop();
}