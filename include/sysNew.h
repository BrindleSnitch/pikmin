#ifndef _SYSNEW_H
#define _SYSNEW_H

#include "system.h"
#include "types.h"
#include <stddef.h>

inline void* operator new(size_t size)
{
	return System::alloc(size);
}
inline void* operator new[](size_t size)
{
	return System::alloc(size);
}
void* operator new(size_t size, int alignment);
void* operator new[](size_t size, int alignment);
void operator delete(void* ptr);
void operator delete[](void* ptr);

// MetroWerks allowed programmers to take the address of an rvalue, but this is illegal
// in C and C++.  This illegal operation effectively acts like a placement new on stack
// allocated space; the only difference is that object destruction would be automatic.
// Pikmin 1 almost never uses destructors, however, so this should be of no consequence.
#if defined(__MWERKS__)
#define stack_new(...) &__VA_ARGS__
#elif defined(_MSC_VER) && _MSC_VER < 1400 // Visual Studio 2005 (8.0)
// MSVC also permits taking the address of an rvalue, however support for variadic macros
// was added after Visual Studio 6.0 (the version we must use for matching the DLL).  The
// reason we wish to use `__VA_ARGS__` here is to support templated types.  This includes
// for modding purposes, so we shouldn't to drop this macro's capabilities to that of the
// least capable compiler.  Luckily, we never NEED to pass a templated type name directly
// to this macro in any matching scenarios, because you can always typedef it to a simpler
// name and use that.
#define stack_new(type) &type
#else
// Everything else (clang, gcc): spell out what MetroWerks was doing implicitly.
// `stack_new(T)(args)` expands to `new (alloca(sizeof(T))) T(args)`, which is
// the placement-new-on-stack-storage described above, and yields a `T*` exactly
// as the illegal `&T(args)` did.
//
// alloca'd storage lives until the enclosing FUNCTION returns, whereas the
// MetroWerks temporary lived until the enclosing scope ended. Longer, so no
// call site can be left holding a pointer that died too early. Two consequences
// worth knowing: destructors do not run (per the note above, Pikmin 1 barely
// uses them), and a stack_new inside a loop accumulates one allocation per
// iteration until the function returns rather than being reclaimed each pass.
// Declared here rather than via <new>: include/stl shadows <stddef.h> and
// <stdlib.h>, so pulling in the real C++ library header breaks its internal
// includes. This file already defines the ordinary operator new inline for the
// same reason.
inline void* operator new(size_t, void* ptr) { return ptr; }
#define stack_new(...) new (__builtin_alloca(sizeof(__VA_ARGS__))) __VA_ARGS__
#endif

#endif // _SYSNEW_H
