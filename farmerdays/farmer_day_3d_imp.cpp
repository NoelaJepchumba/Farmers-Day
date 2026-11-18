/*
 * A Day in the Life of a Farmer  -  3D (enhanced rendering)
 * ----------------------------------------------------------
 * Computer Graphics mini-project: C++ / OpenGL / GLUT (+ GLSL 1.20 shaders)
 *
 *    1. Sunrise     - rooster crows as the sun rises
 *    2. Bedroom     - farmer wakes up and prepares
 *    3. Farm        - farmer walks to the cowshed
 *    4. Milking     - cow is tied, washed and milked
 *    5. Packaging   - milk is transferred into containers
 *    6. Ride        - she cycles to the market with a crate of milk
 *    7. Market      - farmer sells milk and receives money
 *    8. Ride home   - she cycles back with an empty crate
 *    9. Home        - she gets off the bicycle and goes inside
 *   10. Dinner      - she eats dinner while the radio plays
 *
 * Controls:  1..9, 0 scene | N/P next/prev | SPACE pause | R restart
 *            C camera motion on/off | L enhanced rendering on/off | ESC quit
 *
 * Rendering:
 *   - Two-pass shadow mapping: the scene is first rendered from the sun into a
 *     depth texture (framebuffer object), then rendered from the camera; the
 *     fragment shader compares depths (3x3 PCF) to get soft shadows.
 *   - Per-pixel lighting in GLSL: wrap-around diffuse, hemisphere ambient
 *     (sky above / ground bounce below), Blinn-Phong specular, rim light,
 *     fog and filmic tone mapping.
 *   - Procedural materials computed in the shader from 3D noise: cow spots,
 *     grass, dirt, wood planks, straw, tiles, curly hair, leaves, roof rows.
 *   - If shaders are not available the program falls back automatically to
 *     the fixed-function pipeline (press L to compare both).
 *
 * Build (Windows, MinGW + freeglut):
 *     g++ farmer_day_3d.cpp -o farmer_day_3d.exe -lfreeglut -lopengl32
 * Build (Linux):  g++ farmer_day_3d.cpp -o farmer_day_3d -lGL -lglut
 * Build (macOS):  g++ farmer_day_3d.cpp -o farmer_day_3d -framework OpenGL -framework GLUT
 * No GLEW / GLU needed: extension functions are loaded with glutGetProcAddress.
 */

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <GLUT/glut.h>
#include <dlfcn.h>
#else
#include <GL/freeglut.h>   // freeglut's full header: also declares glutGetProcAddress
#endif

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <utility>
#include <vector>

// ---- OpenGL 2.0 / FBO definitions (Windows' gl.h only covers OpenGL 1.1) ----
#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#endif
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif
#ifndef GL_DEPTH_COMPONENT24
#define GL_DEPTH_COMPONENT24 0x81A6
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_MULTISAMPLE
#define GL_MULTISAMPLE 0x809D
#endif

typedef GLuint(APIENTRY* PFN_CreateShader)(GLenum);
typedef void(APIENTRY* PFN_ShaderSource)(GLuint, GLsizei, const char* const*, const GLint*);
typedef void(APIENTRY* PFN_CompileShader)(GLuint);
typedef void(APIENTRY* PFN_GetShaderiv)(GLuint, GLenum, GLint*);
typedef void(APIENTRY* PFN_GetShaderInfoLog)(GLuint, GLsizei, GLsizei*, char*);
typedef GLuint(APIENTRY* PFN_CreateProgram)(void);
typedef void(APIENTRY* PFN_AttachShader)(GLuint, GLuint);
typedef void(APIENTRY* PFN_LinkProgram)(GLuint);
typedef void(APIENTRY* PFN_GetProgramiv)(GLuint, GLenum, GLint*);
typedef void(APIENTRY* PFN_GetProgramInfoLog)(GLuint, GLsizei, GLsizei*, char*);
typedef void(APIENTRY* PFN_UseProgram)(GLuint);
typedef GLint(APIENTRY* PFN_GetUniformLocation)(GLuint, const char*);
typedef void(APIENTRY* PFN_Uniform1i)(GLint, GLint);
typedef void(APIENTRY* PFN_Uniform1f)(GLint, GLfloat);
typedef void(APIENTRY* PFN_Uniform2f)(GLint, GLfloat, GLfloat);
typedef void(APIENTRY* PFN_Uniform3f)(GLint, GLfloat, GLfloat, GLfloat);
typedef void(APIENTRY* PFN_UniformMatrix4fv)(GLint, GLsizei, unsigned char, const GLfloat*);
typedef void(APIENTRY* PFN_GenFramebuffers)(GLsizei, GLuint*);
typedef void(APIENTRY* PFN_BindFramebuffer)(GLenum, GLuint);
typedef void(APIENTRY* PFN_FramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
typedef GLenum(APIENTRY* PFN_CheckFramebufferStatus)(GLenum);

PFN_CreateShader pCreateShader; PFN_ShaderSource pShaderSource; PFN_CompileShader pCompileShader;
PFN_GetShaderiv pGetShaderiv; PFN_GetShaderInfoLog pGetShaderInfoLog; PFN_CreateProgram pCreateProgram;
PFN_AttachShader pAttachShader; PFN_LinkProgram pLinkProgram; PFN_GetProgramiv pGetProgramiv;
PFN_GetProgramInfoLog pGetProgramInfoLog; PFN_UseProgram pUseProgram; PFN_GetUniformLocation pGetUniformLocation;
PFN_Uniform1i pUniform1i; PFN_Uniform1f pUniform1f; PFN_Uniform2f pUniform2f; PFN_Uniform3f pUniform3f;
PFN_UniformMatrix4fv pUniformMatrix4fv; PFN_GenFramebuffers pGenFramebuffers; PFN_BindFramebuffer pBindFramebuffer;
PFN_FramebufferTexture2D pFramebufferTexture2D; PFN_CheckFramebufferStatus pCheckFramebufferStatus;

void* getProc(const char* name) {
#ifdef __APPLE__
    return dlsym(RTLD_DEFAULT, name);
#else
    return reinterpret_cast<void*>(glutGetProcAddress(name));
#endif
}

// ===========================================================================
// Globals
// ===========================================================================
const float W = 1000.0f, H = 600.0f;
const float PI = 3.14159265f;
const int NUM_SCENES = 10;
const float SCENE_DUR[NUM_SCENES] = {10.0f, 16.0f, 10.0f, 36.0f, 19.5f, 11.0f, 14.0f, 11.0f, 14.0f, 14.0f};
const int SHADOW_SIZE = 2048;

int gScene = 0;
float gT = 0;
bool gPaused = false, gCamMotion = true;
int gLastMs = 0;
int gVP[4] = {0, 0, 1000, 600};

enum { PASS_SHADOW, PASS_MAIN };
int gPass = PASS_MAIN;
bool gShaderOK = false, gEnhanced = true;
GLuint gProg = 0, gShadowTex = 0, gShadowFBO = 0;
float gShadowExtent = 16;
bool shadowPass() { return gPass == PASS_SHADOW; }
bool useShader() { return gShaderOK && gEnhanced; }

// Caption shown at the top of the screen (a scene may replace it, e.g. milking steps)
const char* DESCS[NUM_SCENES] = {
    "Morning on the farm",
    "Waking up",
    "Going to the cowshed",
    "Milking the cow",
    "Bottling the milk",
    "Riding to the market",
    "Selling milk",
    "Riding home",
    "Home at last",
    "Dinner time"};
const char* gCaption = nullptr;   // per-frame override of the caption
float gGauge = -1;                // pail gauge (milking), -1 = hidden
char gHud[64] = "";               // small counter on the right of the caption bar

// ===========================================================================
// Math
// ===========================================================================
struct Col { float r, g, b; };
struct V3 { float x, y, z; };
V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V3 operator*(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
float len(V3 a) { return sqrtf(dot(a, a)); }
V3 norm(V3 a) { float l = len(a); return l > 1e-6f ? a * (1 / l) : V3{0, 1, 0}; }
V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
V3 lerp3(V3 a, V3 b, float t) { return a + (b - a) * t; }
float rad(float d) { return d * PI / 180.0f; }
float deg(float r) { return r * 180.0f / PI; }
V3 rotX(V3 v, float d) { float c = cosf(rad(d)), s = sinf(rad(d)); return {v.x, v.y * c - v.z * s, v.y * s + v.z * c}; }
V3 rotY(V3 v, float d) { float c = cosf(rad(d)), s = sinf(rad(d)); return {v.x * c + v.z * s, v.y, -v.x * s + v.z * c}; }
V3 rotZ(V3 v, float d) { float c = cosf(rad(d)), s = sinf(rad(d)); return {v.x * c - v.y * s, v.x * s + v.y * c, v.z}; }
Col mix(Col a, Col b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }
Col mix3(Col a, Col b, Col c, float t) { return t < 0.5f ? mix(a, b, t * 2) : mix(b, c, (t - 0.5f) * 2); }
Col scale(Col c, float s) { return {c.r * s, c.g * s, c.b * s}; }
float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
float ph(float t, float a, float b) { return clampf((t - a) / (b - a), 0, 1); }
float ease(float x) { x = clampf(x, 0, 1); return x * x * (3 - 2 * x); }

// 4x4 column-major matrices (same layout as OpenGL)
struct M4 { float m[16]; };
M4 mul(const M4& a, const M4& b) {
    M4 r;
    for (int c = 0; c < 4; c++)
        for (int w = 0; w < 4; w++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += a.m[k * 4 + w] * b.m[c * 4 + k];
            r.m[c * 4 + w] = s;
        }
    return r;
}
M4 perspM(float fovy, float aspect, float n, float f) {
    M4 r = {};
    float t = 1.0f / tanf(rad(fovy) / 2);
    r.m[0] = t / aspect; r.m[5] = t; r.m[10] = (f + n) / (n - f); r.m[11] = -1; r.m[14] = 2 * f * n / (n - f);
    return r;
}
M4 orthoM(float l, float rr, float b, float t, float n, float f) {
    M4 o = {};
    o.m[0] = 2 / (rr - l); o.m[5] = 2 / (t - b); o.m[10] = -2 / (f - n);
    o.m[12] = -(rr + l) / (rr - l); o.m[13] = -(t + b) / (t - b); o.m[14] = -(f + n) / (f - n); o.m[15] = 1;
    return o;
}
M4 lookAtM(V3 eye, V3 ctr, V3 up) {
    V3 f = norm(ctr - eye), s = norm(cross(f, up)), u = cross(s, f);
    M4 r = {};
    r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;
    r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -dot(s, eye); r.m[13] = -dot(u, eye); r.m[14] = dot(f, eye); r.m[15] = 1;
    return r;
}
M4 invRigid(const M4& v) {             // inverse of a rotation+translation matrix
    M4 r = {};
    r.m[0] = v.m[0]; r.m[1] = v.m[4]; r.m[2] = v.m[8];
    r.m[4] = v.m[1]; r.m[5] = v.m[5]; r.m[6] = v.m[9];
    r.m[8] = v.m[2]; r.m[9] = v.m[6]; r.m[10] = v.m[10];
    float tx = v.m[12], ty = v.m[13], tz = v.m[14];
    r.m[12] = -(r.m[0] * tx + r.m[4] * ty + r.m[8] * tz);
    r.m[13] = -(r.m[1] * tx + r.m[5] * ty + r.m[9] * tz);
    r.m[14] = -(r.m[2] * tx + r.m[6] * ty + r.m[10] * tz);
    r.m[15] = 1;
    return r;
}

float kf(float t, std::initializer_list<std::pair<float, float>> k) {
    auto it = k.begin();
    if (t <= it->first) return it->second;
    std::pair<float, float> prev = *it;
    for (++it; it != k.end(); ++it) {
        if (t <= it->first) return prev.second + (it->second - prev.second) * (t - prev.first) / (it->first - prev.first);
        prev = *it;
    }
    return prev.second;
}
struct Key { float t, x, z; };
struct Track { V3 pos; float heading; bool moving; };
V3 pathAt(const std::vector<Key>& k, float t) {
    if (t <= k[0].t) return {k[0].x, 0, k[0].z};
    for (size_t i = 1; i < k.size(); i++)
        if (t <= k[i].t) {
            float u = (t - k[i - 1].t) / (k[i].t - k[i - 1].t);
            return {k[i - 1].x + (k[i].x - k[i - 1].x) * u, 0, k[i - 1].z + (k[i].z - k[i - 1].z) * u};
        }
    return {k.back().x, 0, k.back().z};
}
Track track(const std::vector<Key>& k, float t, float idleHeading) {
    V3 a = pathAt(k, t), b = pathAt(k, t - 0.05f);
    V3 d = a - b;
    Track tr{a, idleHeading, len(d) > 0.002f};
    if (tr.moving) tr.heading = deg(atan2f(d.x, d.z));
    return tr;
}

// ===========================================================================
// Shaders
// ===========================================================================
const char* VS_SRC = R"(#version 120
uniform mat4 uInvView;
uniform mat4 uLightMat;
varying vec3 vN, vEye, vWorld, vObj, vWN;
varying vec4 vShadow, vCol;
void main() {
    vec4 eye = gl_ModelViewMatrix * gl_Vertex;
    vEye = eye.xyz;
    vN = normalize(gl_NormalMatrix * gl_Normal);
    vec4 w = uInvView * eye;
    vWorld = w.xyz;
    vWN = normalize(mat3(uInvView) * vN);
    vObj = gl_Vertex.xyz;
    vShadow = uLightMat * vec4(w.xyz + vWN * 0.03, 1.0);   // normal offset vs. acne
    vCol = gl_Color;
    gl_Position = gl_ProjectionMatrix * eye;
}
)";

const char* FS_SRC = R"(#version 120
uniform sampler2D uShadow;
uniform int uUseShadow, uLit, uMat, uFog;
uniform float uTexel, uSeed, uSpec, uRim;
uniform vec3 uLightDir, uLightCol, uSkyAmb, uGroundAmb, uFogCol;
uniform vec2 uFogRange;
varying vec3 vN, vEye, vWorld, vObj, vWN;
varying vec4 vShadow, vCol;

float hash(vec3 p) { p = fract(p * 0.3183099 + 0.1); p *= 17.0; return fract(p.x * p.y * p.z * (p.x + p.y + p.z)); }
float noise(vec3 x) {
    vec3 i = floor(x), f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(hash(i), hash(i + vec3(1,0,0)), f.x), mix(hash(i + vec3(0,1,0)), hash(i + vec3(1,1,0)), f.x), f.y),
               mix(mix(hash(i + vec3(0,0,1)), hash(i + vec3(1,0,1)), f.x), mix(hash(i + vec3(0,1,1)), hash(i + vec3(1,1,1)), f.x), f.y), f.z);
}
float fbm(vec3 p) { float a = 0.5, s = 0.0; for (int i = 0; i < 4; i++) { s += a * noise(p); p *= 2.03; a *= 0.5; } return s; }

float shadowFactor() {
    vec3 p = vShadow.xyz / vShadow.w * 0.5 + 0.5;
    if (p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0 || p.z > 1.0) return 1.0;
    float s = 0.0;
    for (int x = -1; x <= 1; x++)
        for (int y = -1; y <= 1; y++) {
            float d = texture2D(uShadow, p.xy + vec2(float(x), float(y)) * uTexel * 1.5).r;
            s += (p.z - 0.0006 > d) ? 0.0 : 1.0;
        }
    return s / 9.0;
}

vec3 material(vec3 base) {
    vec3 Wn = normalize(vWN);
    if (uMat == 1) {                                   // cow: white fur + black spots
        float n = fbm(vObj * 1.9 + vec3(uSeed));
        float spot = smoothstep(0.525, 0.545, n);
        vec3 fur = base * (0.94 + 0.08 * noise(vObj * 30.0));
        return mix(fur, vec3(0.05, 0.05, 0.06), spot);
    }
    if (uMat == 2) {                                   // grass
        float n = fbm(vWorld * vec3(0.45, 0.0, 0.45));
        float n2 = noise(vWorld * vec3(7.0, 0.0, 7.0));
        return base * (0.78 + 0.4 * n) * (0.9 + 0.15 * n2) * vec3(1.0, 1.0 + 0.08 * (n - 0.5), 1.0);
    }
    if (uMat == 3) {                                   // dirt
        float n = fbm(vWorld * vec3(1.2, 1.2, 1.2));
        float n2 = noise(vWorld * 15.0);
        return base * (0.8 + 0.32 * n) * (0.9 + 0.16 * n2);
    }
    if (uMat == 4 || uMat == 5) {                      // wood planks
        float c, a;
        if (uMat == 4) { a = vWorld.x; c = abs(Wn.y) > 0.5 ? vWorld.z : vWorld.y; }
        else { a = vWorld.y; c = abs(Wn.x) > 0.5 ? vWorld.z : vWorld.x; }
        float w = 0.24;
        float id = floor(c / w), f = fract(c / w);
        float gap = smoothstep(0.0, 0.05, f) * smoothstep(1.0, 0.95, f);
        float grain = fbm(vec3(a * 0.8 + id * 13.1, f * 5.0, id));
        float streak = 0.5 + 0.5 * sin(f * 40.0 + grain * 9.0);
        vec3 col = base * (0.82 + 0.3 * hash(vec3(id, 3.0, 7.0))) * (0.82 + 0.3 * grain) * (0.94 + 0.08 * streak);
        return col * mix(0.5, 1.0, gap);
    }
    if (uMat == 6) {                                   // straw / hay
        float s = noise(vec3(vWorld.x * 3.0, vWorld.y * 30.0, vWorld.z * 30.0));
        float s2 = noise(vec3(vWorld.x * 30.0, vWorld.y * 3.0, vWorld.z * 4.0));
        return base * (0.7 + 0.25 * s + 0.25 * s2) * (0.85 + 0.25 * fbm(vWorld * 1.5));
    }
    if (uMat == 7) {                                   // wall tiles
        vec2 uv = abs(Wn.z) > 0.5 ? vWorld.xy : vWorld.zy;
        vec2 g = fract(uv / 0.3);
        float grout = step(0.035, g.x) * step(0.035, g.y);
        return base * mix(0.78, 0.97 + 0.05 * hash(vec3(floor(uv / 0.3), 1.0)), grout);
    }
    if (uMat == 8) {                                   // curly hair
        float n = noise(vObj * 9.0) * 0.55 + noise(vObj * 24.0) * 0.45;
        return base * (0.45 + 1.1 * n);
    }
    if (uMat == 9) {                                   // fabric
        return base * (0.9 + 0.12 * noise(vObj * 35.0 + vec3(uSeed)));
    }
    if (uMat == 10) {                                  // leaves
        float n = fbm(vObj * 3.5 + vec3(uSeed));
        return base * (0.65 + 0.6 * n);
    }
    if (uMat == 11) {                                  // roof rows
        float r = fract(vWorld.y * 5.0);
        float stagger = fract(vWorld.x * 2.5 + floor(vWorld.y * 5.0) * 0.5);
        float edge = smoothstep(0.0, 0.12, r) * (0.85 + 0.15 * smoothstep(0.0, 0.06, stagger));
        return base * (0.72 + 0.28 * edge) * (0.92 + 0.12 * noise(vWorld * 6.0));
    }
    if (uMat == 12) {                                  // plaster
        return base * (0.92 + 0.1 * fbm(vWorld * 2.5));
    }
    return base;
}

void main() {
    vec3 base = material(vCol.rgb);
    if (uLit == 0) { gl_FragColor = vec4(base, vCol.a); return; }
    vec3 N = normalize(vN);
    vec3 L = normalize(uLightDir);
    vec3 V = normalize(-vEye);
    float ndl = dot(N, L);
    float wrap = clamp((ndl + 0.3) / 1.3, 0.0, 1.0);
    float sh = (uUseShadow == 1) ? shadowFactor() : 1.0;
    sh = min(sh, smoothstep(-0.1, 0.2, ndl) * 0.6 + 0.4);
    float up = normalize(vWN).y * 0.5 + 0.5;
    vec3 amb = mix(uGroundAmb, uSkyAmb, up);
    vec3 Hh = normalize(L + V);
    float spec = pow(max(dot(N, Hh), 0.0), 40.0) * uSpec * sh;
    float rim = pow(1.0 - max(dot(N, V), 0.0), 3.0) * uRim;
    vec3 c = base * (amb + uLightCol * wrap * sh) + uLightCol * spec + rim * (uLightCol * 0.25 * sh + uSkyAmb * 0.3);
    c = vec3(1.0) - exp(-c * 1.7);                                 // tone mapping
    float lum = dot(c, vec3(0.299, 0.587, 0.114));
    c = mix(vec3(lum), c, 1.12);                                    // a touch more saturation
    if (uFog == 1) {
        float f = clamp((length(vEye) - uFogRange.x) / (uFogRange.y - uFogRange.x), 0.0, 1.0);
        c = mix(c, uFogCol, f);
    }
    gl_FragColor = vec4(c, vCol.a);
}
)";

struct Uniforms {
    GLint invView, lightMat, shadow, useShadow, lit, mat, fog, texel, seed, spec, rim,
        lightDir, lightCol, skyAmb, groundAmb, fogCol, fogRange;
} U;

GLuint compileShader(GLenum type, const char* src) {
    GLuint s = pCreateShader(type);
    pShaderSource(s, 1, &src, nullptr);
    pCompileShader(s);
    GLint ok = 0;
    pGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048]; pGetShaderInfoLog(s, 2048, nullptr, log);
        std::fprintf(stderr, "Shader compile error:\n%s\n", log);
        return 0;
    }
    return s;
}

void initShaders() {
    const char* v = (const char*)glGetString(GL_VERSION);
    std::printf("OpenGL %s\n", v ? v : "?");
#define LOAD(var, name) var = (decltype(var))getProc(name)
    LOAD(pCreateShader, "glCreateShader"); LOAD(pShaderSource, "glShaderSource");
    LOAD(pCompileShader, "glCompileShader"); LOAD(pGetShaderiv, "glGetShaderiv");
    LOAD(pGetShaderInfoLog, "glGetShaderInfoLog"); LOAD(pCreateProgram, "glCreateProgram");
    LOAD(pAttachShader, "glAttachShader"); LOAD(pLinkProgram, "glLinkProgram");
    LOAD(pGetProgramiv, "glGetProgramiv"); LOAD(pGetProgramInfoLog, "glGetProgramInfoLog");
    LOAD(pUseProgram, "glUseProgram"); LOAD(pGetUniformLocation, "glGetUniformLocation");
    LOAD(pUniform1i, "glUniform1i"); LOAD(pUniform1f, "glUniform1f"); LOAD(pUniform2f, "glUniform2f");
    LOAD(pUniform3f, "glUniform3f"); LOAD(pUniformMatrix4fv, "glUniformMatrix4fv");
    LOAD(pGenFramebuffers, "glGenFramebuffers"); LOAD(pBindFramebuffer, "glBindFramebuffer");
    LOAD(pFramebufferTexture2D, "glFramebufferTexture2D"); LOAD(pCheckFramebufferStatus, "glCheckFramebufferStatus");
    if (!pGenFramebuffers) {
        LOAD(pGenFramebuffers, "glGenFramebuffersEXT"); LOAD(pBindFramebuffer, "glBindFramebufferEXT");
        LOAD(pFramebufferTexture2D, "glFramebufferTexture2DEXT"); LOAD(pCheckFramebufferStatus, "glCheckFramebufferStatusEXT");
    }
#undef LOAD
    if (!pCreateShader || !pUseProgram || !pUniformMatrix4fv || !pGenFramebuffers || !pFramebufferTexture2D) {
        std::printf("Shaders/FBO not available - using fixed-function rendering.\n");
        return;
    }
    GLuint vs = compileShader(GL_VERTEX_SHADER, VS_SRC), fs = compileShader(GL_FRAGMENT_SHADER, FS_SRC);
    if (!vs || !fs) return;
    gProg = pCreateProgram();
    pAttachShader(gProg, vs); pAttachShader(gProg, fs);
    pLinkProgram(gProg);
    GLint ok = 0;
    pGetProgramiv(gProg, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048]; pGetProgramInfoLog(gProg, 2048, nullptr, log);
        std::fprintf(stderr, "Program link error:\n%s\n", log);
        return;
    }
#define UNI(f) U.f = pGetUniformLocation(gProg, "u" #f)
    U.invView = pGetUniformLocation(gProg, "uInvView"); U.lightMat = pGetUniformLocation(gProg, "uLightMat");
    U.shadow = pGetUniformLocation(gProg, "uShadow"); U.useShadow = pGetUniformLocation(gProg, "uUseShadow");
    U.lit = pGetUniformLocation(gProg, "uLit"); U.mat = pGetUniformLocation(gProg, "uMat");
    U.fog = pGetUniformLocation(gProg, "uFog"); U.texel = pGetUniformLocation(gProg, "uTexel");
    U.seed = pGetUniformLocation(gProg, "uSeed"); U.spec = pGetUniformLocation(gProg, "uSpec");
    U.rim = pGetUniformLocation(gProg, "uRim"); U.lightDir = pGetUniformLocation(gProg, "uLightDir");
    U.lightCol = pGetUniformLocation(gProg, "uLightCol"); U.skyAmb = pGetUniformLocation(gProg, "uSkyAmb");
    U.groundAmb = pGetUniformLocation(gProg, "uGroundAmb"); U.fogCol = pGetUniformLocation(gProg, "uFogCol");
    U.fogRange = pGetUniformLocation(gProg, "uFogRange");
