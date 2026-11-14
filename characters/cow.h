#ifndef COW_H
#define COW_H

// Draws the cow standing on the plane y = (y argument), facing +X.
// `time` (seconds) is optional: pass it to get tail swish, ear flicks and a
// gentle head bob. Leave it at 0 for a static pose.
void drawCow(float x, float y, float z, float scale, float time = 0.0f);

#endif
