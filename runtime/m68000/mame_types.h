// Minimal stand-ins for the MAME framework pieces the MAME CPU cores
// (MAME 0.264) refer to, so their sources compile unchanged outside MAME.
#ifndef SF2_MAME_TYPES_H
#define SF2_MAME_TYPES_H

#include <climits>
#include <cstdint>
#include <algorithm>
#include <iterator>
#include <type_traits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef uint32_t offs_t;

enum { CLEAR_LINE = 0, ASSERT_LINE = 1, HOLD_LINE = 2 };
enum { INPUT_LINE_NMI = 7 + 3 };   /* unused here */
enum { M68K_IRQ_NONE = 0, M68K_IRQ_1, M68K_IRQ_2, M68K_IRQ_3, M68K_IRQ_4, M68K_IRQ_5, M68K_IRQ_6, M68K_IRQ_7 };

template <typename... T> inline void logerror(const char *, T &&...) {}
template <typename... T> [[noreturn]] inline void fatalerror(const char *fmt, T &&... args)
{
	std::fprintf(stderr, "core: fatal error: ");
	std::fprintf(stderr, fmt, std::forward<T>(args)...);
	std::abort();
}

template <typename T, typename U> constexpr T BIT(T x, U n) { return (x >> n) & T(1); }
constexpr u32 rotl_32(u32 val, int shift) { shift &= 31; return shift ? (val << shift) | (val >> (32 - shift)) : val; }
constexpr u32 rotr_32(u32 val, int shift) { shift &= 31; return shift ? (val >> shift) | (val << (32 - shift)) : val; }

namespace util {
template <typename T, typename U> constexpr typename std::make_signed<T>::type sext(T value, U width)
{
	typedef typename std::make_signed<T>::type S;
	return S(value << (8 * sizeof(T) - width)) >> (8 * sizeof(T) - width);
}
}

static inline bool WORD_ALIGNED(u32 a) { return (a & 1) == 0; }
struct mame_machine_stub { bool side_effects_disabled() const { return false; } };

#endif
