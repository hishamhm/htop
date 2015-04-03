/* Do not edit this file. It was automatically generated. */

#ifndef HEADER_CRT
#define HEADER_CRT
/*
htop - CRT.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#define ColorPair(i,j) COLOR_PAIR((7-i)*8+j)

#define COLORSCHEME_DEFAULT 0
#define COLORSCHEME_MONOCHROME 1
#define COLORSCHEME_BLACKONWHITE 2
#define COLORSCHEME_BLACKONWHITE2 3
#define COLORSCHEME_MIDNIGHT 4
#define COLORSCHEME_BLACKNIGHT 5

#define Black COLOR_BLACK
#define Red COLOR_RED
#define Green COLOR_GREEN
#define Yellow COLOR_YELLOW
#define Blue COLOR_BLUE
#define Magenta COLOR_MAGENTA
#define Cyan COLOR_CYAN
#define White COLOR_WHITE

//#link curses

#include <stdbool.h>

typedef enum ColorElements_ {
   RESET_COLOR,
   DEFAULT_COLOR,
   FUNCTION_BAR,
   FUNCTION_KEY,
   FAILED_SEARCH,
   PANEL_HEADER_FOCUS,
   PANEL_HEADER_UNFOCUS,
   PANEL_HIGHLIGHT_FOCUS,
   PANEL_HIGHLIGHT_UNFOCUS,
   LARGE_NUMBER,
   METER_TEXT,
   METER_VALUE,
   LED_COLOR,
   UPTIME,
   BATTERY,
   TASKS_RUNNING,
   SWAP,
   PROCESS,
   PROCESS_SHADOW,
   PROCESS_TAG,
   PROCESS_MEGABYTES,
   PROCESS_TREE,
   PROCESS_R_STATE,
   PROCESS_D_STATE,
   PROCESS_BASENAME,
   PROCESS_HIGH_PRIORITY,
   PROCESS_LOW_PRIORITY,
   PROCESS_THREAD,
   PROCESS_THREAD_BASENAME,
   BAR_BORDER,
   BAR_SHADOW,
   GRAPH_1,
   GRAPH_2,
   GRAPH_3,
   GRAPH_4,
   GRAPH_5,
   GRAPH_6,
   GRAPH_7,
   GRAPH_8,
   GRAPH_9,
   MEMORY_USED,
   MEMORY_BUFFERS,
   MEMORY_BUFFERS_TEXT,
   MEMORY_CACHE,
   MEMORY_SLAB,
   LOAD,
   LOAD_AVERAGE_FIFTEEN,
   LOAD_AVERAGE_FIVE,
   LOAD_AVERAGE_ONE,
   CHECK_BOX,
   CHECK_MARK,
   CHECK_TEXT,
   CLOCK,
   HELP_BOLD,
   HOSTNAME,
   CPU_NICE,
   CPU_NICE_TEXT,
   CPU_NORMAL,
   CPU_KERNEL,
   CPU_IOWAIT,
   CPU_IRQ,
   CPU_SOFTIRQ,
   CPU_STEAL,
   CPU_GUEST,
   LAST_COLORELEMENT
} ColorElements;

void CRT_fatalError(const char* note) __attribute__ ((noreturn));

void CRT_handleSIGSEGV(int sgn);


// TODO: centralize these in Settings.

extern int CRT_colorScheme;

extern bool CRT_utf8;

extern int CRT_colors[LAST_COLORELEMENT];

extern int CRT_cursorX;

extern int CRT_scrollHAmount;

char* CRT_termType;

void *backtraceArray[128];

// TODO: pass an instance of Settings instead.

void CRT_init(int delay, int colorScheme);

void CRT_done();

void CRT_fatalError(const char* note);

int CRT_readKey();

void CRT_disableDelay();

void CRT_enableDelay();

void CRT_setColors(int colorScheme);

#endif
