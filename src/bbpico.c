/******************************************************************\
*       BBC BASIC Minimal Console Version                          *
*       Copyright (C) R. T. Russell, 2021-2022                     *
*                                                                  *
*       Modified 2021 by Eric Olson and Memotech-Bill for          *
*       Raspberry Pico                                             *
*                                                                  *
*       bbpico.c Main program, Initialisation, Keyboard handling   *
*       Version 0.45a, 03-Sep-2023                                 *
\******************************************************************/

#ifndef KBD_STDIN
#ifdef PICO_GUI
#define KBD_STDIN   0
#else
#define KBD_STDIN   1
#endif
#endif

#define _GNU_SOURCE
#define __USE_GNU
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "bbccon.h"

#ifndef TOP_OF_STACK_1
extern char __StackTop;
#define TOP_OF_MEM ((void *)&__StackTop - 0x1800)   // Reserve 0x1800 space for stack during initialisation
// #define TOP_OF_MEM  0x20040000
#endif
#ifndef BASIC_RAM
#if PICO == 2
#define BASIC_RAM   0x50000
#else
#define BASIC_RAM   0x20000
#endif
#endif

// Attempt to check consistancy of build

#ifdef PICO
#include <stdarg.h>
#include <pico.h>
#include <hardware/adc.h>
#ifndef HAVE_CYW43
#define HAVE_CYW43  0
#endif
#if HAVE_CYW43
#include <pico/cyw43_arch.h>
#else
#ifdef STDIO_BT
#error Bluetooth Console requires Pico_W
#endif
#endif
#ifndef PICO_GUI
#if ( ! defined(STDIO_USB) ) && ( ! defined(STDIO_UART) ) && ( ! defined(STDIO_BT) )
#error No Console connection defined
#endif
#define HAVE_MODEM  1
#include "zmodem.h"
#endif

#ifdef STDIO_UART
#if ( ! defined(PICO_DEFAULT_UART_RX_PIN) ) || ( ! defined(PICO_DEFAULT_UART_TX_PIN) )
#error No default UART defined
#endif
#endif

#ifdef HAVE_FAT
#include "sd_spi.h"
#if defined(STDIO_UART) || defined (HAVE_PRINTER) || ( SERIAL_DEV == -1 ) || ( SERIAL_DEV == 2 )
#if    ( PICO_DEFAULT_UART_TX_PIN == SD_MOSI_PIN ) || ( PICO_DEFAULT_UART_RX_PIN == SD_MOSI_PIN )   \
    || ( PICO_DEFAULT_UART_TX_PIN == SD_MISO_PIN ) || ( PICO_DEFAULT_UART_RX_PIN == SD_MISO_PIN )   \
    || ( PICO_DEFAULT_UART_TX_PIN == SD_CLK_PIN )  || ( PICO_DEFAULT_UART_RX_PIN == SD_CLK_PIN )    \
    || ( PICO_DEFAULT_UART_TX_PIN == SD_CS_PIN )   || ( PICO_DEFAULT_UART_RX_PIN == SD_CS_PIN )
#error Default UART clashes with SD card
#endif
#endif
#endif

#if PICO_SOUND == 1
#if ( ! defined(PICO_AUDIO_I2S_DATA_PIN) ) || ( ! defined(PICO_AUDIO_I2S_CLOCK_PIN_BASE) )
#error I2S audio pins not defined for sound
#endif
#elif PICO_SOUND == 2
#if ( ! defined(PICO_AUDIO_PWM_L_PIN) )
#error PWM audio pins not defined for sound
#endif
#elif PICO_SOUND == 3
/*
#if ( ! defined(PICO_AUDIO_PWM_L_PIN) ) || ( ! defined(PICO_AUDIO_PWM_R_PIN) )
#error PWM audio pins not defined for SDL sound
#endif
*/
#endif

#if defined(PICO_GUI) && defined(HAVE_PRINTER)
#if ! defined(PICO_DEFAULT_UART_TX_PIN)
#error No default UART defined for Printer
#endif
#endif

#if SERIAL_DEV == -1
#if ! defined(PICO_GUI)
#error STDIO used for both console and /dev/serial
#endif
#if ( ! defined(PICO_DEFAULT_UART_TX_PIN) ) || ( ! defined(PICO_DEFAULT_UART_RX_PIN) )
#error No default UART defined for /dev/serial
#endif
#elif SERIAL_DEV == 1
#if ! defined(PICO_DEFAULT_UART)
#error No default UART defined for SERIAL_DEV=1
#endif
#elif SERIAL_DEV == 2
#if defined(STDIO_UART)
#error UART PICO_DEFAULT_UART used for both console and serial device
#endif
#if defined(HAVE_PRINTER)
#error UART PICO_DEFAULT_UART used for both VDU printer and serial device
#endif
#endif

#endif

// NOTE: the following alignment macros must match those in BBC.h exactly!

typedef __attribute__((aligned(1))) int unaligned_int;

static inline int ILOAD(void* p){ return (intptr_t)p&3 ? *((unaligned_int*)p) : *((int*)p); }
static inline void ISTORE(void* p, int i){ if ((intptr_t)p&3) *((unaligned_int*)p) = i; else *((int *)p) = i; }

#define ESCTIME 200  // Milliseconds to wait for escape sequence
#define QRYTIME 1000 // Milliseconds to wait for cursor query response
#define QSIZE 32     // Twice longest expected escape sequence

#ifdef _WIN32
#define HISTORY 100  // Number of items in command history
#include <windows.h>
#include <conio.h>
typedef int timer_t;
#undef WM_APP
#define myftell _ftelli64
#define myfseek _fseeki64
#define PLATFORM "Win32"
#define realpath(N,R) _fullpath((R),(N),_MAX_PATH)
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x4
#define DISABLE_NEWLINE_AUTO_RETURN 0x8
#define ENABLE_VIRTUAL_TERMINAL_INPUT 0x200
BOOL WINAPI K32EnumProcessModules (HANDLE, HMODULE*, DWORD, LPDWORD);
#else
# ifdef PICO
#define HISTORY 10  // Number of items in command history
#include <hardware/flash.h>
#include <hardware/exception.h>
#include <hardware/watchdog.h>
#include <pico/bootrom.h>
#include <pico/stdlib.h>
#include <pico/time.h>
#include <pico/binary_info.h>
#if PICO_STACK_CHECK & 0x04
#if PICO == 1
#include <hardware/structs/mpu.h>
#elif PICO == 2
#include <hardware/structs/m33.h>
#endif
#endif
#ifdef STDIO_USB
#include <tusb.h>
#endif
#ifdef STDIO_BT
#include <stdio_bt.h>
#endif
#define WM_TIMER 275
#include "lfswrap.h"
extern char __StackLimit;
#if PICO == 2
#if HAVE_CYW43 > 1
#define PLATFORM "Pico 2W"
#else
#define PLATFORM "Pico 2"
#endif
#else
#if HAVE_CYW43 > 1
#define PLATFORM "Pico W"
#else
#define PLATFORM "Pico"
#endif
#endif
#ifdef CAN_SET_RTC
#if PICO == 1
#include <hardware/rtc.h>
#else
#include <pico/aon_timer.h>
#include <pico/util/datetime.h>
#endif
#endif
extern void *sysvar;
#define NCFGVAL     5   // Number of bytes in @picocfg&(
static const struct
    {
    unsigned char   ndim;
    int             dim;
    unsigned char   val[NCFGVAL];
    } __attribute__((packed))
    cfgdata =
        {
        1,
        NCFGVAL,
            {
            0x00
#ifdef PICO_GRAPH
            | 0x08
#endif
#ifdef PICO_GUI
            | 0x04
#endif
#ifdef STDIO_UART
            | 0x02
#endif
#ifdef STDIO_USB
            | 0x01
#endif
#ifdef STDIO_BT
            | 0x10
#endif
            , 0x00
#ifdef HAVE_LFS
            | 0x01
#endif
#ifdef HAVE_FAT
            | 0x02
#endif
#if SERIAL_DEV
            | 0x04
#endif
#ifdef PICO_SOUND
            , PICO_SOUND,
#else
            , 0,
#endif
#if SERIAL_DEV
            SERIAL_DEV,
#else
            0,
#endif
            HAVE_CYW43
#if PICO == 2
            + 0x20
#endif
            }
        };
    
static struct
    {
    long        next;
    char        name[10];
    const void  *addr;
    } __attribute__((packed))
    cfgvar =
        {
        0,
        "picocfg&(",
        &cfgdata
        };
    
# else
#define HISTORY 100  // Number of items in command history
#include <termios.h>
#include <signal.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "dlfcn.h"
#define myftell ftell
#define myfseek fseek
#define PLATFORM "Linux"
#define WM_TIMER 275
# endif
#endif

#ifdef __WIN64__
#undef PLATFORM
#define PLATFORM "Win64"
#endif

#ifdef __APPLE__
#include <dispatch/dispatch.h>
#include <mach-o/dyld.h>
typedef dispatch_source_t timer_t;
dispatch_queue_t timerqueue;
#undef PLATFORM
#define PLATFORM "MacOS"
#endif

#undef MAX_PATH
#ifndef MAX
#define MAX(X,Y) (((X) > (Y)) ? (X) : (Y))
#endif

// Program constants:
#define SCREEN_WIDTH  640
#define SCREEN_HEIGHT 500
#define MAX_PATH 260
#define AUDIOLEN 441 * 4

// Global variables (external linkage):

void *userRAM = NULL;
void *progRAM = NULL;
void *userTOP = NULL;
const int bLowercase = 0;    // Dummy
#define SFY(x) #x
#define MVL(x) SFY(x)
bi_decl (bi_program_description (szVersion));
#ifdef PICO_GUI
bi_decl (bi_program_feature ("VGA display"));
bi_decl (bi_program_feature ("USB host for keyboard"));
#endif
#ifdef PICO_GRAPH
bi_decl (bi_program_feature (MVL(PICO_GRAPH)" display"));
#endif
const char szVersion[] = "BBC BASIC for "PLATFORM
#ifdef PICO_GUI
    " GUI "VERSION
#else
    " Console "VERSION
#endif
    ", Build "__DATE__
#ifdef STDIO_USB
    ", USB Console"
#endif
#ifdef STDIO_UART
    ", UART Console"
#endif
#ifdef STDIO_BT
    ", Bluetooth Console"
#endif
#ifdef PICO_GRAPH
    ", " MVL(PICO_GRAPH) " Display"
#endif
#ifdef HAVE_LFS
    ", Flash Filesystem"
#endif
#ifdef HAVE_FAT
    ", SD Filesystem"
#endif
#if HAVE_CYW43 == 1
    ", cyw43=gpio"
#elif HAVE_CYW43 == 2
    ", cyw43=poll"
#elif HAVE_CYW43 == 3
    ", cyw43=background"
#endif
#if PICO_SOUND == 1
    ", I2S Sound"
#elif PICO_SOUND == 2
    ", PWM Sound"
#elif PICO_SOUND == 3
    ", SDL Sound"
#endif
#if defined(PICO_GUI) && defined(HAVE_PRINTER)
    ", Printer"
#endif
#if SERIAL_DEV == -1
    ", /dev/serial"
#elif SERIAL_DEV == 1
    ", /dev/uart"
