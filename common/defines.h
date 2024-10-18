#pragma once

#define INLINE static inline
#define LOCAL static
#define PERSIST static
#define PACKED __attribute__((packed))

#define BIT(x)          (1 << (x))
#define UNUSED(x)       ((void) x)
#define STRINGIFY(x)    #x
#define ARRAY_LEN(arr)  (sizeof(arr)/sizeof((arr)[0]))

#if defined(__GNUC__) && defined(__cplusplus)
    #define STATIC_ASSERT static_assert
#else
    #error "Only g++ compiler is supported!"
#endif

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef signed char i8;
typedef signed short i16;
typedef signed int i32;
typedef signed long long i64;

typedef float f32;
typedef double f64;

STATIC_ASSERT(sizeof(u8)  == 1, "u8 is not 1 byte");
STATIC_ASSERT(sizeof(u16) == 2, "u16 is not 2 bytes");
STATIC_ASSERT(sizeof(u32) == 4, "u32 is not 4 bytes");
STATIC_ASSERT(sizeof(u64) == 8, "u64 is not 8 bytes");

STATIC_ASSERT(sizeof(i8)  == 1, "i8 is not 1 byte");
STATIC_ASSERT(sizeof(i16) == 2, "i16 is not 2 bytes");
STATIC_ASSERT(sizeof(i32) == 4, "i32 is not 4 bytes");
STATIC_ASSERT(sizeof(i64) == 8, "i64 is not 8 bytes");

STATIC_ASSERT(sizeof(f32) == 4, "f32 is not 4 bytes");
STATIC_ASSERT(sizeof(f64) == 8, "f64 is not 8 bytes");

#define GiB(n) ((n) * 1024ULL * 1024ULL * 1024ULL)
#define MiB(n) ((n) * 1024ULL * 1024ULL)
#define KiB(n) ((n) * 1024ULL)

#define GB(n) ((n) * 1000ULL * 1000ULL * 1000ULL)
#define MB(n) ((n) * 1000ULL * 1000ULL)
#define KB(n) ((n) * 1000ULL)