#undef UNI
    // shadow map: depth texture attached to a framebuffer object
    glGenTextures(1, &gShadowTex);
    glBindTexture(GL_TEXTURE_2D, gShadowTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SHADOW_SIZE, SHADOW_SIZE, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    pGenFramebuffers(1, &gShadowFBO);
    pBindFramebuffer(GL_FRAMEBUFFER, gShadowFBO);
    pFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gShadowTex, 0);
    glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE);
    GLenum st = pCheckFramebufferStatus(GL_FRAMEBUFFER);
    pBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (st != GL_FRAMEBUFFER_COMPLETE) { std::printf("Shadow FBO incomplete - using fixed-function rendering.\n"); return; }
    gShaderOK = true;
    std::printf("Enhanced rendering enabled (GLSL shaders + shadow mapping).\n");
}

// Material / state helpers (no-ops when shaders are off or during the shadow pass)
enum { M_PLAIN, M_SPOTS, M_GRASS, M_DIRT, M_WOOD, M_WOODWALL, M_STRAW, M_TILE, M_HAIR, M_FABRIC, M_LEAF, M_ROOF, M_PLASTER };
bool shaderActive() { return useShader() && gPass == PASS_MAIN; }
void setMat(int m, float seed = 0) { if (shaderActive()) { pUniform1i(U.mat, m); pUniform1f(U.seed, seed); } }
void setSpec(float s) { if (shaderActive()) pUniform1f(U.spec, s); }
void unlit() { glDisable(GL_LIGHTING); if (shaderActive()) pUniform1i(U.lit, 0); }
void lit() { glEnable(GL_LIGHTING); if (shaderActive()) pUniform1i(U.lit, 1); }

// ===========================================================================
// 3D primitives with normals (display lists)
// ===========================================================================
GLuint LIST_CUBE, LIST_SPHERE, LIST_CYL, LIST_GRASS, LIST_COWBODY, LIST_SAVANNA;

void col(Col c, float a = 1) { glColor4f(c.r, c.g, c.b, a); }
void col(float r, float g, float b, float a = 1) { glColor4f(r, g, b, a); }

void box(float x, float y, float z, float sx, float sy, float sz) {
    glPushMatrix(); glTranslatef(x, y, z); glScalef(sx, sy, sz); glCallList(LIST_CUBE); glPopMatrix();
}
void sph(float x, float y, float z, float rx, float ry, float rz) {
    glPushMatrix(); glTranslatef(x, y, z); glScalef(rx, ry, rz); glCallList(LIST_SPHERE); glPopMatrix();
}
void sph(float x, float y, float z, float r) { sph(x, y, z, r, r, r); }
void sph(V3 p, float r) { sph(p.x, p.y, p.z, r, r, r); }
void cyl(float x, float y, float z, float r, float h) {
    glPushMatrix(); glTranslatef(x, y, z); glScalef(r, h, r); glCallList(LIST_CYL); glPopMatrix();
}
void frustum(float r1, float r2, float h, bool capTop = true) {
    const int S = 24;
    float ny = (r1 - r2) / h;
    glBegin(GL_QUAD_STRIP);
    for (int j = 0; j <= S; j++) {
        float b = 2 * PI * j / S;
        V3 nn = norm({cosf(b), ny, sinf(b)});
        glNormal3f(nn.x, nn.y, nn.z);
        glVertex3f(r2 * cosf(b), h, r2 * sinf(b));
        glVertex3f(r1 * cosf(b), 0, r1 * sinf(b));
    }
    glEnd();
    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0, -1, 0); glVertex3f(0, 0, 0);
    for (int j = 0; j <= S; j++) { float b = 2 * PI * j / S; glVertex3f(r1 * cosf(b), 0, r1 * sinf(b)); }
    glEnd();
    if (capTop && r2 > 0) {
        glBegin(GL_TRIANGLE_FAN);
        glNormal3f(0, 1, 0); glVertex3f(0, h, 0);
        for (int j = S; j >= 0; j--) { float b = 2 * PI * j / S; glVertex3f(r2 * cosf(b), h, r2 * sinf(b)); }
        glEnd();
    }
}
// torus in the XZ plane (ring radius R, tube radius r)
void torus(float R, float r, int seg = 28, int sides = 10) {
    for (int i = 0; i < seg; i++) {
        float a0 = 2 * PI * i / seg, a1 = 2 * PI * (i + 1) / seg;
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= sides; j++) {
            float b = 2 * PI * j / sides;
            for (int k = 0; k < 2; k++) {
                float a = k ? a0 : a1;
                V3 n = {cosf(b) * cosf(a), sinf(b), cosf(b) * sinf(a)};
                glNormal3f(n.x, n.y, n.z);
                glVertex3f((R + r * cosf(b)) * cosf(a), r * sinf(b), (R + r * cosf(b)) * sinf(a));
            }
        }
        glEnd();
    }
}
void alignY(V3 a, V3 b) {          // translate to a and rotate +Y onto (b-a)
    V3 d = norm(b - a);
    V3 axis = cross({0, 1, 0}, d);
    glTranslatef(a.x, a.y, a.z);
    if (len(axis) > 1e-5f) glRotatef(deg(acosf(clampf(d.y, -1, 1))), axis.x, axis.y, axis.z);
    else if (d.y < 0) glRotatef(180, 1, 0, 0);
}
void cylBetween(V3 a, V3 b, float r, float r2 = -1) {
    float L = len(b - a);
    if (L < 1e-4f) return;
    glPushMatrix(); alignY(a, b);
    if (r2 < 0) { glScalef(r, L, r); glCallList(LIST_CYL); }
    else frustum(r, r2, L);
    glPopMatrix();
}
void roof(float x0, float x1, float z0, float z1, float yb, float yp) {
    float zm = (z0 + z1) / 2;
    V3 nf = norm({0, z1 - zm, yp - yb}), nb = norm({0, zm - z0, -(yp - yb)});
    glBegin(GL_QUADS);
    glNormal3f(nf.x, nf.y, nf.z);
    glVertex3f(x0, yb, z1); glVertex3f(x1, yb, z1); glVertex3f(x1, yp, zm); glVertex3f(x0, yp, zm);
    glNormal3f(nb.x, nb.y, nb.z);
    glVertex3f(x1, yb, z0); glVertex3f(x0, yb, z0); glVertex3f(x0, yp, zm); glVertex3f(x1, yp, zm);
    glEnd();
    glBegin(GL_TRIANGLES);
    glNormal3f(1, 0, 0); glVertex3f(x1, yb, z1); glVertex3f(x1, yb, z0); glVertex3f(x1, yp, zm);
    glNormal3f(-1, 0, 0); glVertex3f(x0, yb, z0); glVertex3f(x0, yb, z1); glVertex3f(x0, yp, zm);
    glEnd();
}

unsigned gRng = 12345;
float rnd() { gRng = gRng * 1664525u + 1013904223u; return (gRng >> 8) / 16777216.0f; }

bool onFarmFeature(float x, float z) {              // keep grass off paths and buildings
    if (z > 0.35f && z < 2.25f) return true;
    if (x > 6.0f && x < 8.0f && z > -3.2f && z < 0.6f) return true;
    if (x > -8.8f && x < -7.2f && z > -2.8f && z < 0.6f) return true;
    if (x > -10.4f && x < -5.6f && z < -2.2f && z > -6) return true;
    if (x > 4.2f && x < 9.8f && z < -2.7f && z > -7.6f) return true;
    return false;
}

// Cow body: one smooth closed surface built ring by ring along the x axis.
// Each ring is an ellipse whose size follows a rounded "barrel" profile, with a
// deeper belly underneath. Normals come from the surface's partial derivatives.
V3 cowBodyPoint(float u, float th) {
    const float x0 = -0.98f, x1 = 0.95f;
    float k = fabsf(2 * u - 1);
    float e = powf(clampf(1 - powf(k, 3.2f), 0, 1), 0.33f);           // rounded ends
    float x = x0 + (x1 - x0) * u;
    float belly = 1 + 0.18f * sinf(PI * u);
    float top = 0.46f * e * (1 + 0.04f * sinf(PI * u * 2));            // slight shoulder/hip bumps
    float bot = 0.5f * e * belly;
    float rz = 0.48f * e * (1 + 0.06f * (1 - u));                        // wider hips
    float cy = 1.06f;
    float sy = sinf(th), cz = cosf(th);
    return {x, cy + sy * (sy > 0 ? top : bot), rz * cz};
}
void buildCowBody() {
    const int NU = 56, NT = 36;
    const float du = 0.5f / NU, dt = 0.5f * 2 * PI / NT;
    for (int i = 0; i < NU; i++) {
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= NT; j++) {
            float th = 2 * PI * j / NT;
            for (int k = 1; k >= 0; k--) {
                float u = (float)(i + k) / NU;
                V3 P = cowBodyPoint(u, th);
                V3 Pu = cowBodyPoint(clampf(u + du, 0, 1), th) - cowBodyPoint(clampf(u - du, 0, 1), th);
                V3 Pt = cowBodyPoint(u, th + dt) - cowBodyPoint(u, th - dt);
                V3 n = norm(cross(Pt, Pu));
                V3 out = P - V3{P.x, 1.06f, 0};
                if (u <= 0.001f) n = {-1, 0, 0};
                else if (u >= 0.999f) n = {1, 0, 0};
                else if (dot(n, out) < 0) n = n * -1;
                glNormal3f(n.x, n.y, n.z);
                glVertex3f(P.x, P.y, P.z);
            }
        }
        glEnd();
    }
}

void buildLists() {
    LIST_CUBE = glGenLists(6);
    LIST_SAVANNA = LIST_CUBE + 5;
    LIST_COWBODY = LIST_CUBE + 4;
    LIST_SPHERE = LIST_CUBE + 1;
    LIST_CYL = LIST_CUBE + 2;
    LIST_GRASS = LIST_CUBE + 3;

    glNewList(LIST_CUBE, GL_COMPILE);
    glBegin(GL_QUADS);
    const float n[6][3] = {{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
    const float v[6][4][3] = {
        {{-.5f, -.5f, .5f}, {.5f, -.5f, .5f}, {.5f, .5f, .5f}, {-.5f, .5f, .5f}},
        {{.5f, -.5f, -.5f}, {-.5f, -.5f, -.5f}, {-.5f, .5f, -.5f}, {.5f, .5f, -.5f}},
        {{.5f, -.5f, .5f}, {.5f, -.5f, -.5f}, {.5f, .5f, -.5f}, {.5f, .5f, .5f}},
        {{-.5f, -.5f, -.5f}, {-.5f, -.5f, .5f}, {-.5f, .5f, .5f}, {-.5f, .5f, -.5f}},
        {{-.5f, .5f, .5f}, {.5f, .5f, .5f}, {.5f, .5f, -.5f}, {-.5f, .5f, -.5f}},
        {{-.5f, -.5f, -.5f}, {.5f, -.5f, -.5f}, {.5f, -.5f, .5f}, {-.5f, -.5f, .5f}}};
    for (int f = 0; f < 6; f++) { glNormal3fv(n[f]); for (int i = 0; i < 4; i++) glVertex3fv(v[f][i]); }
    glEnd();
    glEndList();

    glNewList(LIST_SPHERE, GL_COMPILE);
    const int ST = 20, SL = 32;
    for (int i = 0; i < ST; i++) {
        float a0 = PI * (-0.5f + (float)i / ST), a1 = PI * (-0.5f + (float)(i + 1) / ST);
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= SL; j++) {
            float b = 2 * PI * j / SL;
            float x0 = cosf(a0) * cosf(b), z0 = cosf(a0) * sinf(b), y0 = sinf(a0);
            float x1 = cosf(a1) * cosf(b), z1 = cosf(a1) * sinf(b), y1 = sinf(a1);
            glNormal3f(x1, y1, z1); glVertex3f(x1, y1, z1);
            glNormal3f(x0, y0, z0); glVertex3f(x0, y0, z0);
        }
        glEnd();
    }
    glEndList();

    glNewList(LIST_CYL, GL_COMPILE);
    const int CS = 24;
    glBegin(GL_QUAD_STRIP);
    for (int j = 0; j <= CS; j++) {
        float b = 2 * PI * j / CS;
        glNormal3f(cosf(b), 0, sinf(b));
        glVertex3f(cosf(b), 1, sinf(b)); glVertex3f(cosf(b), 0, sinf(b));
    }
    glEnd();
    for (int cap = 0; cap < 2; cap++) {
        glBegin(GL_TRIANGLE_FAN);
        glNormal3f(0, cap ? 1.0f : -1.0f, 0);
        glVertex3f(0, (float)cap, 0);
        for (int j = 0; j <= CS; j++) { float b = 2 * PI * (cap ? CS - j : j) / CS; glVertex3f(cosf(b), (float)cap, sinf(b)); }
        glEnd();
    }
    glEndList();

    glNewList(LIST_SAVANNA, GL_COMPILE);                  // dry grass clumps for the open road
    gRng = 31337;
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < 3000; i++) {
        float x = -40 + rnd() * 80, z = -12 + rnd() * 22.0f;
        if (z > 0.3f && z < 2.5f) continue;
        float g = 0.8f + rnd() * 0.3f;
        for (int b = 0; b < 4; b++) {
            float a = rnd() * 2 * PI, h = 0.15f + rnd() * 0.3f, w = 0.022f, dx = cosf(a), dz = sinf(a);
            glNormal3f(-dz * 0.3f, 1, dx * 0.3f);
            glColor3f(0.45f * g, 0.45f * g, 0.20f * g);
            glVertex3f(x - dz * w, 0, z + dx * w); glVertex3f(x + dz * w, 0, z - dx * w);
            glColor3f(0.80f * g, 0.74f * g, 0.40f * g);
            glVertex3f(x + dx * 0.07f, h, z + dz * 0.07f);
        }
    }
    glEnd();
    glEndList();

    glNewList(LIST_COWBODY, GL_COMPILE);
    buildCowBody();
    glEndList();

    // grass tufts and wild flowers scattered over the farm
    glNewList(LIST_GRASS, GL_COMPILE);
    gRng = 777;
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < 2600; i++) {
        float x = -22 + rnd() * 44, z = -9.2f + rnd() * 18.0f;
        if (onFarmFeature(x, z)) continue;
        float g = 0.75f + rnd() * 0.35f;
        for (int b = 0; b < 3; b++) {
            float a = rnd() * 2 * PI, h = 0.12f + rnd() * 0.16f, w = 0.025f;
            float lean = 0.04f + rnd() * 0.06f;
            float dx = cosf(a), dz = sinf(a);
            glNormal3f(-dz * 0.3f, 1, dx * 0.3f);
            glColor3f(0.20f * g, 0.45f * g, 0.14f * g);
            glVertex3f(x - dz * w, 0, z + dx * w); glVertex3f(x + dz * w, 0, z - dx * w);
            glColor3f(0.42f * g, 0.70f * g, 0.25f * g);
            glVertex3f(x + dx * lean, h, z + dz * lean);
        }
    }
    glEnd();
    const Col flowerCols[4] = {{1, 0.9f, 0.2f}, {1, 1, 1}, {0.9f, 0.4f, 0.7f}, {0.6f, 0.5f, 1}};
    for (int i = 0; i < 160; i++) {
        float x = -20 + rnd() * 40, z = -9 + rnd() * 17;
        if (onFarmFeature(x, z)) continue;
        float h = 0.12f + rnd() * 0.1f;
        glColor3f(0.25f, 0.5f, 0.2f); cyl(x, 0, z, 0.008f, h);
        col(flowerCols[i % 4]); sph(x, h, z, 0.035f, 0.02f, 0.035f);
        glColor3f(1, 0.75f, 0.1f); sph(x, h + 0.012f, z, 0.012f);
    }
    for (int i = 0; i < 25; i++) {                                          // rocks
        float x = -20 + rnd() * 40, z = -9 + rnd() * 17;
        if (onFarmFeature(x, z)) continue;
        float s = 0.08f + rnd() * 0.15f, gcol = 0.5f + rnd() * 0.15f;
        glColor3f(gcol, gcol, gcol * 0.95f); sph(x, s * 0.3f, z, s * 1.3f, s * 0.7f, s);
    }
    glEndList();
}

// ===========================================================================
// Camera, light, fog, 2D
// ===========================================================================
M4 gView, gLightVP;
V3 gCtr;

void begin3D(V3 eye, V3 ctr, float fov = 45) {
    gCtr = ctr;
    glEnable(GL_DEPTH_TEST);
    if (shadowPass()) return;          // the light's camera is set in setLight()
    glClear(GL_DEPTH_BUFFER_BIT);
    M4 P = perspM(fov, W / H, 0.1f, 250.0f);
    gView = lookAtM(eye, ctr, {0, 1, 0});
    glMatrixMode(GL_PROJECTION); glLoadMatrixf(P.m);
    glMatrixMode(GL_MODELVIEW); glLoadMatrixf(gView.m);
    glEnable(GL_LIGHTING);
    if (useShader()) {
        pUseProgram(gProg);
        M4 inv = invRigid(gView);
        pUniformMatrix4fv(U.invView, 1, 0, inv.m);
        pUniform1i(U.lit, 1); pUniform1i(U.mat, 0); pUniform1f(U.spec, 0.12f); pUniform1f(U.rim, 0.35f);
        pUniform1i(U.fog, 0);
    }
}
// Directional "sun" light. Also builds the light's view-projection for shadows.
void setLight(V3 dir, Col diffuse, Col ambient) {
    dir = norm(dir);
    V3 up = fabsf(dir.y) > 0.95f ? V3{0, 0, 1} : V3{0, 1, 0};
    float E = gShadowExtent;
    M4 lv = lookAtM(gCtr + dir * 40.0f, gCtr, up);
    M4 lp = orthoM(-E, E, -E, E, 1, 100);
    gLightVP = mul(lp, lv);
    if (shadowPass()) {
        glMatrixMode(GL_PROJECTION); glLoadMatrixf(lp.m);
        glMatrixMode(GL_MODELVIEW); glLoadMatrixf(lv.m);
        return;
    }
    float p[4] = {dir.x, dir.y, dir.z, 0};
    float d[4] = {diffuse.r, diffuse.g, diffuse.b, 1};
    float a[4] = {ambient.r, ambient.g, ambient.b, 1};
    glLightfv(GL_LIGHT0, GL_POSITION, p);           // transformed by the current view matrix
    glLightfv(GL_LIGHT0, GL_DIFFUSE, d);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, a);
    if (useShader()) {
        V3 e = {gView.m[0] * dir.x + gView.m[4] * dir.y + gView.m[8] * dir.z,
                gView.m[1] * dir.x + gView.m[5] * dir.y + gView.m[9] * dir.z,
                gView.m[2] * dir.x + gView.m[6] * dir.y + gView.m[10] * dir.z};
        pUniform3f(U.lightDir, e.x, e.y, e.z);
        pUniform3f(U.lightCol, diffuse.r * 0.95f, diffuse.g * 0.95f, diffuse.b * 0.95f);
        pUniform3f(U.skyAmb, ambient.r * 1.0f, ambient.g * 1.02f, ambient.b * 1.12f);
        pUniform3f(U.groundAmb, ambient.r * 0.62f, ambient.g * 0.55f, ambient.b * 0.45f);
        pUniformMatrix4fv(U.lightMat, 1, 0, gLightVP.m);
        pUniform1i(U.shadow, 0);
        pUniform1i(U.useShadow, 1);
        pUniform1f(U.texel, 1.0f / SHADOW_SIZE);
        glBindTexture(GL_TEXTURE_2D, gShadowTex);
    }
}
void setFog(Col c, float start, float end) {
    if (shadowPass()) return;
    float fc[4] = {c.r, c.g, c.b, 1};
    glFogi(GL_FOG_MODE, GL_LINEAR); glFogfv(GL_FOG_COLOR, fc);
    glFogf(GL_FOG_START, start); glFogf(GL_FOG_END, end);
    if (useShader()) {
        glDisable(GL_FOG);
        pUniform1i(U.fog, 1); pUniform3f(U.fogCol, c.r, c.g, c.b); pUniform2f(U.fogRange, start, end);
    } else glEnable(GL_FOG);
}
void noFog() { glDisable(GL_FOG); if (shaderActive()) pUniform1i(U.fog, 0); }

void begin2D() {
    if (shadowPass()) return;
    if (gShaderOK) pUseProgram(0);
    glDisable(GL_DEPTH_TEST); glDisable(GL_LIGHTING); glDisable(GL_FOG);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, W, 0, H, -1, 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
}
bool project(V3 p, float& sx, float& sy) {
    if (shadowPass()) return false;
    float mv[16], pr[16], e[4], c[4];
    glGetFloatv(GL_MODELVIEW_MATRIX, mv);
    glGetFloatv(GL_PROJECTION_MATRIX, pr);
    for (int i = 0; i < 4; i++) e[i] = mv[i] * p.x + mv[4 + i] * p.y + mv[8 + i] * p.z + mv[12 + i];
    for (int i = 0; i < 4; i++) c[i] = pr[i] * e[0] + pr[4 + i] * e[1] + pr[8 + i] * e[2] + pr[12 + i] * e[3];
    if (c[3] <= 0) return false;
    sx = (c[0] / c[3] + 1) * 0.5f * W;
    sy = (c[1] / c[3] + 1) * 0.5f * H;
    return true;
}

// ---- 2D overlay helpers (skipped during the shadow pass) ------------------------
#define SKIP2D if (shadowPass()) return
void rect(float x, float y, float w, float h) {
    SKIP2D;
    glBegin(GL_QUADS); glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h); glEnd();
}
void gradRect(float x, float y, float w, float h, Col bot, Col top) {
    SKIP2D;
    glBegin(GL_QUADS);
    col(bot); glVertex2f(x, y); glVertex2f(x + w, y);
    col(top); glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
}
void disc2(float cx, float cy, float rx, float ry) {
    SKIP2D;
    glBegin(GL_TRIANGLE_FAN); glVertex2f(cx, cy);
    for (int i = 0; i <= 40; i++) { float a = 2 * PI * i / 40; glVertex2f(cx + rx * cosf(a), cy + ry * sinf(a)); }
    glEnd();
}
void arc2(float cx, float cy, float r, float a0, float a1, float w) {
    SKIP2D;
    glLineWidth(w);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= 20; i++) { float a = rad(a0 + (a1 - a0) * i / 20); glVertex2f(cx + r * cosf(a), cy + r * sinf(a)); }
    glEnd();
}
void text(float x, float y, const char* s, void* font = GLUT_BITMAP_HELVETICA_18) {
    SKIP2D;
    glRasterPos2f(x, y);
    for (const char* c = s; *c; c++) glutBitmapCharacter(font, *c);
}
float textW(const char* s, void* font) {
    float w = 0;
    for (const char* c = s; *c; c++) w += glutBitmapWidth(font, *c);
    return w;
}
void textC(float cx, float y, const char* s, void* font = GLUT_BITMAP_HELVETICA_18) { text(cx - textW(s, font) / 2, y, s, font); }
void text3(float x, float y, float z, const char* s, void* font = GLUT_BITMAP_HELVETICA_18) {
    SKIP2D;
    unlit();
    glRasterPos3f(x, y, z);
    for (const char* c = s; *c; c++) glutBitmapCharacter(font, *c);
    lit();
}
void roundRect(float x, float y, float w, float h, float r) {
    SKIP2D;
    rect(x + r, y, w - 2 * r, h); rect(x, y + r, w, h - 2 * r);
    disc2(x + r, y + r, r, r); disc2(x + w - r, y + r, r, r); disc2(x + r, y + h - r, r, r); disc2(x + w - r, y + h - r, r, r);
}
void bubble(float x, float y, const char* s, bool leftSide = false) {
    SKIP2D;
    void* f = GLUT_BITMAP_HELVETICA_18;
    float w = textW(s, f) + 28, h = 36;
    float bx = leftSide ? x - w + 24 : x - 24, by = y + 18;
    col(0, 0, 0, 0.22f); roundRect(bx + 3, by - 4, w, h, 14);
    col(1, 1, 1, 0.96f); roundRect(bx, by, w, h, 14);
    glBegin(GL_TRIANGLES);
    glVertex2f(x, y); glVertex2f(x + (leftSide ? -6 : 6), by + 2); glVertex2f(x + (leftSide ? -22 : 22), by + 2);
    glEnd();
    col(0.12f, 0.12f, 0.15f); text(bx + 14, by + 12, s, f);
}
void bubble3(V3 p, const char* s, bool leftSide = false) {
    float x, y;
    if (project(p, x, y)) { begin2D(); bubble(x, y, s, leftSide); }
}
void sky2D(Col bottom, Col top) {
    begin2D();
    SKIP2D;
    Col mid = mix(bottom, top, 0.45f);
    gradRect(0, 0, W, 330, bottom, mid);
    gradRect(0, 330, W, H - 330, mid, top);
}
void sun2D(float x, float y, float r, Col c) {
    SKIP2D;
    for (int i = 6; i >= 1; i--) { col(c, 0.05f); disc2(x, y, r * (1 + i * 0.55f), r * (1 + i * 0.55f)); }
    col(c); disc2(x, y, r, r);
    col(1, 1, 0.92f, 0.6f); disc2(x, y, r * 0.75f, r * 0.75f);
}
void stars2D(float a, float t, float seed) {
    SKIP2D;
    for (int i = 0; i < 90 && a > 0; i++) {
        float x = fmodf(i * 137.5f + seed, W), y = 300 + fmodf(i * 71.3f + seed, 300);
        col(1, 1, 1, a * (0.5f + 0.5f * sinf(t * 3 + i)));
        disc2(x, y, 1.1f + (i % 3) * 0.5f, 1.1f + (i % 3) * 0.5f);
    }
}
void cloud2D(float x, float y, float s, Col c, float a = 0.95f) {
    SKIP2D;
    Col sh = scale(c, 0.86f);
    col(sh, a); disc2(x + 4 * s, y - 6 * s, 70 * s, 18 * s);
    col(c, a);
    disc2(x, y, 45 * s, 24 * s); disc2(x + 38 * s, y + 8 * s, 34 * s, 26 * s);
    disc2(x - 38 * s, y + 2 * s, 30 * s, 19 * s); disc2(x + 8 * s, y + 22 * s, 32 * s, 24 * s);
}
void bird2D(float x, float y, float flap) {
    SKIP2D;
    col(0.15f, 0.15f, 0.2f); glLineWidth(2);
    glBegin(GL_LINE_STRIP); glVertex2f(x - 9, y + 4 * flap); glVertex2f(x, y); glVertex2f(x + 9, y + 4 * flap); glEnd();
}
void glow2D(float x, float y, float r, Col c, float a) {
    SKIP2D;
    for (int i = 4; i >= 1; i--) { col(c, a * 0.09f * (5 - i)); disc2(x, y, r * i, r * i); }
}
void blobShadow(float x, float z, float r, float a = 0.28f) {   // only used without shaders
    if (shadowPass() || useShader()) return;
    unlit();
    glDepthMask(GL_FALSE);
    col(0, 0, 0, a);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(x, 0.012f, z);
    for (int i = 0; i <= 24; i++) { float b = 2 * PI * i / 24; glVertex3f(x + r * cosf(b), 0.012f, z + r * 0.75f * sinf(b)); }
    glEnd();
    glDepthMask(GL_TRUE);
    lit();
}