#elif SERIAL_DEV == 2
    ", /dev/uart0, /dev/uart1"
#endif
#if MIN_STACK == 2
    ", Min Stack X"
#elif MIN_STACK
    ", Min Stack"
#endif
#if PICO_STACK_CHECK > 0
    ", Stack Check " MVL(PICO_STACK_CHECK)
#endif
#ifdef CAN_SET_RTC
    ", RTC"
#endif
    ;
const char szNotice[] = "(C) Copyright R. T. Russell, "YEAR;
char *szLoadDir;
char *szLibrary;
char *szUserDir;
char *szTempDir;
char *szCmdLine;
int MaximumRAM = MAXIMUM_RAM;
timer_t UserTimerID;
unsigned int palette[256];
void *TTFcache[1];
#include <stdbool.h>
static bool bSysErr = false;
#ifdef PICO
extern heapptr libase;		// Base of libraries 
extern void *libtop;        // Top of installed libraries
#endif

// Array of VDU command lengths:
static const int vdulen[] = {
    0,   1,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   1,   2,   5,   0,   0,   1,   9,   8,   5,   0,   1,   4,   4,   0,   2 };

#if ( defined(STDIO_USB) || defined(STDIO_UART) || defined(STDIO_BT) )
#define printf(...) fprintf (stdout, __VA_ARGS__)
extern bool bBBCtl;
#endif

// Keycode translation table:
#if KBD_STDIN
static const unsigned char xkey[64] = {
    0, 139, 138, 137, 136,   0, 131,   0, 130,   0,   0,   0,   0,   0,   0,   0,
    145, 146, 147, 148,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0, 134, 135,   0, 132, 133,   0,   0,   0,   0,   0,   0,   0,   0, 149,
    0, 150, 151, 152, 153, 154,   0, 155, 156,   0,   0,   0,   0,   0,   0,   0 };
#endif

#if HAVE_CYW43
#ifndef PICO_DEFAULT_LED_PIN
#define PICO_DEFAULT_LED_PIN    25
#endif
#define PICO_ERROR_NO_CYW43     -257        // No support
int iCyw = PICO_ERROR_NO_CYW43;
void net_clear (void);
#endif

// Declared in bbcans.c:
void oscli (char *);
void xeqvdu (int, int, int);
char *setup (char *, char *, char *, char, unsigned char *);

// Declared in bbmain.c:
void error (int, const char *);
void text (const char *);	// Output NUL-terminated string
void crlf (void);		// Output a newline

// Declared in bbeval.c:
unsigned int rnd (void);	// Return a pseudo-random number

#ifdef PICO_GUI
// Declared in picovdu.c
#include <hardware/dma.h>
void setup_vdu (void);
int vgetc (int x, int y);
void prtscrn (void);
extern bool bPrtScrn;
// Declared in picokbd.c
void setup_keyboard (void);
int testkey (int);
#endif
#ifdef PICO_GRAPH
#include <hardware/dma.h>
void setup_vdu (void);
void getcsrfb(int *px, int *py);
int vpointfb (int xp, int yp);
int vtintfb (int xp, int yp);
int vgetcfb (int x, int y);
int widthsfb (unsigned char *s, int l);
#endif

#ifdef PICO_SOUND
void bell (void);
void envel (const signed char *);
void sound (int, int, int, int);
#if PICO_SOUND == 3
#include <hardware/uart.h>
#endif
// Defined in snd_pico.c
void snd_setup (void);
// Defined in sn76489.c
int snd_free (int ch);
#endif

// Interpreter entry point:
int basic (void *, void *, void *);

// Forward references:
unsigned char osbget (void*, int*);
int oskey (int wait);
void osbput (void*, unsigned char);
void quiet (void);

// Dummy functions:
void gfxPrimitivesSetFont(void) { };
void gfxPrimitivesGetFont(void) { };
void RedefineChar(void) { };

// File scope variables:
#if KBD_STDIN
static unsigned char inputq[QSIZE];
static volatile int inpqr = 0, inpqw = 0;

// Put to STDIN queue:
static int putinp (unsigned char inp)
    {
	unsigned char bl = inpqw;
	unsigned char al = (bl + 1) % QSIZE;
#ifdef STDIO_UART
	static unsigned char bFirst = 1;
	if ( bFirst )
	    {
	    bFirst = 0;
        return 0;
	    }
#endif
	if (al != inpqr)
	    {
		inputq[bl] = inp;
		inpqw = al;
		return 1;
	    }
	return 0;
    }

#ifdef PICO
inline static void myPoll(){
    static int c = PICO_ERROR_TIMEOUT;
    for(;;){
        if ((c != PICO_ERROR_TIMEOUT) && (putinp(c) == 0))
            break;
        c=getchar_timeout_us(0);
        if (c == PICO_ERROR_TIMEOUT)
            break;
        }
    }
#endif

// Get from STDIN queue:
static int getinp (unsigned char *pinp)
    {
#ifdef PICO
	myPoll();
#endif
	unsigned char bl = inpqr;
	if (bl != inpqw)
	    {
		*pinp = inputq[bl];
		inpqr = (bl + 1) % QSIZE;
		return 1;
	    }
	return 0;
    }
#endif  // KBD_STDIN

#ifdef _WIN32
#define RTLD_DEFAULT (void *)(-1)
static void *dlsym (void *handle, const char *symbol)
    {
	void *procaddr;
	HMODULE modules[100];
	long unsigned int i, needed;
	K32EnumProcessModules ((HANDLE)-1, modules, sizeof (modules), &needed);
	for (i = 0; i < needed / sizeof (HMODULE); i++)
	    {
		procaddr = GetProcAddress (modules[i], symbol);
		if (procaddr != NULL) break;
	    }
	return procaddr;
    }

static DWORD WINAPI myThread (void *parm)
    {
	int ch;
	long unsigned int nread;

	while (ReadFile (GetStdHandle(STD_INPUT_HANDLE), &ch, 1, &nread, NULL))
	    {
		if (nread)
			while (putinp (ch) == 0)
				sleep (0);
		else
			sleep (0);
	    }
	return 0;
    }

#elif ! defined(PICO)

void *myThread (void *parm)
    {
	int ch;
	size_t nread;

	do
	    {
		nread = read (STDIN_FILENO, &ch, 1);
		if (nread)
			while (putinp (ch) == 0)
				usleep (1);
		else
			usleep (1);
	    }
	while (nread >= 0);
	return 0;
    }
#endif

#ifdef __linux__
static void *mymap (unsigned int size)
    {
	FILE *fp;
	char line[256];
	void *start, *finish, *base = (void *) 0x400000;

	fp = fopen ("/proc/self/maps", "r");
	if (fp == NULL)
		return NULL;

	while (NULL != fgets (line, 256, fp))
	    {
		sscanf (line, "%p-%p", &start, &finish);
		start = (void *)((size_t)start & -0x1000); // page align (GCC extension)
		if (start >= (base + size)) 
			return base;
		if (finish > (void *)0xFFFFF000)
			return NULL;
		base = (void *)(((size_t)finish + 0xFFF) & -0x1000); // page align
		if (base > ((void *)0xFFFFFFFF - size))
			return NULL;
	    }
	return base;
    }
#endif

// Put event into event queue, unless full:
int putevt (heapptr handler, int msg, int wparam, int lparam)
    {
	unsigned char bl = evtqw;
	unsigned char al = bl + 8;
	int index = bl >> 2;
	if ((al == evtqr) || (eventq == NULL))
		return 0;
	eventq[index] = lparam;
	eventq[index + 1] = msg;
	eventq[index + 64] = wparam;
	eventq[index + 65] = handler;
	evtqw = al;
	return 1;
    }

// Get event from event queue, unless empty:
static heapptr getevt (void)
    {
	heapptr handler;
	unsigned char al = evtqr;
	int index = al >> 2;
	flags &= ~ALERT;
	if ((al == evtqw) || (eventq == NULL))
		return 0;
	lParam = eventq[index];
	iMsg = eventq[index + 1];
	wParam = eventq[index + 64];
	handler = eventq[index + 65];
	al += 8;
	evtqr = al;
	if (al != evtqw)
		flags |= ALERT;
	return handler;
    }

// Put keycode to keyboard queue:
int putkey (char key)
    {
	unsigned char bl = kbdqw;
	unsigned char al = bl + 1;
	if ((key == 0x1B) && !(flags & ESCDIS))
	    {
		flags |= ESCFLG;
		return 0;
	    }
	if (al != kbdqr)
	    {
		keybdq[(int) bl] = key;
		kbdqw = al;
		return 1;
	    }
	return 0;
    }

// Get keycode (if any) from keyboard queue:
int getkey (unsigned char *pkey)
    {
#if defined (PICO) && KBD_STDIN
	myPoll();
#endif
	unsigned char bl = kbdqr;
	if (bl != kbdqw)
	    {
		*pkey = keybdq[(int) bl];
		kbdqr = bl + 1;
		return 1;
	    }
	return 0;
    }

#if KBD_STDIN
// Get a character from any input queue
int anykey (unsigned char *pkey, int tmo)
    {
    if ( kbdqr != kbdqw ) return getkey (pkey);
    if ( inpqr != inpqw ) return getinp (pkey);
    int c = getchar_timeout_us (tmo);
    if ( c == PICO_ERROR_TIMEOUT ) return 0;
    *pkey = c;
    return 1;
    }
#endif

// Get millisecond tick count:
unsigned int GetTicks (void)
    {
#ifdef _WIN32
	return timeGetTime ();
#else
# ifdef PICO
	absolute_time_t t=get_absolute_time();
	return to_ms_since_boot(t);
# else
	struct timespec ts;
	if (clock_gettime (CLOCK_MONOTONIC, &ts))
		clock_gettime (CLOCK_REALTIME, &ts);
	return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
# endif
#endif
    }

#if KBD_STDIN
static int kbchk (void)
    {
#ifdef PICO
	myPoll();
#endif
	return (inpqr != inpqw);
    }

static unsigned char kbget (void)
    {
	unsigned char ch = 0;
	getinp (&ch);
	return ch;
    }

static int kwait (unsigned int timeout)
    {
	int ready;
	timeout += GetTicks ();
	while (!(ready = kbchk ()) && (GetTicks() < timeout))
		usleep (1);
	return ready;
    }
#endif  // KBD_STDIN

