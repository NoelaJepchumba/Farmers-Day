
#include <GL/freeglut.h>
#include <cstdlib>

#include "../Objects/FarmObjects.h"
#include "../Objects/MilkingObjects.h"
#include "../Objects/MarketObjects.h"

int currentScene = 1;

void drawText(float x, float y, const char* text) {
    glColor3f(0.1f, 0.1f, 0.1f);

    glRasterPos2f(x, y);

    while (*text) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *text);
        text++;
    }
}

void display() {

    glClear(GL_COLOR_BUFFER_BIT);

    // Scene 1: Farm objects
    if (currentScene == 1) {

        glClearColor(0.65f, 0.85f, 0.95f, 1.0f);

        drawText(-0.9f, 0.85f, "FARM OBJECTS");

        drawFarmObjects();
    }

    // Scene 2: Milking objects
    else if (currentScene == 2) {

        glClearColor(0.85f, 0.85f, 0.75f, 1.0f);

        drawText(-0.9f, 0.85f, "MILKING OBJECTS");

        drawMilkingObjects();
    }

    // Scene 3: Market objects
    else if (currentScene == 3) {

        glClearColor(0.95f, 0.85f, 0.65f, 1.0f);

        drawText(-0.9f, 0.85f, "MARKET OBJECTS");

        drawMarketObjects();
    }

    // Instructions
    drawText(-0.9f, -0.9f,
             "Press 1: Farm | 2: Milking | 3: Market | ESC: Exit");

    glutSwapBuffers();
}

void keyboard(unsigned char key, int x, int y) {

    if (key == '1') {
        currentScene = 1;
    }

    else if (key == '2') {
        currentScene = 2;
    }

    else if (key == '3') {
        currentScene = 3;
    }

    else if (key == 27) {
        exit(0);
    }

    glutPostRedisplay();
}

void initializeScene() {

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    gluOrtho2D(-1.0, 1.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}
