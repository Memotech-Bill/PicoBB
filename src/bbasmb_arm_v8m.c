/*******************************************************************\
 *       32-bit BBC BASIC Interpreter                              *
 *       (c) 2018-2021  R.T.Russell  http://www.rtrussell.co.uk/   *
 *                                                                 *
 *       bbasmb_arm_v8m.c Assembler for ARMv8m thunb instructions  *
 *       Unified syntax                                            *
 *       Version 0.01, 20-Aug-2024                                 *
\*******************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef __WINDOWS__
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif

#ifndef TEST

#include "BBC.h"

// External routines:
void newlin (void);
void *getput (unsigned char *);
void error (int, const char *);
void token (signed char);
void text (const char*);
void crlf (void);
void comma (void);
void spaces (int);
int range0 (char);
signed char nxt (void);

long long itemi (void);
VAR item (void);
long long expri (void);
VAR expr (void);
VAR exprs (void);
VAR loadn (void *, unsigned char);
void storen (VAR, void *, unsigned char);

// Routines in bbcmos.c:
void *sysadr (char *);
unsigned char osrdch (void);
void oswrch (unsigned char);
int oskey (int);
void osline (char *);
int osopen (int, char *);
void osshut (int);
unsigned char osbget (int, int *);
void osbput (int, unsigned char);
long long getptr (int);
void setptr (int, long long);
long long getext (int);
void oscli (char *);
int osbyte (int, int);
void osword (int, void *);

#if defined(__x86_64__) || defined(__aarch64__)
#define OC ((unsigned int) stavar[15] + (void *)((long long) stavar[13] << 32)) 
#define PC ((unsigned int) stavar[16] + (void *)((long long) stavar[17] << 32)) 
#else
#define OC (void *) stavar[15]
#define PC (void *) stavar[16]
#endif

double valuef (void)
    {
	VAR v = item () ;
	if (v.s.t == -1)
		error (6, NULL) ; // 'Type mismatch'
	if (v.i.t == 0)
	    {
		v.f = v.i.n ;
	    }
    return v.f;
    }

#else

// Dummy definitions for stand-alone testing of the assembler

#define TLEN    256
#define BIT6    0x40
#define MLAB    50

typedef struct {double f;} VAR;

FILE *fIn;
FILE *fOut;
char sLine[TLEN];
const char *esi;
int liston = 0x20;
intptr_t stavar[17];
intptr_t label[MLAB];
int nlab = -1;
int pass;

#define OC (void *) stavar[15]
#define PC (void *) stavar[16]

char nxt (void);
void error (int err, const char *ps);
int itemi (void);
int expri (void);
VAR item (void);
double valuef (void);
static bool address (bool bAlign, int mina, int maxa, int *addr);

#endif

#define BL(a,b,c,d,e,f,g,h) 0b ## a ## b ## c ## d ## e ## f ## g ## h
#define BW(a,b,c,d) 0b ## a ## b ## c ## d

static const int msk[3] = {0, 1, 3};
static const int lim[3] = {255, 510, 1020};

#define count(X)    (sizeof (X) / sizeof (X[0]))

static void poke (const void *p, int n) 
    {
	char *d;
	if (liston & BIT6)
	    {
		d = OC;
		stavar[15] += n;
	    }
	else
		d = PC;

	stavar[16] += n;
	memcpy (d, p, n);
    }

static void *align (int n)
    {
	while (stavar[16] & (n-1))
	    {
		stavar[16]++;
		if (liston & BIT6)
			stavar[15]++;
	    };
	return PC;
    }

static void op16 (int op)
    {
#ifdef TEST
    if (pass == 2) fprintf (fOut, "%04X %02X%02X    ", stavar[16], (op & 0xFF), (op >> 8) & 0xFF);
    stavar[16] += 2;
#else
    align (2);
    poke (&op, 2);
#endif
    }

static void op32 (int op)
    {
#ifdef TEST
    if (pass == 2) fprintf (fOut, "%04X %02X%02X%02X%02X", stavar[16], (op >> 16) & 0xFF, (op >> 24) & 0xFF, (op & 0xFF), (op >> 8) & 0xFF);
    stavar[16] += 4;
#else
    align (2);
    poke ((char *)&op + 2, 2);
    poke (&op, 2);
#endif
    }

static int lookup (const char **arr, int num)
    {
	int i, n;
	for (i = 0; i < num; ++i)
	    {
		n = strlen (*arr);
		if (strnicmp ((const char *)esi, *arr, n) == 0)
            {
            esi += n;
            return i;
            }
        ++arr;
	    }
    return -1;
    }

static const char *asmmsg[] = {
    "Invalid opcode",                   // 101
    "Too many parameters",              // 102
    "Invalid register",                 // 103
    "Low register required",            // 104
    "Invalid alignment",                // 105
    "Register / list conflict",         // 106
    "Invalid register list",            // 107
    "Status flags not set",             // 108
    "Invalid special register",         // 109
    "Instruction sets status flags",    // 110
    "Immediate out of range",           // 111
    "Status flags unaffected",          // 112
    "Invalid floating point register",  // 113
    "Not in if/then block",             // 114
    "Invalid condition code",           // 115
    "Condition code mismatch",          // 116
    "Must be last in if/then block",    // 117
    "Not allowed in if/then block",     // 118
    "Out of range, use .W version",     // 119
    "Offset must be a multiple of 2",   // 120
    "Offset must be a multiple of 4",   // 121
    "Invalid floating point constant",  // 122
    "Invalid data type"                 // 123
    };

static bool asmerr (int ierr)
    {
    if ( liston & 0x20 )
        {
        const char *pserr = NULL;
        if ( ierr > 100 )
            {
            pserr = asmmsg[ierr - 101];
            ierr = 16;
            }
        error (ierr, pserr);
        }
    return false;
    }

static bool IsWide (void)
    {
    if (*esi == '.')
        {
        if (((*(esi+1) == 'W') || (*(esi+1) == 'w')))
            {
            esi += 2;
            return true;
            }
        else if (((*(esi+1) == 'N') || (*(esi+1) == 'n')))
            {
            esi += 2;
            return false;
            }
        }
    return false;
    }

static bool IsEnd (void)
    {
    char ch = nxt ();
#ifndef TEST
    return ((ch == '\r') || (ch == ':') || (ch == ';') || (ch == TREM));
#else
    return ((ch == '\0') || (ch == ':') || (ch == '@') || (ch == '\n'));
#endif
    }

static bool TestEnd (void)
    {
    bool bEnd = IsEnd ();
    if (! bEnd) asmerr (102);   // Too many parameters
    return bEnd;
    }

static bool EndOp (void)
    {
    if (*esi == ' ')
        {
        nxt ();
        return true;
        }
    if (IsEnd ()) return true;
    return asmerr (101);    // Invalid opcode
    }

static const char *registers[] = {
    "lr", "pc", "r0", "r10", "r11", "r12", "r13", "r14", "r15", "r1", 
    "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "sp" };

static const unsigned char regno[] = {
    14, 15, 0, 10, 11, 12, 13, 14, 15, 1, 2, 3, 4, 5, 6, 7, 8, 9, 13 };

static int reg (bool bErr)
    {
	int n;
    nxt ();
	n = lookup (registers, count(registers));
    if ( n >= 0 ) n = regno[n];
    else if (bErr) asmerr (103);    // Invalid register
    return n;
    }

static int reglist (void) 
    {
	int regs = 0;
	if (nxt () != '{')
        {
		asmerr (107);   // Invalid register list
        return 0;
        }
	do
	    {
		++esi;
		int r = reg (true);
        if ( r < 0 ) return 0;
		regs |= 1 << r;
		if (nxt () == '-')
		    {
			++esi;
			int rl = reg (true);
            if ( rl < 0 ) return 0;
			while (++r <= rl)
				regs |= 1 << r;
		    }
	    }
	while (nxt () == ',');
	if (*esi != '}')
        {
		asmerr (107);   // Invalid register list
        return 0;
        }
    ++esi;
	return regs;
    }

static const char *conditions[] = {
    "eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc", "hi", "ls", "ge", "lt", "gt", "le", "al", "hs", "lo"};

static int GetCC (bool bErr)
    {
    int cc = lookup (conditions, count (conditions));
    if (cc > 14) cc -= 13;
    if (bErr && (cc < 0)) return asmerr (115);  // Invalid condition code
    return cc;
    }

static int it_cc;
static int it_cm;
static int it_nc = 0;

static bool op_IT (int d)
    {
    if (it_nc > 0) return asmerr (118);    // Not allowed in If/Then block
    int cm = 0x08;
    it_nc = 1;
    it_cc = 0;
    while (it_nc < 4)
        {
        if ((*esi == 'T') || (*esi == 't'))
            {
            ++it_nc;
            ++esi;
            }
        else if ((*esi == 'E') || (*esi == 'e'))
            {
            ++it_nc;
            it_cc |= cm;
            ++esi;
            }
        else if (*esi <= ' ')
            {
            break;
            }
        else
            {
            asmerr (31);   // Invalid argument
            return false;
            }
        cm >>= 1;
        }
    it_cc |= cm;
    if (! EndOp ()) return false;
    int cc = GetCC (true);
    if (cc < 0) return false;
    it_cc |= cc << 4;
    if (cc & 1)
        {
        if (it_nc > 1) it_cc ^= 0x08;
        if (it_nc > 2) it_cc ^= 0x04;
        if (it_nc > 3) it_cc ^= 0x02;
        }
    op16 (d | it_cc);
    return TestEnd ();
    }

static bool InIT (void)
    {
    if (it_nc == 0) return false;
    int cc = GetCC (false);
    if (cc < 0) asmerr (115);   // Invalid condition code
    else if ((cc << 4) != (it_cc & 0xF0)) asmerr (116); // Condition code mismatch
    if (--it_nc > 0)
        {
        it_cc = (it_cc & 0xE0) | ((it_cc & 0x0F) << 1);
        }
    else
        {
        it_cc = 0;
        }
    return true;
    }

static bool LastIT (void)
    {
    bool bIT = InIT ();
    if (bIT && (it_nc > 0)) return asmerr (117);   // Must be last in If/Then block
    return true;
    }

static bool op_NoneNW (int d)
    {
    InIT ();
    bool bWide = IsWide ();
    if (! EndOp ()) return false;
    if (bWide)
        {
        op32 (BL(1111, 0011, 1010, 1111, 1000, 0000, 0000, 0000) | d);
        }
    else
        {
        op16 (BW(1011, 1111, 0000, 0000) | (d << 4));
        }
    return TestEnd ();
    }

static bool op_NoneW (int d)
    {
    InIT ();
    op32 (d);
    return TestEnd ();
    }

#define RB_SP   (1 << 13)
#define RB_LR   (1 << 14)
#define RB_PC   (1 << 15)

// Push: d = 0, Pop: d = 1
static bool op_Stack (int d)
    {
    InIT ();
    if (! EndOp ()) return false;
    static const int masks[4] = {RB_PC | RB_SP | 0x1F00, RB_LR | RB_SP | 0x1F00, RB_PC | RB_SP, RB_SP};
    char al = nxt ();
    int regs;
    bool bMulti = true;
    if (al == '{')
        {
        // Register list
        regs = reglist ();
        if (! regs) return false;
        if ((regs & masks[d]) && (__builtin_popcount (regs) == 1))
            {
            bMulti = false;
            regs = __builtin_ctz (regs);
            }
        }
    else
        {
        regs = reg (true);
        if (regs < 0) return false;
        if ((regs < 8) || (regs == d + 14)) regs = 1 << regs;
        else bMulti = false;
        }
    if (bMulti)
        {
        if (!(regs & masks[d]))
            {
            int ops[2] = {BW(1011, 0100, 0000, 0000), BW(1011, 1100, 0000, 0000)};
            op16 (ops[d] | (regs & 0xFF) | ((regs & (RB_PC | RB_LR)) ? 0x100 : 0x000));
            }
        else if (!(regs & masks[d+2]))
            {
            int opl[2] = {BL(1110, 1001, 0010, 1101, 0000, 0000, 0000, 0000), BL(1110, 1000, 1011, 1101, 0000, 0000, 0000, 0000)};
            op32 (opl[d] | regs);
            }
        else
            {
            return asmerr (103);    // Invalid register
            }
        }
    else
        {
        // Single register
        int opr[2] = {BL(1111, 1000, 0100, 1101, 0000, 1101, 0000, 0100), BL(1111, 1000, 0101, 1101, 0000, 1011, 0000, 0100)};
        op32 (opr[d] | (regs << 12));
        }
    return TestEnd ();
    }

static bool op_BX (int d)
    {
    if (! LastIT ()) return false;
    if (! EndOp ()) return false;
    int r = reg (true);
    if (r < 0) return false;
    if ((r == 13) || (r == 15)) return asmerr (103);    // Invalid register
    op16 (d | (r << 3));
    return TestEnd ();
    }

static bool match (char ch, int err)
    {
    if (nxt () == ch)
        {
        ++esi;
        return true;
        }
    if (err) asmerr (err);
    return false;
    }

static const char *shift[] = { "lsl", "lsr", "asr", "ror", "rrx" };

int ivalue (void)
    {
    if (nxt () == '#') ++esi;
    return itemi ();
    }

static bool immediate (int minv, int maxv, int *pvalue)
    {
    if (nxt () == '#') ++esi;
    *pvalue = itemi ();
    if ((*pvalue < minv) || (*pvalue > maxv)) return asmerr (111);  // Immediate out of range
    return true;
    }

static bool arg_shift (int *rot)
    {
    int rr = 0;
    int amount = 0;
    *rot = 0;
    if (! match (',', 0)) return true;
    nxt ();
    rr = lookup (shift, count (shift));
    if (rr < 0) return asmerr (31);      // Invalid argument
    if (rr < 4)
        {
        if (! immediate (1, ((rr == 1) || (rr == 2))? 32: 31, &amount) ) return false;
        amount = ((amount & 0x1C) << 10) | ((amount & 0x03) << 6);
        }
    else
        {
        rr = 3;
        }
    *rot = (rr << 4) | amount;
    return true;
    }

static bool constant (int *cval, bool bErr)
    {
    if (nxt () == '#') ++esi;
    int c = itemi ();
    *cval = c;
    if ((c >= 0) && (c <= 0xFF)) return true;
    int lz = __builtin_clz (c);
    int mask = 0xFF << (24 - lz);
    if ((c & mask) == c)
        {
        int mv = lz + 8;
        *cval = ((c >> (24 - lz)) & 0x7F) | ((mv & 0x10) << 22) | ((mv & 0x0E) << 11) | ((mv & 0x01) << 7);
        return true;
        }
    int t = c & 0xFF;
    if (c == ((t << 16) | t))
        {
        *cval = (1 << 12) | t;
        return true;
        }
    if (c == ((t << 24) | (t << 16) | (t << 8) | t))
        {
        *cval = (3 << 12) | t;
        return true;
        }
    t = c & 0xFF00;
    if (c == ((t << 16) | t))
        {
        *cval = (2 << 12) | (t >> 8);
        return true;
        }
    if (bErr) asmerr (111);   // Immediate out of range
    return false;
    }

#ifndef TEST
static bool address (bool bAlign, int mina, int maxa, int *addr)
    {
    void *dest = (void *) (size_t) expri ();
    void *base;
    if (bAlign) base = (void *)((uint32_t)(PC + 4) & 0xFFFFFFFC);
    else base = PC + 4;
    *addr = dest - base;
    if ( *addr & 0x01 ) return asmerr (120);    // 'Invalid alignment'
    if ((*addr < mina) || (*addr > maxa)) return asmerr (8);
    return true;
    }
#endif

static bool op_CMP (int d)
    {
    InIT ();
    bool bWide = IsWide ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int r2 = reg (false);
    if (r2 >= 0)
        {
        // Compare registers
        int rot;
        if (! arg_shift (&rot)) return false;
        if (bWide || (rot != 0))
            {
            if ((r1 == 15) || (r2 == 15) || (r2 == 13)) return asmerr (103);  // Invalid register
            op32 (BL(1110, 1011, 1011, 0000, 0000, 1111, 0000, 0000) | (r1 << 16) | r2 | rot);
            }
        else if ((r1 > 7) || (r2 > 7))
            {
            if ((r1 == 15) || (r2 == 15)) return asmerr (103);  // Invalid register
            op16 (BW(0100, 0101, 0000, 0000) | ((r1 & 0x8) << 4) | (r1 & 0x7) | (r2 << 3));
            }
        else
            {
            op16 (BW(0100, 0010, 1000, 0000) | r1 | (r2 << 3));
            }
        }
    else
        {
        // Compare immediate
        int c;
        if (! constant (&c, true) ) return false;
        if (bWide || (r1 > 7) || (c > 0xFF))
            {
            if (r1 == 15) return asmerr (103);  // Invalid register
            op32 (BL(1111, 0001, 1011, 0000, 0000, 1111, 0000, 0000) | (r1 << 16) | c);
            }
        else
            {
            op16 (BW(0010, 1000, 0000, 0000) | (r1 << 8) | c);
            }
        }
    return TestEnd ();
    }

static bool op_TST (int d)
    {
    InIT ();
    bool bWide = IsWide ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int r2 = reg (false);
    if (r2 >= 0)
        {
        // Compare registers
        int rot;
        if (! arg_shift (&rot)) return false;
        if ((bWide) || (r1 > 7) || (r2 > 7) || (rot != 0))
            {
            if ((r1 == 15) || (r2 == 15) || (r2 == 13)) return asmerr (103);  // Invalid register
            op32 (BL(1110, 1010, 0001, 0000, 0000, 1111, 0000, 0000) | (d & 0x01000000) | (r1 << 16) | r2 | rot);
            }
        else
            {
            op16 (BW(0100, 0010, 0000, 0000) | (d & 0x00C0) | r1 | (r2 << 3));
            }
        }
    else
        {
        // Compare immediate
        int c;
        if (r1 == 15) return asmerr (103);  // Invalid register
        if (! constant (&c, true) ) return false;
        op32 (BL(1111, 0000, 0001, 0000, 0000, 1111, 0000, 0000) | (d & 0x01000000) | (r1 << 16) | c);
        }
    return TestEnd ();
    }

static bool op_REV (int d)
    {
    InIT ();
    bool bWide = IsWide ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int r2 = reg (true);
    if (r2 < 0) return false;
    if (bWide || (r1 > 7) || (r2 > 7))
        {
        if ((r1 == 15) || (r1 == 13) || (r2 == 15) || (r2 == 13)) return asmerr (103);  // Invalid register
        op32 (BL(1111, 1010, 1001, 0000, 1111, 0000, 1000, 0000) | (d << 4) | (r2 << 16) | (r1 << 8) | r2);
        }
    else
        {
        op16 (BW(1011, 1010, 0000, 0000) | (d << 6) | (r2 << 3) | r1);
        }
    return TestEnd ();
    }

static bool arg_ROR (int *amount)
    {
    *amount = 0;
    if (nxt () != ',') return true;
    ++esi;
    nxt ();
    if (strnicmp (esi, "ror", 3)) return asmerr (16);       // Syntax error
    esi += 3;
    if (nxt () == '#') ++esi;
    nxt ();
    *amount = itemi ();
    if (*amount != (*amount & 0x18)) return asmerr (101);   // Invalid immediate
    *amount <<= 1;
    return true;
    }

static bool op_EXT (int d)
    {
    InIT ();
    bool bWide = IsWide ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int r2 = reg (true);
    if (r2 < 0) return false;
    int amount;
    if (! arg_ROR (&amount)) return false;
    if (bWide || (r1 > 7) || (r2 > 7) | (amount != 0))
        {
        if ((r1 == 15) || (r1 == 13) || (r2 == 15) || (r2 == 13)) return asmerr (103);  // Invalid register
        op32 (BL(1111, 1010, 0000, 1111, 1111, 0000, 1000, 0000) | (d & 0x00F00000) | (r1 << 8) | amount | r2);
        }
    else
        {
        op16 (BW(1011, 0010, 0000, 0000) | (d & 0x00C0) | (r2 << 3) | r1);
        }
    return TestEnd ();
    }

static bool IsSF (void)
    {
    if ((*esi == 'S') || (*esi == 's'))
        {
        ++esi;
        return true;
        }
    return false;
    }

#define BIT_SF  (1 << 20)

static bool op_MVN (int d)
    {
    bool bSF = IsSF ();
    bool bIT = InIT ();
    bool bWide = IsWide ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int r2 = reg (false);
    if (r2 >= 0)
        {
        int rot;
        if (! arg_shift (&rot)) return false;
        if (bWide || (r1 > 7) || (r2 > 7) || (rot != 0) || (bSF == bIT))
            {
            if ((r1 == 13) || (r1 == 15) || (r2 == 13) || (r2 == 15))  return asmerr (103);  // Invalid register
            op32 (BL(1110, 1010, 0110, 1111, 0000, 0000, 0000, 0000) | (bSF ? BIT_SF : 0) | (r1 << 8) | r2 | rot);
            }
        else
            {
            op16 (BW(0100, 0011, 1100, 0000) | (r2 << 3) | r1);
            }
        }
    else
        {
        int c;
        if ((r1 == 13) || (r1 == 15))  return asmerr (103);  // Invalid register
        if (! constant (&c, true) ) return false;
        op32 (BL(1111, 0000, 0110, 1111, 0000, 0000, 0000, 0000) | (bSF ? BIT_SF : 0) | (r1 << 8) | c);
        }
    return TestEnd ();
    }

static bool op_ADD_SUB (int d)
    {
    bool bImmC = true;
    if (d & 1) bImmC = false;
    bool bSF = IsSF ();
    bool bIT = InIT ();
    bool bWide = IsWide ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int r2 = reg (false);
    int r3 = -1;
    int c = 0;
    if (r2 >= 0)
        {
        if (match (',', 0))
            {
            r3 = reg (false);
            if (r3 >= 0)
                {
                if (! arg_shift (&c)) return false;
                }
            else
                {
                if (! bImmC)
                    {
                    if (! immediate (0, 4095, &c)) return false;
                    }
                else
                    {
                    bImmC = constant (&c, false);
                    }
                }
            }
        else
            {
            r3 = r2;
            r2 = r1;
            }
        }
    else
        {
        r2 = r1;
        if (! bImmC)
            {
            if (! immediate (0, 4095, &c)) return false;
            }
        else
            {
            bImmC = constant (&c, false);
            }
        }
    if (r3 >= 0)
        {
        if ((! bWide) && (d == 0) && (! bSF) && (c == 0) && (r1 == r2))
            {
            op16 (BW(0100, 0100, 0000, 0000) | ((r1 & 8) << 4) | (r1 & 7) | (r3 << 3));
            }
        else if ((! bWide) && (r1 < 8) && (r2 < 8) && (r3 < 8) && (bSF != bIT))
            {
            op16 (BW(0001, 1000, 0000, 0000) | (d ? 0x200 : 0) | r1 | (r2 << 3) | (r3 << 6));
            }
        else
            {
            if ((r1 == 13) || (r1 == 15) || (r2 == 15) || (r3 == 13) || (r3 == 15))  return asmerr (103);  // Invalid register
            op32 (BL(1110, 1011, 0000, 0000, 0000, 0000, 0000, 0000) | (bSF ? BIT_SF : 0) | (d ? 0xA00000 : 0)
                | (r1 << 8) | (r2 << 16) | r3 | c);
            }
        }
    else
        {
        if (! bImmC)
            {
            if (bSF || (c >= 0x1000)) return asmerr (111);   // Immediate out of range
            if ((r1 == 13) || (r1 == 15))  return asmerr (103); // Invalid register
            op32 (BL(1111, 0010, 0000, 0000, 0000, 0000, 0000, 0000) | (d > 1 ? 0xA00000 : 0) | (r1 << 8) | (r2 << 16)
                | ((c & 0x800) << 15) | ((c & 0x700) << 4 ) | (c & 0xFF));
            }
        else if ((! bWide) && (! bSF) && (r1 == 13) && (r2 == 13) && (c <= 510) && ((c & 3) == 0))
            {
            op16 (BW(1011, 0000, 0000, 0000) | (d > 1 ? 0x80 : 0) | (c >> 2));
            }
        else if ((! bWide) && (d == 0) && (! bSF) && (r2 == 13) && (r1 < 8) && (c <= 1020) && ((c & 3) == 0))
            {
            op16 (BW(1010, 1000, 0000, 0000) | (r1 << 8) | (c >> 2));
            }
        else if ((! bWide) && (d == 0) && (! bSF) && (r2 == 15) && (c <= 1020) && (r1 < 8) && ((c & 3) == 0))
            {
            op16 (BW(1010, 0000, 0000, 0000) | (r1 << 8) | (c >> 2));
            }
        else if ((! bWide) && (bSF != bIT) && (r1 < 8) && (r2 < 8) && (c <= 7))
            {
            op16 (BW(0001, 1100, 0000, 0000) | (d > 1 ? 0x200 : 0) | r1 | (r2 << 3) | (c << 6));
            }
        else if ((! bWide) && (bSF != bIT) && (r1 < 8) && (r2 == r1) && (c <= 256))
            {
            op16 (BW(0011, 0000, 0000, 0000) | (d > 1 ? 0x800 : 0)| (r1 << 8) |c);
            }
        else
            {
            if ((r1 == 13) || (r1 == 15))  return asmerr (103);  // Invalid register
            op32 (BL(1111, 0001, 0000, 0000, 0000, 0000, 0000, 0000) | (d > 1 ? 0xA00000 : 0)
                | (bSF ? BIT_SF : 0) | (r1 << 8) | (r2 << 16) | c);
            }
        }
    return TestEnd ();
    }

static bool op_Shift (int d)
    {
    bool bSF = IsSF ();
    bool bIT = InIT ();
    bool bWide = IsWide ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int r2 = reg (false);
    int r3 = -1;
    int c = 0;
    int op = d & 0x03;
    if (r2 >= 0)
        {
        if (match (',', 0))
            {
            r3 = reg (false);
            if ( r3 < 0 )
                {
                immediate (0, ((op == 1) || (op == 2))? 32: 31, &c);
                }
            }
        else
            {
            r3 = r2;
            r2 = r1;
            }
        }
    else
        {
        r2 = r1;
        immediate (0, ((d == 1) || (d == 2))? 32: 31, &c);
        }
    if (r3 >= 0)
        {
        if ((! bWide) && (r1 < 8) && (r2 == r1) && (bSF != bIT))
            {
            op16 (BW(0100, 0000, 0000, 0000) | (d & 0x03C0) | r1 | (r3 << 3));
            }
        else
            {
            op32 (BL(1111, 1010, 0000, 0000, 1111, 0000, 0000, 0000) | (op << 21)
                | (bSF ? BIT_SF : 0) | (r1 << 8) | (r2 << 16) | r3);
            }
        }
    else
        {
        if ((! bWide) && (op != 0b11) && (r1 < 8) && (r2 < 8) && (bSF != bIT))
            {
            op16 ((op << 11) | r1 | (r2 << 3) | ((c & 0x1F) << 6));
            }
        else
            {
            op32 (BL(1110, 1010, 0100, 1111, 0000, 0000, 0000, 0000) | (op << 4) | (bSF ? BIT_SF : 0)
                | (r1 << 8) | r2 | ((c & 0x1C) << 10) | ((c & 0x03) << 6));
            }
        }
    return TestEnd ();
    }

static bool op_MUL (int d)
    {
    bool bSF = IsSF ();
    bool bIT = InIT ();
    bool bWide = IsWide ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int r2 = reg (true);
    if (r2 < 0) return false;
    int r3;
    if (match (',', 0))
        {
        r3 = reg (true);
        if (r3 < 0) return false;
        }
    else
        {
        r3 = r2;
        r2 = r1;
        }
    if (bWide || (r1 > 7) || (r2 != r1) || (r3 > 7))
        {
        if (bSF) return asmerr (112);   // Status flags not affected
        op32 (BL(1111, 1011, 0000, 0000, 1111, 0000, 0000, 0000) | (r1 << 8) | (r2 << 16) | r3);
        }
    else
        {
        if (bSF == bIT) return asmerr (bSF ? 112 : 110);
        op16 (BW(0100, 0011, 0100, 0000) | r1 | (r3 << 3));
        }
    return TestEnd ();
    }

static bool op_Logic (int d)
    {
    bool bSF = IsSF ();
    bool bIT = InIT ();
    bool bWide = IsWide ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int r2 = reg (false);
    int r3 = -1;
    int c = 0;
    if (r2 >= 0)
        {
        if (match (',', 0))
            {
            r3 = reg (false);
            if (r3 >= 0)
                {
                if (! arg_shift (&c)) return false;
                }
            else
                {
                if (! constant (&c, true)) return false;
                }
            }
        else
            {
            r3 = r2;
            r2 = r1;
            }
        }
    else
        {
        r2 = r1;
        if (! constant (&c, true)) return false;
        }
    if (r3 >= 0)
        {
        if (bWide || (r1 > 7) || (r2 != r1) || (r3 > 7) || (c != 0) || (bSF == bIT) | (d & 1))
            {
            if ((r1 == 13) || (r1 == 15) || (r2 == 13) || (r2 == 15) || (r3 == 13) || (r3 == 15)) return asmerr (103);  // Invalid register
            op32 (BL(1110, 1010, 0000, 0000, 0000, 0000, 0000, 0000) | (d & 0x01E00000) | (bSF ? BIT_SF : 0) | (r1 << 8) | (r2 << 16) | r3 | c);
            }
        else
            {
            op16 (BW(0100, 0000, 0000, 0000) | (d & 0x03C0) | r1 | (r3 << 3));
            }
        }
    else
        {
        if ((r1 == 13) || (r1 == 15) || (r2 == 13) || (r2 == 15)) return asmerr (103);  // Invalid register
        op32 (BL(1111, 0000, 0000, 0000, 0000, 0000, 0000, 0000) | (d & 0x01E00000) | (bSF ? BIT_SF : 0) | (r1 << 8) | (r2 << 16) | c);
        }
    return TestEnd ();
    }

static bool arg_LSL (int *pc)
    {
    *pc = 0;
    if (match (',', 0))
        {
        nxt ();
        if (strnicmp (esi, "lsl", 3)) return asmerr (31);   // Invalid argument
        esi += 3;
        if (! immediate (0, 3, pc)) return false;
        }
    return true;
    }

/* Bits in d:
   0-1  Shift for register offset
   4    Has special form: Rd, [PC, #]
   5    Has a 16-bit form for low registers
   6    Has special 16-bit form: Rd, [SP, #]
   7    Has special 16-bit form: Rd, [PC, #]
 */