// Returns 1 if the cursor position was read successfully or 0 if it was aborted
// (in which case *px and *py will be unchanged) or if px and py are both NULL.
int stdin_handler (int *px, int *py)
    {
#if KBD_STDIN
	int wait = (px != NULL) || (py != NULL);
	static char report[16];
	static char *p = report, *q = report;
	unsigned char ch;

#if ( defined(STDIO_USB) || defined(STDIO_UART) || defined(STDIO_BT) )
    if ( bBBCtl && wait )
        {
        printf ("\x17\x1F\x00");
		fflush (stdout);
        }
    else
#endif
	if (wait)
	    {
		printf ("\033[6n");
		fflush (stdout);
	    }

	do
	    {
		if (kbchk ())
		    {
			ch = kbget ();
				
			if (ch == 0x1B)
			    {
				if (p != report)
					q = p;
				*p++ = 0x1B;
			    }
			else if (p != report)
			    {
				if (p < report + sizeof(report))
					*p++ = ch;
				if ((ch >= 'A') && (ch <= '~') && (ch != '[') && (ch != 'O') &&
					((*(q + 1) == '[') || (*(q + 1) == 'O')))
				    {
					p = q;
					q = report;
								
					if (ch == 'R')
					    {
						int row = 0, col = 0;
						if (sscanf (p, "\033[%d;%dR", &row, &col) == 2)
						    {
							if (px) *px = col - 1;
							if (py) *py = row - 1;
							return 1;
						    }
					    }
					if (ch == '~')
					    {
						int key;
						if (sscanf (p, "\033[%d~", &key))
							putkey (xkey[key + 32]);
					    }
					else
						putkey (xkey[ch - 64]);
				    }
			    }
			else
			    {
				putkey (ch);
				if (((kbdqr - kbdqw - 1) & 0xFF) < 50)
				    {
					p = report;
					q = report;
					wait = 0; // abort
				    }
			    }
		    }
		if ((wait || (p != report)) && !kwait (wait ? QRYTIME : ESCTIME))
		    {
			q = report;
			while (q < p) putkey (*q++);
			p = report;
			q = report;
			wait = 0; // abort
		    }
	    }
	while (wait || (p != report));
#endif  // KBD_STDIN
	return 0;
    }

#ifdef PICO_SOUND
void bell (void)
    {
    signed char envelope[14] = {16, 2, 0, 0, 0, 40, 40, 46, 126, -1, 0, 0, 126, 0};
    envel (envelope);
    sound (0x13, 16, 160, 50);
    }
#else
void bell (void)
    {
    }

// SOUND Channel,Amplitude,Pitch,Duration
void sound (short chan, signed char ampl, unsigned char pitch, unsigned char duration)
    {
	error (255, "Sorry, SOUND not implemented");
    }

// ENVELOPE N,T,PI1,PI2,PI3,PN1,PN2,PN3,AA,AD,AS,AR,ALA,ALD
void envel (signed char *env)
    {
	error (255, "Sorry, ENVELOPE not implemented");
    }

// Disable sound generation:
void quiet (void)
    {
    }
#endif

// Get current MODE number:
int getmodeno (void)
    {
	return modeno;
    }

#ifndef PICO_GUI
// Get text cursor (caret) coordinates:
void getcsr(int *px, int *py)
    {
#ifdef PICO_GRAPH
    if ( (optval & 0x0F) == 14 )
        {
        getcsrfb (px, py);
        }
	else
#endif
        if (!stdin_handler (px, py))
            {
            if (px != NULL) *px = -1; // Flag unable to read
            if (py != NULL) *py = -1;
            }
    }

// Get pixel RGB colour:
int vtint (int x, int y)
    {
#ifdef PICO_GRAPH
    if ( (optval & 0x0F) >= 14 ) return vtintfb (x, y);
#endif
	error (255, "Sorry, TINT not implemented");
	return -1;
    }

// Get nearest palette index:
int vpoint (int x, int y)
    {
#ifdef PICO_GRAPH
    if ( (optval & 0x0F) >= 14 ) return vpointfb (x, y);
#endif
	error (255, "Sorry, POINT not implemented");
	return -1;
    }

int vgetc (int x, int y)
    {
#ifdef PICO_GRAPH
    if ( (optval & 0x0F) >= 14 ) return vgetcfb (x, y);
#endif
	error (255, "Sorry, GETXY not implemented");
	return -1;
    }

// Get string width in graphics units:
int widths (unsigned char *s, int l)
    {
#ifdef PICO_GRAPH
    if ( (optval & 0x0F) >= 14 ) return widthsfb (s, l);
#endif
	error (255, "Sorry, WIDTH not implemented");
	return -1;
    }
#endif

int osbyte (int al, int xy)
    {
    switch (al)
        {
        case 129:
            return (oskey (xy) << 8) + 129;
        case 135:
            return ((vgetc (0x80000000, 0x80000000) << 8) + 135);
        default:
            error (255, "Sorry, OSBYTE function not implemented");
        }
	return -1;
    }

void osword (int al, void *xy)
    {
    if ( al == 4 )
        {
		memcpy (xy + 1, &bbcfont[*(unsigned char*)(xy) << 3], 8);
        }
    else
        {
        error (255, "Sorry, OSWORD function not implemented");
        }
	return;
    }

// ADVAL(n)
static bool init_adc = false;
int adval (int n)
    {
	if (n == -1)
        return (kbdqr - kbdqw - 1) & 0xFF;
#ifdef PICO_SOUND
    if ((n >= -8) && (n <= -5)) return snd_free (-5 - n);
#endif
    if (( n >= 0 ) && ( n <= 5 ))
        {
        if ( ! init_adc )
            {
            adc_init ();
            adc_set_round_robin (0);
            adc_run (false);
            init_adc = true;
            }
        if ( n <= 4 ) adc_gpio_init (n + 25);
        else adc_set_temp_sensor_enabled (true);;
        adc_select_input (n - 1);
        return adc_read ();
        }
	error (255, "Sorry, ADVAL mode not implemented");
	return -1;
    }

// APICALL
#ifdef __llvm__
long long apicall_ (long long (*APIfunc) (size_t, size_t, size_t, size_t, size_t, size_t,
        size_t, size_t, size_t, size_t, size_t, size_t,
        double, double, double, double, double, double, double, double), PARM *p)
    {
    return APIfunc (p->i[0], p->i[1], p->i[2], p->i[3], p->i[4], p->i[5],
        p->i[6], p->i[7], p->i[8], p->i[9], p->i[10], p->i[11],
        p->f[0], p->f[1], p->f[2], p->f[3], p->f[4], p->f[5], p->f[6], p->f[7]);
    }
double fltcall_ (double (*APIfunc) (size_t, size_t, size_t, size_t, size_t, size_t,
        size_t, size_t, size_t, size_t, size_t, size_t,
        double, double, double, double, double, double, double, double), PARM *p)
    {
    return APIfunc (p->i[0], p->i[1], p->i[2], p->i[3], p->i[4], p->i[5],
        p->i[6], p->i[7], p->i[8], p->i[9], p->i[10], p->i[11],
        p->f[0], p->f[1], p->f[2], p->f[3], p->f[4], p->f[5], p->f[6], p->f[7]);
    }
#else
#pragma GCC optimize ("O0")
long long apicall_ (long long (*APIfunc) (size_t, size_t, size_t, size_t, size_t, size_t,
        size_t, size_t, size_t, size_t, size_t, size_t), PARM *p)
    {
#ifdef ARMHF
	if (p->f[0] == 0.0)
		memcpy (&p->f[0], &p->i[0], 48);
#endif
	long long wrapper (volatile double a, volatile double b, volatile double c, volatile double d,
        volatile double e, volatile double f, volatile double g, volatile double h)
        {
		long long result;
#ifdef _WIN32
		static void* savesp;
		asm ("mov %%esp,%0" : "=m" (savesp));
#endif
		result = APIfunc (p->i[0], p->i[1], p->i[2], p->i[3], p->i[4], p->i[5],
            p->i[6], p->i[7], p->i[8], p->i[9], p->i[10], p->i[11]);
#ifdef _WIN32
		asm ("mov %0,%%esp" : : "m" (savesp));
#endif
		return result;
        }

	return wrapper (p->f[0], p->f[1], p->f[2], p->f[3], p->f[4], p->f[5], p->f[6], p->f[7]);
    }
double fltcall_ (double (*APIfunc) (size_t, size_t, size_t, size_t, size_t, size_t,
        size_t, size_t, size_t, size_t, size_t, size_t), PARM *p)
    {
#ifdef ARMHF
	if (p->f[0] == 0.0)
		memcpy (&p->f[0], &p->i[0], 48);
#endif
	double wrapper (volatile double a, volatile double b, volatile double c, volatile double d,
        volatile double e, volatile double f, volatile double g, volatile double h)
        {
		double result;
#ifdef _WIN32
		static void* savesp;
		asm ("mov %%esp,%0" : "=m" (savesp));
#endif
		result = APIfunc (p->i[0], p->i[1], p->i[2], p->i[3], p->i[4], p->i[5],
            p->i[6], p->i[7], p->i[8], p->i[9], p->i[10], p->i[11]);
#ifdef _WIN32
		asm ("mov %0,%%esp" : : "m" (savesp));
#endif
		return result;
        }

	return wrapper (p->f[0], p->f[1], p->f[2], p->f[3], p->f[4], p->f[5], p->f[6], p->f[7]);
    }
#pragma GCC reset_options
#endif

// Call a function in the context of the GUI thread:
size_t guicall (void *func, PARM *parm)
    {
	return apicall_ (func, parm);
    }

// Check for Escape (if enabled) and kill:
void trap (void)
    {
#if KBD_STDIN
	stdin_handler (NULL, NULL);
#endif

	if (flags & KILL)
		error (-1, NULL); // Quit
	if (flags & ESCFLG)
	    {
		flags &= ~ESCFLG;
		kbdqr = kbdqw;
		quiet ();
		if (exchan)
		    {
			fclose (exchan);
			exchan = NULL;
		    }
		error (17, NULL); // 'Escape'
	    }
#ifdef PICO_GUI
    if ( bPrtScrn )
        {
        prtscrn ();
        bPrtScrn = false;
        }
#endif
    }

// Test for escape, kill, pause, single-step, flash and alert:
heapptr xtrap (void)
    {
	trap ();
	if (flags & ALERT)
		return getevt ();
	return 0;
    }

// Read keyboard or F-key expansion:
static int rdkey (unsigned char *pkey)
    {
	if (keyptr)
	    {
		*pkey = *keyptr++;
		if (*keyptr == 0)
			keyptr = NULL;
		return 1;
	    }

	while (getkey (pkey))
	    {
		int keyno = 0;

		if ((*pkey >= 128) && (*pkey < 156))
			keyno = *pkey ^ 144;
		if (keyno >= 24)
			keyno -= 12;
		if (*pkey == 127)
		    {
			*pkey = 8;
			keyno = 24;
		    }
		if (keyno)
		    {
			keyptr = *((unsigned char **)keystr + keyno);
			if (keyptr)
			    {
				*pkey = *keyptr++;
				if (*keyptr == 0)
					keyptr = NULL;
			    }
		    }
		return 1;
	    }
	return 0;
    }

// Wait a maximum period for a keypress, or test key asynchronously:
int oskey (int wait)
    {
	if (wait >= 0)
	    {
		unsigned int start = GetTicks ();
		while (1)
		    {
			unsigned char key;
			trap ();
			if (rdkey (&key))
				return (int) key;
			if ((unsigned int)(GetTicks () - start) >= wait * 10)
				return -1;
			usleep (5000);
		    }
	    }
#ifdef PICO_GUI
    else if ( wait >= -128 )
        return testkey (-wait);
#endif

	if (wait == -256)
		return 's';

	return 0;
    }

// Wait for keypress:
unsigned char osrdch (void)
    {
	unsigned char key;
	if (exchan)
        {
		if (fread (&key, 1, 1, exchan))
			return key;
		fclose (exchan);
 		exchan = NULL;
        }

	if (optval >> 4)
		return osbget ((void *)(size_t)(optval >> 4), NULL);

	while (!rdkey (&key))
        {
		usleep (5000);
		trap ();
        }
	return key;
    }

