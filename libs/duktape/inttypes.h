/*
 * Zarqaa OS stub: inttypes.h for duktape (freestanding)
 *
 * Provides stdint types + PRI* format macros needed by duk_config.h.
 * All integer types are guarded to avoid conflicts with clang's built-in
 * stdint.h (which is already included via zarqaa/types.h in the shim).
 *
 * Must NOT use #include_next (no system stdlib).
 */

#ifndef _ZARQAA_STUB_INTTYPES_H
#define _ZARQAA_STUB_INTTYPES_H

/* ── Import exact-width types from clang built-in stdint.h (already included) ── */
/* We don't redefine them here to avoid typedef redefinition conflicts.
 * zarqaa_duk_shim.h → zarqaa/types.h → <stdint.h> (clang built-in) already
 * provides: uint8_t, uint16_t, uint32_t, uint64_t, int8_t ... int64_t,
 *           size_t, ptrdiff_t, uintptr_t, intptr_t, uintmax_t, intmax_t,
 *           uint_leastN_t, uint_fastN_t variants. */

/* ── PRI* format macros (not in stdint.h) ────────────────────────────── */
#ifndef PRId8
#define PRId8   "d"
#define PRId16  "d"
#define PRId32  "d"
#define PRId64  "lld"
#define PRIi8   "i"
#define PRIi16  "i"
#define PRIi32  "i"
#define PRIi64  "lli"
#define PRIu8   "u"
#define PRIu16  "u"
#define PRIu32  "u"
#define PRIu64  "llu"
#define PRIx8   "x"
#define PRIx16  "x"
#define PRIx32  "x"
#define PRIx64  "llx"
#define PRIX8   "X"
#define PRIX16  "X"
#define PRIX32  "X"
#define PRIX64  "llX"
#define PRIdPTR "lld"
#define PRIiPTR "lli"
#define PRIuPTR "llu"
#define PRIxPTR "llx"
#define PRIXptr "llX"
#define PRIdMAX "lld"
#define PRIiMAX "lli"
#define PRIuMAX "llu"
#define PRIxMAX "llx"
#define SCNMAX  "lld"
#endif /* PRId8 */

#endif /* _ZARQAA_STUB_INTTYPES_H */