// ===========================================================================
// Scenery
// ===========================================================================
const Col GRASS = {0.40f, 0.66f, 0.28f}, DIRT = {0.70f, 0.55f, 0.36f};

void groundPatch(float x0, float z0, float x1, float z1, Col c, float y = 0.006f, float cell = 1.0f) {
    col(c);
    int nx = (int)ceilf((x1 - x0) / cell), nz = (int)ceilf((z1 - z0) / cell);
    float dx = (x1 - x0) / nx, dz = (z1 - z0) / nz;
    glNormal3f(0, 1, 0);
    for (int j = 0; j < nz; j++) {
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= nx; i++) { glVertex3f(x0 + i * dx, y, z0 + (j + 1) * dz); glVertex3f(x0 + i * dx, y, z0 + j * dz); }
        glEnd();
    }
}
void ground(Col c, int m, float size = 90) { setMat(m); groundPatch(-size, -size, size, size, c, 0, 3.0f); setMat(M_PLAIN); }
void hills() {
    setMat(M_GRASS);
    col(0.30f, 0.52f, 0.26f);
    sph(-25, -4, -45, 30, 12, 14); sph(15, -5, -50, 34, 14, 14); sph(50, -4, -42, 25, 11, 12);
    col(0.38f, 0.60f, 0.30f);
    sph(-5, -6, -32, 26, 9, 10); sph(32, -6, -30, 20, 8, 9);
    setMat(M_PLAIN);
}
void tree(float x, float z, float s) {
    setMat(M_WOOD);
    col(0.40f, 0.26f, 0.15f);
    glPushMatrix(); glTranslatef(x, 0, z); frustum(0.2f * s, 0.12f * s, 1.9f * s); glPopMatrix();
    cylBetween({x, 1.4f * s, z}, {x + 0.5f * s, 1.95f * s, z + 0.1f * s}, 0.07f * s);
    setMat(M_LEAF, x * 3.1f);
    col(0.17f, 0.45f, 0.18f); sph(x, 2.3f * s, z, 1.0f * s, 0.9f * s, 1.0f * s);
    col(0.21f, 0.52f, 0.21f); sph(x - 0.6f * s, 1.9f * s, z + 0.25f * s, 0.72f * s); sph(x + 0.6f * s, 2.0f * s, z - 0.1f * s, 0.75f * s);
    col(0.26f, 0.58f, 0.25f); sph(x + 0.15f * s, 2.95f * s, z + 0.15f * s, 0.62f * s); sph(x - 0.2f * s, 2.4f * s, z + 0.7f * s, 0.55f * s);
    setMat(M_PLAIN);
    blobShadow(x, z, 1.1f * s);
}
void fenceX(float x0, float x1, float z, float sp, Col c) {
    setMat(M_WOOD);
    col(c);
    box((x0 + x1) / 2, 0.45f, z, x1 - x0, 0.09f, 0.05f);
    box((x0 + x1) / 2, 0.85f, z, x1 - x0, 0.09f, 0.05f);
    col(scale(c, 0.85f));
    for (float x = x0; x <= x1 + 0.01f; x += sp) {
        box(x, 0.55f, z, 0.12f, 1.1f, 0.12f);
        glPushMatrix(); glTranslatef(x, 1.1f, z); glRotatef(45, 0, 1, 0); glScalef(1, 1, 1);
        frustum(0.085f, 0.0f, 0.08f); glPopMatrix();
    }
    setMat(M_PLAIN);
}
void house(float x, float z, bool lightsOn, float doorA = 0) {
    setMat(M_PLASTER);
    col(0.95f, 0.87f, 0.68f); box(x, 1.3f, z, 4, 2.6f, 3);
    setMat(M_ROOF);
    col(0.70f, 0.22f, 0.17f); roof(x - 2.3f, x + 2.3f, z - 1.75f, z + 1.75f, 2.6f, 4.0f);
    setMat(M_PLASTER);
    col(0.60f, 0.32f, 0.25f); box(x + 1.1f, 3.6f, z - 0.5f, 0.45f, 1.3f, 0.45f);
    col(0.85f, 0.78f, 0.62f); box(x, 0.12f, z, 4.08f, 0.24f, 3.08f);                // plinth
    if (doorA > 0.5f) {                                                             // doorway
        if (lightsOn) { unlit(); col(1.0f, 0.80f, 0.45f); } else col(0.10f, 0.07f, 0.05f);
        box(x, 0.98f, z + 1.505f, 0.85f, 1.9f, 0.01f);
        lit();
    }
    glPushMatrix(); glTranslatef(x - 0.425f, 0, z + 1.51f); glRotatef(doorA, 0, 1, 0);   // hinged door
    setMat(M_WOODWALL);
    col(0.50f, 0.31f, 0.17f); box(0.425f, 0.98f, 0.0f, 0.85f, 1.9f, 0.06f);
    setMat(M_PLAIN);
    col(0.9f, 0.78f, 0.3f); setSpec(0.8f); sph(0.725f, 0.95f, 0.05f, 0.05f); setSpec(0.12f);
    glPopMatrix();
    col(0.45f, 0.42f, 0.38f); box(x, 0.06f, z + 1.75f, 1.1f, 0.12f, 0.45f);        // step
    for (int i = -1; i <= 1; i += 2) {
        float wx = x + i * 1.35f;
        if (lightsOn) { unlit(); col(1.0f, 0.83f, 0.42f); box(wx, 1.55f, z + 1.51f, 0.8f, 0.75f, 0.04f); lit(); }
        else { col(0.45f, 0.62f, 0.78f); setSpec(0.9f); box(wx, 1.55f, z + 1.51f, 0.8f, 0.75f, 0.04f); setSpec(0.12f); }
        col(0.95f, 0.95f, 0.92f);
        box(wx, 1.55f, z + 1.54f, 0.06f, 0.8f, 0.04f); box(wx, 1.55f, z + 1.54f, 0.85f, 0.06f, 0.04f);
        box(wx, 1.15f, z + 1.56f, 0.95f, 0.06f, 0.1f);
        col(0.2f, 0.45f, 0.2f); box(wx - 0.62f, 1.55f, z + 1.53f, 0.22f, 0.8f, 0.03f); box(wx + 0.62f, 1.55f, z + 1.53f, 0.22f, 0.8f, 0.03f);
    }
}
void barn(float x, float z, float doorOpen) {
    setMat(M_WOODWALL);
    col(0.72f, 0.18f, 0.14f); box(x, 1.5f, z, 5, 3, 4);
    setMat(M_ROOF);
    col(0.40f, 0.36f, 0.34f); roof(x - 2.7f, x + 2.7f, z - 2.3f, z + 2.3f, 3.0f, 4.6f);
    setMat(M_PLAIN);
    col(0.96f, 0.95f, 0.92f);
    box(x, 3.0f, z + 2.02f, 5.1f, 0.12f, 0.06f);
    box(x - 2.5f, 1.5f, z + 2.02f, 0.12f, 3, 0.06f); box(x + 2.5f, 1.5f, z + 2.02f, 0.12f, 3, 0.06f);
    col(0.10f, 0.06f, 0.04f); box(x, 1.05f, z + 2.01f, 1.7f, 2.1f, 0.04f);
    float dx = doorOpen * 1.75f;
    setMat(M_WOODWALL);
    col(0.80f, 0.21f, 0.16f); box(x + dx, 1.05f, z + 2.08f, 1.7f, 2.1f, 0.06f);
    setMat(M_PLAIN);
    col(0.96f, 0.95f, 0.92f);
    glPushMatrix(); glTranslatef(x + dx, 1.05f, z + 2.12f);
    glPushMatrix(); glRotatef(51, 0, 0, 1); box(0, 0, 0, 2.6f, 0.1f, 0.03f); glPopMatrix();
    glPushMatrix(); glRotatef(-51, 0, 0, 1); box(0, 0, 0, 2.6f, 0.1f, 0.03f); glPopMatrix();
    glPopMatrix();
    setMat(M_STRAW); col(0.9f, 0.78f, 0.4f);                                       // hay bales outside
    box(x + 3.1f, 0.3f, z + 1.2f, 0.9f, 0.6f, 0.6f); box(x + 3.0f, 0.9f, z + 1.25f, 0.9f, 0.6f, 0.6f);
    box(x + 3.15f, 0.3f, z + 0.5f, 0.9f, 0.6f, 0.6f);
    setMat(M_PLAIN);
}
void farmWorld(bool lightsOn, float barnDoor, float houseDoor = 0) {
    ground(GRASS, M_GRASS);
    setMat(M_DIRT);
    groundPatch(-14, 0.6f, 14, 2.0f, DIRT);
    groundPatch(6.2f, -3.0f, 7.8f, 0.6f, DIRT);
    groundPatch(-8.6f, -2.6f, -7.4f, 0.6f, DIRT);
    setMat(M_PLAIN);
    glCallList(LIST_GRASS);
    hills();
    house(-8, -4, lightsOn, houseDoor);
    barn(7, -5, barnDoor);
    tree(-3, -6.5f, 1.0f); tree(2.5f, -8.5f, 1.2f); tree(12, -2.5f, 0.9f); tree(-13, -1.5f, 1.1f);
    tree(-1.5f, -14, 1.3f); tree(9, -12, 1.2f); tree(-18, -8, 1.4f); tree(18, -7, 1.3f);
    fenceX(-20, 20, -9.5f, 1.6f, {0.64f, 0.47f, 0.29f});
}

// ===========================================================================
// Characters
// ===========================================================================
enum Hold { H_NONE, H_PAIL, H_BOTTLE, H_SPONGE, H_NOTE, H_JUG, H_SPOON };
enum Hair { HAIR_SHORT, HAIR_AFRO, HAIR_WRAP };
struct Person {
    Col skin, shirt, pants, shoes, hairCol, accent;   // accent: apron / head-wrap colour
    int hair; bool apron, dress, boots, gloves;
};
struct Arm { float pitch, roll, elbow = 0; };   // pitch: forward swing, roll: sideways, elbow: forearm bend
struct Pose {
    float walk = 0; bool moving = false;
    Arm L = {0, 8, 8}, R = {0, -8, 8};
    int holdL = H_NONE, holdR = H_NONE;
    float fill = 0, tilt = 0;
    bool sit = false; float sitAngle = 90, knee = -1;      // sitting: thigh angle and knee bend
    bool legIK = false; float legP[2] = {0, 0}, legK[2] = {0, 0};   // explicit leg angles (cycling)
    float lean = 0;                                          // + forward, - backward (lying down)
    bool apron = true;
    float smile = 1, mouth = 0;
};
float hipOf(const Pose& q) { return q.sit ? 0.45f + 0.45f * cosf(rad(q.sitAngle)) : 0.9f; }
float angLerp(float a, float b, float u) { float d = fmodf(b - a + 540.0f, 360.0f) - 180.0f; return a + d * u; }
// Two-bone leg IK in the body's side plane: target (forward, up) relative to the hip
void legIK(float tz, float ty, float& pitch, float& knee) {
    const float L1 = 0.45f, L2 = 0.43f;
    float d = clampf(sqrtf(tz * tz + ty * ty), 0.2f, L1 + L2 - 0.005f);
    float phi = deg(atan2f(tz, -ty));
    float a = deg(acosf(clampf((L1 * L1 + d * d - L2 * L2) / (2 * L1 * d), -1, 1)));
    float g = deg(acosf(clampf((L1 * L1 + L2 * L2 - d * d) / (2 * L1 * L2), -1, 1)));
    pitch = phi + a;
    knee = 180 - g;
}
struct Place { V3 pos; float heading; float scale; };

// The farmer: modelled after the reference (afro, red shirt, green apron, yellow gloves & boots)
const Person FARMER = {{0.50f, 0.31f, 0.20f}, {0.80f, 0.17f, 0.16f}, {0.22f, 0.32f, 0.55f},
                       {0.98f, 0.76f, 0.10f}, {0.07f, 0.05f, 0.04f}, {0.16f, 0.40f, 0.31f},
                       HAIR_AFRO, true, false, true, true};
const Person CUSTOMER = {{0.40f, 0.25f, 0.16f}, {0.55f, 0.24f, 0.62f}, {0.40f, 0.25f, 0.16f},
                         {0.25f, 0.12f, 0.10f}, {0.08f, 0.06f, 0.05f}, {0.96f, 0.55f, 0.15f},
                         HAIR_WRAP, false, true, false, false};
const Person SHOPPER = {{0.33f, 0.21f, 0.13f}, {0.16f, 0.52f, 0.50f}, {0.70f, 0.60f, 0.42f},
                        {0.30f, 0.18f, 0.10f}, {0.06f, 0.05f, 0.04f}, {0, 0, 0},
                        HAIR_SHORT, false, false, false, false};

std::vector<V3> gAfro;
void buildAfro() {
    const int N = 70;
    for (int i = 0; i < N; i++) {
        float y = 1 - 2 * (i + 0.5f) / N, r = sqrtf(1 - y * y), a = i * 2.39996f;
        V3 d = {r * cosf(a), y, r * sinf(a)};
        if (d.z > 0.25f && d.y < 0.55f) continue;     // keep the face free
        if (d.y < -0.45f) continue;
        gAfro.push_back(d);
    }
}

void pail(float fill, float s = 1) {
    glPushMatrix(); glScalef(s, s, s);
    setSpec(0.9f);
    col(0.70f, 0.72f, 0.76f); frustum(0.16f, 0.20f, 0.38f, false);
    col(0.55f, 0.57f, 0.60f);
    glPushMatrix(); glTranslatef(0, 0.1f, 0); frustum(0.177f, 0.179f, 0.03f, false); glPopMatrix();
    glPushMatrix(); glTranslatef(0, 0.38f, 0); torus(0.2f, 0.012f, 24, 6); glPopMatrix();
    float fy = 0.02f + 0.34f * fill;
    col(fill > 0.01f ? Col{0.98f, 0.98f, 0.95f} : Col{0.38f, 0.39f, 0.41f});
    setSpec(fill > 0.01f ? 0.4f : 0.6f);
    glPushMatrix(); glTranslatef(0, fy, 0); glScalef(0.16f + 0.04f * fy / 0.38f, 0.001f, 0.16f + 0.04f * fy / 0.38f);
    glCallList(LIST_CYL); glPopMatrix();
    col(0.4f, 0.4f, 0.43f); setSpec(0.8f);
    glPushMatrix(); glTranslatef(0, 0.38f, 0); glRotatef(90, 1, 0, 0);
    for (int i = 0; i < 12; i++) {
        float a0 = PI * i / 12, a1 = PI * (i + 1) / 12;
        cylBetween({0.2f * cosf(a0), 0, -0.2f * sinf(a0)}, {0.2f * cosf(a1), 0, -0.2f * sinf(a1)}, 0.008f);
    }
    glPopMatrix();
    setSpec(0.12f);
    glPopMatrix();
}
void bottle(float fill, bool lid, float s = 1) {
    glPushMatrix(); glScalef(s, s, s);
    if (fill > 0) { col(0.99f, 0.99f, 0.96f); cyl(0, 0.005f, 0, 0.072f, 0.235f * clampf(fill, 0, 1)); }
    if (fill > 0.98f) cyl(0, 0.24f, 0, 0.03f, 0.06f);
    col(0.95f, 0.80f, 0.28f); cyl(0, 0.07f, 0, 0.081f, 0.07f);
    col(0.2f, 0.45f, 0.75f); cyl(0, 0.085f, 0, 0.082f, 0.012f);
    if (lid) { col(0.15f, 0.35f, 0.8f); setSpec(0.6f); cyl(0, 0.315f, 0, 0.042f, 0.035f); }
    if (!shadowPass()) {
        glDepthMask(GL_FALSE);
        setSpec(1.0f);
        col(0.82f, 0.92f, 1.0f, 0.32f);
        cyl(0, 0, 0, 0.08f, 0.24f);
        glPushMatrix(); glTranslatef(0, 0.24f, 0); frustum(0.08f, 0.036f, 0.04f, false); glPopMatrix();
        cyl(0, 0.27f, 0, 0.036f, 0.05f);
        glDepthMask(GL_TRUE);
    }
    setSpec(0.12f);
    glPopMatrix();
}
const V3 JUG_SPOUT = {0.30f, 0.07f, 0};
void heldObject(int h, float fill, float tilt) {
    glPushMatrix();
    switch (h) {
    case H_PAIL: glTranslatef(0, -0.42f, 0); pail(fill, 0.85f); break;
    case H_BOTTLE: glTranslatef(0, -0.2f, 0); bottle(1, true, 0.85f); break;
    case H_SPONGE: col(0.98f, 0.86f, 0.2f); setMat(M_HAIR); box(0, -0.02f, 0.03f, 0.13f, 0.08f, 0.1f); setMat(M_PLAIN); break;
    case H_NOTE:
        col(0.32f, 0.62f, 0.38f); box(0, 0, 0.05f, 0.18f, 0.09f, 0.01f);
        col(0.18f, 0.42f, 0.22f); sph(0, 0, 0.056f, 0.025f, 0.025f, 0.005f);
        break;
    case H_JUG:
        glRotatef(-tilt, 0, 0, 1);
        setSpec(0.7f);
        col(0.15f, 0.35f, 0.75f); box(0, 0, 0, 0.04f, 0.18f, 0.04f);
        col(0.95f, 0.96f, 0.98f); cyl(0.13f, -0.17f, 0, 0.1f, 0.27f);
        col(0.15f, 0.35f, 0.75f); cyl(0.13f, 0.06f, 0, 0.103f, 0.03f);
        col(0.95f, 0.96f, 0.98f);
        glPushMatrix(); glTranslatef(0.21f, 0.07f, 0); glRotatef(-90, 0, 0, 1); frustum(0.045f, 0.018f, 0.09f); glPopMatrix();
        setSpec(0.12f);
        break;
    case H_SPOON:                                         // tilt = spoon pitch (+ down/forward, - up/back)
        glRotatef(tilt, 1, 0, 0);
        col(0.82f, 0.83f, 0.86f); setSpec(1.0f);
        cylBetween({0, 0, -0.03f}, {0, 0, 0.13f}, 0.006f);
        sph(0, 0.0f, 0.16f, 0.024f, 0.008f, 0.034f);
        setSpec(0.12f);
        if (fill > 0) { col(0.96f, 0.94f, 0.86f); sph(0, 0.01f, 0.16f, 0.018f, 0.012f, 0.024f); }
        break;
    }
    glPopMatrix();
}