// Output byte to VDU stream:
void oswrch (unsigned char vdu)
    {
	unsigned char *pqueue = vduq;

	if (((optval & 0x0F) > 0) && ((optval & 0x0F) < 14))
	    {
		osbput ((void *)(size_t)(optval & 0x0F), vdu);
		return;
	    }

	if (spchan)
	    {
		if (0 == fwrite (&vdu, 1, 1, spchan))
		    {
			fclose (spchan);
			spchan = NULL;
		    }
	    }

#if ( defined(STDIO_USB) || defined(STDIO_UART) || defined(STDIO_BT) )
    if ( bBBCtl )
        {
        putchar (vdu);
        return;
        }
#endif

	if (vduq[10] != 0)		// Filling queue ?
	    {
		vduq[9 - vduq[10]] = vdu;
		vduq[10] --;
		if (vduq[10] != 0)
			return;
		vdu = vduq[9];
		if (vdu >= 0x80)
		    {
			xeqvdu (vdu << 8, 0, 0);
			int ecx = (vdu >> 4) & 3;
			if (ecx == 0) ecx++;
			pqueue -= ecx - 9;
            for (; ecx > 0; ecx--)
                xeqvdu (*pqueue++ << 8, 0, 0);
			fflush (stdout);
			return;
		    }
	    }
	else if (vdu >= 0x20)
	    {
		if ((vdu >= 0x80) && (vflags & UTF8))
		    {
			char ah = (vdu >> 4) & 3;
			if (ah == 0) ah++;
			vduq[10] = ah;	// No. of bytes to follow
			vduq[9] = vdu;
			return;
		    }
		xeqvdu (vdu << 8, 0, 0);
		fflush (stdout);
		return;
	    }
	else
	    {
		vduq[10] = vdulen[(int) vdu];
		vduq[9] = vdu;
		if (vduq[10] != 0)
			return;
	    }

// Mapping of VDU queue to UserEvent parameters,
// VDU 23 (defchr) has the most parameter bytes:
//
//          0  ^
// status-> 0  | ev.user.code
//          V  |
//          h  v
//          g  ^
//          f  | ev.user.data1
//          e  |
//          d  v
//          c  ^
//          b  | ev.user.data2
//          a  |
//  vduq->  n  v

	xeqvdu (*(int*)(pqueue + 8) & 0xFFFF, *(int*)(pqueue + 4), *(int*)pqueue);
    }

// Prepare for outputting an error message:
void reset (void)
    {
	vduq[10] = 0;	// Flush VDU queue
	keyptr = NULL;	// Cancel *KEY expansion
	optval = 0;	// Cancel I/O redirection
	reflag = 0;	// *REFRESH ON
    }

// Input and edit a string :
void osline (char *buffer)
    {
	static char *history[HISTORY] = {NULL};
	static int empty = 0;
	int current = empty;
	char *eol = buffer;
	char *p = buffer;
	*buffer = 0x0D;
#if HAVE_MODEM
    bool bUpload = (exchan == 0) && ((optval >> 4) == 0) && (keyptr == 0);
#endif
	int n;

	while (1)
	    {
		unsigned char key;

		key = osrdch ();
#if HAVE_MODEM
        if (( key == 0x01 ) && bUpload && ( *buffer == 0x0D )) yreceive (3, "\r");
#endif
		switch (key)
		    {
			case 0x0A:
			case 0x0D:
				n = (char *) memchr (buffer, 0x0D, 256) - buffer;
				if (n == 0)
					return;
				if ((current == (empty + HISTORY - 1) % HISTORY) &&
                    (0 == memcmp (buffer, history[current], n)))
					return;
				history[empty] = malloc (n + 1);
				memcpy (history[empty], buffer, n + 1);
				empty = (empty + 1) % HISTORY;
				if (history[empty])
				    {
					free (history[empty]);
					history[empty] = NULL;
				    }
				return;

			case 8:
			case 127:
				if (p > buffer)
				    {
					char *q = p;
					do p--; while ((vflags & UTF8) && (*(signed char*)p < -64));
					oswrch (8);
					memmove (p, q, buffer + 256 - q);
				    }
				break;

			case 21:
				while (p > buffer)
				    {
					char *q = p;
					do p--; while ((vflags & UTF8) && (*(signed char*)p < -64));
					oswrch (8);
					memmove (p, q, buffer + 256 - q);
				    }
				break;

			case 130:
				while (p > buffer)
				    {
					oswrch (8);
					do p--; while ((vflags & UTF8) && (*(signed char*)p < -64));
				    }
				break;

			case 131:
				while (*p != 0x0D)
				    {
					oswrch (9);
					do p++; while ((vflags & UTF8) && (*(signed char*)p < -64));
				    }
				break;

			case 134:
				vflags ^= IOFLAG;
				if (vflags & IOFLAG)
					printf ("\033[1 q");
				else
					printf ("\033[3 q\033[7 q");
				break;

			case 135:
				if (*p != 0x0D)
				    {
					char *q = p;
					do q++; while ((vflags & UTF8) && (*(signed char*)q < -64));
					memmove (p, q, buffer + 256 - q);
				    }
				break;

			case 136:
				if (p > buffer)
				    {
					oswrch (8);
					do p--; while ((vflags & UTF8) && (*(signed char*)p < -64));
				    }
				break;

			case 137:
				if (*p != 0x0D)
				    {
					oswrch (9);
					do p++; while ((vflags & UTF8) && (*(signed char*)p < -64));
				    }
				break;

			case 138:
			case 139:
				if (key == 138)
					n = (current + 1) % HISTORY;
				else
					n = (current + HISTORY - 1) % HISTORY;
				if (history[n])
				    {
					char *s = history[n];
					while (*p != 0x0D)
					    {
						oswrch (9);
						do p++; while ((vflags & UTF8) &&
							(*(signed char*)p < -64));
					    }
					while (p > buffer)
					    {
						oswrch (127);
						do p--; while ((vflags & UTF8) &&
                            (*(signed char*)p < -64));
                        }
					while ((*s != 0x0D) && (p < (buffer + 255)))
					    {
						oswrch (*s);
						*p++ = *s++;
					    }
					*p = 0x0D;
					current = n;
				    }
				break;

			case 132:
			case 133:
			case 140:
			case 141:
				break;

			case 9:
				key = ' ';
			default:
				if (p < (buffer + 255))
				    {
					if (key != 0x0A)
					    {
						oswrch (key);
					    }
					if (key >= 32)
					    {
						memmove (p + 1, p, buffer + 255 - p);
						*p++ = key;
						*(buffer + 255) = 0x0D;
						if ((*p != 0x0D) && (vflags & IOFLAG))
						    {
							char *q = p;
							do q++; while ((vflags & UTF8) &&
								(*(signed char*)q < -64));
							memmove (p, q, buffer + 256 - q);
						    }
					    }
				    }
		    }

		if (*p != 0x0D)
		    {
			oswrch (23);
			oswrch (1);
			for (n = 8; n != 0; n--)
				oswrch (0);
		    }
		n = 0;
		while (*p != 0x0D)
		    {
			oswrch (*p++);
			n++;
		    }
		for (int i = 0; i < (eol - p); i++)
			oswrch (32);
		for (int i = 0; i < (eol - p); i++)
			oswrch (8);
		eol = p;
		while (n)
		    {
			if (!(vflags & UTF8) || (*(signed char*)p >= -64))
				oswrch (8);
			p--;
			n--;
		    }
		if (*p != 0x0D)
		    {
			oswrch (23);
			oswrch (1);
			oswrch (1);
			for (n = 7; n != 0; n--)
				oswrch (0);
		    }
	    }
    }

// Get TIME
int getime (void)
    {
	unsigned int n = GetTicks ();
	if (n < lastick)
		timoff += 0x19999999;
	lastick = n;
	return n / 10 + timoff;
    }

