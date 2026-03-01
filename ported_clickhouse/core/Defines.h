#pragma once

#if defined(__GNUC__) || defined(__clang__)
#define ALWAYS_INLINE inline __attribute__((always_inline))
#define NO_INLINE __attribute__((noinline))
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define ALWAYS_INLINE inline
#define NO_INLINE
#define likely(x) (x)
#define unlikely(x) (x)
#endif
