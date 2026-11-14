
#ifndef ROOSTER_H
#define ROOSTER_H

// Draws the rooster standing on the plane y = (y argument), facing +X.
// `time` (seconds) is optional: pass it for pecking, head movement and a
// gently flicking tail. Leave it at 0 for a static pose.
void drawRooster(float x, float y, float z, float scale, float time = 0.0f);

#endif