#ifdef CAN_SET_RTC
static char *psDWeek[] = {"Err", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static char *psMon[] = {"Err", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
static int iDMon[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

#endif

void vldtim (datetime_t *pdt)
    {
    if (( pdt->year < 0 ) || ( pdt->year > 9999 )) pdt->year = 0;
    if (( pdt->month < 1 ) || ( pdt->month > 12 )) pdt->month = 0;
    if (( pdt->day < 1 ) || ( pdt->day > 31 )) pdt->day = 0;
    if (( pdt->dotw < 0 ) || ( pdt->dotw > 6 )) pdt->dotw = -1;
    if (( pdt->hour < 0 ) || ( pdt->hour > 24 )) pdt->hour = 0;
    if (( pdt->min < 0 ) || ( pdt->min > 59 )) pdt->min = 0;
    if (( pdt->sec < 0 ) || ( pdt->sec > 59 )) pdt->sec = 0;
    }

#if PICO == 2
static void rtc_init (void)
    {
    struct timespec ts;
    ts.tv_sec = 946684800;
    ts.tv_nsec = 0;
    aon_timer_start (&ts);
    }

static void rtc_get_datetime (datetime_t *dt)
    {
    struct timespec ts;
    aon_timer_get_time (&ts);
    time_to_datetime (ts.tv_sec, dt);
    }

static void rtc_set_datetime (datetime_t *dt)
    {
    struct timespec ts;
    datetime_to_time (dt, &ts.tv_sec);
    ts.tv_nsec = 0;
    aon_timer_set_time (&ts);
    }
#endif

int getims (void)
    {
#ifdef CAN_SET_RTC
    datetime_t  dt;
    rtc_get_datetime (&dt);
    vldtim (&dt);
    sprintf (accs, "%s.%02d %s %04d,%02d:%02d:%02d",
        psDWeek[dt.dotw+1], dt.day, psMon[dt.month], dt.year,
        dt.hour, dt.min, dt.sec);
    return 24;
#else
	char *at;
	time_t tt;

	time (&tt);
	at = ctime (&tt);
	ISTORE(accs + 0, ILOAD(at + 0)); // Day-of-week
	ISTORE(accs + 4, ILOAD(at + 8)); // Day-of-month
	ISTORE(accs + 7, ILOAD(at + 4)); // Month
	ISTORE(accs + 11, ILOAD(at + 20)); // Year
	if (*(accs + 4) == ' ') *(accs + 4) = '0';
	memcpy (accs + 16, at + 11, 8); // Time
	accs[3] = '.';
	accs[15] = ',';
	return 24;
#endif
    }

// Put TIME
void putime (int n)
    {
	lastick = GetTicks ();
	timoff = n - lastick / 10;
    }

#ifdef CAN_SET_RTC
void putims2 (const char *psTime, unsigned int ulen)
    {
    if (( psTime == NULL ) || ( ulen < 5 )) return;
    int len = ulen;
    datetime_t  dt;
    rtc_get_datetime (&dt);
    const char *ps = psTime;
    if ( ps[3] == '.' )
        {
        ps += 4;
        len -= 4;
        }
    if (( ps[2] != ':' ) && ( len >= 11 ))
        {
        dt.day = 10 * ( ps[0] - '0' ) + ps[1] - '0';
        ps += 3;
        for (int i = 1; i <= 12; ++i)
            {
            if ( strncasecmp (ps, psMon[i], 3) == 0 )
                {
                dt.month = i;
                break;
                }
            }
        ps += 4;
        dt.year = 1000 * ( ps[0] - '0' ) + 100 * ( ps[1] - '0' ) + 10 * ( ps[2] - '0' ) + ps[3] - '0';
        int iDay = dt.year + dt.year / 4 - dt.year / 100 + dt.year / 400 + iDMon[dt.month-1] + dt.day - 1;
        if (( dt.month < 3 ) && ( dt.year % 4 == 0 ) && (( dt.year % 100 != 0 ) || ( dt.year % 400 == 0 ))) --iDay;
        dt.dotw = iDay % 7;
        ps += 5;
        len -= 12;
        }
    if (( ps[2] == ':' ) && ( len >= 5 ))
        {
        dt.hour = 10 * ( ps[0] - '0' ) + ps[1] - '0';
        ps += 3;
        dt.min = 10 * ( ps[0] - '0' ) + ps[1] - '0';
        if (( ps[2] == ':' ) && ( len >= 8 ))
            {
            ps += 3;
            dt.sec = 10 * ( ps[0] - '0' ) + ps[1] - '0';
            }
        else
            {
            dt.sec = 0;
            }
        }
    vldtim (&dt);
    rtc_set_datetime (&dt);
    }

void putims (const char *psTime)
    {
    if ( psTime == NULL ) return;
    const char *ps = strchr (psTime, '\r');
    if ( ps == NULL ) return;
    putims2 (psTime, ps - psTime);
    }

#endif

// Wait for a specified number of centiseconds:
// On some platforms specifying a negative value causes a task switch
void oswait (int cs)
    {
	unsigned int start = GetTicks ();
	if (cs < 0)
	    {
		sleep (0);
		return;
	    }
	cs *= 10;
	do
	    {
		trap ();
		usleep (1000);
	    }
	while ((unsigned int)(GetTicks () - start) < cs);
    }

#if ! HAVE_MOUSE
// MOUSE x%, y%, b%
void mouse (int *px, int *py, int *pb)
    {
	if (px) *px = 0;
	if (py) *py = 0;
	if (pb) *pb = 0;
    }

// MOUSE ON [type]
void mouseon (int type)
    {
    }

// MOUSE OFF
void mouseoff (void)
    {
    }

// MOUSE TO x%, y%
void mouseto (int x, int y)
    {
    }
#endif

// Get address of an API function:
void *sysadr (char *name)
    {
#ifdef PICO
	extern void *sympico (char *name);
	return sympico (name);
#else
	void *addr = NULL;
	if (addr != NULL)
		return addr; 
	return dlsym (RTLD_DEFAULT, name);
#endif
    }

// Emulation of SDL routines commonly called from BBC BASIC

void *SDL_malloc (size_t size)
    {
    return malloc (size);
    }

void SDL_free (void *ptr)
    {
    free (ptr);
    }

void *SDL_memset (void *ptr, int c, size_t n)
    {
    return memset (ptr, c, n);
    }

void *SDL_memcpy (void *dst, void *src, size_t n)
    {
    return memmove (dst, src, n);
    }

// Call an emulated OS subroutine (if CALL or USR to an address < 0x10000)
int oscall (int addr)
    {
	int al = stavar[1];
	void *xy = (void *) ((size_t)stavar[24] | ((size_t)stavar[25] << 8));
	switch (addr)
	    {
		case 0xFFE0: // OSRDCH
			return (int) osrdch ();

		case 0xFFE3: // OSASCI
			if (al != 0x0D)
			    {
				oswrch (al);
				return 0;
			    }

		case 0xFFE7: // OSNEWL
			crlf ();
			return 0;

		case 0xFFEE: // OSWRCH
			oswrch (al);
			return 0;

		case 0xFFF1: // OSWORD
			memcpy (xy + 1, &bbcfont[*(unsigned char*)(xy) << 3], 8);
			return 0;

		case 0xFFF4: // OSBYTE
			return (vgetc (0x80000000, 0x80000000) << 8);

		case 0xFFF7: // OSCLI
			oscli (xy);
			return 0; 

		default:
			error (8, NULL); // 'Address out of range'
	    }
	return 0;
    }

#if PICO_STACK_CHECK & 0x04
uintptr_t   stk_guard = 0;

void install_stack_guard (void *stack_bottom)
    {
#if PICO == 1
// Configure a 512 byte protected region that is installed at least 64 bytes above stack_bottom
// The stack_trap routine executes with the stack pointer just below this protected region.
    if ( stack_bottom == NULL )
        {
        mpu_hw->ctrl = 0; // Disable mpu
        stk_guard = 0;
        }
    else
        {
        // the minimum we can protect is 32 bytes on a 32 byte boundary, so round up which will
        // just shorten the valid stack range a tad
        stk_guard = ((uintptr_t) stack_bottom + 95u) & ~31u;
        // printf ("stk_guard = %p - %p\n", stk_guard, stk_guard + 512);

        // mask is 1 bit per 32 bytes of the 256 byte range... clear the bit for the segment we want
        uint32_t subregion_select = 0xff0000ffu << ((stk_guard >> 5u) & 7u);
        mpu_hw->ctrl = 5; // enable mpu with background default map
        mpu_hw->rbar = (stk_guard & (uint)~0xff) | 0x10 | 0; // region 0
        mpu_hw->rasr = 1 // enable region
            | (0x7 << 1) // size 2^(7 + 1) = 256
            | (subregion_select & 0xff00)
            | 0x10000000; // XN = disable instruction fetch; no other bits means no permissions
        mpu_hw->rbar = ((stk_guard & (uint)~0xff) + 0x100) | 0x10 | 1; // region 1
        mpu_hw->rasr = 1 // enable region
            | (0x7 << 1) // size 2^(7 + 1) = 256
            | 0x10000000; // XN = disable instruction fetch; no other bits means no permissions
        mpu_hw->rbar = ((stk_guard & (uint)~0xff) + 0x200) | 0x10 | 2; // region 2 - 512 bytes higher
        if ( stk_guard & 0xE0 )
            {
            mpu_hw->rasr = 1 // enable region
                | (0x7 << 1) // size 2^(7 + 1) = 256
                | ((subregion_select >> 16 ) & 0xff00)
                | 0x10000000; // XN = disable instruction fetch; no other bits means no permissions
            }
        else
            {
            mpu_hw->rasr = 0; // disable region
            }
        }
    /*
      uint32_t nrgn = mpu_hw->type;
      printf ("type = 0x%08X, ctrl = 0x%08X\n", nrgn, mpu_hw->ctrl);
      nrgn = ( nrgn >> 8 ) & 0xFF;
      for (int i = 0; i < nrgn; ++i)
      {
      mpu_hw->rnr = i;
      printf ("rbar = 0x%08X, rasr = 0x%08X\n", mpu_hw->rbar, mpu_hw->rasr);
      }
    */
#elif PICO == 2
#if 1
    // Use stack limit register for overrun protection
    // Since the UsageFault exception is not enabled,
    // a stack overrun will cause a HardFault instead.
    if ( stack_bottom == NULL )
        {
        stk_guard = 0;
        asm volatile(
            "msr msplim, %0"
            :
            : "r" (stk_guard)
            );
        m33_hw->ccr &=  ~ M33_CCR_STKOFHFNMIGN_BITS;
        }
    else
        {
        stk_guard = ((uintptr_t) stack_bottom + 95u) & ~31u;
        asm volatile(
            "msr msplim, %0"
            :
            : "r" (stk_guard)
            );
        // Allow HardFault handler to ignore stack limit register
        m33_hw->ccr |=  M33_CCR_STKOFHFNMIGN_BITS;
        }
#else
    // Use MMU for overrun protection
    if ( stack_bottom == NULL )
        {
        mpu_hw->ctrl = 0; // Disable mpu
        stk_guard = 0;
        }
    else
        {
        // the minimum we can protect is 32 bytes on a 32 byte boundary, so round up which will
        // just shorten the valid stack range a tad
        stk_guard = ((uintptr_t) stack_bottom + 95u) & ~31u;
        // printf ("stk_guard = %p\n", stk_guard);

        mpu_hw->ctrl = 5;   // enable mpu with background default map
        mpu_hw->rnr = 0;    // Select region 0
        mpu_hw->rbar = stk_guard  | 0x05;           // Lower limit, Not sharable, Read only, No execute
        mpu_hw->rlar = (stk_guard + 0x200) | 0x11;  // Upper limit, Execution prohibited, Attribute 0, Enabled
        }
#endif
#endif
    }
#endif

// Request memory allocation above HIMEM:
heapptr oshwm (void *addr, int settop)
    {
    char dummy;
#ifdef _WIN32
	if ((addr < userRAM) ||
	    (addr > (userRAM + MaximumRAM)) ||
            (NULL == VirtualAlloc (userRAM, addr - userRAM,
                MEM_COMMIT, PAGE_EXECUTE_READWRITE)))
		return 0;
#else
	if ((addr < userRAM) || (addr > (userRAM + MaximumRAM)) || ( addr >= (void *) &dummy ))
        {
        // printf ("Above MaximumRAM = %p\n", userRAM + MaximumRAM);
        error (0, NULL); // 'No room'
		return 0;
        }
#endif
	else
	    {
		if (settop && (addr > userTOP))
			userTOP = addr;
#if PICO_STACK_CHECK & 0x04
        // printf ("oshwm (%p, %d)\n", addr, settop);
        install_stack_guard (addr > (void *) libase ? addr : libtop);
#endif
		return (size_t) addr;
	    }
    }

// Get a file context from a channel number:
static FILE *lookup (void *chan)
    {
	FILE *file = NULL;

	if ((chan >= (void *)1) && (chan <= (void *)(MAX_PORTS + MAX_FILES)))
		file = (FILE*) filbuf[(size_t)chan];
	else
		file = (FILE*) chan;

	if (file == NULL)
		error (222, "Invalid channel");
	return file;
    }

// Load a file into memory:
void osload (char *p, void *addr, int max)
    {
	int n;
	FILE *file;
	if (NULL == setup (path, p, ".bbc", '\0', NULL))
		error (253, "Bad string");
	file = fopen (path, "rb");
	if (file == NULL)
		error (214, "File or path not found");
	n = fread (addr, 1, max, file);
	fclose (file);
	if (n == 0)
		error (189, "Couldn't read from file");
    }

// Save a file from memory:
void ossave (char *p, void *addr, int len)
    {
	int n;
	FILE *file;
	if (NULL == setup (path, p, ".bbc", '\0', NULL))
		error (253, "Bad string");
	file = fopen (path, "w+b");
	if (file == NULL)
		error (214, "Couldn't create file");
	n = fwrite (addr, 1, len, file);
	fclose (file);
	if (n < len)
		error (189, "Couldn't write to file");
    }

// Open a file:
void *osopen (int type, char *p)
    {
	int chan, first, last;
	FILE *file;
	if (setup (path, p, ".bbc", '\0', NULL) == NULL)
		return 0;
	if (type == 0)
		file = fopen (path, "rb");
	else if (type == 1)
		file = fopen (path, "w+b");
	else
		file = fopen (path, "r+b");
	if (file == NULL)
		return NULL;

	if (0 == memcmp (path, "/dev", 4))
	    {
		first = 1;
		last = MAX_PORTS;
	    }
	else
	    {
		first = MAX_PORTS + 1;
		last = MAX_PORTS + MAX_FILES;
	    }

	for (chan = first; chan <= last; chan++)
	    {
		if (filbuf[chan] == 0)
		    {
			filbuf[chan] = file;
			if (chan > MAX_PORTS)
				*(int *)&fcbtab[chan - MAX_PORTS - 1] = 0;
			return (void *)(size_t)chan;
		    }
	    }
	fclose (file);
	error (192, "Too many open files");
	return NULL; // never happens
    }

// Read file to 256-byte buffer:
static void readb (FILE *context, unsigned char *buffer, FCB *pfcb)
    {
	int amount;
	if (context == NULL)
		error (222, "Invalid channel");
//	if (pfcb->p != pfcb->o) Windows requires fseek to be called ALWAYS
	myfseek (context, (pfcb->p - pfcb->o) & 0xFF, SEEK_CUR);
#ifdef _WIN32
	long long ptr = myftell (context);
#endif
	amount = fread (buffer, 1, 256, context);
#ifdef _WIN32
	myfseek (context, ptr + amount, SEEK_SET); // filetest.bbc fix (32-bit)
#endif
	pfcb->p = 0;
	pfcb->o = amount & 0xFF;
	pfcb->w = 0;
	pfcb->f = (amount != 0);
	while (amount < 256)
		buffer[amount++] = 0;
	return;
    }

// Write 256-byte buffer to file:
static int writeb (FILE *context, unsigned char *buffer, FCB *pfcb)
    {
	int amount;
	if (pfcb->f >= 0)
	    {
		pfcb->f = 0;
		return 0;
	    }
	if (context == NULL)
		error (222, "Invalid channel");
	if (pfcb->f & 1)
		myfseek (context, pfcb->o ? -pfcb->o : -256, SEEK_CUR);
#ifdef _WIN32
	long long ptr = myftell (context);
#endif
	amount = fwrite (buffer, 1, pfcb->w ? pfcb->w : 256, context);
#ifdef _WIN32
	myfseek (context, ptr + amount, SEEK_SET); // assemble.bbc asmtst64.bba fix
#endif
	pfcb->o = amount & 0xFF;
	pfcb->w = 0;
	pfcb->f = 1;
	return (amount == 0);
    }

// Close a single file:
static int closeb (void *chan)
    {
	int result;
	if ((chan > (void *)MAX_PORTS) && (chan <= (void *)(MAX_PORTS+MAX_FILES)))
	    {
		int index = (size_t) chan - MAX_PORTS - 1;
		unsigned char *buffer = (unsigned char *) filbuf[0] + index * 0x100;
		FCB *pfcb = &fcbtab[index];
		FILE *handle = (FILE *) filbuf[(size_t) chan];
		if (writeb (handle, buffer, pfcb))
			return 1;
	    }
	result = fclose (lookup (chan));
	if ((chan >= (void *)1) && (chan <= (void *)(MAX_PORTS + MAX_FILES)))
		filbuf[(size_t)chan] = 0;
	return result;
    }

// Read a byte:
unsigned char osbget (void *chan, int *peof)
    {
	unsigned char byte = 0;
	if (peof != NULL)
		*peof = 0;
	if ((chan > (void *)MAX_PORTS) && (chan <= (void *)(MAX_PORTS+MAX_FILES)))
	    {
		int index = (size_t) chan - MAX_PORTS - 1;
		unsigned char *buffer = (unsigned char *) filbuf[0] + index * 0x100;
		FCB *pfcb = &fcbtab[index];
		if (pfcb->p == pfcb->o)
		    {
			FILE *handle = (FILE *) filbuf[(size_t) chan];
			if (writeb (handle, buffer, pfcb))
				error (189, "Couldn't write to file");
			readb (handle, buffer, pfcb);
			if ((pfcb->f & 1) == 0)
			    {
				if (peof != NULL)
					*peof = 1;
				return 0;
			    } 
		    }
		return buffer[pfcb->p++];
	    }
	if ((0 == fread (&byte, 1, 1, lookup (chan))) && (peof != NULL))
		*peof = 1;
	return byte;
    }

// Write a byte:
void osbput (void *chan, unsigned char byte)
    {
	if ((chan > (void *)MAX_PORTS) && (chan <= (void *)(MAX_PORTS+MAX_FILES)))
	    {
		int index = (size_t) chan - MAX_PORTS - 1;
		unsigned char *buffer = (unsigned char *) filbuf[0] + index * 0x100;
		FCB *pfcb = &fcbtab[index];
		if (pfcb->p == pfcb->o)
		    {
			FILE *handle = (FILE *) filbuf[(size_t) chan];
			if (writeb (handle, buffer, pfcb))
				error (189, "Couldn't write to file");
			readb (handle, buffer, pfcb);
		    }
		buffer[pfcb->p++] = byte;
		pfcb->w = pfcb->p;
		pfcb->f |= 0x80;
		return;
	    }
	if (0 == fwrite (&byte, 1, 1, lookup (chan)))
		error (189, "Couldn't write to file");
    }

// Get file pointer:
long long getptr (void *chan)
    {
	myfseek (lookup (chan), 0, SEEK_CUR);
	long long ptr = myftell (lookup (chan));
	if (ptr == -1)
		error (189, "Couldn't read file pointer");
	if ((chan > (void *)MAX_PORTS) && (chan <= (void *)(MAX_PORTS+MAX_FILES)))
	    {
		FCB *pfcb = &fcbtab[(size_t) chan - MAX_PORTS - 1];
		if (pfcb->o)
			ptr -= pfcb->o;
		else if (pfcb->f & 1)
			ptr -= 256;
		if (pfcb->p)
			ptr += pfcb->p;
		else if (pfcb->f & 0x81)
			ptr += 256;
	    }
	return ptr;
    }

// Set file pointer:
void setptr (void *chan, long long ptr)
    {
	if ((chan > (void *)MAX_PORTS) && (chan <= (void *)(MAX_PORTS+MAX_FILES)))
	    {
		int index = (size_t) chan - MAX_PORTS - 1;
		unsigned char *buffer = (unsigned char *) filbuf[0] + index * 0x100;
		FCB *pfcb = &fcbtab[index];
		FILE *handle = (FILE *) filbuf[(size_t) chan];
		if (writeb (handle, buffer, pfcb))
			error (189, "Couldn't write to file");
		*(int *)pfcb = 0;
	    }
	if (-1 == myfseek (lookup (chan), ptr, SEEK_SET))
		error (189, "Couldn't set file pointer");
    }

// Get file size:
long long getext (void *chan)
    {
	long long newptr = getptr (chan);
	FILE *file = lookup (chan);
	myfseek (file, 0, SEEK_CUR);
	long long ptr = myftell (file);
	myfseek (file, 0, SEEK_END);
	long long size = myftell (file);
	if ((ptr == -1) || (size == -1))
		error (189, "Couldn't set file pointer");
	myfseek (file, ptr, SEEK_SET);
	if (newptr > size)
		return newptr;
	return size;
    }

// Set file size (if possible):
void setext (void *chan, long long ptr)
    {
	FILE *file = lookup (chan);
    fextent (file, ptr);
    }

// Get EOF status:
long long geteof (void *chan)
    {
	if ((chan > (void *)MAX_PORTS) && (chan <= (void *)(MAX_PORTS+MAX_FILES)))
	    {
		FCB *pfcb = &fcbtab[(size_t) chan - MAX_PORTS - 1];
		if ((pfcb->p != 0) && (pfcb->o == 0) && ((pfcb->f) & 1))
			return 0;
	    }
	return -(getptr (chan) >= getext (chan));
    }

// Close file (if chan = 0 all open files closed and errors ignored):
void osshut (void *chan)
    {
	if (chan == NULL)
	    {
		int chan;
		for (chan = MAX_PORTS + MAX_FILES; chan > 0; chan--)
		    {
			if (filbuf[chan])
				closeb ((void *)(size_t)chan); // ignore errors
		    }
		return;
	    }
	if (closeb (chan))
		error (189, "Couldn't close file");
    }

void allocbuf (void)
    {
    void *memptr = userRAM;
	accs = (char*) memptr; memptr += ACCSLEN;		// String accumulator
	buff = (char*) memptr; memptr += 0x100;		    // Temporary string buffer
	path = (char*) memptr; memptr += 0x100;		    // File path
	keystr = (char**) memptr; memptr += 0x100;	    // *KEY strings
	keybdq = (char*) memptr; memptr += 0x100;	    // Keyboard queue
	eventq = (int*) memptr; memptr += 0x200;	    // Event queue
	filbuf[0] = (int*) memptr; memptr += 0x100 * MAX_FILES;	// File buffers
#if PICO_SOUND == 3
	envels = (signed char*) memptr; memptr += 0x100;	// Envelopes
	waves = (short*) memptr; memptr += 0x800;	// Sound wave buffer
#endif
	szLoadDir = (char*) memptr; memptr += 0x100;
	szLibrary = (char*) memptr; memptr += 0x100;
	szUserDir = (char*) memptr; memptr += 0x100;
	szTempDir = (char*) memptr; memptr += 0x100;    // Strings must be allocated on BASIC's heap
#ifdef PICO_GUI
    usrchr = (char*) memptr;                        // User-defined characters (indirect)
#endif
	szCmdLine = (char*) memptr; memptr += 0x100;    // Must be immediately below default progRAM
	progRAM = memptr;                               // Will be raised if @cmd$ exceeds 255 bytes
    userTOP = userRAM + BASIC_RAM;
    install_stack_guard (userTOP);
    }

// Start interpreter:
int entry (void *immediate)
    {
/*
#ifdef PICO_GUI
    allocbuf ();
#else
	accs = (char*) userRAM;		// String accumulator
	buff = (char*) accs + ACCSLEN;		// Temporary string buffer
	path = (char*) buff + 0x100;		// File path
	keystr = (char**) (path + 0x100);	// *KEY strings
	keybdq = (char*) keystr + 0x100;	// Keyboard queue
	eventq = (void*) keybdq + 0x100;	// Event queue
	filbuf[0] = (eventq + 0x200 / 4);	// File buffers n.b. pointer arithmetic!!
#if PICO_SOUND == 3
	envels = (signed char*) (filbuf[0] + 0x800);	// Envelopes
	waves = (short*) (envels + 0x100);	// Sound wave buffer
#endif
#endif
*/

	farray = 1;				// @hfile%() number of dimensions
	fasize = MAX_PORTS + MAX_FILES + 4;	// @hfile%() number of elements

#ifndef _WIN32
	vflags = UTF8;				// Not |= (fails on Linux build)
#endif

	prand.l = (unsigned int) GetTicks () ;	/// Seed PRNG
	prand.h = (prand.l == 0) ;
	rnd ();				// Randomise !

	memset (keystr, 0, 256);
	xeqvdu (0x1700, 0, 0x1F);		// initialise VDU drivers
	spchan = NULL;
	exchan = NULL;

	if (immediate)
	    {
		text (szVersion);
		crlf ();
		text (szNotice);
		crlf ();
#if HAVE_CYW43
        if ( iCyw == PICO_ERROR_NO_CYW43 )
            {
            text ("No cyw43 support");
            }
        else
            {
            text ("cyw43 initialisation ");
            if ( iCyw == 0 ) text ("succeded");
            else text ("failed");
            }
        crlf ();
#endif
	    }

    oshwm (userTOP, 0);
	return basic (progRAM, userTOP, immediate);
    }

#ifdef _WIN32
static void UserTimerProc (UINT uUserTimerID, UINT uMsg, void *dwUser, void *dw1, void *dw2)
    {
	if (timtrp)
		putevt (timtrp, WM_TIMER, 0, 0);
	flags |= ALERT; // Always, for periodic ESCape detection
	return;
    }

timer_t StartTimer (int period)
    {
	return timeSetEvent (period, 0, (LPTIMECALLBACK) UserTimerProc, 0, TIME_PERIODIC); 
    }

void StopTimer (timer_t timerid)
    {
	timeKillEvent (timerid);
    }

void SystemIO (int flag)
    {
	if (!flag)
		SetConsoleMode (GetStdHandle(STD_INPUT_HANDLE), ENABLE_VIRTUAL_TERMINAL_INPUT); 
    }
#endif

#ifdef __linux__
static void UserTimerProc (int sig, siginfo_t *si, void *uc)
    {
	if (timtrp)
		putevt (timtrp, WM_TIMER, 0, 0);
	flags |= ALERT; // Always, for periodic ESCape detection
    }

timer_t StartTimer (int period)
    {
	timer_t timerid;
	struct sigevent evp = {0};
	struct sigaction sig;
	struct itimerspec tm;

	tm.it_interval.tv_nsec = (period % 1000) * 1000000;
	tm.it_interval.tv_sec = (period / 1000);
	tm.it_value = tm.it_interval;

	sig.sa_handler = (void *) &UserTimerProc;
	sigemptyset (&sig.sa_mask);
	sig.sa_flags = 0;

	evp.sigev_value.sival_ptr = &timerid;
	evp.sigev_notify = SIGEV_SIGNAL;
	evp.sigev_signo = SIGALRM;

	timer_create (CLOCK_MONOTONIC, &evp, &timerid);
	timer_settime (timerid, 0, &tm, NULL);
	sigaction (SIGALRM, &sig, NULL);

	return timerid;
    }

void StopTimer (timer_t timerid)
    {
	timer_delete (timerid);
    }

void SystemIO (int flag)
    {
	struct termios tmp;
	tcgetattr (STDIN_FILENO, &tmp);
	if (flag)
	    {
		tmp.c_lflag |= ISIG;
		tmp.c_oflag |= OPOST;
	    }
	else
	    {
		tmp.c_lflag &= ~ISIG;
		tmp.c_oflag &= ~OPOST;
	    }
	tcsetattr (STDIN_FILENO, TCSAFLUSH, &tmp);
    }
#endif

#ifdef PICO
static bool UserTimerProc (struct repeating_timer *prt)
    {
//    myPoll ();
    if (timtrp)
        putevt (timtrp, WM_TIMER, 0, 0);
    flags |= ALERT; // Always, for periodic ESCape detection
    return true;
    }

static struct repeating_timer s_rpt_timer;
timer_t StartTimer (int period)
    {
    timer_t dummy;
    add_repeating_timer_ms (period, UserTimerProc, NULL, &s_rpt_timer);
    return dummy;
    }

void StopTimer (timer_t timerid)
    {
    cancel_repeating_timer (&s_rpt_timer);
    return;
    }

void SystemIO (int flag) {
	return;
    }
#endif

#ifdef __APPLE__
static void UserTimerProc (dispatch_source_t timerid)
    {
	if (timtrp)
		putevt (timtrp, WM_TIMER, 0, 0);
	flags |= ALERT; // Always, for periodic ESCape detection
    }

timer_t StartTimer (int period)
    {
	dispatch_source_t timerid;
	dispatch_time_t start;

	timerid = dispatch_source_create (DISPATCH_SOURCE_TYPE_TIMER, 0, 0, timerqueue);

	dispatch_source_set_event_handler (timerid, ^{UserTimerProc(timerid);});
	start = dispatch_time (DISPATCH_TIME_NOW, period * 1000000);
	dispatch_source_set_timer (timerid, start, period * 1000000, 0);
	dispatch_resume (timerid);

	return timerid;
    }

void StopTimer (timer_t timerid)
    {
	dispatch_source_cancel (timerid);
    }

void SystemIO (int flag)
    {
	struct termios tmp;
	tcgetattr (STDIN_FILENO, &tmp);
	if (flag)
	    {
		tmp.c_lflag |= ISIG;
		tmp.c_oflag |= OPOST;
	    }
	else
	    {
		tmp.c_lflag &= ~ISIG;
		tmp.c_oflag &= ~OPOST;
	    }
	tcsetattr (STDIN_FILENO, TCSAFLUSH, &tmp);
    }
#endif

static void SetLoadDir (char *path)
    {
	char temp[MAX_PATH];
	char *p;
	strcpy (temp, path);
	p = strrchr (temp, '/');
	if (p == NULL) p = strrchr (temp, '\\');
	if (p)
        {
		*p = '\0';
		realpath (temp, szLoadDir);
        }
	else
        {
		getcwd (szLoadDir, MAX_PATH);
        }

#ifdef _WIN32
	strcat (szLoadDir, "\\");
#else
	strcat (szLoadDir, "/");
#endif
    }

#ifdef PICO
void sigbus(void){
	printf("SIGBUS exception caught...\n");
	for(;;);
    }

void led_init (void)
    {
#if HAVE_CYW43
    if ( iCyw != 0 )
        {
#endif
        gpio_init(PICO_DEFAULT_LED_PIN);
        gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
#if HAVE_CYW43
        }
#endif
    }

void led_state (int iLED)
    {
#if HAVE_CYW43
    if ( iCyw == 0 )
        cyw43_arch_gpio_put (CYW43_WL_GPIO_LED_PIN, iLED);
    else
#endif
	    gpio_put(PICO_DEFAULT_LED_PIN, iLED);
    }

static int waitdone=0;
void waitconsole(void){
	if(waitdone) return;
#ifndef PICO_GUI
#if (USB_CON & 2) == 0
	puts_raw("Waiting for connection\r");
#endif
    led_init ();
    int iLED = 0;
	while (true)
        {
        iLED ^= 1;
        led_state (iLED);
#if defined(STDIO_USB) && ((USB_CON & 1) == 0)
        if ( stdio_usb_connected() ) break;
#endif
#ifdef STDIO_BT
        if ( stdio_bt_connected() ) break;
#endif
#if defined(STDIO_UART) || ((USB_CON & 1) == 1)
        unsigned char ch;
        getinp (&ch);
        if ( ch == 0x0D ) break;
#endif
#if (USB_CON & 2) == 0
		putchar_raw('.');
#endif
        myPoll ();
		sleep_ms(1000);
        }
    puts_raw ("\r");
    led_state (0);
#if defined (STDIO_USB)
    if ( stdio_usb_connected() ) stdio_filter_driver (&stdio_usb);
#if defined (STDIO_BT)
    else if ( stdio_bt_connected() ) stdio_filter_driver (&stdio_bt);
#endif
#elif defined (STDIO_BT)
    if ( stdio_bt_connected() ) stdio_filter_driver (&stdio_bt);
#endif
#if defined (STDIO_USB) || defined (STDIO_BT)
    else stdio_filter_driver (&stdio_uart);
#endif
#endif
	waitdone=1;
    }
#endif

void syserror (const char *psMsg)
    {
#ifdef PICO
    bSysErr = true;
    if (waitdone)
        {
        text (psMsg);
        text ("\r\n");
        }
    else
        {
        char *psErr = (char *)progRAM + strlen (progRAM);
        strcpy (psErr, psMsg);
        strcat (psErr, "\r\n");
        }
#else
	fprintf (stderr, "%s\r\n", msg);
#endif
    }

// Report a 'fatal' error:
void faterr (const char *msg)
    {
    reset ();
    syserror (msg);
	error (256, "");
    }

#ifdef PICO
void sys_panic (const char *psMsg, ...)
    {
    char sErr[260];
    va_list va;
    va_start (va, psMsg);
    reset ();
    waitconsole ();
    text ("Panic: ");
    vsprintf (sErr, psMsg, va);
    va_end (va);
    text (sErr);
    text ("\r\n");
    error (256, 0);
    }
#endif

void main_term (int exitcode)
    {

	if (UserTimerID)
		StopTimer (UserTimerID);

#ifdef __APPLE__
    dispatch_release (timerqueue);
#endif

#ifdef _WIN32
	if (_isatty (_fileno (stdout)))
		printf ("\033[0m\033[!p");
	CloseHandle (hThread);
	if (orig_stdout != -1)
		SetConsoleMode (GetStdHandle(STD_OUTPUT_HANDLE), orig_stdout);
	if (orig_stdin != -1)
		SetConsoleMode (GetStdHandle(STD_INPUT_HANDLE), orig_stdin);
#else
# ifdef PICO
#ifndef PICO_GUI
	printf ("\033[0m\033[!p");
	printf ("\nExiting with code %d...rebooting...\n",
		exitcode);
#endif
	watchdog_reboot(0,0,50);
    for(;;) sleep(5);
# else
	if (isatty (STDOUT_FILENO))
		printf ("\033[0m\033[!p");
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
# endif
#endif

	exit (exitcode);
    }

#ifdef PICO
int is_pico_w (void)
    {
    adc_init ();
    adc_select_input (3);
    if ( ( 3.3 / ( 1 << 12 ) ) * adc_read () < 0.25 ) return HAVE_CYW43 + 1;
    else return 0;
    }

// Work around bug in cyw43_arch_deinit()
#if HAVE_CYW43
static bool is_cyw43_init = false;
    
int cyw43_arch_init_safe (void)
    {
    int iErr = 0;
    if ( ! is_cyw43_init )
        {
        iErr = cyw43_arch_init ();
        if ( iErr == 0 ) is_cyw43_init = true;
        }
    return iErr;
    }

int cyw43_arch_init_with_country_safe (uint32_t country)
    {
    int iErr = 0;
    if (( is_cyw43_init ) && ( cyw43_arch_get_country_code () != country ))
        {
        cyw43_arch_deinit ();
        is_cyw43_init = false;
        }
    if ( ! is_cyw43_init )
        {
        iErr = cyw43_arch_init_with_country (country);
        if ( iErr == 0 ) is_cyw43_init = true;
        }
    return iErr;
    }

void cyw43_arch_deinit_safe (void)
    {
    if ( is_cyw43_init )
        {
        cyw43_arch_deinit ();
        is_cyw43_init = false;
        }
    }
#endif
#endif

void *main_init (int argc, char *argv[])
    {
#ifdef PICO
#if defined(PICO_GUI) || defined(PICO_GRAPH)
    dma_channel_claim (0);  // Reserve DMA channel for video
#endif
    stdio_init_all();
	// Wait for UART connection
#if HAVE_CYW43
    if ( is_pico_w () > 1 )
        {
        iCyw = cyw43_arch_init_safe ();
#ifdef STDIO_BT
        stdio_bt_init ();
        stdio_set_driver_enabled (&stdio_bt, true);
#endif
        }
    else
        {
        iCyw = PICO_ERROR_NO_CYW43;
#endif  // HAVE_CYW43
        led_init ();
#if HAVE_CYW43
        }
#endif
	for (int i = 3; i > 0; --i )
	    {
	    // printf ("%d seconds to start\n", i);
        led_state (1);
	    sleep_ms (500);
        led_state (0);
	    sleep_ms (500);
	    }
    // waitconsole();
    // printf ("BBC Basic main build " __DATE__ " " __TIME__ "\n");
    // sleep_ms(500);
#if PICO_STACK_CHECK & 0x04
	extern void __attribute__((used,naked)) stack_trap(void);
	exception_set_exclusive_handler(HARDFAULT_EXCEPTION, stack_trap);
#elif defined(FREE)
	exception_set_exclusive_handler(HARDFAULT_EXCEPTION,sigbus);
#else
	extern void __attribute__((used,naked)) HardFault_Handler(void);
	exception_set_exclusive_handler(HARDFAULT_EXCEPTION,HardFault_Handler);
#endif
	char *cmdline[]={"/autorun.bbc",0};
	argc=1; argv=cmdline;
#ifdef PICO_GUI
    dma_channel_unclaim (0);  // Free DMA channel for video
    setup_vdu ();
    setup_keyboard ();
#elif defined(PICO_GRAPH)
    dma_channel_unclaim (0);  // Free DMA channel for video
    setup_vdu ();
#endif
#ifdef PICO_SOUND
    snd_setup ();
#if PICO_SOUND == 3
    uart_set_baudrate (uart_default, PICO_DEFAULT_UART_BAUD_RATE);
#endif
#endif  // def PICO_SOUND
    cfgvar.next = (intptr_t)sysvar + (intptr_t)(&sysvar) - (intptr_t)(&cfgvar);
    ISTORE (&sysvar, (intptr_t)(&cfgvar) - (intptr_t)(&sysvar));
#endif  // def PICO
    int i;
    char *env, *p, *q;
    int exitcode = 0;
    void *immediate;
    FILE *ProgFile, *TestFile;
    char szAutoRun[MAX_PATH + 1];

#ifdef _WIN32
    int orig_stdout = -1;
    int orig_stdin = -1;
    HANDLE hThread = NULL;

	platform = 0;

	// Allocate the program buffer
	// First reserve the maximum amount:

	char *pstrVirtual = NULL;

	while ((MaximumRAM >= DEFAULT_RAM) &&
            (NULL == (pstrVirtual = (char*) VirtualAlloc (NULL, MaximumRAM,
                    MEM_RESERVE, PAGE_EXECUTE_READWRITE))))
		MaximumRAM /= 2;

	// Now commit the initial amount to physical RAM:

	if (pstrVirtual != NULL)
		userRAM = (char*) VirtualAlloc (pstrVirtual, DEFAULT_RAM,
            MEM_COMMIT, PAGE_EXECUTE_READWRITE);

#endif

#ifdef __linux__
    static struct termios orig_termios;
    pthread_t hThread = 0;

	platform = 1;

	void *base = NULL;

	while ((MaximumRAM >= MINIMUM_RAM) && (NULL == (base = mymap (MaximumRAM))))
		MaximumRAM /= 2;

	// Now commit the initial amount to physical RAM:

	if (base != NULL)
		userRAM = mmap (base, MaximumRAM, PROT_EXEC | PROT_READ | PROT_WRITE, 
            MAP_FIXED | MAP_PRIVATE | MAP_ANON, -1, 0);

#endif

#ifdef __APPLE__
    static struct termios orig_termios;
    pthread_t hThread = NULL;

	platform = 2;

	while ((MaximumRAM >= MINIMUM_RAM) &&
            ((void*)-1 == (userRAM = mmap ((void *)0x10000000, MaximumRAM, 
                    PROT_EXEC | PROT_READ | PROT_WRITE, 
                    MAP_PRIVATE | MAP_ANON, -1, 0))) &&
            ((void*)-1 == (userRAM = mmap ((void *)0x10000000, MaximumRAM, 
                    PROT_READ | PROT_WRITE, 
                    MAP_PRIVATE | MAP_ANON, -1, 0))))
		MaximumRAM /= 2;
#endif

#ifdef PICO
#if PICO == 1
	platform = 6 + ( is_pico_w () << 8 );
#elif PICO == 2
	platform = 0x2006 + ( is_pico_w () << 8 );
#endif
    /*
	MaximumRAM = MINIMUM_RAM;
	userRAM = &__StackLimit;
	if (userRAM + MaximumRAM > (void *)0x20040000) userRAM = 0;
    */
	userRAM = &__StackLimit;
    MaximumRAM = TOP_OF_MEM - userRAM;
/*
  The address 0x20040000 is 8K less than total RAM on the Pico to
  leave space for the current stack when zeroing.  This only works
  for a custom linker script that allocates most of the RAM to the
  stack.  For the default script userRAM = malloc(MaximumRAM);
*/
#endif  // def PICO
	if (userRAM) bzero(userRAM,MaximumRAM);

	if ((userRAM == NULL) || (userRAM == (void *)-1))
	    {
        syserror ("Couldn't allocate memory\r\n");
		main_term (9);
	    }

#if defined __x86_64__ || defined __aarch64__ 
	platform |= 0x40;
#endif

/*
	if (MaximumRAM > DEFAULT_RAM)
		userTOP = userRAM + DEFAULT_RAM;
	else
		userTOP = userRAM + MaximumRAM;
// #ifdef PICO_GUI
    allocbuf ();
#else
	progRAM = userRAM + PAGE_OFFSET; // Will be raised if @cmd$ exceeds 255 bytes
	szCmdLine = progRAM - 0x100;     // Must be immediately below default progRAM
	szTempDir = szCmdLine - 0x100;   // Strings must be allocated on BASIC's heap
	szUserDir = szTempDir - 0x100;
	szLibrary = szUserDir - 0x100;
	szLoadDir = szLibrary - 0x100;
#endif
*/
    allocbuf ();
#ifdef PICO
    mount ((char *)progRAM);
#endif

// Get path to executable:

#ifdef _WIN32
	if (GetModuleFileName(NULL, szLibrary, 256) == 0)
#endif
#ifdef __linux__
        i = readlink ("/proc/self/exe", szLibrary, 255);
	if (i > 0)
		szLibrary[i] = '\0';
	else
#endif
#ifdef __APPLE__
        i = 256;
	if (_NSGetExecutablePath(szLibrary, (unsigned int *)&i))
#endif
	    {
		p = realpath (argv[0], NULL);
		if (p)
		    {
			strncpy (szLibrary, p, 256);
			free (p);
		    }
	    }

	strcpy (szAutoRun, szLibrary);

	q = strrchr (szAutoRun, '/');
	if (q == NULL) q = strrchr (szAutoRun, '\\');
	p = strrchr (szAutoRun, '.');

	if (p > q) *p = '\0';
	strcat (szAutoRun, ".bbc");

	TestFile = fopen (szAutoRun, "rb");
	if (TestFile != NULL) fclose (TestFile);
	else if ((argc >= 2) && (*argv[1] != '-'))
		strcpy (szAutoRun, argv[1]);

	strcpy (szCmdLine, szAutoRun);

	if (argc >= 2)
	    {
		*szCmdLine = 0;
		for (i = 1; i < argc; i++)
		    {
			if (i > 1) strcat (szCmdLine, " ");
			strcat (szCmdLine, argv[i]);
		    }
		progRAM = (void *)(((intptr_t) szCmdLine + strlen(szCmdLine) + 256) & -256);
	    }

	if ((! bSysErr) && *szAutoRun && (NULL != (ProgFile = fopen (szAutoRun, "rb"))))
	    {
		fread (progRAM, 1, userTOP - progRAM, ProgFile);
		fclose (ProgFile);
		immediate = NULL;
        waitdone = 1;
	    }
	else
	    {
#ifdef PICO
		waitconsole();
        if (*((char *)progRAM))
            {
            text (progRAM);
            memset (progRAM, 0, strlen (progRAM));
            }
#endif
		immediate = (void *) 1;
		*szAutoRun = '\0';
	    }

#ifdef PICO
	strcpy(szTempDir, "/tmp");
	strcpy(szUserDir, "/user");
	strcpy(szLibrary, "/lib");
	strcpy(szLoadDir, "/");
	mkdir(szTempDir, 0770);
	mkdir(szUserDir, 0770);
	mkdir(szLibrary, 0770);
	strcat(szTempDir, "/");
	strcat(szUserDir, "/");
	strcat(szLibrary, "/");
#else
	env = getenv ("TMPDIR");
	if (!env) env = getenv ("TMP");
	if (!env) env = getenv ("TEMP");
	if (env) strcpy (szTempDir, env);
	else strcpy (szTempDir, "/tmp");

	env = getenv ("HOME");
	if (!env) env = getenv ("APPDATA");
	if (!env) env = getenv ("HOMEPATH");
	if (env) strcpy (szUserDir, env);

	p = strrchr (szLibrary, '/');
	if (p == NULL) p = strrchr (szLibrary, '\\');
	if (p)
		*p = '\0';

# ifdef _WIN32
	strcat (szTempDir, "\\");
	strcat (szLibrary, "\\lib\\");
	strcat (szUserDir, "\\bbcbasic\\");
	mkdir (szUserDir);
# else
	strcat (szTempDir, "/");
	strcat (szLibrary, "/lib/");
	strcat (szUserDir, "/bbcbasic/");
	mkdir (szUserDir, 0777);
# endif

	SetLoadDir (szAutoRun);
#endif

	if (argc < 2)
		*szCmdLine = 0;
	else if (TestFile == NULL)
		chdir (szLoadDir);

	// Set console for raw input and ANSI output:
#ifdef _WIN32
	// n.b.  Description of DISABLE_NEWLINE_AUTO_RETURN at MSDN is completely wrong!
	// What it actually does is to disable converting LF into CRLF, not wrap action.
	if (GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), (LPDWORD) &orig_stdout)) 
		SetConsoleMode (GetStdHandle(STD_OUTPUT_HANDLE), orig_stdout | ENABLE_WRAP_AT_EOL_OUTPUT |
            ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN);
	if (GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), (LPDWORD) &orig_stdin)) 
		SetConsoleMode (GetStdHandle(STD_INPUT_HANDLE), ENABLE_VIRTUAL_TERMINAL_INPUT); 
	hThread = CreateThread (NULL, 0, myThread, 0, 0, NULL);
#else
# ifndef PICO
	tcgetattr (STDIN_FILENO, &orig_termios);
	struct termios raw = orig_termios;
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_oflag &= ~OPOST;
	raw.c_cflag |= CS8;
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;
	tcsetattr (STDIN_FILENO, TCSAFLUSH, &raw);
    pthread_create(&hThread, NULL, &myThread, NULL);
# endif
#endif

#ifdef __APPLE__
	timerqueue = dispatch_queue_create ("timerQueue", 0);
#endif
#ifdef CAN_SET_RTC
    rtc_init ();
    putims2 ("01 Jan 2000,00:00:00", 20);
#endif

	UserTimerID = StartTimer (250);

#if HAVE_MOUSE
    mouse_init ();
#endif

	flags = 0;
    return immediate;
    }

int main (int argc, char* argv[])
    {
    void *immediate = main_init (argc, argv);
	int exitcode = entry (immediate);
    main_term (exitcode);
    }