// Arm: shoulder -> upper arm (0.3) -> elbow -> forearm + hand (0.32)
void drawArm(float sx, float sy, Arm a, const Person& P, int hold, float fill, float tilt, float lean) {
    glPushMatrix();
    glTranslatef(sx, sy, 0);
    glRotatef(-a.pitch, 1, 0, 0);
    glRotatef(a.roll, 0, 0, 1);
    setMat(M_FABRIC, 2);
    col(P.shirt); sph(0, 0, 0, 0.075f); cyl(0, -0.27f, 0, 0.064f, 0.27f);
    col(scale(P.shirt, 0.9f)); cyl(0, -0.31f, 0, 0.07f, 0.06f);            // rolled sleeve cuff
    setMat(M_PLAIN);
    glTranslatef(0, -0.3f, 0);                                              // elbow
    col(P.skin); sph(0, 0, 0, 0.052f);
    glRotatef(-a.elbow, 1, 0, 0);
    cyl(0, -0.2f, 0, 0.048f, 0.2f);
    if (P.gloves) {
        col(0.98f, 0.80f, 0.12f); setSpec(0.5f);
        glPushMatrix(); glTranslatef(0, -0.26f, 0); frustum(0.052f, 0.064f, 0.08f, false); glPopMatrix();
        sph(0, -0.3f, 0.01f, 0.058f, 0.068f, 0.048f); sph(0.035f, -0.275f, 0.04f, 0.024f);
        setSpec(0.12f);
    } else {
        col(P.skin); sph(0, -0.29f, 0.01f, 0.048f, 0.06f, 0.04f);
    }
    if (hold != H_NONE) {
        glTranslatef(0, -0.32f, 0);
        glRotatef(a.elbow, 1, 0, 0);                                        // undo all arm/body rotations
        glRotatef(-a.roll, 0, 0, 1);                                        // so the object stays upright
        glRotatef(a.pitch, 1, 0, 0);
        glRotatef(-lean, 1, 0, 0);
        heldObject(hold, fill, tilt);
    }
    glPopMatrix();
}
void drawLeg(float x, float hipY, float pitch, float knee, const Person& P) {
    glPushMatrix();
    glTranslatef(x, hipY, 0);
    glRotatef(-pitch, 1, 0, 0);
    Col legC = P.dress ? P.skin : P.pants;
    setMat(M_FABRIC, 5);
    col(legC); glPushMatrix(); glTranslatef(0, -0.45f, 0); frustum(0.07f, 0.09f, 0.45f); glPopMatrix();
    sph(0, -0.45f, 0, 0.072f);
    setMat(M_PLAIN);
    glTranslatef(0, -0.45f, 0);
    glRotatef(knee, 1, 0, 0);
    if (P.boots) {
        col(P.shoes); setSpec(0.55f);
        glPushMatrix(); glTranslatef(0, -0.4f, 0); frustum(0.075f, 0.088f, 0.36f, true); glPopMatrix();
        glPushMatrix(); glTranslatef(0, -0.06f, 0); frustum(0.09f, 0.095f, 0.04f, true); glPopMatrix();
        sph(0, -0.4f, 0.06f, 0.08f, 0.06f, 0.15f);
        col(0.35f, 0.25f, 0.1f); box(0, -0.445f, 0.05f, 0.15f, 0.03f, 0.29f);
        setSpec(0.12f);
    } else {
        col(legC); setMat(M_FABRIC, 5); cyl(0, -0.4f, 0, 0.065f, 0.4f); setMat(M_PLAIN);
        col(P.shoes); setSpec(0.4f); sph(0, -0.41f, 0.05f, 0.07f, 0.05f, 0.13f); setSpec(0.12f);
    }
    glPopMatrix();
}
// apron skirt: part of a widening ring in front of the legs
void apronSkirt(float yTop, float yBot, float rTop, float rBot, float span) {
    const int S = 16;
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= S; i++) {
        float a = rad(-span + 2 * span * i / S);
        V3 n = norm({sinf(a), 0.15f, cosf(a)});
        glNormal3f(n.x, n.y, n.z);
        glVertex3f(rTop * sinf(a), yTop, rTop * cosf(a) * 0.82f);
        glVertex3f(rBot * sinf(a), yBot, rBot * cosf(a) * 0.82f);
    }
    glEnd();
}
void drawHead(const Person& P, float hy, const Pose& q) {
    col(P.skin);
    sph(0, hy, 0, 0.142f, 0.155f, 0.145f);
    sph(0, hy - 0.06f, 0.02f, 0.11f, 0.095f, 0.115f);                              // jaw
    sph(0.143f, hy, -0.005f, 0.022f, 0.038f, 0.028f); sph(-0.143f, hy, -0.005f, 0.022f, 0.038f, 0.028f);
    col(scale(P.skin, 0.92f)); sph(0, hy - 0.022f, 0.147f, 0.027f, 0.023f, 0.022f);  // nose
    for (int s = -1; s <= 1; s += 2) {
        float ex = s * 0.055f;
        col(0.98f, 0.97f, 0.95f); setSpec(0.9f); sph(ex, hy + 0.022f, 0.118f, 0.033f, 0.04f, 0.026f);
        col(0.36f, 0.20f, 0.08f); sph(ex, hy + 0.017f, 0.138f, 0.022f);
        col(0.02f, 0.02f, 0.02f); sph(ex, hy + 0.017f, 0.153f, 0.011f);
        setSpec(0.12f);
        unlit(); col(1, 1, 1); sph(ex + 0.008f, hy + 0.027f, 0.162f, 0.005f); lit();
        col(0.06f, 0.04f, 0.03f);                                                   // eyebrow
        glPushMatrix(); glTranslatef(ex, hy + 0.073f, 0.13f); glRotatef(s * -12.0f, 0, 0, 1); glRotatef(-15, 1, 0, 0);
        box(0, 0, 0, 0.058f, 0.013f, 0.016f); glPopMatrix();
        col(mix(P.skin, {0.85f, 0.35f, 0.35f}, 0.25f)); sph(s * 0.085f, hy - 0.03f, 0.115f, 0.03f, 0.018f, 0.015f);
    }
    // smile: dark mouth with teeth
    col(0.42f, 0.10f, 0.10f); sph(0, hy - 0.066f, 0.122f, 0.046f, 0.012f + 0.01f * q.smile + 0.022f * q.mouth, 0.02f);
    col(0.98f, 0.98f, 0.96f); sph(0, hy - 0.058f, 0.132f, 0.036f, 0.007f, 0.012f);
    col(mix(P.skin, {0.6f, 0.2f, 0.2f}, 0.45f)); sph(0, hy - 0.08f, 0.124f, 0.03f, 0.008f, 0.014f);
    // hair
    if (P.hair == HAIR_AFRO) {
        setMat(M_HAIR);
        col(P.hairCol);
        sph(0, hy + 0.06f, -0.035f, 0.19f, 0.17f, 0.17f);
        for (size_t i = 0; i < gAfro.size(); i++) {
            V3 d = gAfro[i];
            float r = 0.075f + 0.02f * ((i * 37) % 7) / 6.0f;
            sph(d.x * 0.2f, hy + 0.07f + d.y * 0.18f, -0.03f + d.z * 0.19f, r);
        }
        setMat(M_PLAIN);
    } else if (P.hair == HAIR_SHORT) {
        setMat(M_HAIR); col(P.hairCol);
        sph(0, hy + 0.045f, -0.03f, 0.152f, 0.13f, 0.148f);
        setMat(M_PLAIN);
    } else {
        setMat(M_FABRIC, 9); col(P.accent);
        sph(0, hy + 0.07f, -0.025f, 0.168f, 0.145f, 0.168f);
        sph(0, hy + 0.2f, 0.03f, 0.075f, 0.055f, 0.07f);
        col(scale(P.accent, 0.85f)); box(0, hy + 0.045f, 0.0f, 0.34f, 0.04f, 0.3f);
        setMat(M_PLAIN);
    }
}
void drawPerson(const Person& P, Place pl, const Pose& q) {
    glPushMatrix();
    glTranslatef(pl.pos.x, pl.pos.y, pl.pos.z);
    glRotatef(pl.heading, 0, 1, 0);
    glScalef(pl.scale, pl.scale, pl.scale);

    float hipY = hipOf(q);
    float sw = q.moving ? 24 * sinf(q.walk) : 0;
    float bob = q.moving ? 0.02f * fabsf(cosf(q.walk)) : 0;
    glTranslatef(0, bob, 0);
    if (q.legIK) {
        drawLeg(0.1f, hipY, q.legP[0], q.legK[0], P); drawLeg(-0.1f, hipY, q.legP[1], q.legK[1], P);
    } else if (q.sit) {
        float k = q.knee < 0 ? q.sitAngle : q.knee;
        drawLeg(0.1f, hipY, q.sitAngle, k, P); drawLeg(-0.1f, hipY, q.sitAngle, k, P);
    } else {
        drawLeg(0.1f, hipY, sw, sw < 0 ? -sw * 0.9f : 0, P);
        drawLeg(-0.1f, hipY, -sw, sw > 0 ? sw * 0.9f : 0, P);
    }
    // hips
    setMat(M_FABRIC, 5);
    col(P.dress ? P.shirt : P.pants); sph(0, hipY + 0.02f, 0, 0.17f, 0.12f, 0.125f);
    setMat(M_PLAIN);

    glPushMatrix();
    glTranslatef(0, hipY, 0); glRotatef(q.lean, 1, 0, 0); glTranslatef(0, -hipY, 0);
    setMat(M_FABRIC, 1);
    if (P.dress) {
        col(P.shirt);
        glPushMatrix(); glTranslatef(0, hipY - 0.52f, 0); frustum(0.31f, 0.17f, 0.66f); glPopMatrix();
    }
    col(P.shirt);
    sph(0, hipY + 0.15f, 0, 0.15f, 0.13f, 0.115f);                                 // waist
    sph(0, hipY + 0.36f, 0, 0.19f, 0.2f, 0.13f);                                   // chest
    sph(0, hipY + 0.5f, 0, 0.24f, 0.075f, 0.12f);                                  // shoulders
    col(scale(P.shirt, 0.9f));                                                     // collar
    glPushMatrix(); glTranslatef(0.045f, hipY + 0.575f, 0.07f); glRotatef(35, 0, 0, 1); box(0, 0, 0, 0.08f, 0.03f, 0.05f); glPopMatrix();
    glPushMatrix(); glTranslatef(-0.045f, hipY + 0.575f, 0.07f); glRotatef(-35, 0, 0, 1); box(0, 0, 0, 0.08f, 0.03f, 0.05f); glPopMatrix();
    if (P.apron && q.apron) {
        setMat(M_FABRIC, 7);
        col(P.accent);
        sph(0, hipY + 0.33f, 0.072f, 0.135f, 0.16f, 0.07f);                        // bib
        glPushMatrix(); glTranslatef(0, hipY + 0.08f, 0); glScalef(1, 1, 0.8f); cyl(0, 0, 0, 0.163f, 0.075f); glPopMatrix();
        if (q.sit || q.legIK) {                                                 // drapes over the lap
            float th = q.legIK ? 0.5f * (q.legP[0] + q.legP[1]) : q.sitAngle;
            glPushMatrix(); glTranslatef(0, hipY + 0.05f, 0); glRotatef(-th, 1, 0, 0);
            box(0, -0.22f, 0.11f, 0.36f, 0.44f, 0.025f);
            glPopMatrix();
        } else apronSkirt(hipY + 0.1f, 0.33f, 0.175f, 0.27f, 95);
        col(scale(P.accent, 0.85f));
        sph(0.11f, hipY + 0.47f, 0.0f, 0.03f, 0.025f, 0.125f); sph(-0.11f, hipY + 0.47f, 0.0f, 0.03f, 0.025f, 0.125f);
        box(0, hipY + 0.2f, 0.12f, 0.13f, 0.1f, 0.02f);                            // pocket
    }
    setMat(M_PLAIN);
    float shY = hipY + 0.5f;
    Arm aL = q.L, aR = q.R;
    aL.pitch -= sw * 0.8f; aR.pitch += sw * 0.8f;
    drawArm(0.24f, shY, aL, P, q.holdL, q.fill, q.tilt, q.lean);
    drawArm(-0.24f, shY, aR, P, q.holdR, q.fill, q.tilt, q.lean);
    col(P.skin); cyl(0, hipY + 0.55f, 0, 0.055f, 0.12f);
    drawHead(P, hipY + 0.8f, q);
    glPopMatrix();
    glPopMatrix();
    blobShadow(pl.pos.x, pl.pos.z, 0.35f * pl.scale);
}
V3 shoulderLocal(const Pose& q, int side) { return {side * 0.24f, hipOf(q) + 0.5f, 0}; }
V3 armVec(Arm a) { return rotX(rotZ(V3{0, -0.3f, 0} + rotX({0, -0.32f, 0}, -a.elbow), a.roll), -a.pitch); }
// Forward kinematics: where is the hand in the world?
V3 handWorld(Place pl, const Pose& q, int side) {
    float hipY = hipOf(q);
    Arm a = side > 0 ? q.L : q.R;
    V3 local = shoulderLocal(q, side) + armVec(a);
    local = rotX(local - V3{0, hipY, 0}, q.lean) + V3{0, hipY, 0};
    return rotY(local * pl.scale, pl.heading) + pl.pos;
}
// Inverse kinematics: point the arm at a target; with bend=true the elbow bends so the hand reaches it
Arm aimArm(Place pl, const Pose& q, int side, V3 target, bool bend = false) {
    float hipY = hipOf(q);
    V3 l = rotY(target - pl.pos, -pl.heading) * (1 / pl.scale);
    l = rotX(l - V3{0, hipY, 0}, -q.lean) + V3{0, hipY, 0};
    V3 dv = l - shoulderLocal(q, side);
    float d = len(dv);
    V3 dn = norm(dv);
    Arm a = {deg(atan2f(dn.z, -dn.y)), deg(asinf(clampf(dn.x, -1, 1))), 0};
    const float L1 = 0.3f, L2 = 0.32f;
    if (bend && d < L1 + L2 - 0.005f) {
        d = fmaxf(d, 0.1f);
        float a1 = deg(acosf(clampf((L1 * L1 + d * d - L2 * L2) / (2 * L1 * d), -1, 1)));
        float g = deg(acosf(clampf((L1 * L1 + L2 * L2 - d * d) / (2 * L1 * L2), -1, 1)));
        a.pitch -= a1;
        a.elbow = 180 - g;
    }
    return a;
}

// ---------------------------------------------------------------------------
// Cow, modelled after the cartoon reference. Faces +x.
// ---------------------------------------------------------------------------
const Col COW_WHITE = {0.97f, 0.96f, 0.94f}, COW_BLACK = {0.06f, 0.06f, 0.07f};
const Col COW_PINK = {0.96f, 0.66f, 0.70f}, HOOF = {0.86f, 0.62f, 0.46f};
void cowLeg(float x, float z, bool hind, float bend) {
    col(COW_WHITE);
    if (hind) sph(x + 0.05f, 0.85f, z * 0.9f, 0.24f, 0.33f, 0.16f); else sph(x, 0.85f, z * 0.9f, 0.17f, 0.3f, 0.14f);
    col(COW_WHITE);
    V3 knee = {x + (hind ? -0.03f : 0.02f), 0.45f, z}, foot = {x + bend, 0.1f, z};
    cylBetween({x, 0.75f, z}, knee, 0.11f, 0.09f);
    sph(knee, 0.09f);
    cylBetween(knee, foot, 0.085f, 0.08f);
    col(HOOF); setSpec(0.4f);
    glPushMatrix(); glTranslatef(foot.x, 0, foot.z); frustum(0.095f, 0.075f, 0.11f); glPopMatrix();
    col(scale(HOOF, 0.6f)); box(foot.x + 0.06f, 0.05f, foot.z, 0.04f, 0.1f, 0.012f);
    setSpec(0.12f);
}
void drawCow(Place pl, float t, float headDip, float tailSwing, bool moo, bool chewing) {
    glPushMatrix();
    glTranslatef(pl.pos.x, pl.pos.y, pl.pos.z); glRotatef(pl.heading, 0, 1, 0);
    glScalef(pl.scale, pl.scale, pl.scale);
    float breathe = 1 + 0.012f * sinf(t * 2.2f);

    cowLeg(0.55f, 0.2f, false, 0); cowLeg(0.55f, -0.2f, false, 0);
    cowLeg(-0.6f, 0.2f, true, 0); cowLeg(-0.6f, -0.2f, true, 0);
    // body: hindquarters, barrel and chest blend into one shape
    col(COW_WHITE);
    glPushMatrix(); glScalef(1, breathe, breathe);
    setMat(M_SPOTS, 3.0f); glCallList(LIST_COWBODY);
    glPopMatrix();
    setMat(M_PLAIN);
    // udder and teats
    col(COW_PINK); setSpec(0.35f);
    sph(-0.4f, 0.6f, 0, 0.22f, 0.15f, 0.2f);
    for (int i = 0; i < 4; i++) {
        float tx = -0.4f + (i < 2 ? -0.08f : 0.08f), tz = (i % 2 ? -0.08f : 0.08f);
        glPushMatrix(); glTranslatef(tx, 0.4f, tz); frustum(0.022f, 0.03f, 0.12f); glPopMatrix();
    }
    setSpec(0.12f);
    // tail: chain of segments that swings, ending in a black tuft
    col(COW_WHITE);
    V3 prev = {-0.95f, 1.35f, 0};
    for (int i = 1; i <= 5; i++) {
        float u = i / 5.0f;
        V3 p = {-0.98f - 0.08f * u, 1.35f - 0.72f * u, tailSwing * 0.3f * u * u};
        cylBetween(prev, p, 0.028f - 0.008f * u);
        prev = p;
    }
    col(COW_BLACK); setMat(M_HAIR);
    sph(prev.x, prev.y - 0.08f, prev.z, 0.07f, 0.15f, 0.07f);
    setMat(M_PLAIN);

    // head group
    glPushMatrix();
    glTranslatef(0.82f, 1.28f, 0); glRotatef(-headDip, 0, 0, 1);
    col(COW_WHITE); setMat(M_SPOTS, 21.0f);
    sph(0.12f, 0.0f, 0, 0.3f, 0.3f, 0.27f);                                        // neck
    setMat(M_PLAIN);
    // red collar with bell
    glPushMatrix(); glTranslatef(0.1f, -0.04f, 0); glRotatef(-62, 0, 0, 1);
    col(0.78f, 0.12f, 0.10f); setMat(M_FABRIC, 4); torus(0.29f, 0.035f, 32, 8); setMat(M_PLAIN);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(0.37f, -0.24f, 0); glRotatef(10 * sinf(t * 3), 1, 0, 0); glRotatef(15, 0, 0, 1);
    col(0.80f, 0.60f, 0.22f); setSpec(1.0f);
    sph(0, -0.02f, 0, 0.035f);
    glPushMatrix(); glTranslatef(0, -0.17f, 0); frustum(0.085f, 0.05f, 0.15f); glPopMatrix();
    col(0.6f, 0.42f, 0.15f); sph(0, -0.19f, 0, 0.03f);
    setSpec(0.12f);
    glPopMatrix();
    // head
    glPushMatrix(); glTranslatef(0.42f, 0.1f, 0); glRotatef(-18, 0, 0, 1);
    col(COW_WHITE);
    sph(0, 0, 0, 0.26f, 0.24f, 0.22f);
    sph(0.18f, -0.1f, 0, 0.2f, 0.17f, 0.18f);                                      // face toward muzzle
    col(COW_BLACK); setMat(M_HAIR);
    sph(-0.05f, 0.05f, 0.11f, 0.17f, 0.17f, 0.13f); sph(-0.05f, 0.05f, -0.11f, 0.17f, 0.17f, 0.13f);  // eye patches
    sph(-0.1f, 0.15f, 0, 0.16f, 0.1f, 0.17f);
    setMat(M_PLAIN);
    col(COW_PINK); setSpec(0.45f);                                                 // big pink muzzle
    sph(0.33f, -0.17f, 0, 0.15f, 0.13f, 0.17f);
    col(0.35f, 0.15f, 0.15f);
    sph(0.44f, -0.12f, 0.065f, 0.025f, 0.035f, 0.025f); sph(0.44f, -0.12f, -0.065f, 0.025f, 0.035f, 0.025f);
    float jaw = chewing ? 0.02f * (0.5f + 0.5f * sinf(t * 8)) : 0;
    col(0.45f, 0.18f, 0.2f); sph(0.37f, -0.25f - jaw, 0, 0.09f, 0.02f, 0.11f);
    if (moo) { col(0.3f, 0.08f, 0.1f); sph(0.4f, -0.27f, 0, 0.07f, 0.05f, 0.08f); }
    setSpec(0.12f);
    if (chewing) {                                                                 // grass in the mouth
        col(0.45f, 0.72f, 0.18f);
        for (int i = 0; i < 7; i++) {
            float zz = -0.12f + i * 0.04f, ang = (i - 3) * 9.0f + 6 * sinf(t * 8 + i);
            V3 a = {0.4f, -0.26f - jaw, zz}, b = rotY({0.12f, -0.07f - 0.03f * (i % 3), 0}, ang + 180 * (zz < 0)) * 1.0f;
            b = a + V3{fabsf(b.x) * 0.3f, b.y, zz < 0 ? -fabsf(b.z) - 0.08f : fabsf(b.z) + 0.08f};
            cylBetween(a, b, 0.006f, 0.003f);
        }
    }
    for (int s = -1; s <= 1; s += 2) {
        // eyes (big and glossy)
        col(0.99f, 0.99f, 0.98f); setSpec(1.0f);
        sph(0.12f, 0.06f, s * 0.165f, 0.06f, 0.07f, 0.05f);
        col(0.20f, 0.12f, 0.06f); sph(0.15f, 0.055f, s * 0.205f, 0.033f);
        col(0.01f, 0.01f, 0.01f); sph(0.155f, 0.055f, s * 0.226f, 0.018f);
        setSpec(0.12f);
        unlit(); col(1, 1, 1); sph(0.165f, 0.07f, s * 0.236f, 0.007f); lit();
        col(COW_WHITE); sph(0.115f, 0.1f, s * 0.165f, 0.064f, 0.03f, 0.054f);       // eyelid
        // ears (black outside, pink inside)
        glPushMatrix(); glTranslatef(-0.08f, 0.13f, s * 0.24f); glRotatef(s * 25.0f, 1, 0, 0); glRotatef(-15, 0, 0, 1);
        col(COW_BLACK); sph(0, 0, s * 0.08f, 0.05f, 0.035f, 0.11f);
        col(COW_PINK); sph(0.012f, 0.002f, s * 0.09f, 0.035f, 0.02f, 0.08f);
        glPopMatrix();
        // horns (curve up and outward)
        col(0.90f, 0.83f, 0.68f); setSpec(0.5f);
        V3 h0 = {-0.05f, 0.2f, s * 0.12f}, h1 = {-0.06f, 0.29f, s * 0.17f}, h2 = {-0.02f, 0.36f, s * 0.17f};
        cylBetween(h0, h1, 0.035f, 0.026f); sph(h1, 0.026f); cylBetween(h1, h2, 0.026f, 0.004f);
        setSpec(0.12f);
    }
    col(COW_WHITE); sph(-0.06f, 0.22f, 0, 0.07f, 0.05f, 0.07f);                    // little tuft
    glPopMatrix();
    glPopMatrix();
    glPopMatrix();
    blobShadow(pl.pos.x, pl.pos.z, 1.2f * pl.scale, 0.3f);
}
V3 cowTeat(Place pl, int i) {
    V3 l = {-0.4f + (i == 0 ? -0.08f : 0.08f), 0.42f, 0.08f};
    return rotY(l * pl.scale, pl.heading) + pl.pos;
}
V3 cowNeck(Place pl) { return rotY(V3{1.04f, 1.02f, 0.06f} * pl.scale, pl.heading) + pl.pos; }  // collar

void drawRooster(Place pl, float beakOpen, float stretch) {
    glPushMatrix();
    glTranslatef(pl.pos.x, pl.pos.y, pl.pos.z); glRotatef(pl.heading, 0, 1, 0);
    glScalef(pl.scale, pl.scale, pl.scale);
    col(0.95f, 0.65f, 0.1f);
    cyl(0.0f, 0, 0.05f, 0.015f, 0.2f); cyl(0.0f, 0, -0.05f, 0.015f, 0.2f);
    for (int s = -1; s <= 1; s += 2)
        for (int k = -1; k <= 1; k++) cylBetween({0, 0.01f, s * 0.05f}, {0.07f, 0.01f, s * 0.05f + k * 0.03f}, 0.01f);
    const Col tail[3] = {{0.08f, 0.35f, 0.22f}, {0.05f, 0.05f, 0.08f}, {0.70f, 0.18f, 0.05f}};
    setSpec(0.6f);
    for (int i = 0; i < 7; i++) {
        col(tail[i % 3]);
        glPushMatrix(); glTranslatef(-0.18f, 0.42f, (i % 2 ? 0.03f : -0.03f)); glRotatef(20 + i * 13.0f, 0, 0, 1);
        sph(0, 0.19f, 0, 0.05f, 0.22f, 0.03f); glPopMatrix();
    }
    setSpec(0.2f);
    col(0.66f, 0.27f, 0.08f); setMat(M_FABRIC, 3); sph(0, 0.34f, 0, 0.22f, 0.16f, 0.14f);
    col(0.48f, 0.17f, 0.05f); sph(-0.03f, 0.34f, 0.12f, 0.13f, 0.08f, 0.04f); sph(-0.03f, 0.34f, -0.12f, 0.13f, 0.08f, 0.04f);
    float hy = 0.58f + stretch;
    col(0.90f, 0.50f, 0.13f); cylBetween({0.12f, 0.4f, 0}, {0.18f, hy, 0}, 0.075f); sph(0.18f, hy, 0, 0.09f);
    setMat(M_PLAIN);
    col(0.92f, 0.08f, 0.08f);
    sph(0.13f, hy + 0.09f, 0, 0.04f, 0.04f, 0.02f); sph(0.18f, hy + 0.11f, 0, 0.045f, 0.045f, 0.02f);
    sph(0.23f, hy + 0.08f, 0, 0.035f, 0.035f, 0.02f); sph(0.25f, hy - 0.09f, 0, 0.03f, 0.05f, 0.02f);
    col(1, 1, 1); sph(0.21f, hy + 0.02f, 0.07f, 0.022f); sph(0.21f, hy + 0.02f, -0.07f, 0.022f);
    col(0, 0, 0); sph(0.22f, hy + 0.02f, 0.085f, 0.012f); sph(0.22f, hy + 0.02f, -0.085f, 0.012f);
    col(1.0f, 0.8f, 0.1f);
    glPushMatrix(); glTranslatef(0.26f, hy, 0); glRotatef(-90 + beakOpen * 18, 0, 0, 1); frustum(0.03f, 0, 0.09f); glPopMatrix();
    glPushMatrix(); glTranslatef(0.26f, hy - 0.015f, 0); glRotatef(-90 - beakOpen * 22, 0, 0, 1); frustum(0.022f, 0, 0.07f); glPopMatrix();
    glPopMatrix();
}
void drawChicken(V3 p, float heading, float peck) {
    glPushMatrix(); glTranslatef(p.x, p.y, p.z); glRotatef(heading, 0, 1, 0);
    col(0.95f, 0.65f, 0.1f); cyl(0, 0, 0.03f, 0.01f, 0.1f); cyl(0, 0, -0.03f, 0.01f, 0.1f);
    col(0.97f, 0.96f, 0.92f); setMat(M_FABRIC, 8); sph(0, 0.18f, 0, 0.12f, 0.09f, 0.08f); sph(-0.12f, 0.24f, 0, 0.05f, 0.07f, 0.04f);
    float hx = 0.12f + peck * 0.05f, hy = 0.27f - peck * 0.15f;
    sph(hx, hy, 0, 0.05f);
    setMat(M_PLAIN);
    col(0.88f, 0.1f, 0.1f); sph(hx, hy + 0.05f, 0, 0.025f, 0.02f, 0.01f);
    col(0.95f, 0.7f, 0.1f); glPushMatrix(); glTranslatef(hx + 0.04f, hy, 0); glRotatef(-90 - peck * 30, 0, 0, 1); frustum(0.015f, 0, 0.04f); glPopMatrix();
    col(0, 0, 0); sph(hx + 0.025f, hy + 0.015f, 0.035f, 0.008f); sph(hx + 0.025f, hy + 0.015f, -0.035f, 0.008f);
    glPopMatrix();
}

