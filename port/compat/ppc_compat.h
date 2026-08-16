#ifndef PORT_COMPAT_PPC_COMPAT_H
#define PORT_COMPAT_PPC_COMPAT_H

/*
 * Metrowerks CodeWarrior PowerPC intrinsics -> portable equivalents.
 *
 * The original toolchain exposed a handful of PPC instructions as compiler
 * builtins. They appear in the maths headers (include/stl/math.h,
 * include/Vector.h, include/sysMath.h) and are the only thing standing between
 * the game sources and a modern compiler.
 *
 * Force-included ahead of every translation unit (-include). Deliberately does
 * NOT include <math.h>: with include/stl on the header search path that
 * resolves to the decomp's own math.h, which uses these intrinsics itself and
 * would therefore be parsed before the definitions below land.
 *
 * frsqrte on hardware is a fast reciprocal-square-root ESTIMATE, accurate to
 * about 1/32. The exact form below is deliberate for now -- correctness first.
 * If gameplay turns out to depend on the estimate's imprecision (physics and
 * RNG in games of this era sometimes do), revisit this.
 */

static inline float __fabsf(float x) { return __builtin_fabsf(x); }
static inline double __fabs(double x) { return __builtin_fabs(x); }
static inline float __frsqrte(float x) { return 1.0f / __builtin_sqrtf(x); }
static inline float __mwerks_frsqrte(float x) { return 1.0f / __builtin_sqrtf(x); }

#endif
