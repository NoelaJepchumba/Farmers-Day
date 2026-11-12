
#include <GL/freeglut.h>
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