// ---------------------------------------------------------------------------
// Bicycle (faces +x). Step-through frame, rear rack with a milk crate.
// ---------------------------------------------------------------------------
struct Bike { V3 pos; float heading; float wheel; bool crate; int bottles; bool stand; };
const V3 BK_R = {-0.52f, 0.34f, 0}, BK_F = {0.55f, 0.34f, 0}, BK_BB = {-0.05f, 0.3f, 0};
const V3 BK_HIP = {-0.24f, 0.98f, 0};            // rider's hip on the saddle
V3 bikeW(const Bike& b, V3 l) { return rotY(l, b.heading) + b.pos; }
V3 bikePedal(const Bike& b, int side) {          // side +1 = bike's +z side
    float c = rad(-b.wheel * 0.45f) + (side > 0 ? 0.0f : PI);
    return {BK_BB.x + 0.17f * cosf(c), BK_BB.y + 0.17f * sinf(c), side * 0.13f};
}
void bikeWheel(V3 c, float ang) {
    glPushMatrix(); glTranslatef(c.x, c.y, c.z);
    glPushMatrix(); glRotatef(90, 1, 0, 0);
    col(0.08f, 0.08f, 0.08f); setSpec(0.25f); torus(0.31f, 0.03f, 36, 8);
    col(0.78f, 0.79f, 0.82f); setSpec(0.9f); torus(0.285f, 0.011f, 36, 6);
    glPopMatrix();
    glRotatef(ang, 0, 0, 1);
    for (int i = 0; i < 16; i++) {
        float a = 2 * PI * i / 16;
        cylBetween({0, 0, (i % 2) ? 0.025f : -0.025f}, {0.28f * cosf(a), 0.28f * sinf(a), 0}, 0.0035f);
    }
    glPushMatrix(); glRotatef(90, 1, 0, 0); cyl(0, -0.05f, 0, 0.03f, 0.1f); glPopMatrix();
    setSpec(0.12f);
    glPopMatrix();
}
void drawCrate(int bottles) {                     // milk crate standing at the origin
    col(0.96f, 0.72f, 0.10f); setSpec(0.4f);
    box(0, 0.01f, 0, 0.48f, 0.02f, 0.34f);
    for (int k = 0; k < 3; k++) {
        float y = 0.045f + k * 0.07f;
        box(0, y, 0.165f, 0.48f, 0.045f, 0.015f); box(0, y, -0.165f, 0.48f, 0.045f, 0.015f);
        box(0.235f, y, 0, 0.015f, 0.045f, 0.34f); box(-0.235f, y, 0, 0.015f, 0.045f, 0.34f);
    }
    for (int i = 0; i < 4; i++) box((i % 2 ? 0.235f : -0.235f), 0.11f, (i / 2 ? 0.165f : -0.165f), 0.03f, 0.22f, 0.03f);
    setSpec(0.12f);
    for (int i = 0; i < bottles; i++) {
        glPushMatrix(); glTranslatef(-0.11f + (i % 2) * 0.22f, 0.02f, (i / 2) ? 0.075f : -0.075f);
        bottle(1, true, 0.85f); glPopMatrix();
    }
}
void drawBike(const Bike& b) {
    glPushMatrix(); glTranslatef(b.pos.x, b.pos.y, b.pos.z); glRotatef(b.heading, 0, 1, 0);
    const Col frame = {0.10f, 0.45f, 0.62f}, chrome = {0.78f, 0.79f, 0.82f};
    bikeWheel(BK_R, -b.wheel); bikeWheel(BK_F, -b.wheel);
    const V3 S = {-0.2f, 0.86f, 0}, Hb = {0.45f, 0.7f, 0}, Ht = {0.41f, 0.93f, 0}, M = {0.12f, 0.42f, 0};
    col(frame); setSpec(0.7f);
    cylBetween(BK_BB, S, 0.022f);
    cylBetween(Hb, M, 0.024f); cylBetween(M, BK_BB, 0.024f); sph(M, 0.024f);
    cylBetween(Hb, Ht, 0.03f);
    for (int s = -1; s <= 1; s += 2) {
        cylBetween({BK_BB.x, BK_BB.y, s * 0.04f}, {BK_R.x, BK_R.y, s * 0.06f}, 0.012f);
        cylBetween({S.x, S.y, s * 0.02f}, {BK_R.x, BK_R.y, s * 0.06f}, 0.012f);
        cylBetween({Hb.x, Hb.y, s * 0.02f}, {BK_F.x, BK_F.y, s * 0.05f}, 0.015f);
    }
    for (int k = 0; k < 7; k++) {                                       // mudguards
        float a0 = rad(20 + k * 20.0f), a1 = rad(40 + k * 20.0f);
        cylBetween(BK_R + V3{0.37f * cosf(a0 + PI * 0.15f), 0.37f * sinf(a0 + PI * 0.15f), 0},
                   BK_R + V3{0.37f * cosf(a1 + PI * 0.15f), 0.37f * sinf(a1 + PI * 0.15f), 0}, 0.03f);
        if (k < 5) cylBetween(BK_F + V3{0.37f * cosf(a0), 0.37f * sinf(a0), 0}, BK_F + V3{0.37f * cosf(a1), 0.37f * sinf(a1), 0}, 0.03f);
    }
    col(chrome); setSpec(0.95f);
    cylBetween(S, {-0.22f, 0.9f, 0}, 0.013f);                         // seat post
    cylBetween(Ht, {0.37f, 1.04f, 0}, 0.014f);                         // stem
    cylBetween({0.36f, 1.04f, -0.28f}, {0.36f, 1.04f, 0.28f}, 0.013f); // handlebar
    sph(0.47f, 0.84f, 0, 0.045f);                                       // lamp
    col(0.1f, 0.1f, 0.1f); setSpec(0.3f);
    cylBetween({0.36f, 1.04f, 0.2f}, {0.33f, 1.04f, 0.31f}, 0.019f);
    cylBetween({0.36f, 1.04f, -0.2f}, {0.33f, 1.04f, -0.31f}, 0.019f);
    col(0.38f, 0.22f, 0.12f); sph(-0.24f, 0.915f, 0, 0.13f, 0.035f, 0.075f);   // saddle
    col(chrome); setSpec(0.9f);
    glPushMatrix(); glTranslatef(BK_BB.x, BK_BB.y, 0.06f); glRotatef(90, 1, 0, 0); torus(0.09f, 0.009f, 24, 6); glPopMatrix();
    col(0.3f, 0.3f, 0.32f);
    cylBetween({BK_BB.x, BK_BB.y + 0.09f, 0.06f}, {BK_R.x, BK_R.y + 0.05f, 0.06f}, 0.005f);
    cylBetween({BK_BB.x, BK_BB.y - 0.09f, 0.06f}, {BK_R.x, BK_R.y - 0.05f, 0.06f}, 0.005f);
    for (int s = -1; s <= 1; s += 2) {                                  // cranks + pedals
        V3 p = bikePedal(b, s);
        col(0.28f, 0.28f, 0.3f); cylBetween({BK_BB.x, BK_BB.y, s * 0.07f}, {p.x, p.y, s * 0.07f}, 0.012f);
        col(0.12f, 0.12f, 0.12f); box(p.x, p.y, s * 0.12f, 0.1f, 0.025f, 0.08f);
    }
    col(0.22f, 0.22f, 0.24f); setSpec(0.6f);                           // rear rack
    box(-0.6f, 0.62f, 0, 0.5f, 0.02f, 0.24f);
    for (int s = -1; s <= 1; s += 2) {
        cylBetween({-0.82f, 0.62f, s * 0.1f}, {BK_R.x, BK_R.y, s * 0.07f}, 0.009f);
        cylBetween({-0.38f, 0.62f, s * 0.1f}, {-0.25f, 0.8f, s * 0.03f}, 0.009f);
    }
    if (b.stand) cylBetween({-0.12f, 0.3f, -0.05f}, {-0.28f, 0.0f, -0.24f}, 0.012f);   // kickstand
    else cylBetween({-0.12f, 0.3f, -0.05f}, {-0.45f, 0.33f, -0.07f}, 0.012f);
    setSpec(0.12f);
    if (b.crate) { glPushMatrix(); glTranslatef(-0.6f, 0.63f, 0); drawCrate(b.bottles); glPopMatrix(); }
    glPopMatrix();
    blobShadow(b.pos.x, b.pos.z, 0.7f, 0.25f);
}
// Rider on the saddle: feet follow the pedals (leg IK), hands hold the grips (arm IK)
Pose riderPose(const Bike& b, Place& pl) {
    Pose q; q.legIK = true; q.lean = 18;
    pl = {bikeW(b, {BK_HIP.x, 0, 0}), b.heading + 90, 1.0f};
    pl.pos.y = b.pos.y + BK_HIP.y - 0.9f;
    for (int i = 0; i < 2; i++) {                // leg 0 = her left = bike's -z side
        V3 pd = bikePedal(b, i == 0 ? -1 : 1);
        legIK(pd.x - BK_HIP.x, pd.y + 0.05f - BK_HIP.y, q.legP[i], q.legK[i]);
    }
    q.L = aimArm(pl, q, 1, bikeW(b, {0.33f, 1.04f, -0.26f}), true);
    q.R = aimArm(pl, q, -1, bikeW(b, {0.33f, 1.04f, 0.26f}), true);
    return q;
}

// ===========================================================================
// SCENE 1 - SUNRISE
// ===========================================================================
void scene1(float t) {
    float p = ease(ph(t, 0, 8.5f));
    Col bot = mix3({0.14f, 0.13f, 0.32f}, {1.0f, 0.60f, 0.35f}, {0.75f, 0.89f, 1.0f}, p);
    Col top = mix3({0.02f, 0.02f, 0.10f}, {0.36f, 0.36f, 0.66f}, {0.26f, 0.54f, 0.92f}, p);
    sky2D(bot, top);
    stars2D(1 - ph(p, 0, 0.45f), t, 0);
    sun2D(780, 190 + p * 290, 40, mix({1.0f, 0.5f, 0.15f}, {1.0f, 0.93f, 0.55f}, p));
    Col cc = mix({1.0f, 0.72f, 0.72f}, {1, 1, 1}, p);
    cloud2D(fmodf(150 + t * 12, 1200) - 100, 505, 1.0f, cc);
    cloud2D(fmodf(650 + t * 8, 1200) - 100, 545, 0.7f, cc);

    float cm = gCamMotion ? t : 0;
    gShadowExtent = 14;
    begin3D({2.2f - cm * 0.08f, 1.7f + cm * 0.02f, 7.6f - cm * 0.18f}, {-0.9f, 1.5f, 2.2f});
    setLight({0.75f, 0.12f + 0.75f * p, -0.45f},
             mix3({0.25f, 0.25f, 0.45f}, {1.0f, 0.64f, 0.40f}, {1, 0.96f, 0.88f}, p),
             mix3({0.12f, 0.12f, 0.22f}, {0.34f, 0.29f, 0.33f}, {0.42f, 0.45f, 0.52f}, p));
    setFog(bot, 30, 95);
    farmWorld(p < 0.35f, 0);
    fenceX(-7, 7, 3, 1.4f, {0.64f, 0.47f, 0.29f});
    bool crow = (t > 2.5f && t < 5.0f) || (t > 6.0f && t < 8.5f);
    float open = crow ? 0.5f + 0.5f * sinf(t * 14) : 0;
    Place rp = {{0, 1.18f, 3}, 0, 1.5f};
    drawRooster(rp, open, crow ? 0.05f : 0);
    for (int i = 0; i < 3; i++)
        drawChicken({-3.2f + i * 0.7f, 0, 1.4f + (i % 2) * 0.4f}, 30.0f * i, (sinf(t * 5 + i * 2) > 0.6f) ? 1.0f : 0.0f);

    if (crow) {
        V3 beak = {rp.pos.x + 0.36f * rp.scale, rp.pos.y + 0.62f * rp.scale, rp.pos.z};
        float bx, by;
        if (project(beak, bx, by)) {
            begin2D();
            for (int i = 0; i < 3; i++) {
                float k = fmodf(t * 2 + i / 3.0f, 1.0f);
                col(1, 1, 1, 1 - k); arc2(bx, by, 10 + k * 34, -35, 35, 2.5f);
            }
            bubble(bx + 30, by + 30, "Cock-a-doodle-doo!");
        }
    }
}

// ===========================================================================
// SCENE 2 - BEDROOM: sits up, swings her legs off the bed, stands, washes,
// puts on her apron, opens the door and walks out
// ===========================================================================
void apronOnHook(float x, float y, float z) {
    setMat(M_FABRIC, 7); col(FARMER.accent);
    box(x, y - 0.35f, z, 0.32f, 0.7f, 0.03f);
    box(x, y - 0.05f, z, 0.22f, 0.12f, 0.03f);
    col(scale(FARMER.accent, 0.85f)); box(x, y - 0.26f, z + 0.02f, 0.34f, 0.06f, 0.02f);
    setMat(M_PLAIN);
}
const V3 BED_DOOR_HINGE = {3.8f, 0, -2.95f};
V3 bedroomKnob(float doorA) { return BED_DOOR_HINGE + rotY({-0.78f, 1.05f, 0.06f}, doorA); }

void scene2(float t) {
    sky2D({0.2f, 0.18f, 0.15f}, {0.2f, 0.18f, 0.15f});
    float cm = gCamMotion ? t : 0;
    gShadowExtent = 7;
    begin3D({0.3f + 0.7f * sinf(cm * 0.15f), 2.05f, 3.4f}, {-0.3f, 1.0f, -1.8f}, 58);
    setLight({-0.35f, 0.8f, 0.7f}, {0.90f, 0.82f, 0.70f}, {0.44f, 0.41f, 0.40f});
    noFog();
    float doorA = 80 * ease(ph(t, 13.0f, 13.8f));

    // floor, walls (back wall has a real opening for the door), rug
    setMat(M_WOOD); col(0.62f, 0.42f, 0.24f); box(0, -0.05f, 0, 8, 0.1f, 6.2f);
    setMat(M_PLASTER); col(0.95f, 0.88f, 0.74f);
    box(-0.55f, 1.75f, -3.05f, 6.9f, 3.5f, 0.1f);            // left of the door
    box(3.9f, 1.75f, -3.05f, 0.2f, 3.5f, 0.1f);              // right of the door
    box(3.35f, 2.875f, -3.05f, 0.9f, 1.25f, 0.1f);           // above the door
    if (!shadowPass()) {                                      // side walls don't block the sunlight
        col(0.92f, 0.84f, 0.68f); box(-4.05f, 1.75f, 0, 0.1f, 3.5f, 6.2f);
        col(0.92f, 0.84f, 0.68f); box(4.05f, 1.75f, 0, 0.1f, 3.5f, 6.2f);
    }
    setMat(M_WOOD);
    col(0.55f, 0.36f, 0.2f); box(-0.55f, 0.08f, -2.98f, 6.9f, 0.16f, 0.04f);
    col(0.96f, 0.96f, 0.93f);                                 // door frame
    box(2.87f, 1.15f, -2.98f, 0.07f, 2.3f, 0.09f); box(3.83f, 1.15f, -2.98f, 0.07f, 2.3f, 0.09f);
    box(3.35f, 2.28f, -2.98f, 1.03f, 0.07f, 0.09f);
    setMat(M_PLAIN);
    if (!shadowPass()) { col(0.98f, 0.96f, 0.9f); box(0, 3.55f, 0, 8.2f, 0.1f, 6.2f); }
    setMat(M_FABRIC, 6); col(0.70f, 0.32f, 0.25f); box(-1.2f, 0.005f, -0.8f, 2.6f, 0.01f, 1.6f);
    col(0.95f, 0.85f, 0.55f); box(-1.2f, 0.007f, -0.8f, 2.3f, 0.01f, 1.3f);
    col(0.70f, 0.32f, 0.25f); box(-1.2f, 0.009f, -0.8f, 2.0f, 0.01f, 1.0f);
    setMat(M_PLAIN);

    // outside, seen through the doorway
    setMat(M_GRASS); groundPatch(1.0f, -9.0f, 6.5f, -3.1f, GRASS, 0.0f, 0.5f); setMat(M_PLAIN);
    tree(5.2f, -6.8f, 0.9f);
    setMat(M_LEAF, 4); col(0.25f, 0.55f, 0.25f); sph(2.2f, 0.25f, -5.0f, 0.5f, 0.4f, 0.4f); setMat(M_PLAIN);
    if (!shadowPass()) {
        unlit();
        glBegin(GL_QUADS);
        col(0.80f, 0.90f, 1.0f); glVertex3f(-1.0f, 0.0f, -8.9f); glVertex3f(8.0f, 0.0f, -8.9f);
        col(0.45f, 0.68f, 0.96f); glVertex3f(8.0f, 6.0f, -8.9f); glVertex3f(-1.0f, 6.0f, -8.9f);
        glEnd();
        lit();
    }

    // window with the curtains drawn (it's still early)
    if (!shadowPass()) {
        unlit();
        glBegin(GL_QUADS);
        col(1.0f, 0.80f, 0.55f); glVertex3f(-3.0f, 1.3f, -2.99f); glVertex3f(-1.4f, 1.3f, -2.99f);
        col(0.55f, 0.76f, 0.97f); glVertex3f(-1.4f, 2.5f, -2.99f); glVertex3f(-3.0f, 2.5f, -2.99f);
        glEnd();
        lit();
    }
    setMat(M_WOOD); col(0.95f, 0.95f, 0.92f);
    box(-2.2f, 1.3f, -2.95f, 1.75f, 0.08f, 0.12f); box(-2.2f, 2.5f, -2.95f, 1.75f, 0.08f, 0.12f);
    box(-3.0f, 1.9f, -2.95f, 0.08f, 1.25f, 0.12f); box(-1.4f, 1.9f, -2.95f, 0.08f, 1.25f, 0.12f);
    box(-2.2f, 1.24f, -2.88f, 1.9f, 0.05f, 0.22f);
    setMat(M_FABRIC, 2); col(0.80f, 0.22f, 0.26f);
    for (int i = 0; i < 12; i++)                                 // closed curtains: 12 folds
        box(-3.12f + 0.075f + i * 0.155f, 1.85f, -2.86f + (i % 2) * 0.035f, 0.165f, 1.62f, 0.04f);
    setMat(M_PLAIN);
    col(0.5f, 0.35f, 0.2f); setSpec(0.6f);
    glPushMatrix(); glTranslatef(-3.3f, 2.68f, -2.86f); glRotatef(-90, 0, 0, 1); cyl(0, 0, 0, 0.02f, 2.2f); glPopMatrix();
    setSpec(0.12f);

    // picture + clock (hands stay still: 6:05 in the morning)
    setMat(M_WOOD); col(0.45f, 0.28f, 0.15f); box(-0.9f, 2.3f, -2.98f, 0.7f, 0.55f, 0.04f); setMat(M_PLAIN);
    col(0.55f, 0.75f, 0.9f); box(-0.9f, 2.3f, -2.955f, 0.58f, 0.43f, 0.02f);
    col(0.4f, 0.65f, 0.3f); box(-0.9f, 2.17f, -2.945f, 0.58f, 0.16f, 0.02f);
    glPushMatrix(); glTranslatef(0.4f, 2.6f, -2.98f); glRotatef(90, 1, 0, 0);
    setMat(M_WOOD); col(0.45f, 0.28f, 0.14f); cyl(0, 0, 0, 0.32f, 0.05f); setMat(M_PLAIN);
    col(1, 1, 0.97f); cyl(0, 0.0f, 0, 0.27f, 0.07f);
    glPopMatrix();
    col(0.1f, 0.1f, 0.1f);
    for (int i = 0; i < 12; i++) { float a = rad(i * 30.0f); box(0.4f + 0.22f * cosf(a), 2.6f + 0.22f * sinf(a), -2.905f, 0.02f, 0.02f, 0.01f); }
    glPushMatrix(); glTranslatef(0.4f, 2.6f, -2.9f); glRotatef(-30, 0, 0, 1); box(0, 0.1f, 0, 0.022f, 0.2f, 0.01f); glPopMatrix();
    glPushMatrix(); glTranslatef(0.4f, 2.6f, -2.9f); glRotatef(177, 0, 0, 1); box(0, 0.07f, 0, 0.035f, 0.14f, 0.01f); glPopMatrix();

    // door (hinged on the right, opens into the room)
    glPushMatrix(); glTranslatef(BED_DOOR_HINGE.x, 0, BED_DOOR_HINGE.z); glRotatef(doorA, 0, 1, 0);
    setMat(M_WOODWALL);
    col(0.62f, 0.42f, 0.23f); box(-0.45f, 1.1f, 0, 0.9f, 2.2f, 0.06f);
    col(0.52f, 0.34f, 0.18f); box(-0.45f, 1.55f, 0.035f, 0.65f, 0.8f, 0.02f); box(-0.45f, 0.55f, 0.035f, 0.65f, 0.7f, 0.02f);
    setMat(M_PLAIN);
    col(0.9f, 0.78f, 0.3f); setSpec(0.9f); sph(-0.78f, 1.05f, 0.06f, 0.04f); setSpec(0.12f);
    glPopMatrix();

    // apron on its hook
    col(0.3f, 0.3f, 0.3f); setSpec(0.8f);
    glPushMatrix(); glTranslatef(2.4f, 1.75f, -2.95f); glRotatef(90, 1, 0, 0); cyl(0, 0, 0, 0.02f, 0.12f); glPopMatrix();
    setSpec(0.12f);
    if (t < 11.9f) apronOnHook(2.4f, 1.78f, -2.9f);

    // wash stand
    setMat(M_WOOD); col(0.48f, 0.30f, 0.15f);
    box(1.0f, 0.45f, -2.6f, 0.06f, 0.9f, 0.06f); box(1.6f, 0.45f, -2.6f, 0.06f, 0.9f, 0.06f);
    box(1.0f, 0.45f, -2.2f, 0.06f, 0.9f, 0.06f); box(1.6f, 0.45f, -2.2f, 0.06f, 0.9f, 0.06f);
    box(1.3f, 0.92f, -2.4f, 0.75f, 0.05f, 0.55f);
    setMat(M_PLAIN);
    col(0.85f, 0.87f, 0.9f); setSpec(0.8f);
    glPushMatrix(); glTranslatef(1.3f, 0.95f, -2.4f); frustum(0.16f, 0.25f, 0.12f, false); glPopMatrix();
    col(0.5f, 0.75f, 0.95f); cyl(1.3f, 0.95f, -2.4f, 0.22f, 0.09f);
    col(0.95f, 0.95f, 0.97f); glPushMatrix(); glTranslatef(1.55f, 0.95f, -2.55f); frustum(0.06f, 0.04f, 0.2f); glPopMatrix();
    setSpec(0.12f);
    setMat(M_FABRIC, 3); col(0.95f, 0.95f, 0.9f); box(1.67f, 0.75f, -2.4f, 0.02f, 0.3f, 0.25f); setMat(M_PLAIN);

    // bed, bedside table, lamp
    setMat(M_WOOD); col(0.50f, 0.30f, 0.14f);
    box(-2.3f, 0.3f, -2.2f, 2.4f, 0.3f, 1.3f);
    box(-3.5f, 0.65f, -2.2f, 0.1f, 1.3f, 1.35f); box(-1.12f, 0.45f, -2.2f, 0.08f, 0.9f, 1.35f);
    box(-3.8f, 0.3f, -1.25f, 0.5f, 0.6f, 0.5f);
    setMat(M_FABRIC, 1);
    col(0.97f, 0.97f, 0.94f); box(-2.3f, 0.53f, -2.2f, 2.3f, 0.16f, 1.25f);
    col(1, 1, 1); sph(-3.1f, 0.66f, -2.2f, 0.22f, 0.08f, 0.4f);
    setMat(M_PLAIN);
    col(0.9f, 0.85f, 0.75f); cyl(-3.8f, 0.6f, -1.25f, 0.07f, 0.03f); cyl(-3.8f, 0.6f, -1.25f, 0.015f, 0.3f);
    unlit(); col(1.0f, 0.93f, 0.75f); glPushMatrix(); glTranslatef(-3.8f, 0.85f, -1.25f); frustum(0.14f, 0.08f, 0.16f); glPopMatrix(); lit();

    // ---- the farmer ----
    const float HX = -2.3f, FEET_Z = -1.75f + 0.45f * sinf(rad(75));
    Pose q;
    Place pl;
    q.apron = t > 11.9f;
    if (t < 6.3f) {
        // lying -> sitting up in bed -> turning to the edge -> feet down to the floor
        float up = ease(ph(t, 3.0f, 4.5f)), sw = ease(ph(t, 4.5f, 5.5f)), dn = ease(ph(t, 5.5f, 6.3f));
        q.sit = true;
        q.lean = -90 * (1 - up);
        q.sitAngle = 90 - 15 * dn;
        q.knee = 75 * dn;
        q.smile = 0.3f + 0.7f * up;
        float hipW = 0.71f - (0.71f - (0.034f + 0.45f + 0.45f * cosf(rad(75)))) * dn;
        pl = {{HX, hipW - hipOf(q), -2.2f + 0.45f * sw}, 90 - 90 * sw, 1.0f};
    } else if (t < 7.3f) {
        // stand up: feet stay on the floor, hips rise and move over the feet
        float u = ease(ph(t, 6.3f, 7.3f));
        q.sit = true; q.sitAngle = 75 * (1 - u); q.knee = q.sitAngle;
        q.lean = 30 * sinf(PI * u);
        pl = {{HX, 0.034f * (1 - u), FEET_Z - 0.45f * sinf(rad(q.sitAngle))}, 0, 1.0f};
    } else {
        std::vector<Key> path = {{7.3f, HX, FEET_Z}, {9.1f, 1.0f, -1.4f}, {9.4f, 1.3f, -1.75f}, {10.9f, 1.3f, -1.75f},
                                 {11.5f, 2.4f, -2.15f}, {12.2f, 2.4f, -2.15f}, {12.8f, 2.6f, -2.25f}, {13.8f, 2.6f, -2.25f},
                                 {14.3f, 3.25f, -2.5f}, {16.0f, 3.25f, -4.9f}};
        Track tr = track(path, t, 180);
        pl = {tr.pos, tr.heading, 1.0f};
        q.moving = tr.moving; q.walk = t * 9;
        if (t > 9.4f && t < 10.9f) {                                          // splash water on her face
            q.L = {95 + 12 * sinf(t * 12), -20, 70}; q.R = {95 + 12 * sinf(t * 12 + 1.5f), 20, 70};
            q.lean = 15;
        }
        if (t > 11.5f && t < 12.1f) q.L = aimArm(pl, q, 1, {2.4f, 1.6f, -2.85f}, true);   // take the apron
        if (t > 12.8f && t < 13.7f) q.L = aimArm(pl, q, 1, bedroomKnob(doorA), true);   // open the door
    }
    drawPerson(FARMER, pl, q);

    // blanket: covers her while asleep, pushed back when she sits up
    setMat(M_FABRIC, 4);
    const Col pc[3] = {{0.96f, 0.80f, 0.30f}, {0.85f, 0.35f, 0.3f}, {0.95f, 0.95f, 0.9f}};
    if (t < 4.5f) {
        float x0 = -2.75f + 0.45f * ease(ph(t, 3.0f, 4.5f));                 // top edge slides down
        col(0.25f, 0.42f, 0.70f); box((x0 - 1.15f) / 2, 0.78f, -2.2f, -1.15f - x0, 0.14f, 1.3f);
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 3; j++) {
                float cx = x0 + 0.17f + i * ((-1.15f - x0 - 0.34f) / 3);
                col(pc[(i + j) % 3]); box(cx, 0.853f, -2.6f + j * 0.4f, 0.22f, 0.01f, 0.3f);
            }
    } else {
        col(0.25f, 0.42f, 0.70f); box(-1.9f, 0.68f, -2.68f, 1.4f, 0.14f, 0.3f);    // folded at the back
    }
    setMat(M_PLAIN);
    if (t > 9.4f && t < 10.9f && !shadowPass()) {
        unlit();
        for (int i = 0; i < 8; i++) {
            float k = fmodf(t * 2 + i / 8.0f, 1.0f);
            col(0.55f, 0.8f, 1.0f, 1 - k); sph(1.2f + (i % 4) * 0.06f, 1.65f - k * 0.65f, -2.3f, 0.022f);
        }
        lit();
    }
    float hx, hy;
    if (t < 3.2f && project({-3.0f, 1.0f, -2.2f}, hx, hy)) {
        begin2D();
        for (int i = 0; i < 3; i++) {
            float k = fmodf(t * 0.6f + i / 3.0f, 1.0f);
            col(0.25f, 0.25f, 0.6f, 1 - k);
            text(hx + 10 + k * 50, hy + 10 + k * 90, "Z", k > 0.5f ? GLUT_BITMAP_TIMES_ROMAN_24 : GLUT_BITMAP_HELVETICA_18);
        }
    } else if (t > 6.6f && t < 8.4f) bubble3(pl.pos + V3{0, 1.95f, 0}, "Good morning!");
}

