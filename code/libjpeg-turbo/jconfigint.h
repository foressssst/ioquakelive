#define BUILD  "20261123"

#if (defined(__GNUC__) || defined(__clang__)) && !defined(_WIN32)
#define HIDDEN  __attribute__((visibility("hidden")))
#else
#define HIDDEN
#endif

#undef inline

#if defined(_MSC_VER) && !defined(__clang__)
#define INLINE  __forceinline
#define THREAD_LOCAL  __declspec(thread)
#else
#define INLINE  __inline__ __attribute__((always_inline))
#define THREAD_LOCAL  __thread
#endif

#define PACKAGE_NAME  "libjpeg-turbo"

#define VERSION  "3.2.1"

#if defined(_WIN64) || defined(__LP64__) || defined(__x86_64__) || defined(__aarch64__) || defined(_M_ARM64)
#define SIZEOF_SIZE_T  8
#else
#define SIZEOF_SIZE_T  4
#endif

#if (defined(__GNUC__) || defined(__clang__)) && !defined(_WIN32)
#define HAVE_BUILTIN_CTZL
#endif

#if defined(_MSC_VER)
#define HAVE_INTRIN_H
#endif

#if defined(_MSC_VER) && defined(HAVE_INTRIN_H)
#if (SIZEOF_SIZE_T == 8)
#define HAVE_BITSCANFORWARD64
#elif (SIZEOF_SIZE_T == 4)
#define HAVE_BITSCANFORWARD
#endif
#endif

#if defined(__has_attribute)
#if __has_attribute(fallthrough)
#define FALLTHROUGH  __attribute__((fallthrough));
#else
#define FALLTHROUGH
#endif
#else
#define FALLTHROUGH
#endif

#ifndef BITS_IN_JSAMPLE
#define BITS_IN_JSAMPLE  8
#endif

#undef C_ARITH_CODING_SUPPORTED
#undef D_ARITH_CODING_SUPPORTED
#undef WITH_SIMD

#if BITS_IN_JSAMPLE == 8

#define C_ARITH_CODING_SUPPORTED 1

#define D_ARITH_CODING_SUPPORTED 1

#ifdef TURBO_SIMD
#define WITH_SIMD 1
#if defined(__aarch64__) || defined(_M_ARM64)
#define SIMD_ARCHITECTURE  ARM64
#else
#define SIMD_ARCHITECTURE  X86_64
#endif
#endif

#endif