static bool op_LD_ST (int d)
    {
    InIT ();
    bool bWide = IsWide ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int r2 = -1;
    int r3 = -1;
    int c = 0;
    int puw = 0;
    int sh = d & 3;
    if (match ('[', 0))
        {
        puw = 0b100;
        r2 = reg (true);
        if (r2 < 0) return false;
        if (match (',', 0))
            {
            r3 = reg(false);
            if (r3 >= 0)
                {
                if (! arg_LSL (&c)) return false;
                }
            else
                {
                if (! immediate (-4095, 4095, &c)) return false;
                }
            }
        if (! match (']', 16)) return false;
        }
    else
        {
        if (! address (true, -4095, 4095, &c)) return false;
        }
    if (match ('!', 0))
        {
        puw = 0b101;
        }
    else if ((c == 0) && match (',', 0))
        {
        immediate (-255, 255, &c);
        puw = 0b001;
        }
    if (d & 0x10)
        {
        if ((r3 < 0) && ( puw == 0b100) && (r2 == 15)) r2 = -1;
        }
    else if (r2 == -1)
        {
        r2 = 15;
        puw = 0b100;
        }
    if (r3 >= 0)
        {
        if (bWide || (r1 > 7) || (r2 > 7) || (r3 > 7) || (c != 0))
            {
            if ((r3 == 13) || (r3 == 15)) return asmerr (103);  // Invalid register
            op32 (BL(1111, 1000, 0000, 0000, 0000, 0000, 0000, 0000) | (d & 0x01700000) | (r1 << 12) | (r2 << 16) | r3 | (c << 4));
            }
        else
            {
            op16 (BW(0101, 0000, 0000, 0000) | (d & 0x0E00) | r1 | (r2 << 3) | (r3 << 6));
            }
        }
    else if (r2 >= 0)
        {
        if ((d & 0x20) && (! bWide) && (puw == 0b100) && (r1 < 8) && (r2 < 8) && (c >= 0) && (c <= (lim[sh] >> 3)))
            {
            op16 (BW(0000, 0000, 0000, 0000) | (d & 0xF800) | r1 | (r2 << 3) | (c << (6 - sh)));
            }
        else if ((d & 0x40) && (! bWide) && (puw == 0b100) && (r1 < 8) && (r2 == 13) && (c >= 0) && (c <= lim[sh]) && ((c & msk[sh]) == 0))
            {
            op16 (BW(1001, 0000, 0000, 0000) | (d & 0x0800) | (r1 << 8) | (c >> sh));
            }
        else
            {
            if (c >= 0) puw |= 0b010;
            else c = -c;
            if (puw == 0b110)
                {
                op32 (BL(1111, 1000, 1000, 0000, 0000, 0000, 0000, 0000) | (d & 0x01700000) | (r1 << 12) | (r2 << 16) | c);
                }
            else
                {
                if (c > 255) return asmerr (111);   // Imediate out of range
                op32 (BL(1111, 1000, 0000, 0000, 0000, 1000, 0000, 0000) | (d & 0x01700000) | (r1 << 12) | (r2 << 16) | (puw << 8) | c);
                }
            }
        }
    else
        {
        if ((d & 0x80) && (! bWide) && (r1 < 8))
            {
            if ((c & msk[sh]) != 0) asmerr (121);
            else if ((c < 0) || (c > lim[sh])) asmerr (119);
            op16 (BW(0100, 1000, 0000, 0000) | (r1 << 8) | (c >> sh));
            }
        else
            {
            if (c >= 0) puw = 0b010;
            else c = -c;
            op32 (BL(1111, 1000, 0000, 1111, 0000, 0000, 0000, 0000) | (d & 0x01700000) | (puw << 22) | (r1 << 12) | c);
            }
        }
    return TestEnd ();
    }