// ===========================================================================
// SCENE 3 - FARM
// ===========================================================================
void scene3(float t) {
    Col bot = {0.78f, 0.90f, 1.0f}, top = {0.30f, 0.56f, 0.94f};
    sky2D(bot, top);
    sun2D(140, 500, 34, {1.0f, 0.95f, 0.6f});
    cloud2D(fmodf(300 + t * 16, 1200) - 100, 510, 1.0f, {1, 1, 1});
    cloud2D(fmodf(820 + t * 11, 1200) - 100, 460, 0.8f, {1, 1, 1});
    for (int i = 0; i < 3; i++) bird2D(fmodf(t * 40 + i * 30, 1100) - 50, 430 + i * 12.0f, sinf(t * 10 + i));

    std::vector<Key> path = {{0, -8, -2.2f}, {1.4f, -7.2f, 1.3f}, {7.0f, 6.0f, 1.3f}, {8.3f, 7.0f, -2.4f}, {10, 7.0f, -3.6f}};
    Track tr = track(path, t, 180);
    float fx = tr.pos.x;
    V3 eye = gCamMotion ? V3{fx * 0.55f - 0.5f, 2.8f, 10.0f} : V3{0, 3.2f, 13};
    V3 ctr = gCamMotion ? V3{fx * 0.8f, 1.1f, -1} : V3{0, 1.2f, -1};
    gShadowExtent = 16;
    begin3D(eye, ctr);
    setLight({-0.55f, 0.85f, 0.45f}, {1.0f, 0.96f, 0.88f}, {0.42f, 0.45f, 0.52f});
    setFog(bot, 35, 100);
    farmWorld(false, ease(ph(t, 6.5f, 8.0f)));
    for (int i = 0; i < 4; i++)
        drawChicken({-4.2f + (i % 2) * 0.7f, 0, -0.6f - (i / 2) * 0.6f}, 40.0f + i * 70, sinf(t * 5 + i * 2) > 0.6f ? 1.0f : 0.0f);
    Pose q;
    q.moving = tr.moving; q.walk = t * 9;
    Place pl = {tr.pos, tr.heading, 1.0f};
    drawPerson(FARMER, pl, q);
    if (t > 1.5f && t < 3.6f) bubble3(pl.pos + V3{0, 2.0f, 0}, "Time to milk Daisy!");
}

// ===========================================================================
// SCENE 4 - MILKING
// Everything she needs is already in the shed: the rope on a wall hook, hay
// bales, a feeding trough, a bucket of water with a sponge, the milking pail
// on a bench and the stool. She ties the cow, gives it hay, washes the udder,
// waits for the water to drip off, puts the pail under the cow and milks.
// ===========================================================================
// collar point, which moves when the cow lowers its head to eat
V3 cowCollar(Place pl, float dip) {
    V3 l = V3{0.82f, 1.28f, 0} + rotZ({0.22f, -0.26f, 0}, -dip) + V3{0, 0, 0.06f};
    return rotY(l * pl.scale, pl.heading) + pl.pos;
}
void ropeCoil(V3 c) {                                  // a coil of rope hanging from a point
    setMat(M_FABRIC, 9); col(0.76f, 0.60f, 0.34f);
    for (int i = 0; i < 3; i++) {
        glPushMatrix(); glTranslatef(c.x, c.y - 0.15f, c.z + 0.012f * i); glRotatef(90, 1, 0, 0); glRotatef(i * 9.0f, 0, 0, 1);
        torus(0.13f + 0.01f * i, 0.016f, 24, 6); glPopMatrix();
    }
    setMat(M_PLAIN);
}
void hayBundle(V3 c, float s) {
    setMat(M_STRAW); col(0.92f, 0.80f, 0.42f);
    sph(c.x, c.y, c.z, 0.26f * s, 0.14f * s, 0.17f * s);
    sph(c.x + 0.12f * s, c.y + 0.05f * s, c.z, 0.14f * s, 0.1f * s, 0.12f * s);
    sph(c.x - 0.13f * s, c.y + 0.03f * s, c.z + 0.03f * s, 0.13f * s, 0.09f * s, 0.11f * s);
    setMat(M_PLAIN);
}
void trough(V3 c, float hay) {                         // long side along z, in front of the cow
    setMat(M_WOOD); col(0.50f, 0.32f, 0.16f);
    for (int i = 0; i < 4; i++) box(c.x + (i % 2 ? 0.2f : -0.2f), 0.16f, c.z + (i / 2 ? 0.38f : -0.38f), 0.06f, 0.32f, 0.06f);
    box(c.x, 0.32f, c.z, 0.52f, 0.04f, 0.92f);
    box(c.x - 0.25f, 0.45f, c.z, 0.04f, 0.26f, 0.92f); box(c.x + 0.25f, 0.45f, c.z, 0.04f, 0.26f, 0.92f);
    box(c.x, 0.45f, c.z - 0.45f, 0.52f, 0.26f, 0.04f); box(c.x, 0.45f, c.z + 0.45f, 0.52f, 0.26f, 0.04f);
    setMat(M_PLAIN);
    if (hay > 0.02f) {
        setMat(M_STRAW); col(0.90f, 0.78f, 0.40f);
        sph(c.x, 0.34f + 0.12f * hay, c.z, 0.22f, 0.04f + 0.1f * hay, 0.42f);
        setMat(M_PLAIN);
    }
}
void shedDetails() {
    // roof rafters and posts
    setMat(M_WOOD); col(0.40f, 0.25f, 0.13f);
    for (int i = 0; i < 6; i++) box(-4.5f + i * 1.8f, 3.45f, -0.3f, 0.16f, 0.16f, 4.6f);
    box(-5.0f, 1.6f, 1.2f, 0.18f, 3.2f, 0.18f);
    box(-2.0f, 1.2f, -2.5f, 3.0f, 0.1f, 0.06f);                          // tool rail
    setMat(M_PLAIN);
    // pitchfork, shovel and rake leaning on the back wall
    setMat(M_WOOD); col(0.55f, 0.40f, 0.22f);
    cylBetween({-3.0f, 0.02f, -2.38f}, {-2.85f, 1.55f, -2.47f}, 0.02f);
    cylBetween({-2.5f, 0.35f, -2.38f}, {-2.4f, 1.6f, -2.47f}, 0.02f);
    cylBetween({-1.75f, 0.1f, -2.36f}, {-1.6f, 1.7f, -2.47f}, 0.02f);
    setMat(M_PLAIN);
    col(0.45f, 0.46f, 0.5f); setSpec(0.8f);
    for (int k = -2; k <= 1; k++) cylBetween({-3.0f + k * 0.04f + 0.02f, 0.02f, -2.37f}, {-2.98f + k * 0.04f + 0.02f, 0.3f, -2.39f}, 0.006f);
    box(-2.98f, 0.31f, -2.39f, 0.18f, 0.02f, 0.02f);
    box(-2.5f, 0.2f, -2.38f, 0.22f, 0.3f, 0.02f);                       // shovel blade
    box(-1.75f, 0.1f, -2.36f, 0.4f, 0.03f, 0.03f);                      // rake head
    for (int k = 0; k < 7; k++) box(-1.93f + k * 0.06f, 0.06f, -2.33f, 0.01f, 0.06f, 0.01f);
    setSpec(0.12f);
    // shelf with jars and a lantern
    setMat(M_WOOD); col(0.48f, 0.30f, 0.15f); box(0.4f, 1.55f, -2.42f, 1.2f, 0.04f, 0.25f); setMat(M_PLAIN);
    const Col jar[3] = {{0.75f, 0.55f, 0.25f}, {0.55f, 0.65f, 0.35f}, {0.8f, 0.8f, 0.75f}};
    for (int i = 0; i < 3; i++) { col(jar[i]); setSpec(0.7f); cyl(0.0f + i * 0.18f, 1.57f, -2.42f, 0.06f, 0.16f); }
    col(0.2f, 0.2f, 0.22f); cyl(0.72f, 1.57f, -2.42f, 0.07f, 0.03f); cyl(0.72f, 1.83f, -2.42f, 0.05f, 0.04f);
    unlit(); col(1.0f, 0.82f, 0.45f); cyl(0.72f, 1.6f, -2.42f, 0.055f, 0.23f); lit();
    setSpec(0.12f);
    // feed sacks
    setMat(M_FABRIC, 6); col(0.70f, 0.58f, 0.40f);
    sph(-4.4f, 0.3f, -0.4f, 0.28f, 0.32f, 0.22f); sph(-4.35f, 0.62f, -0.4f, 0.18f, 0.08f, 0.14f);
    sph(-4.45f, 0.28f, 0.15f, 0.27f, 0.3f, 0.22f);
    setMat(M_PLAIN);
    // hay bales next to the trough
    setMat(M_STRAW); col(0.92f, 0.80f, 0.42f);
    box(3.6f, 0.3f, -1.6f, 0.9f, 0.6f, 0.6f); box(3.55f, 0.9f, -1.65f, 0.9f, 0.6f, 0.6f); box(4.1f, 0.3f, -0.9f, 0.6f, 0.6f, 0.9f);
    setMat(M_PLAIN);
    // hook on the back wall (the rope hangs here)
    col(0.3f, 0.3f, 0.32f); setSpec(0.8f);
    glPushMatrix(); glTranslatef(2.6f, 1.55f, -2.55f); glRotatef(90, 1, 0, 0); cyl(0, 0, 0, 0.02f, 0.12f); glPopMatrix();
    setSpec(0.12f);
    // bench for the milking pail
    setMat(M_WOOD); col(0.47f, 0.30f, 0.15f);
    box(-1.9f, 0.45f, 1.25f, 0.8f, 0.05f, 0.4f);
    for (int i = 0; i < 4; i++) box(-1.9f + (i % 2 ? 0.33f : -0.33f), 0.22f, 1.25f + (i / 2 ? 0.15f : -0.15f), 0.05f, 0.44f, 0.05f);
    setMat(M_PLAIN);
}

void scene4(float t) {
    sky2D({0.3f, 0.2f, 0.1f}, {0.3f, 0.2f, 0.1f});
    float cm = gCamMotion ? t : 0;
    gShadowExtent = 6.5f;
    begin3D({0.9f + 1.3f * sinf(cm * 0.06f), 2.1f, 5.2f}, {0.3f, 0.95f, 0.0f}, 50);
    setLight({0.45f, 0.8f, 0.7f}, {1.0f, 0.88f, 0.70f}, {0.40f, 0.35f, 0.30f});
    noFog();

    // shed shell
    setMat(M_STRAW); col(0.86f, 0.73f, 0.42f); box(0, -0.05f, 0, 12, 0.1f, 8);
    setMat(M_PLAIN);
    if (!shadowPass()) {
        col(0.74f, 0.62f, 0.30f);
        gRng = 99;
        for (int i = 0; i < 120; i++) {
            float x = -5.5f + rnd() * 11, z = -2.5f + rnd() * 6.0f;
            glPushMatrix(); glTranslatef(x, 0.006f, z); glRotatef(rnd() * 360, 0, 1, 0); box(0, 0, 0, 0.3f, 0.008f, 0.012f); glPopMatrix();
        }
    }
    setMat(M_WOODWALL); col(0.56f, 0.36f, 0.20f); box(0, 2, -2.6f, 12, 4, 0.1f);
    col(0.52f, 0.34f, 0.19f); box(-5.0f, 2, 0, 0.1f, 4, 6);
    setMat(M_WOOD); col(0.42f, 0.26f, 0.14f); box(0, 3.3f, -2.4f, 12, 0.25f, 0.3f);
    box(-3.5f, 1.6f, -2.5f, 0.2f, 3.2f, 0.15f); box(4.3f, 1.6f, -2.5f, 0.2f, 3.2f, 0.15f);
    setMat(M_PLAIN);
    if (!shadowPass()) { unlit(); col(0.70f, 0.88f, 1.0f); box(-1.0f, 2.3f, -2.54f, 1.2f, 0.7f, 0.02f); lit(); }
    setMat(M_WOOD); col(0.42f, 0.26f, 0.14f); box(-1.0f, 2.3f, -2.52f, 0.06f, 0.7f, 0.04f); box(-1.0f, 2.3f, -2.52f, 1.2f, 0.06f, 0.04f);
    setMat(M_STRAW); col(0.92f, 0.80f, 0.42f);
    sph(-3.9f, 0.2f, -1.6f, 1.0f, 0.7f, 0.75f);
    setMat(M_WOOD); col(0.45f, 0.29f, 0.14f); box(2.3f, 1.3f, 0, 0.2f, 2.6f, 0.2f);       // tying post
    setMat(M_PLAIN);
    col(0.55f, 0.55f, 0.6f); setSpec(0.9f);
    glPushMatrix(); glTranslatef(2.18f, 1.05f, 0); glRotatef(90, 0, 0, 1); torus(0.06f, 0.012f, 16, 6); glPopMatrix();
    setSpec(0.12f);
    shedDetails();
    drawChicken({-2.6f, 0, 1.5f}, 200, sinf(t * 4) > 0.5f ? 1.0f : 0.0f);
    drawChicken({-3.1f, 0, 1.0f}, 120 + 20 * sinf(t * 0.5f), sinf(t * 5 + 1) > 0.6f ? 1.0f : 0.0f);

    const V3 STOOL = {-0.4f, 0, 0.8f}, BUCKET = {-1.35f, 0, 0.05f}, PAILSPOT = {-0.4f, 0, 0.12f};
    const V3 BENCH_PAIL = {-1.9f, 0.475f, 1.25f}, TROUGH = {1.3f, 0, 0}, ROPE_HOOK = {2.6f, 1.55f, -2.45f};

    // stool and water bucket with sponge
    setMat(M_WOOD); col(0.48f, 0.31f, 0.15f);
    cyl(STOOL.x, 0.33f, STOOL.z, 0.2f, 0.06f);
    for (int i = 0; i < 3; i++) { float a = i * 2.09f; cyl(STOOL.x + 0.13f * cosf(a), 0, STOOL.z + 0.13f * sinf(a), 0.025f, 0.34f); }
    setMat(M_PLAIN);
    glPushMatrix(); glTranslatef(BUCKET.x, 0, BUCKET.z); pail(0); glPopMatrix();
    col(0.45f, 0.72f, 0.95f); setSpec(0.9f); cyl(BUCKET.x, 0.27f, BUCKET.z, 0.185f, 0.02f); setSpec(0.12f);
    bool spongeInHand = t > 17.5f && t < 21.9f;
    if (!spongeInHand) { col(0.98f, 0.86f, 0.2f); setMat(M_HAIR); box(BUCKET.x + 0.05f, 0.31f, BUCKET.z, 0.13f, 0.06f, 0.1f); setMat(M_PLAIN); }

    // trough: filled with hay at ~11.3 s, the cow eats from it
    float hay = ease(ph(t, 11.0f, 11.6f)) * (1 - 0.35f * ph(t, 12.0f, 36.0f));
    trough(TROUGH, hay);
    float eat = ease(ph(t, 11.4f, 12.3f));
    float dip = 4 * sinf(t * 1.3f) * (1 - eat) + eat * (46 + 6 * sinf(t * 2.6f));
    Place cow = {{0, 0, 0}, 0, 1.0f};
    bool moo = t > 1.0f && t < 2.4f;
    drawCow(cow, t, dip, sinf(t * 2.2f), moo, eat > 0.5f);
    V3 neck = cowCollar(cow, dip), post = {2.18f, 1.05f, 0};
    float rope = ease(ph(t, 5.5f, 7.3f));
    if (rope > 0) {
        setMat(M_FABRIC, 9); col(0.76f, 0.60f, 0.34f);
        V3 end = lerp3(neck, post, rope), mid = lerp3(neck, end, 0.5f) - V3{0, 0.1f * rope, 0};
        cylBetween(neck, mid, 0.02f); cylBetween(mid, end, 0.02f); sph(mid, 0.02f);
        setMat(M_PLAIN);
    }
    if (t < 3.6f) ropeCoil(ROPE_HOOK);

    // ---- the farmer ----
    std::vector<Key> path = {
        {0, 4.3f, 1.7f}, {1.0f, 3.3f, 0.6f}, {3.0f, 2.6f, -1.95f}, {4.2f, 2.6f, -1.95f},          // rope hook
        {5.3f, 1.75f, -0.72f}, {7.6f, 1.75f, -0.72f},                                            // tie the cow
        {8.6f, 3.0f, -1.15f}, {9.5f, 3.0f, -1.15f},                                              // hay bales
        {10.6f, 1.3f, -0.82f}, {11.6f, 1.3f, -0.82f},                                            // trough
        {12.5f, 2.85f, -0.55f}, {13.7f, 2.85f, 1.15f}, {16.4f, -0.95f, 1.3f}, {17.1f, -0.95f, 0.55f},
        {22.1f, -0.95f, 0.55f},                                                                  // wash
        {22.8f, -1.45f, 1.15f}, {24.0f, -1.45f, 1.15f},                                          // fetch the pail
        {24.4f, -0.95f, 1.45f}, {25.1f, 0.05f, 1.45f}, {25.8f, 0.05f, 0.72f}};                   // to the cow
    Track tr = track(path, t, 0);
    Pose q; q.moving = tr.moving; q.walk = t * 9;
    Place pl = {tr.pos, tr.heading, 1.0f};
    auto turn = [&](float t0, float from, float to) { pl.heading = angLerp(from, to, ease(ph(t, t0, t0 + 0.35f))); };
    float pailFill = ph(t, 28.8f, 35.4f);
    bool pailInHand = t > 23.7f && t < 26.0f, pailOnFloor = t > 26.25f;
    bool ropeInHand = t > 3.6f && t < 5.5f, hayInArms = t > 9.1f && t < 10.9f;
    V3 carry = pl.pos + rotY({0, 1.02f, 0.38f}, pl.heading);                // where she holds the hay

    if (t >= 3.0f && t < 4.2f) {                                            // take the rope off the hook
        turn(3.0f, -164, 180);
        q.lean = 15 * sinf(PI * ph(t, 3.0f, 4.2f));
        q.L = aimArm(pl, q, 1, ROPE_HOOK + V3{0, -0.1f, 0.05f}, true);
    } else if (t >= 5.3f && t < 7.6f) {                                    // tie it from the collar to the post
        turn(5.3f, -35, 0);
        V3 knot = lerp3(neck, post, fmaxf(rope, 0.05f));
        q.L = aimArm(pl, q, 1, knot + V3{0.05f * sinf(t * 10), 0, 0}, true);
        q.R = aimArm(pl, q, -1, knot - V3{0.12f, 0.05f * sinf(t * 10), 0}, true);
    } else if (t >= 8.6f && t < 9.5f) {                                    // pick up an armful of hay
        turn(8.6f, 109, 127);
        q.lean = 25 * sinf(PI * ph(t, 8.6f, 9.5f));
        V3 bale = {3.4f, 1.2f, -1.45f};
        if (!hayInArms) { q.L = aimArm(pl, q, 1, bale + V3{0.15f, 0, 0}, true); q.R = aimArm(pl, q, -1, bale - V3{0.15f, 0, 0}, true); }
    } else if (t >= 10.6f && t < 11.6f) {                                  // drop it into the trough
        turn(10.6f, -78, 0);
        q.lean = 30 * sinf(PI * ph(t, 10.6f, 11.6f));
        if (!hayInArms) { V3 tc = TROUGH + V3{0, 0.6f, -0.25f}; q.L = aimArm(pl, q, 1, tc + V3{0.15f, 0, 0}, true); q.R = aimArm(pl, q, -1, tc - V3{0.15f, 0, 0}, true); }
    } else if (t >= 17.1f && t < 22.1f) {                                  // wash the udder
        turn(17.1f, 180, 135);
        V3 water = BUCKET + V3{0, 0.32f, 0};
        if (t < 17.9f) { q.lean = 35 * sinf(PI * ph(t, 17.1f, 17.9f)); q.L = aimArm(pl, q, 1, water, true); }
        else if (t < 21.5f) { q.lean = 26; q.L = aimArm(pl, q, 1, {-0.4f + 0.12f * sinf(t * 9), 0.62f, 0.18f}, true); }
        else { q.lean = 35 * sinf(PI * ph(t, 21.5f, 22.1f)); q.L = aimArm(pl, q, 1, water, true); }
        if (spongeInHand) q.holdL = H_SPONGE;
    } else if (t >= 22.8f && t < 24.0f) {                                  // wait for the water to drip off, take the pail
        turn(22.8f, -40, -90);
        if (t > 23.3f) { q.lean = 25 * sinf(PI * ph(t, 23.3f, 24.0f)); q.R = aimArm(pl, q, -1, BENCH_PAIL + V3{0, 0.4f, 0}, true); }
    } else if (t >= 25.8f && t < 26.6f) {                                  // place the pail under the udder
        turn(25.8f, 180, -142);
        q.lean = 38 * sinf(PI * ph(t, 25.8f, 26.6f));
        q.R = aimArm(pl, q, -1, PAILSPOT + V3{0, 0.45f, 0}, true);
    } else if (t >= 26.6f) {
        if (t < 26.9f) turn(26.6f, -142, 90);
        else if (t < 27.8f) {                                               // sit down on the stool
            float u = ease(ph(t, 26.9f, 27.8f));
            q.sit = true; q.sitAngle = 90 * u;
            q.lean = 25 * sinf(PI * u);
            pl = {{0.05f - 0.45f * sinf(rad(q.sitAngle)), 0, 0.8f}, 90, 1.0f};
        } else {
            q.sit = true;
            pl = {{STOOL.x, 0, STOOL.z}, angLerp(90, 180, ease(ph(t, 27.8f, 28.5f))), 1.0f};
            if (t > 28.5f) {
                q.lean = 25;
                bool left = sinf(t * 10) > 0;
                V3 tA = cowTeat(cow, 0), tB = cowTeat(cow, 1);
                q.L = aimArm(pl, q, 1, tA + V3{0, left ? 0.0f : 0.07f, 0}, true);
                q.R = aimArm(pl, q, -1, tB + V3{0, left ? 0.07f : 0.0f, 0}, true);
                if (t > 28.8f && t < 35.5f) {
                    V3 s = left ? tA : tB;
                    col(1, 1, 1); cylBetween(s, {PAILSPOT.x + (s.x - PAILSPOT.x) * 0.3f, 0.04f + 0.34f * pailFill, 0.1f}, 0.011f);
                }
            }
        }
    }
    if (hayInArms) {                                                         // both arms around the hay
        q.L = aimArm(pl, q, 1, carry + rotY({0.17f, 0, 0}, pl.heading), true);
        q.R = aimArm(pl, q, -1, carry - rotY({0.17f, 0, 0}, pl.heading), true);
    }
    if (pailInHand) q.holdR = H_PAIL;
    drawPerson(FARMER, pl, q);

    if (ropeInHand) ropeCoil(handWorld(pl, q, 1));
    if (hayInArms) hayBundle(carry, 1.0f);
    else if (t > 10.9f && t < 11.3f) hayBundle(lerp3(carry, TROUGH + V3{0, 0.45f, 0}, ease(ph(t, 10.9f, 11.3f))), 1.0f);

    // milking pail: on the bench, in her hand, being lowered, or under the udder
    if (t < 23.7f) { glPushMatrix(); glTranslatef(BENCH_PAIL.x, BENCH_PAIL.y, BENCH_PAIL.z); pail(0); glPopMatrix(); }
    else if (pailOnFloor) { glPushMatrix(); glTranslatef(PAILSPOT.x, 0, PAILSPOT.z); pail(pailFill); glPopMatrix(); }
    else if (t >= 26.0f) {
        V3 h = handWorld(pl, q, -1) - V3{0, 0.42f * 0.85f, 0};
        V3 p = lerp3(h, PAILSPOT, ease(ph(t, 26.0f, 26.25f)));
        glPushMatrix(); glTranslatef(p.x, p.y, p.z); pail(0); glPopMatrix();
    }
    if (!shadowPass()) {
        unlit();
        if (spongeInHand) {                                                  // water dripping from the sponge
            V3 s = handWorld(pl, q, 1);
            for (int i = 0; i < 8; i++) {
                float k = fmodf(t * 1.6f + i / 8.0f, 1.0f);
                col(0.55f, 0.78f, 1.0f, 0.9f * (1 - k));
                sph(s.x + ((i * 7) % 5 - 2) * 0.015f, s.y - 0.05f - k * (s.y - 0.05f), s.z + ((i * 3) % 5 - 2) * 0.015f, 0.016f);
            }
        }
        if (t > 18.2f && t < 21.9f)                                         // soap on the udder
            for (int i = 0; i < 6; i++) {
                float k = fmodf(t * 0.8f + i / 6.0f, 1.0f);
                col(1, 1, 1, 0.6f * (1 - k)); sph(-0.5f + i * 0.04f, 0.68f + k * 0.2f, 0.2f, 0.025f + (i % 3) * 0.01f);
            }
        if (t > 18.5f && t < 24.3f) {                                       // washing water drips off the udder
            float fade = 1 - ph(t, 23.3f, 24.3f);
            for (int i = 0; i < 10; i++) {
                float k = fmodf(t * 1.3f + i / 10.0f, 1.0f);
                col(0.6f, 0.8f, 1.0f, 0.85f * (1 - k) * fade);
                sph(-0.48f + (i % 4) * 0.05f, 0.42f - k * 0.4f, 0.0f + ((i * 3) % 4) * 0.04f, 0.014f);
            }
        }
        if (t > 18.5f) {                                                     // wet patch on the straw
            glDepthMask(GL_FALSE);
            col(0.25f, 0.18f, 0.08f, 0.3f * ph(t, 18.5f, 22.0f));
            glPushMatrix(); glTranslatef(-0.38f, 0.008f, 0.06f); glScalef(0.32f, 0.001f, 0.25f); glCallList(LIST_SPHERE); glPopMatrix();
            glDepthMask(GL_TRUE);
        }
        lit();
    }
    if (moo) bubble3(cowCollar(cow, dip) + V3{0.5f, 0.8f, 0}, "Mooo!");
}

