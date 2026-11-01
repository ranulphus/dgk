/* dgk/vec.h - small vector helpers: dgk_v3 (floats, for drawing) and
 * dgk_d3 (doubles, for positions too large for a float's 24 bits: a
 * star system in metres). Draw a far position relative to the camera:
 * dgk_d3_rel(p, eye) is small, and fits a float. Not for anything that
 * must agree to the bit between DOS and Linux (x87 and SSE round
 * differently): that is integer work. */
#ifndef DGK_VEC_H
#define DGK_VEC_H

#include "dgk/gfx.h"
#include <math.h>

typedef struct { double x, y, z; } dgk_d3;

static inline dgk_v3 dgk_v3_make(float x, float y, float z) { dgk_v3 r = { x, y, z }; return r; }
static inline dgk_v3 dgk_v3_add(dgk_v3 a, dgk_v3 b) { return dgk_v3_make(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline dgk_v3 dgk_v3_sub(dgk_v3 a, dgk_v3 b) { return dgk_v3_make(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline dgk_v3 dgk_v3_scale(dgk_v3 a, float s) { return dgk_v3_make(a.x * s, a.y * s, a.z * s); }
static inline float  dgk_v3_dot(dgk_v3 a, dgk_v3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline dgk_v3 dgk_v3_cross(dgk_v3 a, dgk_v3 b)
{
    return dgk_v3_make(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
static inline float  dgk_v3_len(dgk_v3 a) { return sqrtf(dgk_v3_dot(a, a)); }
static inline dgk_v3 dgk_v3_norm(dgk_v3 a)          /* the zero vector stays zero */
{
    float l = dgk_v3_len(a);
    return l > 0.0f ? dgk_v3_scale(a, 1.0f / l) : a;
}

static inline dgk_d3 dgk_d3_make(double x, double y, double z) { dgk_d3 r = { x, y, z }; return r; }
static inline dgk_d3 dgk_d3_add(dgk_d3 a, dgk_d3 b) { return dgk_d3_make(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline dgk_d3 dgk_d3_sub(dgk_d3 a, dgk_d3 b) { return dgk_d3_make(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline dgk_d3 dgk_d3_scale(dgk_d3 a, double s) { return dgk_d3_make(a.x * s, a.y * s, a.z * s); }
static inline double dgk_d3_dot(dgk_d3 a, dgk_d3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline dgk_d3 dgk_d3_cross(dgk_d3 a, dgk_d3 b)
{
    return dgk_d3_make(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
static inline double dgk_d3_len(dgk_d3 a) { return sqrt(dgk_d3_dot(a, a)); }
static inline double dgk_d3_dist(dgk_d3 a, dgk_d3 b) { return dgk_d3_len(dgk_d3_sub(a, b)); }
static inline dgk_d3 dgk_d3_norm(dgk_d3 a)
{
    double l = dgk_d3_len(a);
    return l > 0.0 ? dgk_d3_scale(a, 1.0 / l) : a;
}
static inline dgk_d3 dgk_d3_lerp(dgk_d3 a, dgk_d3 b, double t)
{
    return dgk_d3_make(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);
}
/* p - origin, as floats: the difference is taken in double first. */
static inline dgk_v3 dgk_d3_rel(dgk_d3 p, dgk_d3 origin)
{
    return dgk_v3_make((float)(p.x - origin.x), (float)(p.y - origin.y), (float)(p.z - origin.z));
}
static inline dgk_d3 dgk_d3_from_v3(dgk_v3 a) { return dgk_d3_make(a.x, a.y, a.z); }

#endif