static bool op_MOV (int d)
    {
    bool bImmC = true;
    if ((*esi == 'W') || (*esi == 'w'))
        {
        bImmC = false;
        ++esi;
        }
    bool bSF = IsSF ();
    bool bIT = InIT ();
    bool bWide = IsWide ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int r2 = reg (false);
    int r3 = -1;
    int rr = 0;
    int c = 0;
    if (r2 >= 0)
        {
        if (match (',', 0))
            {
            nxt ();
            rr = lookup (shift, count (shift));
            if (rr < 0) return asmerr (31);    // Invalid argument
            r3 = reg (false);
            if (r3 < 0)
                {
                if (rr < 4)
                    {
                    if (! immediate (1, ((rr == 1) || (rr == 2))? 32: 31, &c) ) return false;
                    }
                else
                    {
                    rr = 3;
                    }
                }
            else if (rr == 4)
                {
                return asmerr (31);    // Invalid argument
                }
            }
        }
    else
        {
        if (! bImmC)
            {
            if (! immediate (0, 0xFFFF, &c)) return false;
            }
        else
            {
            bImmC = constant (&c, false);
            }
        if (! bImmC)
            {
            if (bSF  || (c < 0) || (c >= 0x10000)) return asmerr (111);   // Immediate out of range
            }
        }
    if (r3 >= 0)
        {
        if (bWide || (r1 > 7) || (r2 > 7) || (bSF == bIT))
            {
            op32 (BL(1111, 1010, 0000, 0000, 1111, 0000, 0000, 0000) | (rr << 17) | (bSF ? BIT_SF : 0) | (r1 << 8) |r2);
            }
        else
            {
            static const int shftop[] = {BW(0100, 0000, 1000, 0000), BW(0100, 0000, 1100, 0000), BW(0100, 0010, 0000, 000), BW(0100, 0001, 1100, 0000)};
            op16 (shftop[rr] | (r1 << 3) | r3);
            }
        }
    else if (r2 >= 0)
        {
        if ((! bWide) && (r1 < 8) && (r2 < 8) && (rr < 3) && (bSF != bIT))
            {
            op16 ((rr << 11) | (c << 6) | r1 | (r2 << 3));
            }
        else if ((! bWide) && (rr == 0) && (c == 0) && (! bSF))
            {
            op16 (BW(0100, 0110, 0000, 0000) | ((r1 & 0x08) << 4) | (r1 & 0x07) | (r2 << 3));
            }
        else
            {
            op32 (BL(1110, 1010, 0100, 1111, 0000, 0000, 0000, 0000) | (bSF ? BIT_SF : 0)
                | ((c & 0x1C) << 10) | (r1 << 8) | ((c & 0x03) << 6) | (rr << 4) | r2);
            }
        }
    else
        {
        if (! bImmC)
            {
            op32 (BL(1111, 0010, 0100, 0000, 0000, 0000, 0000, 0000) | (r1 << 8)
                | ((c & 0xF000) << 4) | ((c & 0x0800) << 15) | ((c & 0x0700) << 4) | (c & 0x00FF));
            }
        else if (bWide | (r1 > 7) | (c > 255) | (bSF == bIT))
            {
            op32 (BL(1111, 0000, 0100, 1111, 0000, 0000, 0000, 0000) | (bSF ? BIT_SF : 0) | (r1 << 8) | c);
            }
        else
            {
            op16 (BW(0010, 0000, 0000, 0000) | (r1 << 8) | c);
            }
        }
    return TestEnd ();
    }

static bool op_Bit (int d)
    {
    InIT ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int r2 = reg (true);
    if (r2 < 0) return false;
    if ((r1 == 13) || (r1 == 15) || (r2 == 13) || (r2 == 15)) return asmerr (103);  // Invalid register
    op32 (d | (r1 << 8) | (r2 << 16) | r2);
    return TestEnd ();
    }

static bool op_TT (int d)
    {
    InIT ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int r2 = reg (true);
    if (r2 < 0) return false;
    if ((r1 == 13) || (r1 == 15) || (r2 == 13) || (r2 == 15)) return asmerr (103);  // Invalid register
    op32 (d | (r1 << 8) | (r2 << 16));
    return TestEnd ();
    }

static bool op_EXT16 (int d)
    {
    InIT ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int r2 = reg (true);
    if (r2 < 0) return false;
    int amount;
    if (! arg_ROR (&amount)) return false;
    if ((r1 == 15) || (r1 == 13) || (r2 == 15) || (r2 == 13)) return asmerr (103);  // Invalid register
    op32 (d | (r1 << 8) | amount | r2);
    return TestEnd ();
    }

static bool op_Arith (int d)
    {
    InIT ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int r2 = reg (true);
    if (r2 < 0) return false;
    int r3;
    if (match (',', 0))
        {
        r3 = reg (true);
        if (r3 < 0) return false;
        }
    else
        {
        r3 = r2;
        r2 = r1;
        }
    if ((r1 == 13) || (r1 == 15) || (r2 == 13) || (r2 == 15) || (r3 == 13) || (r3 == 15)) return asmerr (103);
    if (d & 1)
        {
        op32 ((d - 1) | (r1 << 8) | (r2 << 16) | r3);
        }
    else
        {
        op32 (d | (r1 << 8) | r2 | (r3 << 16));
        }
    return TestEnd ();
    }

static bool op_MulAcc (int d)
    {
    InIT ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int r2 = reg (true);
    if (r2 < 0) return false;
    if (! match (',', 5)) return false;
    int r3 = reg (true);
    if (r3 < 0) return false;
    if (! match (',', 5)) return false;
    int r4 = reg (true);
    if (r4 < 0) return false;
    if (d & 1)
        {
        if ((r1 == 13) || (r1 == 15) || (r2 == 13) || (r2 == 15) || (r2 == r1)
            || (r3 == 13) || (r3 == 15) || (r4 == 13) || (r4 == 15)) return asmerr (103);   // Invalid register
        op32 ((d - 1) | (r1 << 12) | (r2 << 8) | (r3 << 16) | r4);
        }
    else
        {
        op32 (d | (r1 << 8) | (r2 << 16) | r3 | (r4 << 12));
        }
    return TestEnd ();
    }

static bool op_LDA_STL (int d)
    {
    InIT ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    if (! match ('[', 16)) return false;
    int r2 = reg (true);
    if (r2 < 0) return false;
    if (! match (']', 16)) return false;
    if ((r1 == 13) || (r1 == 15)) return asmerr (103);
    op32 (d | (r1 << 12) | (r2 << 16));
    return TestEnd ();
    }

static bool op_STLEX (int d)
    {
    InIT ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int r2 = reg (true);
    if (r2 < 0) return false;
    if (! match (',', 5)) return false;
    if (! match ('[', 16)) return false;
    int r3 = reg (true);
    if (r3 < 0) return false;
    if (! match (']', 16)) return false;
    if ((r1 == 13) || (r1 == 15) || (r2 == 13) || (r2 == 15) || (r2 == r1)
        || (r3 == 13) || (r3 == 15) || (r3 == r1)) return asmerr (103);
    op32 (d | r1 | (r2 << 12) | (r3 << 16));
    return TestEnd ();
    }

static bool op_PKH (int d)
    {
    InIT ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int r2 = reg (true);
    if (r2 < 0) return false;
    int r3 = -1;
    int c = 0;
    if (match (',', 0))
        {
        bool bShft = true;
        r3 = reg (false);
        if (r3 >= 0)
            {
            bShft = match (',', 0);
            }
        if (bShft)
            {
            nxt ();
            if (d & 0x20)
                {
                if (strnicmp (esi, "asr", 3)) return asmerr (31);    // Invalid argument
                }
            else
                {
                if (strnicmp (esi, "lsl", 3)) return asmerr (31);    // Invalid argument
                }
            esi += 3;
            if (! immediate (1, 31, &c)) return false;
            }
        }
    if (r3 < 0)
        {
        r3 = r2;
        r2 = r1;
        }
    if ((r1 == 13) || (r1 == 15) || (r2 == 13) || (r2 == 15) || (r3 == 13) || (r3 == 15)) return asmerr (103);
    op32 (d | (r1 << 8) | (r2 << 16) | r3 | ((c & 0x1C) << 10) | ((c & 0x03) << 6));
    return TestEnd ();
    }

static bool op_PL (int d)
    {
    InIT ();
    if (! EndOp ()) return false;
    int r1 = -1;
    int r2 = -1;
    int c;
    if (match ('[', 0))
        {
        r1 = reg (true);
        if (r1 < 0) return false;
        if (match (',', 0))
            {
            r2 = reg (false);
            if (r2 >= 0)
                {
                if (! arg_LSL (&c)) return false;
                }
            else
                {
                if (! immediate (-255, 4095, &c)) return false;
                }
            }
        if (! match (']', 16)) return false;
        }
    else
        {
        bool bW = (d & 0x00300000) == 0x00300000;
        if (bW) r1 = 15;
        if (! address (true, bW ? -255 : -4095, 4095, &c)) return false;
        }
    if (r1 < 0)
        {
        if (c < 0)
            {
            c = -c;
            }
        else
            {
            c |= 0x00800000;
            }
        op32 (BL(1111, 1000, 0000, 1111, 1111, 0000, 0000, 0000) | d | c);
        }
    else if (r2 < 0)
        {
        if (c < 0)
            {
            c = (-c) | 0x0C00;
            }
        else
            {
            c |= 0x00800000;
            }
        op32 (BL(1111, 1000, 0000, 0000, 1111, 0000, 0000, 0000) | d | (r1 << 16) | c);
        }
    else
        {
        op32 (BL(1111, 1000, 0000, 0000, 1111, 0000, 0000, 0000) | d | (r1 << 16) | r2 | (c << 4));
        }
    return TestEnd ();
    }

static bool op_XTAB (int d)
    {
    InIT ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int r2 = reg (true);
    if (r2 < 0) return false;
    int r3 = -1;
    int c = 0;
    bool bShft = false;
    if (match (',', 0))
        {
        r3 = reg (false);
        if (r3 >= 0)
            {
            bShft = match (',', 0);
            }
        else
            {
            r3 = r2;
            r2 = r1;
            bShft = true;
            }
        }
    if (bShft)
        {
        --esi;
        if (! arg_ROR (&c)) return false;
        }
    if ((r1 == 13) || (r1 == 15) || (r2 == 13) || (r3 == 13) || (r3 == 15)) return asmerr (103);
    op32 (d | (r1 << 8) | (r2 << 16) | r3 | c);
    return TestEnd ();
    }

static bool op_LDT_STT (int d)
    {
    InIT ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    if (! match ('[', 16)) return false;
    int r2 = reg (true);
    if (r2 < 0) return false;
    int c = 0;
    int sh = d & 3;
    if (match (',', 0))
        {
        if (! immediate (0, lim[sh], &c)) return false;
        if ((c & msk[sh]) != 0) return asmerr (119 + sh);   // Must be multiple of 2 / 4 (error 120 / 121)
        }
    if (! match (']', 16)) return false;
    if ((r1 == 13) || (r1 == 15)) return asmerr (103);
    op32 ((d & 0xFFFFFFFC) | (r1 << 12) | (r2 << 16) | (c >> sh));
    return TestEnd ();
    }

static bool op_LDRD_STRD (int d)
    {
    InIT ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int r2 = reg (true);
    if (r2 < 0) return false;
    if (! match (',', 5)) return false;
    int r3 = 15;
    int puw = 0x01000000;
    int c = 0;
    if (match ('[', 0))
        {
        r3 = reg (true);
        if (r3 < 0) return false;
        if (match (']', 0))
            {
            if (match (',', 0))
                {
                puw = 0x00200000;
                if (! immediate (-1020, 1020, &c)) return false;
                }
            }
        else
            {
            if (match (',', 0))
                {
                if (! immediate (-1020, 1020, &c)) return false;
                }
            if (! match (']', 16)) return false;
            if (match ('!', 0)) puw = 0x01200000;
            }
        }
    else
        {
        if (! address (true, -1020, 1020, &c)) return false;
        }
    if ((r1 == 13) || (r1 == 15) || (r2 == 13) || (r2 == 15) || (r2 == r1)) return asmerr (103);    // Invalid register
    if (c & 0x03) return asmerr (121);      // Must be multiple of 4
    if (c >= 0) puw |= 0x00800000;
    else        c = -c;
    op32 (d | puw | (r1 << 12) | (r2 << 8) | (r3 << 16) | (c >> 2));
    }

static bool op_STREX (int d)
    {
    InIT ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int r2 = reg (true);
    if (r2 < 0) return false;
    if (! match (',', 5)) return false;
    if (! match ('[', 16)) return false;
    int r3 = reg (true);
    if (r3 < 0) return false;
    int c = 0;
    if (match (',', 0))
        {
        if (! immediate (0, 1020, &c)) return false;
        }
    if (! match (']', 16)) return false;
    if ((r1 == 13) || (r1 == 15) || (r2 == 15) || (r3 == 15) || (r2 == r1) || (r3 == r1)) return asmerr (103);    // Invalid register
    if (c & 0x03) return asmerr (121);      // Must be a multiple of 4
    op32 (d | (r1 << 8) | (r2 << 12) | (r3 << 16) | (c >> 2));
    }

static bool op_TEQ (int d)
    {
    InIT ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int r2 = reg (false);
    if (r2 >= 0)
        {
        // Compare registers
        int rot;
        if (! arg_shift (&rot)) return false;
        op32 (BL(1110, 1010, 1001, 0000, 0000, 1111, 0000, 0000) | (r1 << 16) | r2 | rot);
        }
    else
        {
        // Compare immediate
        int c;
        if (r1 == 15) return asmerr (103);  // Invalid register
        if (! constant (&c, true) ) return false;
        op32 (BL(1111, 0000, 1001, 0000, 0000, 1111, 0000, 0000) | (r1 << 16) | c);
        }
    return TestEnd ();
    }

static bool op_RSB (int d)
    {
    bool bSF = IsSF ();
    bool bIT = InIT ();
    bool bWide = IsWide ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int r2 = reg (true);
    if (r2 < 0) return false;
    if (! match (',', 5)) return false;
    int r3 = reg (false);
    int c = 0;
    if (r3 >= 0)
        {
        if (! arg_shift (&c)) return false;
        }
    else
        {
        if (! constant (&c, true)) return false;
        }
    if (r3 >= 0)
        {
        if ((r1 == 13) || (r1 == 15) || (r2 == 13) || (r2 == 15) || (r3 == 13) || (r3 == 15)) return asmerr (103);  // Invalid register
        op32 (BL(1110, 1011, 1100, 0000, 0000, 0000, 0000, 0000) | (bSF ? BIT_SF : 0) | (r1 << 8) | (r2 << 16) | r3 | c);
        }
    else if (bWide || (r1 > 7) || (r2 > 7) || (c != 0) || (bSF == bIT))
        {
        if ((r1 == 13) || (r1 == 15) || (r2 == 13) || (r2 == 15)) return asmerr (103);  // Invalid register
        op32 (BL(1111, 0001, 1100, 0000, 0000, 0000, 0000, 0000) | (bSF ? BIT_SF : 0) | (r1 << 8) | (r2 << 16) | c);
        }
    else
        {
        op16 (BW(0100, 0010, 0100, 0000) | r1 | (r2 << 3));
        }
    return TestEnd ();
    }