// ===========================================================================
// SCENE 5 - PACKAGING: she fills the jug from the pail once, then pours slowly
// into each bottle, taking a small step between bottles.
// ===========================================================================
void scene5(float t) {
    sky2D({0.8f, 0.85f, 0.88f}, {0.8f, 0.85f, 0.88f});
    float cm = gCamMotion ? t : 0;
    gShadowExtent = 6;
    begin3D({0.0f + 0.35f * sinf(cm * 0.15f), 2.2f, 3.7f}, {-0.4f, 1.2f, -0.2f}, 52);
    setLight({-0.35f, 0.85f, 0.75f}, {1.0f, 0.98f, 0.94f}, {0.45f, 0.47f, 0.50f});
    noFog();

    setMat(M_TILE); col(0.72f, 0.74f, 0.76f); box(0, -0.05f, 0, 10, 0.1f, 6);
    col(0.86f, 0.93f, 0.95f); box(0, 2, -1.6f, 10, 4, 0.1f);
    setMat(M_PLAIN);
    col(0.98f, 0.98f, 1.0f); box(0.6f, 2.6f, -1.53f, 2.2f, 1.0f, 0.02f);
    col(0.15f, 0.35f, 0.7f); box(0.6f, 3.03f, -1.52f, 2.2f, 0.14f, 0.02f);
    col(0.15f, 0.35f, 0.7f); text3(-0.25f, 2.85f, -1.5f, "CLEAN - COOL - FRESH");
    col(0.3f, 0.3f, 0.3f);
    text3(-0.15f, 2.6f, -1.5f, "1. Wash hands & containers", GLUT_BITMAP_HELVETICA_12);
    text3(-0.15f, 2.45f, -1.5f, "2. Filter milk while pouring", GLUT_BITMAP_HELVETICA_12);
    text3(-0.15f, 2.3f, -1.5f, "3. Seal and keep cool", GLUT_BITMAP_HELVETICA_12);
    setMat(M_WOOD); col(0.6f, 0.42f, 0.24f); box(-2.8f, 2.2f, -1.4f, 1.6f, 0.05f, 0.3f); setMat(M_PLAIN);
    for (int i = 0; i < 4; i++) { glPushMatrix(); glTranslatef(-3.4f + i * 0.4f, 2.225f, -1.4f); bottle(0, false, 1.0f); glPopMatrix(); }

    const float BX[4] = {-0.6f, -0.15f, 0.3f, 0.75f};
    const float TOPY = 0.92f, BZ = 0.15f, BS = 1.4f, ARM = 115, POUR = 48;
    const float PAIL_X = -1.6f;
    const float POUR_START[4] = {3.0f, 7.0f, 11.0f, 15.0f};       // each pour lasts 3 s, 1 s step between
    Pose q;
    q.L = {ARM, 0, 0}; q.holdL = H_JUG;
    Place pl = {{0, 0, -0.42f}, 0, 1.0f};
    V3 hand0 = handWorld(pl, q, 1);
    float handDX = hand0.x - pl.pos.x;
    float spoutDX = handDX + rotZ(JUG_SPOUT, -POUR).x;
    float dipX = PAIL_X - handDX - 0.13f;                      // jug body over the pail

    float fills[4], bucket = 1 - 0.85f * ease(ph(t, 0.6f, 1.5f));
    bool lids[4];
    for (int i = 0; i < 4; i++) {
        fills[i] = ph(t, POUR_START[i] + 0.6f, POUR_START[i] + 2.4f);
        lids[i] = t > POUR_START[i] + 3.2f;
    }
    // where she stands: by the pail, then a small slow step to each bottle
    float fx = dipX, tilt = 0;
    bool moving = false;
    if (t < 1.6f) {
        float d = sinf(PI * ph(t, 0.3f, 1.5f));                  // lower the jug into the pail and lift it
        q.L = {ARM - 35 * d, 0, 0};
        q.lean = 15 * d;
    } else {
        float prevX = dipX;
        for (int i = 0; i < 4; i++) {
            float target = BX[i] - spoutDX, s0 = (i == 0) ? 1.6f : POUR_START[i] - 1.0f;
            if (t >= s0 && t < POUR_START[i]) { fx = prevX + (target - prevX) * ease(ph(t, s0, POUR_START[i])); moving = true; }
            if (t >= POUR_START[i]) {
                fx = target;
                float u = t - POUR_START[i];
                tilt = POUR * ease(ph(u, 0.0f, 0.6f)) * (1 - ease(ph(u, 2.4f, 3.0f)));
            }
            prevX = target;
        }
        if (t > 18.0f) q.L = {ARM - 50 * ease(ph(t, 18.0f, 18.8f)), 0, 0};   // lower the jug
    }
    pl.pos.x = fx;
    q.moving = moving; q.walk = t * 5.5f; q.tilt = tilt;
    q.R = {15, -8, 10};
    drawPerson(FARMER, pl, q);
    if (tilt > 35) {
        V3 h = handWorld(pl, q, 1), s = h + rotZ(JUG_SPOUT, -tilt);
        col(1, 1, 1); cylBetween(s, {s.x, TOPY + 0.33f * BS, BZ}, 0.014f);
    }
    setMat(M_WOOD); col(0.58f, 0.38f, 0.20f); box(0, TOPY - 0.04f, 0.35f, 6.4f, 0.08f, 1.0f);
    setMat(M_WOODWALL); col(0.48f, 0.30f, 0.16f); box(0, 0.44f, 0.83f, 6.2f, 0.8f, 0.04f);
    setMat(M_WOOD); col(0.40f, 0.25f, 0.12f);
    for (int i = 0; i < 4; i++) box(-2.25f + i * 1.5f, 0.44f, 0.86f, 1.2f, 0.6f, 0.02f);
    setMat(M_PLAIN);
    col(0.85f, 0.75f, 0.3f); setSpec(0.9f);
    for (int i = 0; i < 4; i++) sph(-2.25f + i * 1.5f, 0.6f, 0.88f, 0.03f);
    setSpec(0.12f);
    glPushMatrix(); glTranslatef(PAIL_X, TOPY, BZ); pail(bucket, 1.2f); glPopMatrix();
    setMat(M_FABRIC, 2); col(0.95f, 0.95f, 0.92f); box(1.6f, TOPY + 0.01f, 0.2f, 0.5f, 0.02f, 0.4f); setMat(M_PLAIN);
    for (int i = 0; i < 4; i++) { glPushMatrix(); glTranslatef(BX[i], TOPY, BZ); bottle(fills[i], lids[i], BS); glPopMatrix(); }
}

// ===========================================================================
// SCENE 7 - MARKET (her bicycle is parked beside the stall)
// ===========================================================================
void awning(float x0, float x1, float z0, float z1, float y0, float y1, Col a, Col b, int n) {
    float w = (x1 - x0) / n;
    setMat(M_FABRIC, 6);
    for (int i = 0; i < n; i++) {
        col(i % 2 ? b : a);
        float xa = x0 + i * w, xb = xa + w;
        V3 nn = norm({0, z1 - z0, y1 - y0});
        glBegin(GL_QUADS);
        glNormal3f(nn.x, nn.y, nn.z);
        glVertex3f(xa, y0, z1); glVertex3f(xb, y0, z1); glVertex3f(xb, y1, z0); glVertex3f(xa, y1, z0);
        glEnd();
        glBegin(GL_TRIANGLES);                                            // scalloped edge
        glNormal3f(0, 0, 1);
        glVertex3f(xa, y0, z1); glVertex3f(xb, y0, z1); glVertex3f((xa + xb) / 2, y0 - 0.12f, z1);
        glEnd();
    }
    setMat(M_PLAIN);
}
void sceneMarket(float t) {
    Col bot = {0.86f, 0.92f, 1.0f}, top = {0.40f, 0.64f, 0.95f};
    sky2D(bot, top);
    cloud2D(fmodf(200 + t * 10, 1200) - 100, 540, 0.9f, {1, 1, 1});
    float cm = gCamMotion ? t : 0;
    gShadowExtent = 10;
    begin3D({-0.3f + 0.4f * sinf(cm * 0.25f), 1.95f, 4.8f}, {0.55f, 1.2f, 0}, 47);
    setLight({-0.5f, 0.8f, 0.55f}, {1.0f, 0.95f, 0.86f}, {0.44f, 0.45f, 0.50f});
    setFog(bot, 30, 90);

    ground({0.80f, 0.66f, 0.48f}, M_DIRT);
    const Col bc[5] = {{0.95f, 0.75f, 0.45f}, {0.6f, 0.8f, 0.75f}, {0.92f, 0.62f, 0.55f}, {0.75f, 0.7f, 0.9f}, {0.95f, 0.88f, 0.6f}};
    for (int i = 0; i < 5; i++) {
        float bx = -8 + i * 4.0f, bh = 4.5f + (i * 53 % 30) * 0.08f;
        setMat(M_PLASTER); col(bc[i]); box(bx, bh / 2, -7, 3.9f, bh, 3);
        col(scale(bc[i], 0.8f)); box(bx, bh + 0.1f, -5.55f, 3.95f, 0.2f, 0.25f);
        setMat(M_PLAIN);
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 3; c++) {
                col(0.95f, 0.95f, 0.92f); box(bx - 1.1f + c * 1.1f, 1.3f + r * 1.2f, -5.49f, 0.65f, 0.8f, 0.04f);
                col(0.35f, 0.48f, 0.6f); setSpec(0.9f); box(bx - 1.1f + c * 1.1f, 1.3f + r * 1.2f, -5.46f, 0.55f, 0.7f, 0.04f); setSpec(0.12f);
            }
        setMat(M_WOOD); col(0.45f, 0.28f, 0.16f); box(bx + 0.9f, 0.0f, -5.45f, 0.9f, 2.2f, 0.06f); setMat(M_PLAIN);
    }
    // fruit stall + shopper
    setMat(M_WOOD); col(0.5f, 0.32f, 0.15f);
    for (int i = 0; i < 4; i++) box(-4.2f + (i % 2) * 2.2f, 1.1f, -0.4f + (i / 2) * 1.0f, 0.08f, 2.2f, 0.08f);
    setMat(M_PLAIN);
    awning(-4.3f, -1.9f, -0.5f, 0.75f, 2.0f, 2.3f, {0.2f, 0.6f, 0.3f}, {1, 1, 1}, 6);
    setMat(M_WOOD); col(0.6f, 0.4f, 0.2f); box(-3.1f, 0.85f, 0.1f, 2.2f, 0.08f, 0.9f);
    setMat(M_WOODWALL); box(-3.1f, 0.42f, 0.5f, 2.2f, 0.8f, 0.04f); setMat(M_PLAIN);
    setSpec(0.5f);
    col(1.0f, 0.55f, 0.1f);
    for (int i = 0; i < 9; i++) sph(-3.9f + (i % 5) * 0.32f, 0.98f + (i / 5) * 0.12f, 0.0f + (i / 5) * 0.2f, 0.1f);
    col(0.85f, 0.15f, 0.12f);
    for (int i = 0; i < 6; i++) sph(-2.55f + (i % 3) * 0.2f, 0.97f, -0.2f + (i / 3) * 0.2f, 0.085f);
    col(1.0f, 0.86f, 0.2f); sph(-2.4f, 0.97f, 0.2f, 0.22f, 0.06f, 0.08f); sph(-2.35f, 1.02f, 0.32f, 0.22f, 0.06f, 0.08f);
    setSpec(0.12f);
    Pose idle; idle.L = {25 + 8 * sinf(t), 5};
    drawPerson(SHOPPER, {{-3.2f, 0, 1.1f}, 180, 1.05f}, idle);

    // milk stall
    setMat(M_WOOD); col(0.5f, 0.32f, 0.15f);
    for (int i = 0; i < 4; i++) box(-1.25f + (i % 2) * 2.5f, 1.15f, -0.45f + (i / 2) * 0.9f, 0.08f, 2.3f, 0.08f);
    setMat(M_PLAIN);
    awning(-1.4f, 1.4f, -0.6f, 0.75f, 2.15f, 2.5f, {0.85f, 0.15f, 0.15f}, {1, 1, 1}, 8);
    setMat(M_WOOD); col(0.98f, 0.95f, 0.85f); box(0, 2.85f, -0.5f, 2.2f, 0.5f, 0.06f); setMat(M_PLAIN);
    col(0.15f, 0.3f, 0.7f); text3(-0.95f, 2.78f, -0.45f, "FRESH MILK - KSh 60");

    Bike parked = {{2.55f, 0, -0.2f}, 70, 0, true, 0, true};
    drawBike(parked);
    Place fpl = {{0.3f, 0, -0.75f}, 0, 1.0f};
    V3 meet = {0.95f, 1.22f, 0.15f};
    std::vector<Key> cpath = {{0, 7, 1.0f}, {3.5f, 1.45f, 0.85f}, {9.5f, 1.45f, 0.85f}, {14, 8, 1.6f}};
    Track ct = track(cpath, t, -150);
    Place cpl = {ct.pos, ct.heading, 1.0f};

    Pose fq; fq.R = {10, -6};
    if (t > 5.0f && t < 6.6f) { fq.L = aimArm(fpl, fq, 1, meet); fq.holdL = H_BOTTLE; }
    else if (t > 8.4f && t < 10.8f) { fq.L = t < 9.2f ? aimArm(fpl, fq, 1, meet) : Arm{30, 5}; fq.holdL = H_NOTE; }
    drawPerson(FARMER, fpl, fq);

    Pose cq; cq.moving = ct.moving; cq.walk = t * 9;
    if (t > 6.6f) { cq.L = {12, 10}; cq.holdL = H_BOTTLE; }
    if (t > 5.4f && t < 6.6f) cq.R = aimArm(cpl, cq, -1, meet);
    if (t > 7.0f && t < 8.4f) { cq.R = aimArm(cpl, cq, -1, meet); cq.holdR = H_NOTE; }
    drawPerson(CUSTOMER, cpl, cq);

    int left = t > 5.0f ? 3 : 4;
    setMat(M_WOOD); col(0.60f, 0.40f, 0.21f); box(0, 0.86f, 0, 2.4f, 0.08f, 0.9f);
    setMat(M_WOODWALL); col(0.50f, 0.32f, 0.16f); box(0, 0.42f, 0.44f, 2.4f, 0.8f, 0.04f); setMat(M_PLAIN);
    col(0.96f, 0.96f, 0.92f); box(0, 0.5f, 0.47f, 1.1f, 0.3f, 0.02f);
    col(0.2f, 0.2f, 0.2f); text3(-0.33f, 0.46f, 0.5f, "Fresh daily", GLUT_BITMAP_HELVETICA_12);
    for (int i = 0; i < left; i++) { glPushMatrix(); glTranslatef(-0.9f + i * 0.32f, 0.9f, 0.05f); bottle(1, true); glPopMatrix(); }
    setMat(M_STRAW); col(0.75f, 0.6f, 0.35f); box(0.75f, 0.95f, 0.05f, 0.4f, 0.1f, 0.3f); setMat(M_PLAIN);  // basket

    V3 fh = handWorld(fpl, fq, 1);
    if (t > 6.0f && t < 6.6f) {
        V3 p = lerp3(fh, handWorld(cpl, cq, -1), ease(ph(t, 6.0f, 6.6f)));
        glPushMatrix(); glTranslatef(p.x, p.y, p.z); heldObject(H_BOTTLE, 1, 0); glPopMatrix();
    }
    float sx = 0, sy = 0;
    bool headOK = project(fpl.pos + V3{0, 1.95f, 0}, sx, sy);
    if (t > 3.6f && t < 5.2f) bubble3(cpl.pos + V3{0, 1.9f, 0}, "Fresh milk, please!", true);
    if (t > 5.2f && t < 6.8f) bubble3(fpl.pos + V3{0, 2.0f, 0}, "Here you go!");
    if (t > 7.0f && t < 8.4f) bubble3(cpl.pos + V3{0, 1.9f, 0}, "Here is KSh 60.", true);
    if (t > 8.6f && t < 10.2f) bubble3(fpl.pos + V3{0, 2.0f, 0}, "Asante sana!");
    begin2D();
    if (t > 8.4f && t < 10.4f && headOK) {
        float k = ph(t, 8.4f, 10.4f);
        col(0.1f, 0.55f, 0.15f, 1 - k);
        text(sx - 140, sy + k * 60, "+ KSh 60", GLUT_BITMAP_TIMES_ROMAN_24);
    }
}

// ===========================================================================
// Rough murram road (ride home): open countryside, no farm buildings
// ===========================================================================
void acacia(float x, float z, float s) {
    setMat(M_WOOD); col(0.36f, 0.26f, 0.18f);
    cylBetween({x, 0, z}, {x + 0.15f * s, 1.4f * s, z}, 0.1f * s, 0.07f * s);
    cylBetween({x + 0.15f * s, 1.4f * s, z}, {x - 0.6f * s, 2.15f * s, z + 0.1f * s}, 0.06f * s, 0.035f * s);
    cylBetween({x + 0.15f * s, 1.4f * s, z}, {x + 0.8f * s, 2.25f * s, z - 0.1f * s}, 0.06f * s, 0.035f * s);
    setMat(M_LEAF, x); col(0.32f, 0.46f, 0.18f);
    sph(x + 0.1f * s, 2.35f * s, z, 1.6f * s, 0.26f * s, 1.25f * s);
    sph(x - 0.7f * s, 2.2f * s, z + 0.2f * s, 0.85f * s, 0.2f * s, 0.7f * s);
    sph(x + 0.9f * s, 2.3f * s, z - 0.2f * s, 0.85f * s, 0.2f * s, 0.7f * s);
    setMat(M_PLAIN);
    blobShadow(x, z, 1.4f * s);
}
void roughRoadWorld() {
    ground({0.66f, 0.64f, 0.34f}, M_GRASS);                                 // dry grassland
    const Col MURRAM = {0.70f, 0.42f, 0.27f};
    setMat(M_DIRT);
    groundPatch(-60, 0.4f, 60, 2.4f, MURRAM, 0.006f, 1.0f);
    groundPatch(-60, 0.95f, 60, 1.12f, scale(MURRAM, 0.78f), 0.008f, 1.0f);  // wheel ruts
    groundPatch(-60, 1.6f, 60, 1.77f, scale(MURRAM, 0.78f), 0.008f, 1.0f);
    setMat(M_PLAIN);
    gRng = 4242;
    col(scale(MURRAM, 0.55f));
    for (int i = 0; i < 40; i++) {                                          // potholes
        float x = -40 + rnd() * 80, z = 0.6f + rnd() * 1.6f;
        sph(x, 0.0f, z, 0.18f + rnd() * 0.2f, 0.012f, 0.12f + rnd() * 0.12f);
    }
    for (int i = 0; i < 160; i++) {                                         // loose stones
        float x = -40 + rnd() * 80, z = 0.45f + rnd() * 1.9f, s = 0.025f + rnd() * 0.05f, g = 0.5f + rnd() * 0.2f;
        col(g, g * 0.9f, g * 0.85f); sph(x, s * 0.4f, z, s * 1.3f, s * 0.7f, s);
    }
    glCallList(LIST_SAVANNA);
    hills();
    const float ac[10][3] = {{-30, -6, 1.2f}, {-18, -9, 1.4f}, {-7, -5, 1.0f}, {4, -8, 1.3f}, {13, -4.5f, 1.1f},
                             {24, -7, 1.5f}, {33, -5, 1.0f}, {-12, 6, 1.2f}, {9, 7, 1.3f}, {28, 6.5f, 1.1f}};
    for (auto& a : ac) acacia(a[0], a[1], a[2]);
    setMat(M_LEAF, 3); col(0.35f, 0.45f, 0.2f);                             // bushes
    for (int i = 0; i < 18; i++) { float x = -36 + i * 4.3f, z = (i % 2) ? -2.8f : 3.6f; sph(x, 0.2f, z, 0.45f, 0.3f, 0.4f); }
    setMat(M_DIRT); col(0.66f, 0.38f, 0.24f);                               // termite mounds
    for (int i = 0; i < 4; i++) { glPushMatrix(); glTranslatef(-22 + i * 15.0f, 0, -3.4f); frustum(0.35f, 0.06f, 1.3f); glPopMatrix(); }
    setMat(M_PLAIN);
    // power poles and wires along the road
    for (int i = 0; i < 9; i++) {
        float x = -36 + i * 10.0f;
        setMat(M_WOOD); col(0.35f, 0.25f, 0.17f); cyl(x, 0, -0.9f, 0.08f, 5.0f); box(x, 4.7f, -0.9f, 0.08f, 0.08f, 1.0f); setMat(M_PLAIN);
        if (i < 8) {
            col(0.1f, 0.1f, 0.1f);
            for (int w = -1; w <= 1; w += 2) {
                V3 a = {x, 4.72f, -0.9f + w * 0.4f}, b = {x + 10, 4.72f, -0.9f + w * 0.4f}, m = lerp3(a, b, 0.5f) - V3{0, 0.35f, 0};
                cylBetween(a, m, 0.008f); cylBetween(m, b, 0.008f);
            }
        }
    }
}

// ===========================================================================
// Road beyond the farm (used by the bicycle scenes)
// ===========================================================================
void roadExtras(bool signpost) {
    setMat(M_DIRT); groundPatch(14, 0.6f, 48, 2.0f, DIRT); setMat(M_PLAIN);
    tree(21, -3.5f, 1.1f); tree(27, -6, 1.3f); tree(33, -3.2f, 1.0f); tree(39, -5.5f, 1.2f);
    tree(24, 5.5f, 1.0f); tree(35, 6.0f, 1.2f);
    for (int i = 0; i < 14; i++)                                             // maize field
        for (int j = 0; j < 5; j++) {
            float x = 22.5f + i * 0.55f, z = -1.0f - j * 0.6f, h = 1.5f + 0.25f * ((i * 7 + j * 3) % 4) / 3.0f;
            setMat(M_LEAF, (float)i);
            col(0.38f, 0.62f, 0.24f);
            glPushMatrix(); glTranslatef(x, 0, z); frustum(0.04f, 0.01f, h); glPopMatrix();
            col(0.32f, 0.58f, 0.22f);
            sph(x + 0.13f, h * 0.55f, z, 0.2f, 0.035f, 0.05f); sph(x - 0.11f, h * 0.72f, z, 0.18f, 0.035f, 0.05f);
            col(0.85f, 0.75f, 0.35f); sph(x, h, z, 0.03f, 0.08f, 0.03f);
        }
    setMat(M_PLAIN);
    if (signpost) {
        setMat(M_WOOD); col(0.5f, 0.33f, 0.18f);
        box(17, 0.8f, 2.35f, 0.08f, 1.6f, 0.08f);
        col(0.95f, 0.92f, 0.8f); box(17.25f, 1.45f, 2.4f, 0.7f, 0.22f, 0.04f);
        setMat(M_PLAIN);
        col(0.15f, 0.25f, 0.5f); text3(16.95f, 1.41f, 2.43f, "MARKET  >", GLUT_BITMAP_HELVETICA_12);
    }
}

// ===========================================================================
// SCENES 6 & 8 - RIDING TO THE MARKET / RIDING BACK HOME
// ===========================================================================
void rideScene(float t, bool toMarket) {
    Col bot, top;
    if (toMarket) { bot = {0.78f, 0.90f, 1.0f}; top = {0.30f, 0.56f, 0.94f}; }
    else { bot = {1.0f, 0.82f, 0.58f}; top = {0.42f, 0.55f, 0.86f}; }
    sky2D(bot, top);
    if (toMarket) sun2D(160, 500, 34, {1.0f, 0.95f, 0.6f});
    else sun2D(120, 330, 40, {1.0f, 0.75f, 0.35f});
    cloud2D(fmodf(300 + t * 14, 1200) - 100, 505, 1.0f, toMarket ? Col{1, 1, 1} : Col{1, 0.9f, 0.85f});
    cloud2D(fmodf(850 + t * 9, 1200) - 100, 455, 0.8f, toMarket ? Col{1, 1, 1} : Col{1, 0.9f, 0.85f});
    for (int i = 0; i < 3; i++) bird2D(fmodf(t * 35 + i * 30, 1100) - 50, 430 + i * 12.0f, sinf(t * 10 + i));

    float dist = 2.0f * t;                                                     // 2 m/s
    Bike b;
    b.heading = toMarket ? 0.0f : 180.0f;
    b.pos = toMarket ? V3{-5.0f + dist, 0, 1.3f} : V3{19.0f - dist, 0, 1.3f};
    if (!toMarket)                                                             // bumpy murram road
        b.pos.y = 0.025f * (0.5f + 0.5f * sinf(dist * 9.0f)) * (0.5f + 0.5f * sinf(dist * 2.3f + 1.0f));
    b.wheel = deg(dist / 0.34f);
    b.crate = true; b.bottles = toMarket ? 4 : 0; b.stand = false;
    float dir = toMarket ? 1.0f : -1.0f, bx = b.pos.x;
    gShadowExtent = 10;
    if (gCamMotion) begin3D({bx - 1.5f * dir, 1.55f, 5.6f}, {bx + 0.7f * dir, 0.95f, 1.3f}, 45);
    else begin3D({toMarket ? 6.0f : 8.0f, 2.5f, 12.0f}, {toMarket ? 6.0f : 8.0f, 1.0f, 1.3f}, 45);
    if (toMarket) setLight({-0.55f, 0.85f, 0.45f}, {1.0f, 0.96f, 0.88f}, {0.42f, 0.45f, 0.52f});
    else setLight({-0.85f, 0.42f, 0.35f}, {1.0f, 0.78f, 0.52f}, {0.38f, 0.35f, 0.40f});
    setFog(bot, 35, 100);
    if (toMarket) { farmWorld(false, 0); roadExtras(true); }
    else roughRoadWorld();
    drawBike(b);
    Place pl;
    Pose q = riderPose(b, pl);
    drawPerson(FARMER, pl, q);
    if (!toMarket && !shadowPass()) {                                          // dust behind the back wheel
        unlit(); glDepthMask(GL_FALSE);
        for (int i = 0; i < 8; i++) {
            float k = fmodf(t * 1.4f + i / 8.0f, 1.0f);
            V3 d = bikeW(b, {-0.6f - 0.9f * k, 0.08f + 0.3f * k, ((i * 5) % 7 - 3) * 0.05f * k});
            col(0.80f, 0.62f, 0.48f, 0.35f * (1 - k)); sph(d, 0.08f + 0.2f * k);
        }
        glDepthMask(GL_TRUE); lit();
    }
    if (toMarket && t > 1.0f && t < 3.2f) bubble3(pl.pos + V3{0, 1.95f, 0}, "Off to the market!");
    if (!toMarket && t > 1.0f && t < 3.2f) bubble3(pl.pos + V3{0, 1.95f, 0}, "All the milk is sold!");
}
void scene6(float t) { rideScene(t, true); }
void scene8(float t) { rideScene(t, false); }

