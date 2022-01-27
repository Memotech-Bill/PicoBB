/******************************************************************\
*       BBC BASIC for Raspberry Pi Pico                            *
*       Copyright (c) R. T. Russell, 2000-2021                     *
*                                                                  *
*       bbccfg.h constant, variable and structure declarations     *
*       Version 1.26a, 08-Nov-2021                                 *
\******************************************************************/

#ifndef H_BBCCFG
#define H_BBCCFG

#include <stdint.h>

// Alignment helper types:
typedef __attribute__((aligned(1))) int unaligned_int;
typedef __attribute__((aligned(1))) intptr_t unaligned_intptr_t;
typedef __attribute__((aligned(1))) unsigned int unaligned_uint;
typedef __attribute__((aligned(2))) unsigned int twoaligned_uint;
typedef __attribute__((aligned(1))) unsigned short unaligned_ushort;
typedef __attribute__((aligned(1))) void* unaligned_void_ptr;
typedef __attribute__((aligned(1))) char* unaligned_char_ptr;

// Helper macros to fix alignment problem:
#if PICO_ALIGN > 1
#define IALIGN(x)    x = ((x + 3) & 0xFFFFFFFCu)
#define ALIGN(x)    x = (void *)(((uint32_t)x + 3) & 0xFFFFFFFCu)

#define ILOAD(p)    *((int*)(p))
#define ISTORE(p,i) *((int*)(p)) = i
#define TLOAD(p)    *((intptr_t*)(p))
#define TSTORE(p,i) *((intptr_t*)(p)) = i 
#define ULOAD(p)    *((uint32_t*)(p))
#define USTORE(p,i) *((uint32_t*)(p)) = i 
#define VLOAD(p)    *((void**)(p))
#define VSTORE(p,i) *((void**)(p)) = i 
#define CLOAD(p)    *((char**)(p))
#define CSTORE(p,i) *((char**)(p)) = i 

static inline int XLOAD(void* p){ return (intptr_t)p&3 ? *((unaligned_int*)p) : *((int*)p); }
static inline void XSTORE(void* p, int i){ if ((intptr_t)p&3) *((unaligned_int*)p) = i; else *((int *)p) = i; }

#if PICO_ALIGN == 3
#define VTYPE   0x0C    // Variant type (stored in 12 bytes)
#define VATYPE  0x4C    // Variant array type
#define U2LOAD(p)    *((uint32_t*)(p))
#define U2STORE(p,i) *((uint32_t*)(p)) = i 
#else
#define VTYPE   0x0A    // Variant type (stored in 10 bytes)
#define VATYPE  0x4A    // Variant array type
#define U2LOAD(p)    *((twoaligned_uint*)(p))
#define U2STORE(p,i) *((twoaligned_uint*)(p)) = i 
#endif
#define S2LOAD(p)    *((unsigned short*)(p))
#define S2STORE(p,i) *((unsigned short*)(p)) = i 

#else

#define IALIGN(x)
#define ALIGN(x)

#ifdef PICO
static inline int ILOAD(void* p){ return (intptr_t)p&3 ? *((unaligned_int*)p) : *((int*)p); }
static inline void ISTORE(void* p, int i){ if ((intptr_t)p&3) *((unaligned_int*)p) = i; else *((int *)p) = i; }
#else
#define ILOAD(p)    *((unaligned_int*)(p))
#define ISTORE(p,i) *((unaligned_int*)(p)) = i
#endif 

#define TLOAD(p)    *((unaligned_intptr_t*)(p))
#define TSTORE(p,i) *((unaligned_intptr_t*)(p)) = i 
#define ULOAD(p)    *((unaligned_uint*)(p))
#define USTORE(p,i) *((unaligned_uint*)(p)) = i 
#define U2LOAD(p)    *((unaligned_uint*)(p))
#define U2STORE(p,i) *((unaligned_uint*)(p)) = i 
#define VLOAD(p)    *((unaligned_void_ptr*)(p))
#define VSTORE(p,i) *((unaligned_void_ptr*)(p)) = i 
#define CLOAD(p)    *((unaligned_char_ptr*)(p))
#define CSTORE(p,i) *((unaligned_char_ptr*)(p)) = i 

#define XLOAD(p)    *((unaligned_int*)(p))
#define XSTORE(p,i) *((unaligned_int*)(p)) = i
#define S2LOAD(p)    *((unaligned_ushort*)(p))
#define S2STORE(p,i) *((unaligned_ushort*)(p)) = i 

#define VTYPE   0x0A    // Variant type (stored in 10 bytes)
#define VATYPE  0x4A    // Variant array type

#endif

// Used for line numbers in program source - not aligned
#define SLOAD(p)    *((unaligned_ushort*)(p))
#define SSTORE(p,i) *((unaligned_ushort*)(p)) = i 

#endif