static bool op_TB (int d)
    {
    if (! LastIT ()) return false;
    if (! EndOp ()) return false;
    if (! match ('[', 16)) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int r2 = reg (true);
    if (r2 < 0) return false;
    if (d & 0x10)
        {
        if (! match (',', 5)) return false;
        nxt ();
        if (strnicmp (esi, "lsl", 3)) return asmerr (31); // Invalid argument
        esi += 3;
        int c;
        if (! immediate (1, 1, &c)) return false;
        }
    if (! match (']', 16)) return false;
    if ((r1 == 13) || (r2 == 13) || (r2 == 15)) return asmerr (103);
    op32 (d | (r1 << 16) | r2);
    return TestEnd ();
    }

static bool op_LDM (int d)
    {
    InIT ();
    bool bWide = IsWide ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    bool bWB = match ('!', 0);
    if (! match (',', 5)) return false;
    int rl = reglist ();
    if (rl == 0) return false;
    if ((! bWide) && (r1 == 13) && bWB && ((rl & 0x7F00) == 0))
        {
        op16 (BW(1011, 1100, 0000, 0000) | ((rl & 0x8000) >> 7) | (rl & 0x00FF));
        }
    else if ((! bWide) && (r1 < 8) && ((rl & 0xFF00) == 0) && (((rl & ( 1 << r1)) == 0) == bWB))
        {
        op16 (BW(1100, 1000, 0000, 0000) | (r1 << 8) | rl);
        }
    else
        {
        if ((r1 == 15) || ((rl & 0x2000) != 0) || ((rl & 0xC000) == 0xC000)) return asmerr (103);
        op32 (BL(1110, 1000, 1001, 0000, 0000, 0000, 0000, 0000) | (bWB ? 0x00200000 : 0) | (r1 << 16) | rl);
        }
    return TestEnd ();
    }

static bool op_STM (int d)
    {
    InIT ();
    bool bWide = IsWide ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    bool bWB = match ('!', 0);
    if (! match (',', 5)) return false;
    int rl = reglist ();
    if (rl == 0) return false;
    if ((! bWide) && (r1 < 8) && ((rl & 0xFF00) == 0) && (((rl & ( 1 << r1)) == 0) == bWB))
        {
        op16 (BW(1100, 0000, 0000, 0000) | (r1 << 8) | rl);
        }
    else
        {
        if ((r1 == 15) || ((rl & 0xA000) != 0)) return asmerr (103);
        op32 (BL(1110, 1000, 1000, 0000, 0000, 0000, 0000, 0000) | (bWB ? 0x00200000 : 0) | (r1 << 16) | rl);
        }
    return TestEnd ();
    }

static bool op_STMDB (int d)
    {
    InIT ();
    bool bWide = IsWide ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    bool bWB = match ('!', 0);
    if (! match (',', 5)) return false;
    int rl = reglist ();
    if (rl == 0) return false;
    if ((! bWide) && (r1 == 13) && bWB && ((rl & 0xBF00) == 0))
        {
        op16 (BW(1011, 0100, 0000, 0000) | ((rl & 0x4000) >> 6) | (rl & 0x00FF));
        }
    else
        {
        if ((r1 == 15) || ((rl & 0xA000) != 0)) return asmerr (103);
        op32 (BL(1110, 1001, 0000, 0000, 0000, 0000, 0000, 0000) | (bWB ? 0x00200000 : 0) | (r1 << 16) | rl);
        }
    return TestEnd ();
    }

static bool op_LDMDB (int d)
    {
    InIT ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    bool bWB = match ('!', 0);
    if (! match (',', 5)) return false;
    int rl = reglist ();
    if (rl == 0) return false;
    if ((r1 == 15) || ((rl & 0x2000) != 0) || ((rl & 0xC000) == 0xC000)) return asmerr (103);
    op32 (BL(1110, 1001, 0001, 0000, 0000, 0000, 0000, 0000) | (bWB ? 0x00200000 : 0) | (r1 << 16) | rl);
    return TestEnd ();
    }

static bool op_CPS (int d)
    {
    if (InIT ()) return asmerr (118);   // Not allowed inIf/Then block
    if (! EndOp ()) return false;
    int flags = 0;
    nxt ();
    for (int i = 0; i < 2; ++i)
        {
        if (IsEnd ()) break;
        else if (match ('f', 0)) flags |= 1;
        else if (match ('F', 0)) flags |= 1;
        else if (match ('i', 0)) flags |= 2;
        else if (match ('I', 0)) flags |= 2;
        else return asmerr (31);     // Invalid argument
        }
    if (flags == 0) return asmerr (31);     // Invalid argument
    op16 (d | flags);
    return TestEnd ();
    }

static bool op_DBG (int d)
    {
    InIT ();
    if (! EndOp ()) return false;
    int c = 0;
    if (! immediate (0, 15, &c)) return false;
    op32 (d | c);
    return TestEnd ();
    }

static bool op_Barrier (int d)
    {
    InIT ();
    if (! EndOp ()) return false;
    if (nxt () > ' ')
        {
        if (strnicmp (esi, "sy", 2)) return asmerr (31);    // Invalid argument
        esi += 2;
        }
    op32 (d);
    return TestEnd ();
    }

static bool op_UDF (int d)
    {
    InIT ();
    bool bWide = IsWide ();
    if (! EndOp ()) return false;
    int c;
    if (! immediate (0, 65535, &c)) return asmerr (111);   // Immediate out of range
    if (bWide || (c > 255))
        {
        op32 (BL(1111, 0111, 1111, 0000, 1010, 0000, 0000, 0000) | ((c & 0xF000) << 4) | (c & 0x0FFF));
        }
    else
        {
        op16 (BW(1101, 1110, 0000, 0000) | c);
        }
    return TestEnd ();
    }

static bool op_BKPT (int d)
    {
    bool bWide = IsWide ();
    if (! EndOp ()) return false;
    int c;
    if (! immediate (0, 255, &c)) return asmerr (111);   // Immediate out of range
    op16 (d | c);
    return TestEnd ();
    }

static bool op_B (int d)
    {
    int cc = -1;
    if (it_nc > 0)
        {
        if (! LastIT ()) return false;
        }
    else
        {
        cc = GetCC (false);
        }
    bool bWide = IsWide ();
    if (! EndOp ()) return false;
    int c;
    if (! address (false, 0xFF000001, 0x00FFFFFF, &c)) return false;
    if (c & 1) return asmerr (120);     // Must be multiple of 2
    if (cc >= 0)
        {
        if (bWide)
            {
            if ((c > 0xFFFFF) || (c <= (int) 0xFFF00001)) asmerr (1);  // Jump out of range
            c = (c & 0x04000000) | ((c & 0x80000) >> 8) | ((c & 0x40000) >> 5) | ((c & 0x3F000) << 4) | ((c & 0x00FFE) >> 1);
            op32 (BL(1111, 0000, 0000, 0000, 1000, 0000, 0000, 0000) | (cc << 22) | c);
            }
        else
            {
            if (c & 1) asmerr (120);    // Must be multiple of 2
            if ((c < -254) | (c > 254)) asmerr (119);   // Out of range, use .W
            op16 (BW(1101, 0000, 0000, 0000) | (cc << 8) | ((c & 0x1FE) >> 1));
            }
        }
    else
        {
        if (bWide)
            {
            c = (c & 0x04000000) | ((c & 0x800000) >> 10) | ((c & 0x400000) >> 11) | ((c & 0x3FF000) << 4) | ((c & 0x000FFE) >> 1);
            if ((c & 0x04000000) == 0) c ^= 0x2800;
            op32 (BL(1111, 0000, 0000, 0000, 1001, 0000, 0000, 0000) | c);
            }
        else
            {
            if ((c < -4094) | (c > 4094)) asmerr (119);     // Out of range, use .W
            op16 (BW(1110, 0000, 0000, 0000) | ((c & 0xFFE) >> 1));
            }
        }
    return TestEnd ();
    }

static bool op_BL (int d)
    {
    if (! LastIT ()) return false;
    if (! EndOp ()) return false;
    int c;
    if (! address (false, 0xFF000001, 0x00FFFFFF, &c)) return false;
    if (c & 1) return asmerr (120);     // Muust be multiple of 2
    c = (c & 0x04000000) | ((c & 0x800000) >> 10) | ((c & 0x400000) >> 11) | ((c & 0x3FF000) << 4) | ((c & 0x000FFE) >> 1);
    if ((c & 0x04000000) == 0) c ^= 0x2800;
    op32 (BL(1111, 0000, 0000, 0000, 1101, 0000, 0000, 0000) | c);
    return TestEnd ();
    }

static bool op_RRX (int d)
    {
    InIT ();
    bool bSF = IsSF ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    int r2 = r1;
    if (match (',', 0))
        {
        r2 = reg (true);
        if (r2 < 0) return false;
        }
    op32 (d | (bSF ? BIT_SF : 0) | (r1 << 8) | r2);
    return TestEnd ();
    }

static bool op_ADR (int d)
    {
    InIT ();
    bool bWide = IsWide ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int c;
    if (! address (true, -4095, 4095, &c)) return false;
    if (bWide || (r1 > 7))
        {
        if (c >= 0)
            {
            c = ((c & 0x800) << 14) | ((c & 0x700) << 4) | (c & 0x0FF);
            }
        else
            {
            c = -c;
            c = 0x00A00000 | ((c & 0x800) << 14) | ((c & 0x700) << 4) | (c & 0x0FF);
            }
        op32 (BL(1111, 0010, 0000, 1111, 0000, 0000, 0000, 0000) | (r1 << 8) | c);
        }
    else
        {
        if ((c & 3) != 0) asmerr (121);     // Must be multiple of 4
        if ((c < 0) || (c > 1020)) asmerr (119);   // Out of range, use .W
        op16 (BW(1010, 0000, 0000, 0000) | (r1 << 8) | (c >> 2));
        }
    return TestEnd ();
    }

static bool op_CBZ (int d)
    {
    if (InIT ()) return asmerr (118);   // Not allowed in If/Then block
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    if (r1 > 7) return asmerr (103);    // Invalid register
    if (! match (',', 5)) return false;
    int c;
    if (! address (false, 0, 126, &c)) return false;
    if (c & 1) return asmerr (120);     // Must be multiple of 2
    op16 (d | r1 | ((c & 0x40) << 3) | ((c & 0x3E) << 2));
    return TestEnd ();
    }

static bool op_MOVT (int d)
    {
    InIT ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int c;
    if (! immediate (0, 0xFFFF, &c)) return false;
    c = ((c & 0xF000) << 4) | ((c & 0x0800) << 15) | ((c & 0x0700) << 4) | (c & 0x00FF);
    op32 (d | (r1 << 8) | c);
    return TestEnd ();
    }

static bool op_SAT (int d)
    {
    InIT ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int sat;
    if (! immediate (1, 32, &sat)) return false;
    if (! match (',', 5)) return false;
    int r2 = reg (true);
    if (r2 < 0) return false;
    int dir = 0;
    int sh = 0;
    if ((d & 0x00200000) == 0)
        {
        if (match (',', 0))
            {
            nxt ();
            dir = lookup (shift, count (shift));
            if ((dir != 0) && (dir != 2)) return asmerr (31);    // Invalide argument
            dir >>= 1;
            if (! immediate (dir, 31, &sh)) return false;
            }
        }
    else
        {
        if (sat > 16) return asmerr (111);  // Invalid immediate
        }
    op32 ((d & 0xFFFFFFFE) | (r1 << 8) | (r2 << 16) | (dir << 21) | ((sh & 0x1C) << 10) | ((sh & 0x03) << 6) | (sat - (d & 1)));
    return TestEnd ();
    }

static bool op_BitField (int d)
    {
    InIT ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int r2 = 0;
    if ((d & 0x10000) == 0)
        {
        r2 = reg (true);
        if (r2 < 0) return false;
        if (! match (',', 5)) return false;
        }
    int lsb;
    if (! immediate (0, 31, &lsb)) return false;
    if (! match (',', 5)) return false;
    int wth;
    if (! immediate (1, 31 - lsb, &wth)) return false;
    if ((r1 == 13) || (r1 == 15) || (r2 == 13)) return asmerr (103);    // Invalid register
    if (d & 0x200000) wth += lsb;
    op32 (d | (r1 << 8) | (r2 << 16) | ((lsb & 0x1C) << 10) | ((lsb & 0x03) << 6) | (wth - 1));
    return TestEnd ();
    }

static int sreg (bool bErr)
    {
    char ch = nxt ();
    if ((ch != 's') && (ch != 'S'))
        {
        if ( bErr ) asmerr (113);   // Invalid floating point register
        return -1;
        }
    ++esi;
    if ((*esi < '0') || (*esi > '9'))
        {
        --esi;
        if ( bErr ) asmerr (113);   // Invalid floating point register
        return -1;
        }
    int r = *esi - '0';
    ++esi;
    if (((r < 3) && (*esi >= '0') && (*esi <= '9')) || ((r == 3) && (*esi >= '0') && (*esi <= '1')))
        {
        r = 10 * r + *esi - '0';
        ++esi;
        }
    return r;
    }

static const char *types[] = {".F32", ".F", ".S16", ".S32", ".U16", ".U32"};

static bool IsFloat (void)
    {
    if (*esi <= ' ') return true;
    if (*esi == '.')
        {
        int t = lookup (types, count (types));
        if (((t == 0) || (t == 1)) && (*esi <= ' ')) return true;
        asmerr (123);   // Invalid data type
        }
    else
        {
        asmerr (101);   // Invalid opcode
        }
    return false;
    }

static bool fimmediate (int *imm)
    {
    if (nxt () == '#') ++esi;
    *imm = 0;
    double f = valuef ();
    union
        {
        float       f;
        int    u;
        } bits;
    bits.f = f;
    if ((double) bits.f != f) return asmerr (122);        // Invalid FP constant
    if ((bits.u & 0x0007FFFF) != 0) return asmerr (122);    // Invalid FP constant
    int e = bits.u & 0x7E000000;
    if ((e != 0x3E000000) && (e != 0x40000000)) return asmerr (122);   // Invalid FP constant
    *imm = ((bits.u & 0x80000000) >> 24) | ((bits.u & 0x20000000) >> 23) | ((bits.u & 0x01F80000) >> 19);
    return true;
    }

static bool op_VMOV (int d)
    {
    InIT ();
    if (! IsFloat ()) return false;
    int r1 = reg (false);
    int r2;
    int r3;
    int r4;
    if (r1 >= 0)
        {
        if (! match (',', 5)) return false;
        r2 = sreg (false);
        if (r2 >= 0)
            {
            op32 (BL(1110, 1110, 0001, 0000, 0000, 1010, 0001, 0000) | (r1 << 12) | ((r2 & 0x01) << 7) | ((r2 & 0x1E) << 15));
            return TestEnd ();
            }
        r2 = reg (true);
        if (r2 < 0) return false;
        if (! match (',', 5)) return false;
        r3 = sreg (true);
        if (r3 < 0) return false;
        if (! match (',', 5)) return false;
        r4 = sreg (true);
        if (r4 < 0) return false;
        if (r4 != r3 + 1) return asmerr (113);  // Invalid FP register
        op32 (BL(1110, 1100, 0101, 0000, 0000, 1010, 0001, 0000) | (r1 << 12) | (r2 << 16) | ((r3 & 0x01) << 5) | ((r3 & 0x1E) >> 1));
        return TestEnd ();
        }
    r1 = sreg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    r2 = reg (false);
    if (r2 >= 0)
        {
        op32 (BL(1110, 1110, 0000, 0000, 0000, 1010, 0001, 0000) | ((r1 & 0x1E) << 15) | ((r1 & 0x01) << 7) | (r2 << 12));
        return TestEnd ();
        }
    r2 = sreg (false);
    if (r2 >= 0)
        {
        if (match (',', 0))
            {
            if (r2 != r1 + 1) return asmerr (113);  // Invalid FP register
            r3 = reg (true);
            if (r3 < 0) return false;
            if (! match (',', 5)) return false;
            r4 = reg (true);
            if (r4 < 0) return false;
            op32 (BL(1110, 1100, 0100, 0000, 0000, 1010, 0001, 0000) | ((r1 & 0x01) << 5) | (r1 >> 1) | (r3 << 12) | (r4 << 16));
            return TestEnd ();
            }
        op32 (BL(1110, 1110, 1011, 0000, 0000, 1010, 0100, 0000) | ((r1 & 0x01) << 22) | ((r1 & 0x1E) << 11) | ((r2 & 0x01) << 5) | ((r2 & 0x1E) >> 1));
        return TestEnd ();
        }
    int c;
    if (! fimmediate (&c)) return false;
    op32 (BL(1110, 1110, 1011, 0000, 0000, 1010, 0000, 0000) | ((r1 & 0x01) << 22) | ((r1 & 0x1E) << 11) | ((c & 0xF0) << 16) | (c & 0x0F));
    return TestEnd ();
    }