// ===========================================================================
// SCENE 9 - HOME AT SUNSET: stops, gets off the bicycle, parks it, opens the
// door and goes inside
// ===========================================================================
void scene9(float t) {
    float p = 0.5f + 0.42f * ph(t, 0, 14);
    Col bot = mix3({1.0f, 0.80f, 0.55f}, {1.0f, 0.52f, 0.28f}, {0.30f, 0.20f, 0.36f}, p);
    Col top = mix3({0.45f, 0.55f, 0.85f}, {0.52f, 0.34f, 0.60f}, {0.05f, 0.05f, 0.18f}, p);
    sky2D(bot, top);
    stars2D(ph(p, 0.65f, 1.0f), t, 31);
    if (p > 0.7f) { col(0.95f, 0.95f, 0.85f, ph(p, 0.7f, 1)); disc2(150, 520, 22, 22); col(top); disc2(160, 526, 20, 20); }
    sun2D(720, 430 - p * 300, 44, mix({1.0f, 0.78f, 0.35f}, {1.0f, 0.38f, 0.15f}, p));
    cloud2D(fmodf(500 + t * 10, 1200) - 100, 500, 1.0f, mix({1, 0.85f, 0.8f}, {0.45f, 0.35f, 0.5f}, p));

    // the bicycle rolls in and stops
    float u = ph(t, 0, 4.5f);
    float dist = 10.6f * (1 - (1 - u) * (1 - u));
    Bike b = {{4.0f - dist, 0, 1.3f}, 180, deg(dist / 0.34f), true, 0, t > 4.6f};

    float cm = gCamMotion ? t : 0;
    gShadowExtent = 12;
    begin3D({-3.4f + 0.5f * sinf(cm * 0.12f), 2.0f, 5.6f}, {-6.4f, 1.0f, -0.7f});
    setLight({0.8f, 0.7f - 0.6f * p, -0.5f},
             mix3({1.0f, 0.92f, 0.80f}, {1.0f, 0.60f, 0.36f}, {0.22f, 0.22f, 0.40f}, p),
             mix3({0.42f, 0.42f, 0.48f}, {0.35f, 0.28f, 0.32f}, {0.13f, 0.13f, 0.24f}, p));
    setFog(bot, 30, 100);
    float doorA = 75 * ease(ph(t, 8.6f, 9.4f)) * (1 - ease(ph(t, 11.0f, 11.6f)));
    bool lightsOn = t > 11.8f;
    farmWorld(lightsOn, 0, doorA);
    drawBike(b);

    Place pl;
    Pose q;
    bool visible = true;
    if (t < 4.5f) {
        q = riderPose(b, pl);
    } else if (t < 5.1f) {                                       // slide forward off the saddle, feet down
        float k = ease(ph(t, 4.5f, 5.1f));
        Place p0;
        Pose q0 = riderPose(b, p0);
        q = q0;
        for (int i = 0; i < 2; i++) { q.legP[i] *= 1 - k; q.legK[i] *= 1 - k; }
        q.lean = 18 * (1 - k) + 5 * k;
        pl = {lerp3(p0.pos, bikeW(b, {0.05f, 0, 0}), k), p0.heading, 1.0f};
        q.L = aimArm(pl, q, 1, bikeW(b, {0.33f, 1.04f, -0.26f}), true);
        q.R = aimArm(pl, q, -1, bikeW(b, {0.33f, 1.04f, 0.26f}), true);
    } else if (t < 5.8f) {                                       // lift one leg over the low frame
        float k = ease(ph(t, 5.1f, 5.8f)), lift = sinf(PI * k);
        q.legIK = true; q.lean = 5;
        q.legP[0] = 80 * lift; q.legK[0] = 100 * lift;
        pl = {lerp3(bikeW(b, {0.05f, 0, 0}), bikeW(b, {0.05f, 0, 0.55f}), k), b.heading + 90, 1.0f};
        if (k < 0.6f) q.L = aimArm(pl, q, 1, bikeW(b, {0.33f, 1.04f, -0.26f}), true);
    } else {                                                     // walk to the door, open it, go in
        V3 s = bikeW(b, {0.05f, 0, 0.55f});
        std::vector<Key> path = {{5.8f, s.x, s.z}, {7.0f, -8.0f, 0.5f}, {8.4f, -8.0f, -2.05f},
                                 {9.4f, -8.0f, -2.05f}, {10.7f, -8.0f, -3.1f}};
        Track tr = track(path, t, 180);
        pl = {tr.pos, tr.heading, 1.0f};
        q.moving = tr.moving; q.walk = t * 9;
        if (t > 8.4f && t < 9.3f) q.L = aimArm(pl, q, 1, V3{-8.425f, 0, -2.49f} + rotY({0.725f, 0.95f, 0.05f}, doorA), true);
        visible = t < 10.6f;
    }
    if (visible) drawPerson(FARMER, pl, q);
    if (!shadowPass()) {
        unlit();
        for (int i = 0; i < 14 && p > 0.7f; i++) {
            float a = ph(p, 0.7f, 1) * (0.5f + 0.5f * sinf(t * 4 + i * 2));
            float x = -9 + 12 * fmodf(i * 0.37f + t * 0.015f * (i % 3 + 1), 1.0f);
            col(1.0f, 1.0f, 0.5f, a); sph(x, 0.5f + 0.4f * sinf(t * 0.7f + i), 2.5f + (i % 4) * 0.8f, 0.03f);
        }
        lit();
    }
    float w1x = 0, w1y = 0, w2x = 0, w2y = 0;
    bool v1 = project({-9.35f, 1.55f, -2.45f}, w1x, w1y), v2 = project({-6.65f, 1.55f, -2.45f}, w2x, w2y);
    if (t > 6.0f && t < 7.8f) bubble3(pl.pos + V3{0, 2.0f, 0}, "Home sweet home!");
    begin2D();
    if (lightsOn) {
        float a = ph(t, 11.8f, 12.5f);
        if (v1) glow2D(w1x, w1y, 14, {1.0f, 0.85f, 0.4f}, a);
        if (v2) glow2D(w2x, w2y, 14, {1.0f, 0.85f, 0.4f}, a);
    }
}

// ===========================================================================
// SCENE 10 - DINNER WITH THE RADIO PLAYING
// ===========================================================================
void note2D(float x, float y, float s, float a, bool twin) {
    SKIP2D;
    col(1.0f, 0.86f, 0.4f, a);
    disc2(x, y, 6.5f * s, 4.8f * s);
    glLineWidth(2.2f);
    glBegin(GL_LINES); glVertex2f(x + 6 * s, y); glVertex2f(x + 6 * s, y + 22 * s); glEnd();
    if (twin) {
        disc2(x + 18 * s, y + 4 * s, 6.5f * s, 4.8f * s);
        glBegin(GL_LINES); glVertex2f(x + 24 * s, y + 4 * s); glVertex2f(x + 24 * s, y + 26 * s); glEnd();
        glLineWidth(4);
        glBegin(GL_LINES); glVertex2f(x + 6 * s, y + 22 * s); glVertex2f(x + 24 * s, y + 26 * s); glEnd();
    } else {
        glBegin(GL_LINES); glVertex2f(x + 6 * s, y + 22 * s); glVertex2f(x + 13 * s, y + 14 * s); glEnd();
    }
}
void scene10(float t) {
    sky2D({0.15f, 0.12f, 0.1f}, {0.15f, 0.12f, 0.1f});
    float cm = gCamMotion ? t : 0;
    gShadowExtent = 4.5f;
    begin3D({1.25f + 0.3f * sinf(cm * 0.2f), 1.5f, 1.55f}, {-0.1f, 1.0f, -0.85f}, 50);
    setLight({0.25f, 1.0f, 0.45f}, {1.0f, 0.80f, 0.56f}, {0.30f, 0.26f, 0.26f});
    noFog();
    const float TOP = 0.75f;

    // room
    setMat(M_WOOD); col(0.55f, 0.36f, 0.2f); box(0, -0.05f, -0.5f, 6, 0.1f, 5);
    setMat(M_PLASTER); col(0.93f, 0.80f, 0.62f); box(0, 1.75f, -2.6f, 6, 3.5f, 0.1f);
    if (!shadowPass()) { col(0.90f, 0.77f, 0.58f); box(-3.0f, 1.75f, -0.5f, 0.1f, 3.5f, 5); }
    setMat(M_WOOD); col(0.45f, 0.28f, 0.15f); box(0, 0.08f, -2.53f, 6, 0.16f, 0.04f);
    setMat(M_PLAIN);
    // window: night outside
    if (!shadowPass()) {
        unlit();
        glBegin(GL_QUADS);
        col(0.10f, 0.12f, 0.30f); glVertex3f(0.6f, 1.2f, -2.54f); glVertex3f(1.8f, 1.2f, -2.54f);
        col(0.02f, 0.03f, 0.12f); glVertex3f(1.8f, 2.2f, -2.54f); glVertex3f(0.6f, 2.2f, -2.54f);
        glEnd();
        col(0.95f, 0.95f, 0.85f); glPushMatrix(); glTranslatef(1.5f, 1.95f, -2.535f); glScalef(0.08f, 0.08f, 0.001f); glCallList(LIST_SPHERE); glPopMatrix();
        col(1, 1, 1);
        for (int i = 0; i < 9; i++) { glPushMatrix(); glTranslatef(0.7f + fmodf(i * 0.37f, 1.0f), 1.3f + fmodf(i * 0.23f, 0.85f), -2.535f); glScalef(0.008f, 0.008f, 0.001f); glCallList(LIST_SPHERE); glPopMatrix(); }
        lit();
    }
    setMat(M_WOOD); col(0.95f, 0.95f, 0.92f);
    box(1.2f, 1.2f, -2.5f, 1.3f, 0.07f, 0.1f); box(1.2f, 2.2f, -2.5f, 1.3f, 0.07f, 0.1f);
    box(0.6f, 1.7f, -2.5f, 0.07f, 1.0f, 0.1f); box(1.8f, 1.7f, -2.5f, 0.07f, 1.0f, 0.1f); box(1.2f, 1.7f, -2.51f, 0.04f, 1.0f, 0.05f);
    setMat(M_FABRIC, 2); col(0.85f, 0.65f, 0.2f);
    for (int i = 0; i < 3; i++) { box(0.35f + i * 0.07f, 1.75f, -2.47f + (i % 2) * 0.03f, 0.08f, 1.3f, 0.04f); box(1.95f + i * 0.07f, 1.75f, -2.47f + (i % 2) * 0.03f, 0.08f, 1.3f, 0.04f); }
    setMat(M_PLAIN);
    // shelf with plates, a picture
    setMat(M_WOOD); col(0.5f, 0.32f, 0.17f); box(-1.4f, 1.55f, -2.45f, 1.2f, 0.05f, 0.25f); setMat(M_PLAIN);
    for (int i = 0; i < 3; i++) {
        glPushMatrix(); glTranslatef(-1.8f + i * 0.4f, 1.73f, -2.48f); glRotatef(90, 1, 0, 0);
        col(i == 1 ? Col{0.2f, 0.45f, 0.75f} : Col{0.95f, 0.95f, 0.92f}); setSpec(0.6f); cyl(0, 0, 0, 0.15f, 0.02f); setSpec(0.12f);
        glPopMatrix();
    }
    setMat(M_WOOD); col(0.4f, 0.25f, 0.13f); box(-0.4f, 2.25f, -2.53f, 0.6f, 0.45f, 0.03f); setMat(M_PLAIN);
    col(0.9f, 0.6f, 0.3f); box(-0.4f, 2.25f, -2.51f, 0.5f, 0.35f, 0.01f);
    col(0.25f, 0.5f, 0.3f); box(-0.4f, 2.14f, -2.505f, 0.5f, 0.13f, 0.01f);

    // hanging lamp
    float lampY = 2.0f;
    col(0.15f, 0.15f, 0.15f); cylBetween({0, 3.6f, -0.4f}, {0, lampY + 0.18f, -0.4f}, 0.008f);
    col(0.85f, 0.55f, 0.2f); setSpec(0.5f);
    glPushMatrix(); glTranslatef(0, lampY, -0.4f); frustum(0.24f, 0.07f, 0.18f, false); glPopMatrix();
    setSpec(0.12f);
    unlit(); col(1.0f, 0.93f, 0.7f); sph(0, lampY + 0.03f, -0.4f, 0.06f); lit();

    // table, cloth, chair
    setMat(M_WOOD); col(0.5f, 0.3f, 0.15f);
    box(0, TOP - 0.035f, -0.35f, 1.5f, 0.05f, 0.9f);
    for (int i = 0; i < 4; i++) box((i % 2 ? 0.68f : -0.68f), (TOP - 0.06f) / 2, -0.35f + (i / 2 ? 0.38f : -0.38f), 0.06f, TOP - 0.06f, 0.06f);
    setMat(M_FABRIC, 3); col(0.94f, 0.92f, 0.86f); box(0, TOP - 0.005f, -0.35f, 1.3f, 0.012f, 0.72f);
    col(0.75f, 0.2f, 0.2f);
    for (int i = 0; i < 5; i++) box(-0.52f + i * 0.26f, TOP + 0.002f, -0.35f, 0.05f, 0.004f, 0.72f);
    setMat(M_WOOD); col(0.45f, 0.27f, 0.13f);
    box(0, 0.42f, -1.1f, 0.46f, 0.05f, 0.44f); box(0, 0.85f, -1.33f, 0.46f, 0.8f, 0.05f);
    for (int i = 0; i < 4; i++) box((i % 2 ? 0.2f : -0.2f), 0.21f, -1.1f + (i / 2 ? 0.19f : -0.19f), 0.04f, 0.42f, 0.04f);
    setMat(M_PLAIN);

    // dinner: ugali, sukuma wiki, stew, and a cup of tea
    col(0.97f, 0.97f, 0.95f); setSpec(0.6f); cyl(0, TOP, -0.55f, 0.15f, 0.015f); setSpec(0.12f);
    col(0.97f, 0.95f, 0.88f); sph(-0.04f, TOP + 0.03f, -0.57f, 0.07f, 0.05f, 0.06f);
    setMat(M_LEAF, 2); col(0.18f, 0.42f, 0.15f);
    sph(0.06f, TOP + 0.025f, -0.5f, 0.06f, 0.02f, 0.05f); sph(0.05f, TOP + 0.03f, -0.6f, 0.05f, 0.02f, 0.04f);
    setMat(M_PLAIN);
    col(0.55f, 0.25f, 0.12f); sph(0.0f, TOP + 0.02f, -0.45f, 0.05f, 0.015f, 0.04f);
    col(0.88f, 0.88f, 0.9f); setSpec(0.7f); cyl(0.36f, TOP, -0.42f, 0.045f, 0.1f);
    glPushMatrix(); glTranslatef(0.41f, TOP + 0.05f, -0.42f); glRotatef(90, 1, 0, 0); torus(0.03f, 0.008f, 12, 6); glPopMatrix();
    col(0.6f, 0.38f, 0.2f); cyl(0.36f, TOP + 0.088f, -0.42f, 0.04f, 0.01f); setSpec(0.12f);

    // the radio
    const V3 R0 = {-0.5f, TOP, -0.66f};
    setMat(M_WOOD); col(0.55f, 0.2f, 0.12f); box(R0.x, R0.y + 0.1f, R0.z, 0.36f, 0.2f, 0.14f); setMat(M_PLAIN);
    col(0.12f, 0.1f, 0.08f);
    glPushMatrix(); glTranslatef(R0.x - 0.07f, R0.y + 0.1f, R0.z + 0.071f); glRotatef(90, 1, 0, 0); cyl(0, 0, 0, 0.065f, 0.006f); glPopMatrix();
    col(0.35f, 0.3f, 0.25f);
    for (int k = 0; k < 4; k++) box(R0.x - 0.07f, R0.y + 0.06f + k * 0.027f, R0.z + 0.078f, 0.11f, 0.006f, 0.004f);
    unlit(); col(1.0f, 0.75f, 0.3f); box(R0.x + 0.09f, R0.y + 0.14f, R0.z + 0.072f, 0.12f, 0.04f, 0.004f); lit();
    col(0.85f, 0.82f, 0.75f); setSpec(0.7f);
    sph(R0.x + 0.06f, R0.y + 0.065f, R0.z + 0.075f, 0.018f); sph(R0.x + 0.12f, R0.y + 0.065f, R0.z + 0.075f, 0.018f);
    col(0.8f, 0.8f, 0.82f); setSpec(1.0f);
    cylBetween({R0.x + 0.14f, R0.y + 0.2f, R0.z - 0.03f}, {R0.x + 0.32f, R0.y + 0.62f, R0.z - 0.1f}, 0.004f);
    setSpec(0.12f);
    col(0.3f, 0.15f, 0.08f); box(R0.x, R0.y + 0.215f, R0.z, 0.2f, 0.015f, 0.025f);

    // her: eating with a spoon, left hand resting on the table
    Place pl = {{0, 0, -1.05f}, 0, 1.0f};
    col(0.82f, 0.83f, 0.86f); setSpec(1.0f);                               // spoon resting on the plate
    cylBetween({0.04f, TOP + 0.02f, -0.62f}, {0.16f, TOP + 0.025f, -0.48f}, 0.006f);
    sph(0.02f, TOP + 0.022f, -0.64f, 0.03f, 0.008f, 0.022f);
    setSpec(0.12f);
    Pose q; q.sit = true; q.sitAngle = 90; q.lean = 6; q.smile = 1;
    // she waves now and then; otherwise her hands rest on the table
    float wave = ease(ph(t, 1.5f, 2.1f)) * (1 - ease(ph(t, 5.0f, 5.6f))) + ease(ph(t, 8.2f, 8.8f)) * (1 - ease(ph(t, 11.4f, 12.0f)));
    Arm rest = aimArm(pl, q, -1, {-0.3f, TOP + 0.05f, -0.72f}, true);
    Arm up = {150, -25 + 22 * sinf(t * 9), 35};
    q.R = {rest.pitch + (up.pitch - rest.pitch) * wave, rest.roll + (up.roll - rest.roll) * wave, rest.elbow + (up.elbow - rest.elbow) * wave};
    q.L = aimArm(pl, q, 1, {0.3f, TOP + 0.05f, -0.75f}, true);
    drawPerson(FARMER, pl, q);

    // overlays: lamp glow, music notes from the radio, closing words
    float lx = 0, ly = 0, rx = 0, ry = 0;
    bool lv = project({0, lampY + 0.03f, -0.4f}, lx, ly), rv = project(R0 + V3{-0.07f, 0.2f, 0}, rx, ry);
    if (t > 8.6f && t < 11.2f) bubble3(pl.pos + V3{0, 1.6f, 0.1f}, "Good night!");
    begin2D();
    if (lv) glow2D(lx, ly, 22, {1.0f, 0.85f, 0.5f}, 0.5f);
    if (rv)
        for (int i = 0; i < 5; i++) {
            float k = fmodf(t * 0.33f + i * 0.2f, 1.0f);
            note2D(rx + 10 + 70 * k + 14 * sinf(t * 2 + i * 1.7f), ry + 15 + 140 * k, 0.9f + 0.3f * (i % 2),
                   ph(k, 0, 0.15f) * (1 - k), i % 2 == 0);
        }
    if (t > 10.2f) {
        float a = ph(t, 10.2f, 11.2f);
        col(0, 0, 0, 0.35f * a); rect(W / 2 - 220, 330, 440, 100);
        col(1, 0.95f, 0.8f, a);
        textC(W / 2, 395, "THE END", GLUT_BITMAP_TIMES_ROMAN_24);
        textC(W / 2, 360, "A Day in the Life of a Farmer", GLUT_BITMAP_HELVETICA_18);
    }
}

// ===========================================================================
// Frame
// ===========================================================================
void runScene() {
    gCaption = nullptr; gGauge = -1; gHud[0] = 0;
    switch (gScene) {
    case 0: scene1(gT); break;
    case 1: scene2(gT); break;
    case 2: scene3(gT); break;
    case 3: scene4(gT); break;
    case 4: scene5(gT); break;
    case 5: scene6(gT); break;
    case 6: sceneMarket(gT); break;
    case 7: scene8(gT); break;
    case 8: scene9(gT); break;
    case 9: scene10(gT); break;
    }
}
void vignette() {
    glBegin(GL_TRIANGLE_FAN);
    col(0, 0, 0, 0); glVertex2f(W / 2, H / 2);
    for (int i = 0; i <= 48; i++) {
        float a = 2 * PI * i / 48;
        col(0, 0, 0, 0.38f); glVertex2f(W / 2 + cosf(a) * W * 0.75f, H / 2 + sinf(a) * H * 0.75f);
    }
    glEnd();
}
// Only one caption bar, at the top of the screen
void drawOverlay() {
    begin2D();
    if (useShader()) vignette();
    const char* cap = gCaption ? gCaption : DESCS[gScene];
    col(0, 0, 0, 0.42f); rect(0, H - 46, W, 46);
    col(1, 1, 1); text(18, H - 29, cap);
    if (gGauge >= 0) {
        col(1, 1, 1); text(W - 222, H - 28, "Pail", GLUT_BITMAP_HELVETICA_12);
        col(0.3f, 0.3f, 0.3f); rect(W - 185, H - 32, 165, 16);
        col(0.98f, 0.98f, 0.95f); rect(W - 185, H - 32, 165 * gGauge, 16);
    } else if (gHud[0]) {
        col(1.0f, 0.88f, 0.4f); text(W - textW(gHud, GLUT_BITMAP_HELVETICA_18) - 18, H - 29, gHud);
    }
    float d = SCENE_DUR[gScene];
    float a = fmaxf(1 - gT / 0.8f, (gT - (d - 0.8f)) / 0.8f);
    if (a > 0) { col(0, 0, 0, clampf(a, 0, 1)); rect(0, 0, W, H); }
}

void display() {
    if (useShader()) {                                   // pass 1: depth from the sun
        gPass = PASS_SHADOW;
        pUseProgram(0);
        pBindFramebuffer(GL_FRAMEBUFFER, gShadowFBO);
        glViewport(0, 0, SHADOW_SIZE, SHADOW_SIZE);
        glClear(GL_DEPTH_BUFFER_BIT);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glDisable(GL_LIGHTING); glDisable(GL_BLEND);
        glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(1.5f, 3.0f);
        runScene();
        glDisable(GL_POLYGON_OFFSET_FILL);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glEnable(GL_BLEND);
        pBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(gVP[0], gVP[1], gVP[2], gVP[3]);
    }
    gPass = PASS_MAIN;                                   // pass 2: the real image
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    runScene();
    drawOverlay();
    glutSwapBuffers();
}

void reshape(int w, int h) {
    float aspect = W / H;
    int vw = w, vh = (int)(w / aspect);
    if (vh > h) { vh = h; vw = (int)(h * aspect); }
    gVP[0] = (w - vw) / 2; gVP[1] = (h - vh) / 2; gVP[2] = vw; gVP[3] = vh;
    glViewport(gVP[0], gVP[1], gVP[2], gVP[3]);
}

void goScene(int s) { gScene = (s + NUM_SCENES) % NUM_SCENES; gT = 0; }

void timer(int) {
    int now = glutGet(GLUT_ELAPSED_TIME);
    float dt = (now - gLastMs) / 1000.0f;
    gLastMs = now;
    if (dt > 0.1f) dt = 0.1f;
    if (!gPaused) { gT += dt; if (gT > SCENE_DUR[gScene]) goScene(gScene + 1); }
    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);
}

void keyboard(unsigned char key, int, int) {
    if (key >= '1' && key <= '9') goScene(key - '1');
    else if (key == '0') goScene(9);
    else if (key == ' ') gPaused = !gPaused;
    else if (key == 'n' || key == 'N') goScene(gScene + 1);
    else if (key == 'p' || key == 'P') goScene(gScene - 1);
    else if (key == 'r' || key == 'R') goScene(0);
    else if (key == 'c' || key == 'C') gCamMotion = !gCamMotion;
    else if (key == 'l' || key == 'L') gEnhanced = !gEnhanced;
    else if (key == 27) exit(0);
}

void initGL() {
    glClearColor(0, 0, 0, 1);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_NORMALIZE);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    float spec[4] = {0.15f, 0.15f, 0.15f, 1}, white[4] = {0.25f, 0.25f, 0.25f, 1};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 20);
    glLightfv(GL_LIGHT0, GL_SPECULAR, white);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_MULTISAMPLE);
    buildLists();
    buildAfro();
    initShaders();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH | GLUT_MULTISAMPLE);
    glutInitWindowSize(1000, 600);
    glutInitWindowPosition(100, 60);
    glutCreateWindow("A Day in the Life of a Farmer - 3D OpenGL");
    initGL();
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    gLastMs = glutGet(GLUT_ELAPSED_TIME);
    glutTimerFunc(16, timer, 0);
    std::printf("Keys: 1-9 and 0 = scene, N/P next/prev, SPACE pause, R restart, C camera motion, L enhanced rendering, ESC quit\n");
    glutMainLoop();
    return 0;
}