static bool op_VCMP (int d)
    {
    InIT ();
    if (! IsFloat ()) return false;
    int r1 = sreg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int r2 = sreg (false);
    if (r2 >= 0)
        {
        op32 (BL(1110, 1110, 1011, 0100, 0000, 1010, 0100, 0000) | d | ((r1 & 0x01) << 22) | ((r1 & 0x1E) << 11) | ((r2 & 0x01) << 5) | ((r2 & 0x1E) >> 1));
        }
    else
        {
        match ('#', 0);
        double f = valuef ();
        if (f != 0.0) return asmerr (122);    // Invalid FP constatnt
        op32 (BL(1110, 1110, 1011, 0101, 0000, 1010, 0100, 0000) | d | ((r1 & 0x01) << 22) | ((r1 & 0x1E) << 11));
        }
    return TestEnd ();
    }

static bool op_VOne (int d)
    {
    InIT ();
    if (! IsFloat ()) return false;
    int r1 = sreg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int r2 = sreg (true);
    if (r2 < 0) return false;
    op32 (d | ((r1 & 0x01) << 22) | ((r1 & 0x1E) << 11) | ((r2 & 0x01) << 5) | ((r2 & 0x1E) >> 1));
    return TestEnd ();
    }

static bool op_VCVT (int d)
    {
    InIT ();
    if (*esi != '.') return asmerr (101);
    int t1 = lookup (types, count (types));
    if (t1 < 0) return asmerr (123);    // Invalid data type
    int t2 = lookup (types, count (types));
    if (t2 < 0) return asmerr (123);    // Invalid data type
    if ((t1 < 2) == (t2 < 2)) return asmerr (123);  // Invalid data type
    if (t2 < 2) t2 = t1;
    if (! EndOp ()) return false;
    int r1 = sreg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int r2 = sreg (true);
    if (r2 < 0) return false;
    if (match (',', 0))
        {
        if (r2 != r1) return asmerr (113);  // Invalid FP register
        int bits;
        if (! immediate (1, 32, &bits)) return false;
        bits = 32 - bits;
        op32 (BL(1110, 1110, 1011, 1010, 0000, 1010, 1100, 0000) | (t1 >= 2 ? 0x40000 : 0) | (t2 >= 4 ? 0x10000 : 0) | ((t2 & 1) << 7)
            | ((r1 & 0x01) << 22) | ((r1 & 0x1E) << 11) | ((bits & 0x01) << 5) | ((bits & 0x1E) >> 1));
        }
    else if (t1 < 2)
        {
        if ((t2 & 1) == 0) return asmerr (123);     // Invalid data type
        op32 (BL(1110, 1110, 1011, 1000, 0000, 1010, 0100, 0000) | (t2 < 4 ? 0x80 : 0)
            | ((r1 & 0x01) << 22) | ((r1 & 0x1E) << 11) | ((r2 & 0x01) << 5) | (r2 >> 1));
        }
    else
        {
        if ((t2 & 1) == 0) return asmerr (123);     // Invalid data type
        op32 (BL(1110, 1110, 1011, 1100, 0000, 1010, 1100, 0000) | (t2 < 4 ? 0x1000 : 0)
            | ((r1 & 0x01) << 22) | ((r1 & 0x1E) << 11) | ((r2 & 0x01) << 5) | (r2 >> 1));
        }
    return TestEnd ();
    }

static bool op_VCVTX (int d)
    {
    if (InIT ()) return asmerr (118);   // Not allowed in If/Then block
    int t1 = lookup (types, count (types));
    if ((t1 != 3) && (t1 != 5)) return asmerr (123);    // Invalid data type
    int t2 = lookup (types, count (types));
    if ((t2 != 0) && (t2 != 1)) return asmerr (123);    // Invalid data type
    if (! EndOp ()) return false;
    int r1 = sreg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int r2 = sreg (true);
    if (r2 < 0) return false;
    if (d & 0x10000000)
        {
        op32 (d | (t1 == 3 ? 0x80 : 0) | ((r1 & 0x01) << 22) | ((r1 & 0x1E) << 11) | ((r2 & 0x01) << 5) | (r2 >> 1));
        }
    else
        {
        op32 (d | (t1 == 3 ? 0x10000 : 0) | ((r1 & 0x01) << 22) | ((r1 & 0x1E) << 11) | ((r2 & 0x01) << 5) | (r2 >> 1));
        }
    return TestEnd ();
    }

static bool op_VArith (int d)
    {
    InIT ();
    if (! IsFloat ()) return false;
    int r1 = sreg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int r2 = sreg (true);
    if (r2 < 0) return false;
    int r3;
    if (match (',', 0))
        {
        r3 = sreg (true);
        if (r3 < 0) return false;
        }
    else
        {
        r3 = r2;
        r2 = r1;
        }
    op32 (d | ((r1 & 0x01) << 22) | ((r1 & 0x1E) << 11) | ((r2 & 0x01) << 7) | ((r2 & 0x1E) << 15) | ((r3 & 0x01) << 5) | (r3 >> 1));
    return TestEnd ();
    }

static bool op_VLD_VST (int d)
    {
    InIT ();
    if (! IsFloat ()) return false;
    int r1 = sreg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    int r2 = 15;
    int c = 0;
    int sh = d & 3;
    if (match ('[', 0))
        {
        r2 = reg (true);
        if (r2 < 0) return false;
        if (match (',', 0))
            {
            if (! immediate (-1020, 1020, &c)) return false;
            }
        if (! match (']', 16)) return false;
        }
    else
        {
        if (! address (true, -1020, 1020, &c)) return false;
        }
    if (c & 3) return asmerr (121); // Must be multiple of 4
    if (c >= 0) c = 0x800000 | (c >> 2);
    else        c = (-c) >> 2;
    op32 (d | ((r1 & 0x01) << 22) | ((r1 & 0x1E) << 11) | (r2 << 16) | c);
    return TestEnd ();
    }

static int sreglist (void)
    {
    if (! match ('{', 16)) return 0;
    int r1 = sreg (true);
    if (r1 < 0) return 0;
    if (! match ('-', 16)) return 0;
    int r2 = sreg (true);
    if (r2 < 0) return 0;
    if (! match ('}', 16)) return 0;
    if (r2 < r1)
        {
        asmerr (107);   // Invalid register list
        return 0;
        }
    return ((r1 & 0x01) << 22) | ((r1 & 0x1E) << 11) | (r2 + 1 - r1);
    }

static bool op_VStack (int d)
    {
    InIT ();
    if (! IsFloat ()) return false;
    int sl = sreglist ();
    if (sl == 0) return false;
    op32 (d | sl);
    return TestEnd ();
    }

static bool op_VMulti (int d)
    {
    InIT ();
    if (! IsFloat ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    bool bWB = match ('!', 0);
    if ((d & 0x200000) && (! bWB)) return asmerr (16);  // Syntax error
    if (! match (',', 5)) return false;
    int sl = sreglist ();
    if (sl == 0) return false;
    op32 (d | (bWB ? 0x200000 : 0) | (r1 << 16) | sl);
    return TestEnd ();
    }

static bool op_VLazy (int d)
    {
    InIT ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    if (match (',', 0))
        {
        int sl = sreglist ();
        if (sl == 0) return false;
        }
    op32 (d | (r1 << 16));
    return TestEnd ();
    }

static const char *spec_reg[] = {
    "APSR",		        // 0b00000000
    "IAPSR",			// 0b00000001
    "EAPSR",			// 0b00000010
    "XPSR",				// 0b00000011
    "IPSR",				// 0b00000101
    "EPSR",				// 0b00000110
    "IEPSR",			// 0b00000111
    "MSP",				// 0b00001000
    "PSP",				// 0b00001001
    "MSPLIM",			// 0b00001010
    "PSPLIM",			// 0b00001011
    "PRIMASK",			// 0b00010000
    "BASEPRI",			// 0b00010001
    "BASEPRI_MAX",		// 0b00010010
    "FAULTMASK",		// 0b00010011
    "CONTROL",			// 0b00010100
    "PAC_KEY_P_0",		// 0b00100000
    "PAC_KEY_P_1",		// 0b00100001
    "PAC_KEY_P_2",		// 0b00100010
    "PAC_KEY_P_3",		// 0b00100011
    "PAC_KEY_U_0",		// 0b00100100
    "PAC_KEY_U_1",		// 0b00100101
    "PAC_KEY_U_2",		// 0b00100110
    "PAC_KEY_U_3",		// 0b00100111
    "MSP_NS",			// 0b10001000
    "PSP_NS",			// 0b10001001
    "MSPLIM_NS",		// 0b10001010
    "PSPLIM_NS",		// 0b10001011
    "PRIMASK_NS",		// 0b10010000
    "BASEPRI_NS",		// 0b10010001
    "FAULTMASK_NS",		// 0b10010011
    "CONTROL_NS",		// 0b10010100
    "SP_NS",			// 0b10011000
    "PAC_KEY_P_0_NS",	// 0b10100000
    "PAC_KEY_P_1_NS",	// 0b10100001
    "PAC_KEY_P_2_NS",	// 0b10100010
    "PAC_KEY_P_3_NS",	// 0b10100011
    "PAC_KEY_U_0_NS",	// 0b10100100
    "PAC_KEY_U_1_NS",	// 0b10100101
    "PAC_KEY_U_2_NS",	// 0b10100110
    "PAC_KEY_U_3_NS" 	// 0b10100111
    };

static const int spec_idx[] = {
    0b00000000,		// APSR
    0b00000001,		// IAPSR
    0b00000010,		// EAPSR
    0b00000011,		// XPSR
    0b00000101,		// IPSR
    0b00000110,		// EPSR
    0b00000111,		// IEPSR
    0b00001000,		// MSP
    0b00001001,		// PSP
    0b00001010,		// MSPLIM
    0b00001011,		// PSPLIM
    0b00010000,		// PRIMASK
    0b00010001,		// BASEPRI
    0b00010010,		// BASEPRI_MAX
    0b00010011,		// FAULTMASK
    0b00010100,		// CONTROL
    0b00100000,		// PAC_KEY_P_0
    0b00100001,		// PAC_KEY_P_1
    0b00100010,		// PAC_KEY_P_2
    0b00100011,		// PAC_KEY_P_3
    0b00100100,		// PAC_KEY_U_0
    0b00100101,		// PAC_KEY_U_1
    0b00100110,		// PAC_KEY_U_2
    0b00100111,		// PAC_KEY_U_3
    0b10001000,		// MSP_NS
    0b10001001,		// PSP_NS
    0b10001010,		// MSPLIM_NS
    0b10001011,		// PSPLIM_NS
    0b10010000,		// PRIMASK_NS
    0b10010001,		// BASEPRI_NS
    0b10010011,		// FAULTMASK_NS
    0b10010100,		// CONTROL_NS
    0b10011000,		// SP_NS
    0b10100000,		// PAC_KEY_P_0_NS
    0b10100001,		// PAC_KEY_P_1_NS
    0b10100010,		// PAC_KEY_P_2_NS
    0b10100011,		// PAC_KEY_P_3_NS
    0b10100100,		// PAC_KEY_U_0_NS
    0b10100101,		// PAC_KEY_U_1_NS
    0b10100110,		// PAC_KEY_U_2_NS
    0b10100111 		// PAC_KEY_U_3_NS
    };

static bool op_MRS (int d)
    {
    InIT ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    if (! match (',', 5)) return false;
    nxt ();
    int sr = lookup (spec_reg, count (spec_reg));
    if (sr < 0) return asmerr (109);    // Invalid special register
    sr = spec_idx[sr];
    op32 (d | (r1 << 8) | sr);
    return TestEnd ();
    }

static const char *srmask[] = {"_g", "_nzcvq", "_nzcvqg"};

static bool op_MSR (int d)
    {
    InIT ();
    if (! EndOp ()) return false;
    nxt ();
    int sr = lookup (spec_reg, count (spec_reg));
    if (sr < 0) return asmerr (109);    // Invalid special register
    sr = spec_idx[sr];
    int mask = 0b10;
    if (sr <= 3)
        {
        mask = lookup (srmask, count(srmask)) + 1;
        if (mask == 0) return asmerr (109); // Invalid special register
        }
    if (! match (',', 5)) return false;
    int r1 = reg (true);
    if (r1 < 0) return false;
    op32 (d | (r1 << 16) | sr | (mask << 10));
    return TestEnd ();
    }

static bool op_VMRS (int d)
    {
    InIT ();
    if (! EndOp ()) return false;
    int r1 = reg (true);
    if (r1 == 15) return asmerr (103);
    if (r1 < 0)
        {
        if (strnicmp (esi, "APSR_nzcv", 9)) return asmerr (109);    // Invalid special register
        esi += 9;
        r1 = 15;
        };
    if (! match (',', 5)) return false;
    int sr = 1;
    nxt ();
    if (strnicmp (esi, "FPSCR", 5)) return asmerr (109);    // Invalid special register
    esi += 5;
    op32 (d | (r1 << 12) | (sr << 16));
    return TestEnd ();
    }

static bool op_VMSR (int d)
    {
    InIT ();
    if (! EndOp ()) return false;
    int sr = 1;
    nxt ();
    if (strnicmp (esi, "FPSCR", 5)) return asmerr (109);    // Invalid special register
    esi += 5;
    if (! match (',', 5)) return false;
    int r1 = reg (true);
    if (r1 == 15) return asmerr (103);
    if (r1 < 0)
        {
        if (strnicmp (esi, "APSR_nzcv", 9)) return asmerr (109);    // Invalid special register
        esi += 9;
        r1 = 15;
        };
    op32 (d | (r1 << 12) | (sr << 16));
    return TestEnd ();
    }

static bool po_OPT (int d)
    {
    liston = (liston & 0x0F) | (expri () << 4);
    return TestEnd ();
    }

static bool po_DB (int d)
    {
#ifndef TEST
    VAR v = expr ();
    if (v.s.t == -1)
        {
        if (v.s.l > 256)
            asmerr (19); // 'String too long'
        poke (v.s.p + zero, v.s.l);
        return TestEnd ();
        }
    if (v.i.t)
        v.i.n = v.f;
    poke (&v.i.n, 1);
#endif
    return TestEnd ();
    }

static bool po_EQUB (int d)
    {
    int n = expri ();
    poke (&n, 1);
    return TestEnd ();
    }

static bool po_EQUW (int d)
    {
    align (2);
    int n = expri ();
    poke (&n, 2);
    return TestEnd ();
    }

#ifndef TEST
static const char *oslist[] = {
    "osrdch", "oswrch", "oskey", "osline", "oscli", "osopen", "osbyte", "osword",
    "osshut", "osbget", "osbput", "getptr", "setptr", "getext" };

static const void *osfunc[] = {
    osrdch, oswrch, oskey, osline, oscli, osopen, osbyte, osword, 
    osshut, osbget, osbput, getptr, setptr, getext };
#endif

static bool po_EQUD (int d)
    {
#ifndef TEST
    VAR v = expr ();
    long long n;
    if (v.s.t == -1)
        {
        signed char *oldesi = esi;
        int i;
        memcpy (accs, v.s.p + zero, v.s.l);
        *(accs + v.s.l) = 0;
        esi = (signed char *)accs;
        nxt ();
        i = lookup (oslist, sizeof(oslist) /
            sizeof(oslist[0]));
        esi = oldesi;
        if (i >= 0)
            n = (size_t) osfunc[i];
        else
            n = (size_t) sysadr (accs);
        if (n == 0)
            asmerr (51); // 'No such system call'
        }
    else if (v.i.t == 0)
        n = v.i.n;
    else
        n = v.f;
    align (4);
    if (d == 1) poke (&n, 8);
    else        poke (&n, 4);
#endif
    return TestEnd ();
    }

static bool po_EQUS (int d)
    {
#ifndef TEST
    VAR v = exprs ();
    if (v.s.l > 256)
        asmerr (19); // 'String too long'
    poke (v.s.p + zero, v.s.l);
#endif
    return TestEnd ();
    }

static bool po_ALIGN (int d)
    {
    align (4);
    if ((nxt() >= '1') && (*esi <= '9'))
        {
        int n = expri ();
        if ((n & (n - 1)) || (n & 0xFFFFFF03) || (n == 0))
            asmerr (105); // 'invalid alignment'
        int nop = BW(1011, 1111, 0000, 0000);
        while (stavar[16] & (n - 1))
            poke (&nop, 2); 
        }
    return TestEnd ();
    }

typedef struct
    {
    char        s[8];
    bool        (*f)(int);
    int    d;
    }
    op_def;

static const op_def opcodes[] = {
    {"adc",     op_Logic,       BL(0000, 0001, 0100, 0000, 0000, 0001, 0100, 0000) },    // adc
    {"add",     op_ADD_SUB,                                                      0 },    // add
    {"addw",    op_ADD_SUB,                                                      1 },    // add
    {"adr",     op_ADR,                                                          0 },    // adr
    {"align",   po_ALIGN,                                                        0 },    // align
    {"and",     op_Logic,       BL(0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000) },    // and
    {"asr",     op_Shift,                               BW(0000, 0001, 1100, 0010) },    // asr
    {"b",       op_B,                                                            0 },    // b
    {"bfc",     op_BitField,    BL(1111, 0011, 0110, 1111, 0000, 0000, 0000, 0000) },    // bfc
    {"bfi",     op_BitField,    BL(1111, 0011, 0110, 0000, 0000, 0000, 0000, 0000) },    // bfi
    {"bic",     op_Logic,       BL(0000, 0000, 0010, 0000, 0000, 0011, 1000, 0000) },    // bic
    {"bkpt",    op_BKPT,                                BW(1011, 1110, 0000, 0000) },    // bkpt
    {"bl",      op_BL,                                                           0 },    // bl
    {"blx",     op_BX,                                  BW(0100, 0111, 1000, 0000) },    // blx
    {"blxns",   op_BX,                                  BW(0100, 0111, 1000, 0100) },    // blxns
    {"bx",      op_BX,                                  BW(0100, 0111, 0000, 0000) },    // bx
    {"bxns",    op_BX,                                  BW(0100, 0111, 0000, 0100) },    // bxns
    {"cbnz",    op_CBZ,                                 BW(1011, 1001, 0000, 0000) },    // cbnz
    {"cbz",     op_CBZ,                                 BW(1011, 0001, 0000, 0000) },    // cbz
    {"clrex",   op_NoneW,       BL(1111, 0011, 1011, 1111, 1000, 1111, 0010, 1111) },    // clrex
    {"clz",     op_Bit,         BL(1111, 1010, 1011, 0000, 1111, 0000, 1000, 0000) },    // clz
    {"cmn",     op_TST,         BL(0000, 0001, 0000, 0000, 0000, 0000, 1100, 0000) },    // cmn
    {"cmp",     op_CMP,                                                          0 },    // cmp
    {"cpsid",   op_CPS,                                 BW(1011, 0110, 0111, 0000) },    // cpsid
    {"cpsie",   op_CPS,                                 BW(1011, 0110, 0110, 0000) },    // cpsie
    {"csdb",    op_NoneW,       BL(1111, 0011, 1010, 1111, 1000, 0000, 0001, 0100) },    // csdb
    {"db",      po_DB,                                                           0 },    // db
    {"dbg",     op_DBG,         BL(1111, 0011, 1010, 1111, 1000, 0000, 1111, 0000) },    // dbg
    {"dcb",     po_EQUB,                                                         0 },    // dcb
    {"dcd",     po_EQUD,                                                         0 },    // dcd
    {"dcs",     po_EQUS,                                                         0 },    // dcs
    {"dcw",     po_EQUW,                                                         0 },    // dcw
    {"dmb",     op_Barrier,     BL(1111, 0011, 1011, 1111, 1000, 1111, 0101, 1111) },    // dmb
    {"dsb",     op_Barrier,     BL(1111, 0011, 1011, 1111, 1000, 1111, 0100, 1111) },    // dsb
    {"eor",     op_Logic,       BL(0000, 0000, 1000, 0000, 0000, 0000, 0100, 0000) },    // eor
    {"equb",    po_EQUB,                                                         0 },    // equb
    {"equd",    po_EQUD,                                                         0 },    // equd
    {"equq",    po_EQUD,                                                         1 },    // equq
    {"equs",    po_EQUS,                                                         0 },    // equs
    {"equw",    po_EQUW,                                                         0 },    // equw
    {"isb",     op_Barrier,     BL(1111, 0011, 1011, 1111, 1000, 1111, 0110, 1111) },    // isb
    {"it",      op_IT,                                  BW(1011, 1111, 0000, 0000) },    // it
    {"lda",     op_LDA_STL,     BL(1110, 1000, 1101, 0000, 0000, 1111, 1010, 1111) },    // lda
    {"ldab",    op_LDA_STL,     BL(1110, 1000, 1101, 0000, 0000, 1111, 1000, 1111) },    // ldab
    {"ldaex",   op_LDA_STL,     BL(1110, 1000, 1101, 0000, 0000, 1111, 1110, 1111) },    // ldaex
    {"ldaexb",  op_LDA_STL,     BL(1110, 1000, 1101, 0000, 0000, 1111, 1100, 1111) },    // ldaexb
    {"ldaexh",  op_LDA_STL,     BL(1110, 1000, 1101, 0000, 0000, 1111, 1101, 1111) },    // ldaexh
    {"ldah",    op_LDA_STL,     BL(1110, 1000, 1101, 0000, 0000, 1111, 1001, 1111) },    // ldah
    {"ldm",     op_LDM,                                                          0 },    // ldm
    {"ldmdb",   op_LDMDB,                                                        0 },    // ldmdb
    {"ldmea",   op_LDMDB,                                                        0 },    // ldmea
    {"ldmfd",   op_LDM,                                                          0 },    // ldmfd
    {"ldmia",   op_LDM,                                                          0 },    // ldmia
    {"ldr",     op_LD_ST,       BL(0000, 0000, 0101, 0000, 0110, 1000, 1111, 0010) },    // ldr
    {"ldrb",    op_LD_ST,       BL(0000, 0000, 0001, 0000, 0111, 1100, 0011, 0000) },    // ldrb
    {"ldrbt",   op_LDT_STT,     BL(1111, 1000, 0001, 0000, 0000, 1110, 0000, 0000) },    // ldrbt
    {"ldrd",    op_LDRD_STRD,   BL(1110, 1000, 0101, 0000, 0000, 0000, 0000, 0000) },    // ldrd
    {"ldrex",   op_LDT_STT,     BL(1110, 1000, 0101, 0000, 0000, 1111, 0000, 0010) },    // ldrex
    {"ldrexb",  op_LDA_STL,     BL(1110, 1000, 1101, 0000, 0000, 1111, 0100, 1111) },    // ldrexb
    {"ldrexh",  op_LDA_STL,     BL(1110, 1000, 1101, 0000, 0000, 1111, 0101, 1111) },    // ldrexh
    {"ldrh",    op_LD_ST,       BL(0000, 0000, 0011, 0000, 1000, 1010, 0011, 0001) },    // ldrh
    {"ldrht",   op_LDT_STT,     BL(1111, 1000, 0011, 0000, 0000, 1110, 0000, 0000) },    // ldrht
    {"ldrsb",   op_LD_ST,       BL(0000, 0001, 0001, 0000, 0000, 0110, 0001, 0000) },    // ldrsb
    {"ldrsbt",  op_LDT_STT,     BL(1111, 1001, 0001, 0000, 0000, 1110, 0000, 0000) },    // ldrsbt
    {"ldrsh",   op_LD_ST,       BL(0000, 0001, 0011, 0000, 0000, 1110, 0001, 0001) },    // ldrsh
    {"ldrsht",  op_LDT_STT,     BL(1111, 1001, 0011, 0000, 0000, 1110, 0000, 0000) },    // ldrsht
    {"ldrt",    op_LDT_STT,     BL(1111, 1000, 0101, 0000, 0000, 1110, 0000, 0000) },    // ldrt
    {"lsl",     op_Shift,                               BW(0000, 0000, 1000, 0000) },    // lsl
    {"lsr",     op_Shift,                               BW(0000, 0000, 1100, 0001) },    // lsr
    {"mla",     op_MulAcc,      BL(1111, 1011, 0000, 0000, 0000, 0000, 0000, 0000) },    // mla
    {"mls",     op_MulAcc,      BL(1111, 1011, 0000, 0000, 0000, 0000, 0001, 0000) },    // mls
    {"mov",     op_MOV,                                                          0 },    // mov
    {"movt",    op_MOVT,        BL(1111, 0010, 1100, 0000, 0000, 0000, 0000, 0000) },    // movt
    {"mrs",     op_MRS,         BL(1111, 0011, 1110, 1111, 1000, 0000, 0000, 0000) },    // mrs
    {"msr",     op_MSR,         BL(1111, 0011, 1000, 0000, 1000, 0000, 0000, 0000) },    // msr
    {"mul",     op_MUL,                                                          0 },    // mul
    {"mvn",     op_MVN,                                                          0 },    // mvn
    {"nop",     op_NoneNW,                                                   0b000 },    // nop
    {"opt",     po_OPT,                                                          0 },    // opt
    {"orn",     op_Logic,       BL(0000, 0000, 0110, 0000, 0000, 0000, 0000, 0001) },    // orn
    {"orr",     op_Logic,       BL(0000, 0000, 0100, 0000, 0000, 0011, 0000, 0000) },    // orr
    {"pkhbt",   op_PKH,         BL(1110, 1010, 1100, 0000, 0000, 0000, 0000, 0000) },    // pkhbt
    {"pkhtb",   op_PKH,         BL(1110, 1010, 1100, 0000, 0000, 0000, 0010, 0000) },    // pkhtb
    {"pld",     op_PL,          BL(0000, 0000, 0001, 0000, 0000, 0000, 0000, 0000) },    // pld
    {"pldw",    op_PL,          BL(0000, 0000, 0011, 0000, 0000, 0000, 0000, 0000) },    // pldw
    {"pli",     op_PL,          BL(0000, 0001, 0001, 0000, 0000, 0000, 0000, 0000) },    // pli
    {"pop",     op_Stack,                                                        1 },    // pop
    {"pssbb",   op_NoneW,       BL(1111, 0011, 1011, 1111, 1000, 1111, 0100, 0100) },    // pssbb
    {"push",    op_Stack,                                                        0 },    // push
    {"qadd",    op_Arith,       BL(1111, 1010, 1000, 0000, 1111, 0000, 1000, 0000) },    // qadd
    {"qadd16",  op_Arith,       BL(1111, 1010, 1001, 0000, 1111, 0000, 0001, 0001) },    // qadd16
    {"qadd8",   op_Arith,       BL(1111, 1010, 1000, 0000, 1111, 0000, 0001, 0001) },    // qadd8
    {"qasx",    op_Arith,       BL(1111, 1010, 1010, 0000, 1111, 0000, 0001, 0001) },    // qasx
    {"qdadd",   op_Arith,       BL(1111, 1010, 1000, 0000, 1111, 0000, 1001, 0000) },    // qdadd
    {"qdsub",   op_Arith,       BL(1111, 1010, 1000, 0000, 1111, 0000, 1011, 0000) },    // qdsub
    {"qsax",    op_Arith,       BL(1111, 1010, 1110, 0000, 1111, 0000, 0001, 0001) },    // qsax
    {"qsub",    op_Arith,       BL(1111, 1010, 1000, 0000, 1111, 0000, 1010, 0000) },    // qsub
    {"qsub16",  op_Arith,       BL(1111, 1010, 1101, 0000, 1111, 0000, 0001, 0001) },    // qsub16
    {"qsub8",   op_Arith,       BL(1111, 1010, 1100, 0000, 1111, 0000, 0001, 0001) },    // qsub8
    {"rbit",    op_Bit,         BL(1111, 1010, 1001, 0000, 1111, 0000, 1010, 0000) },    // rbit
    {"rev",     op_REV,                                                       0b00 },    // rev
    {"rev16",   op_REV,                                                       0b01 },    // rev16
    {"revsh",   op_REV,                                                       0b11 },    // revsh
    {"ror",     op_Shift,                               BW(0000, 0001, 1100, 0011) },    // ror
    {"rrx",     op_RRX,         BL(1110, 1010, 0100, 1111, 0000, 0000, 0011, 0000) },    // rrx
    {"rsb",     op_RSB,                                                          0 },    // rsb
    {"sadd16",  op_Arith,       BL(1111, 1010, 1001, 0000, 1111, 0000, 0000, 0001) },    // sadd16
    {"sadd8",   op_Arith,       BL(1111, 1010, 1000, 0000, 1111, 0000, 0000, 0001) },    // sadd8
    {"sasx",    op_Arith,       BL(1111, 1010, 1010, 0000, 1111, 0000, 0000, 0001) },    // sasx
    {"sbc",     op_Logic,       BL(0000, 0001, 0110, 0000, 0000, 0001, 1000, 0000) },    // sbc
    {"sbfx",    op_BitField,    BL(1111, 0011, 0100, 0000, 0000, 0000, 0000, 0000) },    // sbfx
    {"sdiv",    op_Arith,       BL(1111, 1011, 1001, 0000, 1111, 0000, 1111, 0001) },    // sdiv
    {"sel",     op_Arith,       BL(1111, 1010, 1010, 0000, 1111, 0000, 1000, 0001) },    // sel
    {"sev",     op_NoneNW,                                                   0b100 },    // sev
    {"sg",      op_NoneW,       BL(1110, 1001, 0111, 1111, 1110, 1001, 0111, 1111) },    // sg
    {"shadd16", op_Arith,       BL(1111, 1010, 1001, 0000, 1111, 0000, 0010, 0001) },    // shadd16
    {"shadd8",  op_Arith,       BL(1111, 1010, 1000, 0000, 1111, 0000, 0010, 0001) },    // shadd8
    {"shasx",   op_Arith,       BL(1111, 1010, 1010, 0000, 1111, 0000, 0010, 0001) },    // shasx
    {"shsax",   op_Arith,       BL(1111, 1010, 1110, 0000, 1111, 0000, 0010, 0001) },    // shsax
    {"shsub16", op_Arith,       BL(1111, 1010, 1101, 0000, 1111, 0000, 0010, 0001) },    // shsub16
    {"shsub8",  op_Arith,       BL(1111, 1010, 1100, 0000, 1111, 0000, 0010, 0001) },    // shsub8
    {"smlabb",  op_MulAcc,      BL(1111, 1011, 0001, 0000, 0000, 0000, 0000, 0000) },    // smlabb
    {"smlabt",  op_MulAcc,      BL(1111, 1011, 0001, 0000, 0000, 0000, 0001, 0000) },    // smlabt
    {"smlad",   op_MulAcc,      BL(1111, 1011, 0010, 0000, 0000, 0000, 0000, 0000) },    // smlad
    {"smladx",  op_MulAcc,      BL(1111, 1011, 0010, 0000, 0000, 0000, 0001, 0000) },    // smladx
    {"smlal",   op_MulAcc,      BL(1111, 1011, 1100, 0000, 0000, 0000, 0000, 0001) },    // smlal
    {"smlalbb", op_MulAcc,      BL(1111, 1011, 1100, 0000, 0000, 0000, 1000, 0001) },    // smlalbb
    {"smlalbt", op_MulAcc,      BL(1111, 1011, 1100, 0000, 0000, 0000, 1001, 0001) },    // smlalbt
    {"smlald",  op_MulAcc,      BL(1111, 1011, 1100, 0000, 0000, 0000, 1100, 0001) },    // smlald
    {"smlaldx", op_MulAcc,      BL(1111, 1011, 1100, 0000, 0000, 0000, 1101, 0001) },    // smlaldx
    {"smlaltb", op_MulAcc,      BL(1111, 1011, 1100, 0000, 0000, 0000, 1010, 0001) },    // smlaltb
    {"smlaltt", op_MulAcc,      BL(1111, 1011, 1100, 0000, 0000, 0000, 1011, 0001) },    // smlaltt
    {"smlatb",  op_MulAcc,      BL(1111, 1011, 0001, 0000, 0000, 0000, 0010, 0000) },    // smlatb
    {"smlatt",  op_MulAcc,      BL(1111, 1011, 0001, 0000, 0000, 0000, 0011, 0000) },    // smlatt
    {"smlawb",  op_MulAcc,      BL(1111, 1011, 0011, 0000, 0000, 0000, 0000, 0000) },    // smlawb
    {"smlawt",  op_MulAcc,      BL(1111, 1011, 0011, 0000, 0000, 0000, 0001, 0000) },    // smlawt
    {"smlsd",   op_MulAcc,      BL(1111, 1011, 0100, 0000, 0000, 0000, 0000, 0000) },    // smlsd
    {"smlsdx",  op_MulAcc,      BL(1111, 1011, 0100, 0000, 0000, 0000, 0001, 0000) },    // smlsdx
    {"smlsld",  op_MulAcc,      BL(1111, 1011, 1101, 0000, 0000, 0000, 1100, 0001) },    // smlsld
    {"smlsldx", op_MulAcc,      BL(1111, 1011, 1101, 0000, 0000, 0000, 1101, 0001) },    // smlsldx
    {"smmla",   op_MulAcc,      BL(1111, 1011, 0101, 0000, 0000, 0000, 0000, 0000) },    // smmla
    {"smmlar",  op_MulAcc,      BL(1111, 1011, 0101, 0000, 0000, 0000, 0001, 0000) },    // smmlar
    {"smmls",   op_MulAcc,      BL(1111, 1011, 0110, 0000, 0000, 0000, 0000, 0000) },    // smmls
    {"smmlsr",  op_MulAcc,      BL(1111, 1011, 0110, 0000, 0000, 0000, 0001, 0000) },    // smmlsr
    {"smmul",   op_MulAcc,      BL(1111, 1011, 0101, 0000, 0000, 0000, 0000, 0000) },    // smmul
    {"smmulr",  op_MulAcc,      BL(1111, 1011, 0101, 0000, 0000, 0000, 0001, 0000) },    // smmulr
    {"smuad",   op_Arith,       BL(1111, 1011, 0010, 0000, 1111, 0000, 0000, 0001) },    // smuad
    {"smuadx",  op_Arith ,      BL(1111, 1011, 0010, 0000, 1111, 0000, 0001, 0001) },    // smuadx
    {"smulbb",  op_Arith,       BL(1111, 1011, 0001, 0000, 1111, 0000, 0000, 0001) },    // smulbb
    {"smulbt",  op_Arith,       BL(1111, 1011, 0001, 0000, 1111, 0000, 0001, 0001) },    // smulbt
    {"smull",   op_MulAcc,      BL(1111, 1011, 1000, 0000, 0000, 0000, 0000, 0001) },    // smull
    {"smultb",  op_Arith,       BL(1111, 1011, 0001, 0000, 1111, 0000, 0010, 0001) },    // smultb
    {"smultt",  op_Arith,       BL(1111, 1011, 0001, 0000, 1111, 0000, 0011, 0001) },    // smultt
    {"smulwb",  op_Arith,       BL(1111, 1011, 0011, 0000, 1111, 0000, 0000, 0001) },    // smulwb
    {"smulwt",  op_Arith,       BL(1111, 1011, 0011, 0000, 1111, 0000, 0001, 0001) },    // smulwt
    {"smusd",   op_Arith,       BL(1111, 1011, 0100, 0000, 1111, 0000, 0000, 0001) },    // smusd
    {"smusdx",  op_Arith,       BL(1111, 1011, 0100, 0000, 1111, 0000, 0001, 0001) },    // smusdx
    {"ssat",    op_SAT,         BL(1111, 0011, 0000, 0000, 0000, 0000, 0000, 0001) },    // ssat
    {"ssat16",  op_SAT,         BL(1111, 0011, 0010, 0000, 0000, 0000, 0000, 0001) },    // ssat16
    {"ssax",    op_Arith,       BL(1111, 1010, 1110, 0000, 1111, 0000, 0000, 0001) },    // ssax
    {"ssbb",    op_NoneW,       BL(1111, 0011, 1011, 1111, 1000, 1111, 0100, 0000) },    // ssbb
    {"ssub16",  op_Arith,       BL(1111, 1010, 1101, 0000, 1111, 0000, 0000, 0001) },    // ssub16
    {"ssub8",   op_Arith,       BL(1111, 1010, 1100, 0000, 1111, 0000, 0000, 0001) },    // ssub8
    {"stl",     op_LDA_STL,     BL(1110, 1000, 1100, 0000, 0000, 1111, 1010, 1111) },    // stl
    {"stlb",    op_LDA_STL,     BL(1110, 1000, 1100, 0000, 0000, 1111, 1000, 1111) },    // stlb
    {"stlex",   op_STLEX,       BL(1110, 1000, 1100, 0000, 0000, 1111, 1110, 0000) },    // stlex
    {"stlexb",  op_STLEX,       BL(1110, 1000, 1100, 0000, 0000, 1111, 1100, 0000) },    // stlexb
    {"stlexh",  op_STLEX,       BL(1110, 1000, 1100, 0000, 0000, 1111, 1101, 0000) },    // stlexh
    {"stlh",    op_LDA_STL,     BL(1110, 1000, 1100, 0000, 0000, 1111, 1001, 1111) },    // stlh
    {"stm",     op_STM,                                                          0 },    // stm
    {"stmdb",   op_STMDB,                                                        0 },    // stmdb
    {"stmea",   op_STM,                                                          0 },    // stmea
    {"stmfd",   op_STMDB,                                                        0 },    // stmfd
    {"stmia",   op_STM,                                                          0 },    // stmia
    {"str",     op_LD_ST,       BL(0000, 0000, 0100, 0000, 0110, 0000, 0110, 0010) },    // str
    {"strb",    op_LD_ST,       BL(0000, 0000, 0000, 0000, 0111, 0100, 0010, 0000) },    // strb
    {"strbt",   op_LDT_STT,     BL(1111, 1000, 0000, 0000, 0000, 1110, 0000, 0000) },    // strbt
    {"strd",    op_LDRD_STRD,   BL(1110, 1000, 0100, 0000, 0000, 0000, 0000, 0000) },    // strd
    {"strex",   op_STREX,       BL(1110, 1000, 0100, 0000, 0000, 0000, 0000, 0000) },    // strex
    {"strexb",  op_STLEX,       BL(1110, 1000, 1100, 0000, 0000, 1111, 0100, 0000) },    // strexb
    {"strexh",  op_STLEX,       BL(1110, 1000, 1100, 0000, 0000, 1111, 0101, 0000) },    // strexh
    {"strh",    op_LD_ST,       BL(0000, 0000, 0010, 0000, 1000, 0010, 0010, 0001) },    // strh
    {"strht",   op_LDT_STT,     BL(1111, 1000, 0010, 0000, 0000, 1110, 0000, 0000) },    // strht
    {"strt",    op_LDT_STT,     BL(1111, 1000, 0100, 0000, 0000, 1110, 0000, 0000) },    // strt
    {"sub",     op_ADD_SUB,                                                      2 },    // sub
    {"subw",    op_ADD_SUB,                                                      3 },    // subw
    {"svc",     op_BKPT,                                BW(1101, 1111, 0000, 0000) },    // svc
    {"sxtab",   op_XTAB,        BL(1111, 1010, 0100, 0000, 1111, 0000, 1000, 0000) },    // sxtab
    {"sxtab16", op_XTAB,        BL(1111, 1010, 0010, 0000, 1111, 0000, 1000, 0000) },    // sxtab16
    {"sxtah",   op_XTAB,        BL(1111, 1010, 0000, 0000, 1111, 0000, 1000, 0000) },    // sxtah
    {"sxtb",    op_EXT,         BL(0000, 0000, 0100, 0000, 0000, 0000, 0100, 0000) },    // sxtb
    {"sxtb16",  op_EXT16,       BL(1111, 1010, 0010, 1111, 1111, 0000, 1000, 0000) },    //sxtb16
    {"sxth",    op_EXT,         BL(0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000) },    // sxth
    {"tbb",     op_TB,          BL(1110, 1000, 1101, 0000, 1111, 0000, 0000, 0000) },    // tbb
    {"tbh",     op_TB,          BL(1110, 1000, 1101, 0000, 1111, 0000, 0001, 0000) },    // tbh
    {"teq",     op_TEQ,                                                          0 },    // teq
    {"tst",     op_TST,         BL(0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000) },    // tst
    {"tt",      op_TT,          BL(1110, 1000, 0100, 0000, 1111, 0000, 0000, 0000) },    // tt
    {"tta",     op_TT,          BL(1110, 1000, 0100, 0000, 1111, 0000, 1000, 0000) },    // tta
    {"ttat",    op_TT,          BL(1110, 1000, 0100, 0000, 1111, 0000, 1100, 0000) },    // ttat
    {"ttt",     op_TT,          BL(1110, 1000, 0100, 0000, 1111, 0000, 0100, 0000) },    // ttt
    {"uadd16",  op_Arith,       BL(1111, 1010, 1001, 0000, 1111, 0000, 0100, 0001) },    // uadd16
    {"uadd8",   op_Arith,       BL(1111, 1010, 1000, 0000, 1111, 0000, 0100, 0001) },    // uadd8
    {"uasx",    op_Arith,       BL(1111, 1010, 1010, 0000, 1111, 0000, 0100, 0001) },    // uasx
    {"ubfx",    op_BitField,    BL(1111, 0011, 1100, 0000, 0000, 0000, 0000, 0000) },    // ubfx
    {"udf",     op_UDF,                                                          0 },    // udf
    {"udiv",    op_Arith,       BL(1111, 1011, 1011, 0000, 1111, 0000, 1111, 0001) },    // udiv
    {"uhadd16", op_Arith,       BL(1111, 1010, 1001, 0000, 1111, 0000, 0110, 0001) },    // uhadd16
    {"uhadd8",  op_Arith,       BL(1111, 1010, 1000, 0000, 1111, 0000, 0110, 0001) },    // uhadd8
    {"uhasx",   op_Arith,       BL(1111, 1010, 1010, 0000, 1111, 0000, 0110, 0001) },    // uhasx
    {"uhsax",   op_Arith,       BL(1111, 1010, 1110, 0000, 1111, 0000, 0110, 0001) },    // uhsax
    {"uhsub16", op_Arith,       BL(1111, 1010, 1101, 0000, 1111, 0000, 0110, 0001) },    // uhsub16
    {"uhsub8",  op_Arith,       BL(1111, 1010, 1100, 0000, 1111, 0000, 0110, 0001) },    // uhsub8
    {"umaal",   op_MulAcc,      BL(1111, 1011, 1110, 0000, 0000, 0000, 0110, 0001) },    // umaal
    {"umlal",   op_MulAcc,      BL(1111, 1011, 1110, 0000, 0000, 0000, 0000, 0001) },    // umlal
    {"umull",   op_MulAcc,      BL(1111, 1011, 1010, 0000, 0000, 0000, 0000, 0001) },    // umull
    {"uqadd16", op_Arith,       BL(1111, 1010, 1001, 0000, 1111, 0000, 0101, 0001) },    // uqadd16
    {"uqadd8",  op_Arith,       BL(1111, 1010, 1000, 0000, 1111, 0000, 0101, 0001) },    // uqadd8
    {"uqasx",   op_Arith,       BL(1111, 1010, 1010, 0000, 1111, 0000, 0101, 0001) },    // uqasx
    {"uqsax",   op_Arith,       BL(1111, 1010, 1110, 0000, 1111, 0000, 0101, 0001) },    // uqsax
    {"uqsub16", op_Arith,       BL(1111, 1010, 1101, 0000, 1111, 0000, 0101, 0001) },    // uqsub16
    {"uqsub8",  op_Arith,       BL(1111, 1010, 1100, 0000, 1111, 0000, 0101, 0001) },    // uqsub8
    {"usad8",   op_Arith,       BL(1111, 1011, 0111, 0000, 1111, 0000, 0000, 0001) },    // usad8
    {"usada8",  op_MulAcc,      BL(1111, 1011, 0111, 0000, 0000, 0000, 0000, 0000) },    // usada8
    {"usat",    op_SAT,         BL(1111, 0011, 1000, 0000, 0000, 0000, 0000, 0000) },    // usat
    {"usat16",  op_SAT,         BL(1111, 0011, 1010, 0000, 0000, 0000, 0000, 0000) },    // usat16
    {"usax",    op_Arith,       BL(1111, 1010, 1110, 0000, 1111, 0000, 0100, 0001) },    // usax
    {"usub16",  op_Arith,       BL(1111, 1010, 1101, 0000, 1111, 0000, 0100, 0001) },    // usub16
    {"usub8",   op_Arith,       BL(1111, 1010, 1100, 0000, 1111, 0000, 0100, 0001) },    // usub8
    {"uxtab",   op_XTAB,        BL(1111, 1010, 0101, 0000, 1111, 0000, 1000, 0000) },    // uxtab
    {"uxtab16", op_XTAB,        BL(1111, 1010, 0011, 0000, 1111, 0000, 1000, 0000) },    // uxtab16
    {"uxtah",   op_XTAB,        BL(1111, 1010, 0001, 0000, 1111, 0000, 1000, 0000) },    // uxtah
    {"uxtb",    op_EXT,         BL(0000, 0000, 0101, 0000, 0000, 0000, 1100, 0000) },    // uxtb
    {"uxtb16",  op_EXT16,       BL(1111, 1010, 0011, 1111, 1111, 0000, 1000, 0000) },    // uxtb16
    {"uxth",    op_EXT,         BL(0000, 0000, 0001, 0000, 0000, 0000, 1000, 0000) },    // uxth
    {"vabs",    op_VOne,        BL(1110, 1110, 1011, 0000, 0000, 1010, 1100, 0000) },    // vabs
    {"vadd",    op_VArith,      BL(1110, 1110, 0011, 0000, 0000, 1010, 0000, 0000) },    // vadd
    {"vcmp",    op_VCMP,        BL(0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000) },    // vcmp
    {"vcmpe",   op_VCMP,        BL(0000, 0000, 0000, 0000, 0000, 0000, 1000, 0000) },    // vcmpe
    {"vcvt",    op_VCVT,                                                         0 },    // vcvt
    {"vcvta",   op_VCVTX,       BL(1111, 1110, 1011, 1100, 0000, 1010, 0100, 0000) },    // vcvta
    {"vcvtm",   op_VCVTX,       BL(1111, 1110, 1011, 1111, 0000, 1010, 0100, 0000) },    // vcvtm
    {"vcvtn",   op_VCVTX,       BL(1111, 1110, 1011, 1101, 0000, 1010, 0100, 0000) },    // vcvtn
    {"vcvtp",   op_VCVTX,       BL(1111, 1110, 1011, 1110, 0000, 1010, 0100, 0000) },    // vcvtp
    {"vcvtr",   op_VCVTX,       BL(1110, 1110, 1011, 1100, 0000, 1010, 0100, 0000) },    // vcvtr
    {"vdiv",    op_VArith,      BL(1110, 1110, 1000, 0000, 0000, 1010, 0000, 0000) },    // vdiv
    {"vfma",    op_VArith,      BL(1110, 1110, 1010, 0000, 0000, 1010, 0000, 0000) },    // vfma
    {"vfms",    op_VArith,      BL(1110, 1110, 1010, 0000, 0000, 1010, 0100, 0000) },    // vfms
    {"vfnma",   op_VArith,      BL(1110, 1110, 1001, 0000, 0000, 1010, 0100, 0000) },    // vfnma
    {"vfnms",   op_VArith,      BL(1110, 1110, 1001, 0000, 0000, 1010, 0000, 0000) },    // vfnms
    {"vldm",    op_VMulti,      BL(1110, 1100, 1001, 0000, 0000, 1010, 0000, 0000) },    // vldm
    {"vldmdb",  op_VMulti,      BL(1110, 1101, 0011, 0000, 0000, 1010, 0000, 0000) },    // vldmdb
    {"vldmia",  op_VMulti,      BL(1110, 1100, 1001, 0000, 0000, 1010, 0000, 0000) },    // vldmia
    {"vldr",    op_VLD_VST,     BL(1110, 1101, 0001, 0000, 0000, 1010, 0000, 0000) },    // vldr
    {"vlldm",   op_VLazy,       BL(1110, 1100, 0011, 0000, 0000, 1010, 0000, 0000) },    // vlldm
    {"vlstm",   op_VLazy,       BL(1110, 1100, 0010, 0000, 0000, 1010, 0000, 0000) },    // vlstm
    {"vmaxnm",  op_VArith,      BL(1111, 1110, 1000, 0000, 0000, 1010, 0000, 0000) },    // vmaxnm
    {"vminnm",  op_VArith,      BL(1111, 1110, 1000, 0000, 0000, 1010, 0100, 0000) },    // vminnm
    {"vmla",    op_VArith,      BL(1110, 1110, 0000, 0000, 0000, 1010, 0000, 0000) },    // vmla
    {"vmls",    op_VArith,      BL(1110, 1110, 0000, 0000, 0000, 1010, 0100, 0000) },    // vmls
    {"vmov",    op_VMOV,                                                         0 },    // vmov
    {"vmrs",    op_VMRS,        BL(1110, 1110, 1111, 0000, 0000, 1010, 0001, 0000) },    // vmrs
    {"vmsr",    op_VMSR,        BL(1110, 1110, 1110, 0000, 0000, 1010, 0001, 0000) },    // vmsr
    {"vmul",    op_VArith,      BL(1110, 1110, 0010, 0000, 0000, 1010, 0000, 0000) },    // vmul
    {"vneg",    op_VOne,        BL(1110, 1110, 1011, 0001, 0000, 1010, 0100, 0000) },    // vneg
    {"vnmla",   op_VArith,      BL(1110, 1110, 0001, 0000, 0000, 1010, 0100, 0000) },    // vnmla
    {"vnmls",   op_VArith,      BL(1110, 1110, 0001, 0000, 0000, 1010, 0000, 0000) },    // vnmls
    {"vnmul",   op_VArith,      BL(1110, 1110, 0010, 0000, 0000, 1010, 0100, 0000) },    // vnmul
    {"vpop",    op_VStack,      BL(1110, 1100, 1011, 1101, 0000, 1010, 0000, 0000) },    // vpop
    {"vpush",   op_VStack,      BL(1110, 1101, 0010, 1101, 0000, 1010, 0000, 0000) },    // vpush
    {"vrinta",  op_VOne,        BL(1111, 1110, 1011, 1000, 0000, 1010, 0100, 0000) },    // vrinta
    {"vrintm",  op_VOne,        BL(1111, 1110, 1011, 1011, 0000, 1010, 0100, 0000) },    // vrintm
    {"vrintn",  op_VOne,        BL(1111, 1110, 1011, 1001, 0000, 1010, 0100, 0000) },    // vrintn
    {"vrintp",  op_VOne,        BL(1111, 1110, 1011, 1010, 0000, 1010, 0100, 0000) },    // vrintp
    {"vrintr",  op_VOne,        BL(1110, 1110, 1011, 0110, 0000, 1010, 0100, 0000) },    // vrintr
    {"vrintx",  op_VOne,        BL(1110, 1110, 1011, 0111, 0000, 1010, 0100, 0000) },    // vrintx
    {"vrintz",  op_VOne,        BL(1110, 1110, 1011, 0110, 0000, 1010, 1100, 0000) },    // vrintz
    {"vscclrm", op_VStack,      BL(1110, 1100, 1001, 1111, 0000, 1011, 0000, 0000) },    // vscclrm
    {"vseleq",  op_VArith,      BL(1111, 1110, 0000, 0000, 0000, 1010, 0000, 0000) },    // vseleq
    {"vselge",  op_VArith,      BL(1111, 1110, 0010, 0000, 0000, 1010, 0000, 0000) },    // vselge
    {"vselgt",  op_VArith,      BL(1111, 1110, 0011, 0000, 0000, 1010, 0000, 0000) },    // vselgt
    {"vselvs",  op_VArith,      BL(1111, 1110, 0001, 0000, 0000, 1010, 0000, 0000) },    // vselvs
    {"vsqrt",   op_VOne,        BL(1110, 1110, 1011, 0001, 0000, 1010, 1100, 0000) },    // vsqrt
    {"vstm",    op_VMulti,      BL(1110, 1100, 1000, 0000, 0000, 1010, 0000, 0000) },    // vstm
    {"vstmdb",  op_VMulti,      BL(1110, 1101, 0010, 0000, 0000, 1010, 0000, 0000) },    // vstmdb
    {"vstmia",  op_VMulti,      BL(1110, 1100, 1000, 0000, 0000, 1010, 0000, 0000) },    // vstmia
    {"vstr",    op_VLD_VST,     BL(1110, 1101, 0000, 0000, 0000, 1010, 0000, 0000) },    // vstr
    {"vsub",    op_VArith,      BL(1110, 1110, 0011, 0000, 0000, 1010, 0100, 0000) },    // vsub
    {"wfe",     op_NoneNW,                                                   0b010 },    // wfe
    {"wfi",     op_NoneNW,                                                   0b011 },    // wfi
    {"yield",   op_NoneNW,                                                   0b001 }     // yield
    };

static int wlen (const char *ps)
    {
    int n = 0;
    while (*ps >= '0')
        {
        ++n;
        ++ps;
        }
    return n;
    }

static int opcode_lookup (const char *ps)
    {
    // Bisection search
    int n1 = -1;
    int n2 = count (opcodes);
    int n3;
    int m;
    while (true)
        {
        if ( n2 == n1 + 1 ) return -1;
        n3 = (n1 + n2) / 2;
        m = stricmp (ps, opcodes[n3].s);
        if (m == 0) break;
        if (m > 0) n1 = n3;
        else       n2 = n3;
        }
    return n3;
    }

static int lookup_opcode (void)
    {
    // Copy opcode
    char code[10];
    int n, m;
    for (n = 0; n < sizeof (code) - 1; ++n)
        {
        if (esi[n] < '0') break;
        code[n] = esi[n];
        }
    code[n] = '\0';
    if (! strnicmp (code, "it", 2)) code[2] = '\0';
    m = opcode_lookup (code);
    // Remove flags switch
    if ((m == -1) && (n > 1))
        {
        --n;
        if ((code[n] == 'S') || (code[n] == 's'))
            {
            code[n] = '\0';
            m = opcode_lookup (code);
            if (m < 0)
                {
                code[n] = 's';
                ++n;
                }
            }
        else
            {
            ++n;
            }
        }
    // Remove condition code
    if ((m == -1) && (n > 2))
        {
        n -= 2;
        for (m = 0; m < count (conditions); ++m)
            {
            if (! stricmp (&code[n], conditions[m]))
                {
                code[n] = '\0';
                m = opcode_lookup (code);
                break;
                }
            }
        }
    // Remove flags switch
    if ((m == -1) && (n > 1))
        {
        --n;
        if ((code[n] == 'S') || (code[n] == 's'))
            {
            code[n] = '\0';
            m = opcode_lookup (code);
            }
        }
    if (m >= 0) esi += strlen (opcodes[m].s);
    return m;
    }

#ifndef TEST
static void tabit (int x)
{
	if (vcount == x) 
		return ;
	if (vcount > x)
		crlf () ;
	spaces (x - vcount) ;
}

static void listasm (void *oldpc, signed char *oldesi)
    {
    void *p;
    int n = PC - oldpc;
    if (liston & BIT6)
        p = OC - n;
    else
        p = PC - n;

    do
        {
#if (defined (_WIN32)) && (__GNUC__ < 9)
        sprintf (accs, "%08I64X ", (long long) (size_t) oldpc);
#else
        sprintf (accs, "%08llX ", (long long) (size_t) oldpc);
#endif
        char *ps = accs + 9;
        switch (n)
            {
            default:
                sprintf (ps, "%02X ", *((unsigned char *)p));
                ps += 3;
                ++p;
            case 3:
                sprintf (ps, "%02X ", *((unsigned char *)p));
                ps += 3;
                ++p;
            case 2:
                sprintf (ps, "%02X ", *((unsigned char *)p));
                ps += 3;
                ++p;
            case 1:
                sprintf (ps, "%02X ", *((unsigned char *)p));
                ps += 3;
                ++p;
            case 0:
                break;
            }
        if (n > 4)
            {
            n -= 4;
            oldpc += 4;
            }
        else 
            n = 0;

        text (accs);

        if (*oldesi == '.')
            {
            tabit (21);
            do	
                {
                token (*oldesi++ );
                }
            while (range0(*oldesi));
            while (*oldesi == ' ') oldesi++;
            }
        tabit (30);
        while ((*oldesi != ':') && (*oldesi != 0x0D)) 
            token (*oldesi++);
        crlf ();
        }
    while (n);
    }

static void asmlabel (void)
    {
    VAR v;
    unsigned char type;
    void *ptr = getput (&type);
    if (ptr == NULL)
        asmerr (16); // 'Syntax error'
    if (type >= 128)
        asmerr (6); // 'Type mismatch'
    if ((liston & BIT5) == 0)
        {
        v = loadn (ptr, type);
        if (v.i.n)
            asmerr (3); // 'Multiple label'
        }
    v.i.t = 0;
    v.i.n = (intptr_t) PC;
    storen (v, ptr, type);
    }

void assemble (void)
    {
	signed char al;
	signed char *oldesi = esi;
	int init = 1 ;
	void *oldpc = PC;

	while (1)
	    {
		int mnemonic, condition, instruction;

		if (liston & BIT7)
		    {
			int tmp;
			if (liston & BIT6)
				tmp = stavar[15];
			else
				tmp = stavar[16];
			if (tmp >= stavar[12])
				error (8, NULL); // 'Address out of range'
		    }

		al = nxt ();
		esi++;

		switch (al) 
		    {
			case 0:
				esi--;
				liston = (liston & 0x0F) | 0x30;
				return;

			case ']':
				liston = (liston & 0x0F) | 0x30;
				return;

			case 0x0D:
				newlin ();
				if (*esi == 0x0D)
					break;
			case ':':
				if (liston & BIT4) listasm (oldpc, oldesi);
				nxt ();
#ifdef __arm__
				if ((liston & BIT6) == 0)
					__builtin___clear_cache (oldpc, PC); 
#endif
				oldpc = PC;
				oldesi = esi;
				break;

			case ';':
			case TREM:
				while ((*esi != 0x0D) && (*esi != ':')) esi++;
				break;

			case '.':
                if (init) oldpc = align (4);
                asmlabel ();
                break;

			default:
                --esi;
                oldpc = PC;
                instruction = lookup_opcode ();
                if (instruction >= 0)
                    {
                    if (opcodes[instruction].f != po_OPT) init = 0;
                    opcodes[instruction].f (opcodes[instruction].d);
                    }
                else
                    {
                    asmerr (101); // Invalid opcode
                    }
                while (! IsEnd ()) ++esi;
                break;
            }
        };
    }

#else

char nxt (void)
    {
    while ((*esi) && (*esi <= ' ')) ++esi;
    return *esi;
    }

void error (int err, const char *ps)
    {
    if (pass == 2)
        {
        fprintf (fOut, "*** Error %d:", err);
        if (ps != NULL) fprintf (fOut, " %s:", ps);
        fprintf (fOut, "\n");
        }
    }

int itemi (void)
    {
    bool bNeg = false;
    bool bHex = false;
    int v = 0;
    int n = 0;
    while (true)
        {
        char ch = *esi;
        if ((ch >= '0') && (ch <= '9'))
            {
            if ( bHex)  v = 16 * v + ch - '0';
            else        v = 10 * v + ch - '0';
            }
        else if ((n == 0) && (ch == '-'))
            {
            if (v != 0) break;
            bNeg = true;
            }
        else if ((n == 1) && ((ch == 'x') || (ch == 'X')))
            {
            if (v != 0) break;
            bHex = true;
            }
        else if (bHex)
            {
            if ((ch >= 'A') && (ch <= 'F'))
                {
                v = 16 * v + ch - 'A' + 10;
                }
            else if ((ch >= 'a') && (ch <= 'f'))
                {
                v = 16 * v + ch - 'a' + 10;
                }
            else
                {
                break;
                }
            }
        else
            {
            break;
            }
        ++esi;
        ++n;
        }
    if (bNeg) v = -v;
    return v;
    }

int expri (void)
    {
    return itemi ();
    }

double valuef (void)
    {
    char sNum[20];
    bool bDP = false;
    int n = 0;
    if ((*esi == '+') || (*esi == '-'))
        {
        sNum[n] = *esi;
        ++n;
        ++esi;
        }
    while (n < sizeof (sNum) - 1)
        {
        if ((*esi >= '0') && (*esi <= '9'))
            {
            sNum[n] = *esi;
            ++n;
            ++esi;
            }
        else if ((! bDP) && (*esi == '.'))
            {
            bDP = true;
            sNum[n] = *esi;
            ++n;
            ++esi;
            }
        else
            {
            break;
            }
        }
    sNum[n] = '\0';
    return atof (sNum);
    }

static bool address (bool bAlign, int mina, int maxa, int *addr)
    {
    *addr = 0;
    char ch = nxt ();
    if ((ch == 'L') || (ch == 'l'))
        {
        ++esi;
        int idx = itemi ();
        if (pass == 1) return true;
        if (idx >= nlab) return false;
        intptr_t base = stavar[16] + 4;
        if (bAlign) base &= 0xFFFFFFFC;
        *addr = label[idx] - base;
        return true;
        }
    return false;
    }

int main (int nArg, const char *psArg[])
    {
    fIn = fopen (psArg[1], "r");
    if (fIn == NULL)
        {
        fprintf (stderr, "Unable to open input file: %s\n", psArg[1]);
        return 1;
        }
    if (nArg > 2)
        {
        fOut = fopen (psArg[2], "w");
            {
            fprintf (stderr, "Unable to open output file: %s\n", psArg[2]);
            return 1;
            }
        }
    else
        {
        fOut = stdout;
        }
    for (pass = 1; pass <= 2; ++pass)
        {
        stavar[16] = 0;
        int nLine = 0;
        rewind (fIn);
        while (true)
            {
            if (fgets (sLine, sizeof (sLine), fIn) == NULL) break;
            sLine[sizeof (sLine)-1] = '\0';
            int n = strlen (sLine);
            if ((n > 0) && (sLine[n-1] == '\n')) sLine[n-1] = '\0';
            esi = sLine;
            if (pass == 2) fprintf (fOut, "%4d ", ++nLine);
            if (nxt () == '.')
                {
                ++esi;
                if (! strnicmp (esi, "balign", 6))
                    {
                    if (pass == 2) fprintf (fOut, "%04X ", stavar[16]);
                    esi += 6;
                    nxt ();
                    int aln = itemi () - 1;
                    if ((stavar[16] & aln) && (pass == 2))  fprintf (fOut, "00BF    ");
                    stavar[16] = (stavar[16] + aln) & (~ aln);
                    }
                }
            else if (! IsEnd ())
                {
                char *ptr = strchr (esi, ':');
                if (ptr != NULL)
                    {
                    if ((*esi == 'L') || (*esi == 'l'))
                        {
                        ++esi;
                        int idx = itemi ();
                        if (idx < MLAB)
                            {
                            label[idx] = stavar[16];
                            if (idx >= nlab) nlab = idx + 1;
                            }
                        if (*esi == ':') ++esi;
                        nxt ();
                        }
                    }
                if (! IsEnd ())
                    {
                    int instruction = lookup_opcode ();
                    if (instruction >= 0)
                        {
                        opcodes[instruction].f (opcodes[instruction].d);
                        }
                    else
                        {
                        error (16, "Unrecognised opcode");
                        return 1;
                        }
                    }
                }
            else
                {
                if (pass == 2) fprintf (fOut, "             ");
                }
            if (pass == 2) fprintf (fOut, " %s\n", sLine);
            }
        }
    return 0;
    }
#endif
