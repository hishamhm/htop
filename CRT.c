/*
htop - CRT.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"
#include "CRT.h"

#include "StringUtils.h"
#include "RichString.h"

#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <langinfo.h>
#if HAVE_SETUID_ENABLED
#include <unistd.h>
#include <sys/types.h>
#endif

#define ColorIndex(i,j) ((7-i)*8+j)

#define ColorPair(i,j) COLOR_PAIR(ColorIndex(i,j))

#define Black COLOR_BLACK
#define Red COLOR_RED
#define Green COLOR_GREEN
#define Yellow COLOR_YELLOW
#define Blue COLOR_BLUE
#define Magenta COLOR_MAGENTA
#define Cyan COLOR_CYAN
#define White COLOR_WHITE

#define ColorPairGrayBlack ColorPair(Magenta,Magenta)
#define ColorIndexGrayBlack ColorIndex(Magenta,Magenta)

#define KEY_WHEELUP KEY_F(20)
#define KEY_WHEELDOWN KEY_F(21)
#define KEY_RECLICK KEY_F(22)

//#link curses

/*{
#include <stdbool.h>

typedef enum TreeStr_ {
   TREE_STR_HORZ,
   TREE_STR_VERT,
   TREE_STR_RTEE,
   TREE_STR_BEND,
   TREE_STR_TEND,
   TREE_STR_OPEN,
   TREE_STR_SHUT,
   TREE_STR_COUNT
} TreeStr;

typedef enum ColorSchemes_ {
   COLORSCHEME_DEFAULT = 0,
   COLORSCHEME_MONOCHROME = 1,
   COLORSCHEME_BLACKONWHITE = 2,
   COLORSCHEME_LIGHTTERMINAL = 3,
   COLORSCHEME_MIDNIGHT = 4,
   COLORSCHEME_BLACKNIGHT = 5,
   COLORSCHEME_BROKENGRAY = 6,
   LAST_COLORSCHEME = 7,
} ColorSchemes;

typedef enum ColorElements_ {
   COLOR_RESET_COLOR,
   COLOR_DEFAULT_COLOR,
   COLOR_FUNCTION_BAR,
   COLOR_FUNCTION_KEY,
   COLOR_FAILED_SEARCH,
   COLOR_PANEL_HEADER_FOCUS,
   COLOR_PANEL_HEADER_UNFOCUS,
   COLOR_PANEL_SELECTION_FOCUS,
   COLOR_PANEL_SELECTION_FOLLOW,
   COLOR_PANEL_SELECTION_UNFOCUS,
   COLOR_LARGE_NUMBER,
   COLOR_METER_TEXT,
   COLOR_METER_VALUE,
   COLOR_LED_COLOR,
   COLOR_UPTIME,
   COLOR_BATTERY,
   COLOR_TASKS_RUNNING,
   COLOR_SWAP,
   COLOR_PROCESS,
   COLOR_PROCESS_SHADOW,
   COLOR_PROCESS_TAG,
   COLOR_PROCESS_MEGABYTES,
   COLOR_PROCESS_TREE,
   COLOR_PROCESS_R_STATE,
   COLOR_PROCESS_D_STATE,
   COLOR_PROCESS_BASENAME,
   COLOR_PROCESS_HIGH_PRIORITY,
   COLOR_PROCESS_LOW_PRIORITY,
   COLOR_PROCESS_THREAD,
   COLOR_PROCESS_THREAD_BASENAME,
   COLOR_BAR_BORDER,
   COLOR_BAR_SHADOW,
   COLOR_GRAPH_1,
   COLOR_GRAPH_2,
   COLOR_MEMORY_USED,
   COLOR_MEMORY_BUFFERS,
   COLOR_MEMORY_BUFFERS_TEXT,
   COLOR_MEMORY_CACHE,
   COLOR_LOAD,
   COLOR_LOAD_AVERAGE_FIFTEEN,
   COLOR_LOAD_AVERAGE_FIVE,
   COLOR_LOAD_AVERAGE_ONE,
   COLOR_CHECK_BOX,
   COLOR_CHECK_MARK,
   COLOR_CHECK_TEXT,
   COLOR_CLOCK,
   COLOR_HELP_BOLD,
   COLOR_HOSTNAME,
   COLOR_CPU_NICE,
   COLOR_CPU_NICE_TEXT,
   COLOR_CPU_NORMAL,
   COLOR_CPU_KERNEL,
   COLOR_CPU_IOWAIT,
   COLOR_CPU_IRQ,
   COLOR_CPU_SOFTIRQ,
   COLOR_CPU_STEAL,
   COLOR_CPU_GUEST,
   LAST_COLORELEMENT
} ColorElements;

void CRT_fatalError(const char* note) __attribute__ ((noreturn));

void CRT_handleSIGSEGV(int sgn);

#define KEY_ALT(x) (KEY_F(64 - 26) + (x - 'A'))

}*/

const char *CRT_treeStrAscii[TREE_STR_COUNT] = {
   "-", // TREE_STR_HORZ
   "|", // TREE_STR_VERT
   "`", // TREE_STR_RTEE
   "`", // TREE_STR_BEND
   ",", // TREE_STR_TEND
   "+", // TREE_STR_OPEN
   "-", // TREE_STR_SHUT
};

#ifdef HAVE_LIBNCURSESW

const char *CRT_treeStrUtf8[TREE_STR_COUNT] = {
   "\xe2\x94\x80", // TREE_STR_HORZ ─
   "\xe2\x94\x82", // TREE_STR_VERT │
   "\xe2\x94\x9c", // TREE_STR_RTEE ├
   "\xe2\x94\x94", // TREE_STR_BEND └
   "\xe2\x94\x8c", // TREE_STR_TEND ┌
   "+",            // TREE_STR_OPEN +
   "\xe2\x94\x80", // TREE_STR_SHUT ─
};

bool CRT_utf8 = false;

#endif

const char **CRT_treeStr = CRT_treeStrAscii;

static bool CRT_hasColors;

int CRT_delay = 0;

int* CRT_colors;

int CRT_colorSchemes[LAST_COLORSCHEME][LAST_COLORELEMENT] = {
   [COLORSCHEME_DEFAULT] = {
      [COLOR_RESET_COLOR] = ColorPair(White,Black),
      [COLOR_DEFAULT_COLOR] = ColorPair(White,Black),
      [COLOR_FUNCTION_BAR] = ColorPair(Black,Cyan),
      [COLOR_FUNCTION_KEY] = ColorPair(White,Black),
      [COLOR_PANEL_HEADER_FOCUS] = ColorPair(Black,Green),
      [COLOR_PANEL_HEADER_UNFOCUS] = ColorPair(Black,Green),
      [COLOR_PANEL_SELECTION_FOCUS] = ColorPair(Black,Cyan),
      [COLOR_PANEL_SELECTION_FOLLOW] = ColorPair(Black,Yellow),
      [COLOR_PANEL_SELECTION_UNFOCUS] = ColorPair(Black,White),
      [COLOR_FAILED_SEARCH] = ColorPair(Red,Cyan),
      [COLOR_UPTIME] = A_BOLD | ColorPair(Cyan,Black),
      [COLOR_BATTERY] = A_BOLD | ColorPair(Cyan,Black),
      [COLOR_LARGE_NUMBER] = A_BOLD | ColorPair(Red,Black),
      [COLOR_METER_TEXT] = ColorPair(Cyan,Black),
      [COLOR_METER_VALUE] = A_BOLD | ColorPair(Cyan,Black),
      [COLOR_LED_COLOR] = ColorPair(Green,Black),
      [COLOR_TASKS_RUNNING] = A_BOLD | ColorPair(Green,Black),
      [COLOR_PROCESS] = A_NORMAL,
      [COLOR_PROCESS_SHADOW] = A_BOLD | ColorPairGrayBlack,
      [COLOR_PROCESS_TAG] = A_BOLD | ColorPair(Yellow,Black),
      [COLOR_PROCESS_MEGABYTES] = ColorPair(Cyan,Black),
      [COLOR_PROCESS_BASENAME] = A_BOLD | ColorPair(Cyan,Black),
      [COLOR_PROCESS_TREE] = ColorPair(Cyan,Black),
      [COLOR_PROCESS_R_STATE] = ColorPair(Green,Black),
      [COLOR_PROCESS_D_STATE] = A_BOLD | ColorPair(Red,Black),
      [COLOR_PROCESS_HIGH_PRIORITY] = ColorPair(Red,Black),
      [COLOR_PROCESS_LOW_PRIORITY] = ColorPair(Green,Black),
      [COLOR_PROCESS_THREAD] = ColorPair(Green,Black),
      [COLOR_PROCESS_THREAD_BASENAME] = A_BOLD | ColorPair(Green,Black),
      [COLOR_BAR_BORDER] = A_BOLD,
      [COLOR_BAR_SHADOW] = A_BOLD | ColorPairGrayBlack,
      [COLOR_SWAP] = ColorPair(Red,Black),
      [COLOR_GRAPH_1] = A_BOLD | ColorPair(Cyan,Black),
      [COLOR_GRAPH_2] = ColorPair(Cyan,Black),
      [COLOR_MEMORY_USED] = ColorPair(Green,Black),
      [COLOR_MEMORY_BUFFERS] = ColorPair(Blue,Black),
      [COLOR_MEMORY_BUFFERS_TEXT] = A_BOLD | ColorPair(Blue,Black),
      [COLOR_MEMORY_CACHE] = ColorPair(Yellow,Black),
      [COLOR_LOAD_AVERAGE_FIFTEEN] = ColorPair(Cyan,Black),
      [COLOR_LOAD_AVERAGE_FIVE] = A_BOLD | ColorPair(Cyan,Black),
      [COLOR_LOAD_AVERAGE_ONE] = A_BOLD | ColorPair(White,Black),
      [COLOR_LOAD] = A_BOLD,
      [COLOR_HELP_BOLD] = A_BOLD | ColorPair(Cyan,Black),
      [COLOR_CLOCK] = A_BOLD,
      [COLOR_CHECK_BOX] = ColorPair(Cyan,Black),
      [COLOR_CHECK_MARK] = A_BOLD,
      [COLOR_CHECK_TEXT] = A_NORMAL,
      [COLOR_HOSTNAME] = A_BOLD,
      [COLOR_CPU_NICE] = ColorPair(Blue,Black),
      [COLOR_CPU_NICE_TEXT] = A_BOLD | ColorPair(Blue,Black),
      [COLOR_CPU_NORMAL] = ColorPair(Green,Black),
      [COLOR_CPU_KERNEL] = ColorPair(Red,Black),
      [COLOR_CPU_IOWAIT] = A_BOLD | ColorPair(Black, Black),
      [COLOR_CPU_IRQ] = ColorPair(Yellow,Black),
      [COLOR_CPU_SOFTIRQ] = ColorPair(Magenta,Black),
      [COLOR_CPU_STEAL] = ColorPair(Cyan,Black),
      [COLOR_CPU_GUEST] = ColorPair(Cyan,Black),
   },
   [COLORSCHEME_MONOCHROME] = {
      [COLOR_RESET_COLOR] = A_NORMAL,
      [COLOR_DEFAULT_COLOR] = A_NORMAL,
      [COLOR_FUNCTION_BAR] = A_REVERSE,
      [COLOR_FUNCTION_KEY] = A_NORMAL,
      [COLOR_PANEL_HEADER_FOCUS] = A_REVERSE,
      [COLOR_PANEL_HEADER_UNFOCUS] = A_REVERSE,
      [COLOR_PANEL_SELECTION_FOCUS] = A_REVERSE,
      [COLOR_PANEL_SELECTION_FOLLOW] = A_REVERSE,
      [COLOR_PANEL_SELECTION_UNFOCUS] = A_BOLD,
      [COLOR_FAILED_SEARCH] = A_REVERSE | A_BOLD,
      [COLOR_UPTIME] = A_BOLD,
      [COLOR_BATTERY] = A_BOLD,
      [COLOR_LARGE_NUMBER] = A_BOLD,
      [COLOR_METER_TEXT] = A_NORMAL,
      [COLOR_METER_VALUE] = A_BOLD,
      [COLOR_LED_COLOR] = A_NORMAL,
      [COLOR_TASKS_RUNNING] = A_BOLD,
      [COLOR_PROCESS] = A_NORMAL,
      [COLOR_PROCESS_SHADOW] = A_DIM,
      [COLOR_PROCESS_TAG] = A_BOLD,
      [COLOR_PROCESS_MEGABYTES] = A_BOLD,
      [COLOR_PROCESS_BASENAME] = A_BOLD,
      [COLOR_PROCESS_TREE] = A_BOLD,
      [COLOR_PROCESS_R_STATE] = A_BOLD,
      [COLOR_PROCESS_D_STATE] = A_BOLD,
      [COLOR_PROCESS_HIGH_PRIORITY] = A_BOLD,
      [COLOR_PROCESS_LOW_PRIORITY] = A_DIM,
      [COLOR_PROCESS_THREAD] = A_BOLD,
      [COLOR_PROCESS_THREAD_BASENAME] = A_REVERSE,
      [COLOR_BAR_BORDER] = A_BOLD,
      [COLOR_BAR_SHADOW] = A_DIM,
      [COLOR_SWAP] = A_BOLD,
      [COLOR_GRAPH_1] = A_BOLD,
      [COLOR_GRAPH_2] = A_NORMAL,
      [COLOR_MEMORY_USED] = A_BOLD,
      [COLOR_MEMORY_BUFFERS] = A_NORMAL,
      [COLOR_MEMORY_BUFFERS_TEXT] = A_NORMAL,
      [COLOR_MEMORY_CACHE] = A_NORMAL,
      [COLOR_LOAD_AVERAGE_FIFTEEN] = A_DIM,
      [COLOR_LOAD_AVERAGE_FIVE] = A_NORMAL,
      [COLOR_LOAD_AVERAGE_ONE] = A_BOLD,
      [COLOR_LOAD] = A_BOLD,
      [COLOR_HELP_BOLD] = A_BOLD,
      [COLOR_CLOCK] = A_BOLD,
      [COLOR_CHECK_BOX] = A_BOLD,
      [COLOR_CHECK_MARK] = A_NORMAL,
      [COLOR_CHECK_TEXT] = A_NORMAL,
      [COLOR_HOSTNAME] = A_BOLD,
      [COLOR_CPU_NICE] = A_NORMAL,
      [COLOR_CPU_NICE_TEXT] = A_NORMAL,
      [COLOR_CPU_NORMAL] = A_BOLD,
      [COLOR_CPU_KERNEL] = A_BOLD,
      [COLOR_CPU_IOWAIT] = A_NORMAL,
      [COLOR_CPU_IRQ] = A_BOLD,
      [COLOR_CPU_SOFTIRQ] = A_BOLD,
      [COLOR_CPU_STEAL] = A_REVERSE,
      [COLOR_CPU_GUEST] = A_REVERSE,
   },
   [COLORSCHEME_BLACKONWHITE] = {
      [COLOR_RESET_COLOR] = ColorPair(Black,White),
      [COLOR_DEFAULT_COLOR] = ColorPair(Black,White),
      [COLOR_FUNCTION_BAR] = ColorPair(Black,Cyan),
      [COLOR_FUNCTION_KEY] = ColorPair(Black,White),
      [COLOR_PANEL_HEADER_FOCUS] = ColorPair(Black,Green),
      [COLOR_PANEL_HEADER_UNFOCUS] = ColorPair(Black,Green),
      [COLOR_PANEL_SELECTION_FOCUS] = ColorPair(Black,Cyan),
      [COLOR_PANEL_SELECTION_FOLLOW] = ColorPair(Black,Yellow),
      [COLOR_PANEL_SELECTION_UNFOCUS] = ColorPair(Blue,White),
      [COLOR_FAILED_SEARCH] = ColorPair(Red,Cyan),
      [COLOR_UPTIME] = ColorPair(Yellow,White),
      [COLOR_BATTERY] = ColorPair(Yellow,White),
      [COLOR_LARGE_NUMBER] = ColorPair(Red,White),
      [COLOR_METER_TEXT] = ColorPair(Blue,White),
      [COLOR_METER_VALUE] = ColorPair(Black,White),
      [COLOR_LED_COLOR] = ColorPair(Green,White),
      [COLOR_TASKS_RUNNING] = ColorPair(Green,White),
      [COLOR_PROCESS] = ColorPair(Black,White),
      [COLOR_PROCESS_SHADOW] = A_BOLD | ColorPair(Black,White),
      [COLOR_PROCESS_TAG] = ColorPair(White,Blue),
      [COLOR_PROCESS_MEGABYTES] = ColorPair(Blue,White),
      [COLOR_PROCESS_BASENAME] = ColorPair(Blue,White),
      [COLOR_PROCESS_TREE] = ColorPair(Green,White),
      [COLOR_PROCESS_R_STATE] = ColorPair(Green,White),
      [COLOR_PROCESS_D_STATE] = A_BOLD | ColorPair(Red,White),
      [COLOR_PROCESS_HIGH_PRIORITY] = ColorPair(Red,White),
      [COLOR_PROCESS_LOW_PRIORITY] = ColorPair(Green,White),
      [COLOR_PROCESS_THREAD] = ColorPair(Blue,White),
      [COLOR_PROCESS_THREAD_BASENAME] = A_BOLD | ColorPair(Blue,White),
      [COLOR_BAR_BORDER] = ColorPair(Blue,White),
      [COLOR_BAR_SHADOW] = ColorPair(Black,White),
      [COLOR_SWAP] = ColorPair(Red,White),
      [COLOR_GRAPH_1] = A_BOLD | ColorPair(Blue,White),
      [COLOR_GRAPH_2] = ColorPair(Blue,White),
      [COLOR_MEMORY_USED] = ColorPair(Green,White),
      [COLOR_MEMORY_BUFFERS] = ColorPair(Cyan,White),
      [COLOR_MEMORY_BUFFERS_TEXT] = ColorPair(Cyan,White),
      [COLOR_MEMORY_CACHE] = ColorPair(Yellow,White),
      [COLOR_LOAD_AVERAGE_FIFTEEN] = ColorPair(Black,White),
      [COLOR_LOAD_AVERAGE_FIVE] = ColorPair(Black,White),
      [COLOR_LOAD_AVERAGE_ONE] = ColorPair(Black,White),
      [COLOR_LOAD] = ColorPair(Black,White),
      [COLOR_HELP_BOLD] = ColorPair(Blue,White),
      [COLOR_CLOCK] = ColorPair(Black,White),
      [COLOR_CHECK_BOX] = ColorPair(Blue,White),
      [COLOR_CHECK_MARK] = ColorPair(Black,White),
      [COLOR_CHECK_TEXT] = ColorPair(Black,White),
      [COLOR_HOSTNAME] = ColorPair(Black,White),
      [COLOR_CPU_NICE] = ColorPair(Cyan,White),
      [COLOR_CPU_NICE_TEXT] = ColorPair(Cyan,White),
      [COLOR_CPU_NORMAL] = ColorPair(Green,White),
      [COLOR_CPU_KERNEL] = ColorPair(Red,White),
      [COLOR_CPU_IOWAIT] = A_BOLD | ColorPair(Black, White),
      [COLOR_CPU_IRQ] = ColorPair(Blue,White),
      [COLOR_CPU_SOFTIRQ] = ColorPair(Blue,White),
      [COLOR_CPU_STEAL] = ColorPair(Cyan,White),
      [COLOR_CPU_GUEST] = ColorPair(Cyan,White),
   },
   [COLORSCHEME_LIGHTTERMINAL] = {
      [COLOR_RESET_COLOR] = ColorPair(Black,Black),
      [COLOR_DEFAULT_COLOR] = ColorPair(Black,Black),
      [COLOR_FUNCTION_BAR] = ColorPair(Black,Cyan),
      [COLOR_FUNCTION_KEY] = ColorPair(Black,Black),
      [COLOR_PANEL_HEADER_FOCUS] = ColorPair(Black,Green),
      [COLOR_PANEL_HEADER_UNFOCUS] = ColorPair(Black,Green),
      [COLOR_PANEL_SELECTION_FOCUS] = ColorPair(Black,Cyan),
      [COLOR_PANEL_SELECTION_FOLLOW] = ColorPair(Black,Yellow),
      [COLOR_PANEL_SELECTION_UNFOCUS] = ColorPair(Blue,Black),
      [COLOR_FAILED_SEARCH] = ColorPair(Red,Cyan),
      [COLOR_UPTIME] = ColorPair(Yellow,Black),
      [COLOR_BATTERY] = ColorPair(Yellow,Black),
      [COLOR_LARGE_NUMBER] = ColorPair(Red,Black),
      [COLOR_METER_TEXT] = ColorPair(Blue,Black),
      [COLOR_METER_VALUE] = ColorPair(Black,Black),
      [COLOR_LED_COLOR] = ColorPair(Green,Black),
      [COLOR_TASKS_RUNNING] = ColorPair(Green,Black),
      [COLOR_PROCESS] = ColorPair(Black,Black),
      [COLOR_PROCESS_SHADOW] = A_BOLD | ColorPairGrayBlack,
      [COLOR_PROCESS_TAG] = ColorPair(White,Blue),
      [COLOR_PROCESS_MEGABYTES] = ColorPair(Blue,Black),
      [COLOR_PROCESS_BASENAME] = ColorPair(Green,Black),
      [COLOR_PROCESS_TREE] = ColorPair(Blue,Black),
      [COLOR_PROCESS_R_STATE] = ColorPair(Green,Black),
      [COLOR_PROCESS_D_STATE] = A_BOLD | ColorPair(Red,Black),
      [COLOR_PROCESS_HIGH_PRIORITY] = ColorPair(Red,Black),
      [COLOR_PROCESS_LOW_PRIORITY] = ColorPair(Green,Black),
      [COLOR_PROCESS_THREAD] = ColorPair(Blue,Black),
      [COLOR_PROCESS_THREAD_BASENAME] = A_BOLD | ColorPair(Blue,Black),
      [COLOR_BAR_BORDER] = ColorPair(Blue,Black),
      [COLOR_BAR_SHADOW] = ColorPairGrayBlack,
      [COLOR_SWAP] = ColorPair(Red,Black),
      [COLOR_GRAPH_1] = A_BOLD | ColorPair(Cyan,Black),
      [COLOR_GRAPH_2] = ColorPair(Cyan,Black),
      [COLOR_MEMORY_USED] = ColorPair(Green,Black),
      [COLOR_MEMORY_BUFFERS] = ColorPair(Cyan,Black),
      [COLOR_MEMORY_BUFFERS_TEXT] = ColorPair(Cyan,Black),
      [COLOR_MEMORY_CACHE] = ColorPair(Yellow,Black),
      [COLOR_LOAD_AVERAGE_FIFTEEN] = ColorPair(Black,Black),
      [COLOR_LOAD_AVERAGE_FIVE] = ColorPair(Black,Black),
      [COLOR_LOAD_AVERAGE_ONE] = ColorPair(Black,Black),
      [COLOR_LOAD] = ColorPair(White,Black),
      [COLOR_HELP_BOLD] = ColorPair(Blue,Black),
      [COLOR_CLOCK] = ColorPair(White,Black),
      [COLOR_CHECK_BOX] = ColorPair(Blue,Black),
      [COLOR_CHECK_MARK] = ColorPair(Black,Black),
      [COLOR_CHECK_TEXT] = ColorPair(Black,Black),
      [COLOR_HOSTNAME] = ColorPair(White,Black),
      [COLOR_CPU_NICE] = ColorPair(Cyan,Black),
      [COLOR_CPU_NICE_TEXT] = ColorPair(Cyan,Black),
      [COLOR_CPU_NORMAL] = ColorPair(Green,Black),
      [COLOR_CPU_KERNEL] = ColorPair(Red,Black),
      [COLOR_CPU_IOWAIT] = A_BOLD | ColorPair(Black, Black),
      [COLOR_CPU_IRQ] = A_BOLD | ColorPair(Blue,Black),
      [COLOR_CPU_SOFTIRQ] = ColorPair(Blue,Black),
      [COLOR_CPU_STEAL] = ColorPair(Black,Black),
      [COLOR_CPU_GUEST] = ColorPair(Black,Black),
   },
   [COLORSCHEME_MIDNIGHT] = {
      [COLOR_RESET_COLOR] = ColorPair(White,Blue),
      [COLOR_DEFAULT_COLOR] = ColorPair(White,Blue),
      [COLOR_FUNCTION_BAR] = ColorPair(Black,Cyan),
      [COLOR_FUNCTION_KEY] = A_NORMAL,
      [COLOR_PANEL_HEADER_FOCUS] = ColorPair(Black,Cyan),
      [COLOR_PANEL_HEADER_UNFOCUS] = ColorPair(Black,Cyan),
      [COLOR_PANEL_SELECTION_FOCUS] = ColorPair(Black,White),
      [COLOR_PANEL_SELECTION_FOLLOW] = ColorPair(Black,Yellow),
      [COLOR_PANEL_SELECTION_UNFOCUS] = A_BOLD | ColorPair(Yellow,Blue),
      [COLOR_FAILED_SEARCH] = ColorPair(Red,Cyan),
      [COLOR_UPTIME] = A_BOLD | ColorPair(Yellow,Blue),
      [COLOR_BATTERY] = A_BOLD | ColorPair(Yellow,Blue),
      [COLOR_LARGE_NUMBER] = A_BOLD | ColorPair(Red,Blue),
      [COLOR_METER_TEXT] = ColorPair(Cyan,Blue),
      [COLOR_METER_VALUE] = A_BOLD | ColorPair(Cyan,Blue),
      [COLOR_LED_COLOR] = ColorPair(Green,Blue),
      [COLOR_TASKS_RUNNING] = A_BOLD | ColorPair(Green,Blue),
      [COLOR_PROCESS] = ColorPair(White,Blue),
      [COLOR_PROCESS_SHADOW] = A_BOLD | ColorPair(Black,Blue),
      [COLOR_PROCESS_TAG] = A_BOLD | ColorPair(Yellow,Blue),
      [COLOR_PROCESS_MEGABYTES] = ColorPair(Cyan,Blue),
      [COLOR_PROCESS_BASENAME] = A_BOLD | ColorPair(Cyan,Blue),
      [COLOR_PROCESS_TREE] = ColorPair(Cyan,Blue),
      [COLOR_PROCESS_R_STATE] = ColorPair(Green,Blue),
      [COLOR_PROCESS_D_STATE] = A_BOLD | ColorPair(Red,Blue),
      [COLOR_PROCESS_HIGH_PRIORITY] = ColorPair(Red,Blue),
      [COLOR_PROCESS_LOW_PRIORITY] = ColorPair(Green,Blue),
      [COLOR_PROCESS_THREAD] = ColorPair(Green,Blue),
      [COLOR_PROCESS_THREAD_BASENAME] = A_BOLD | ColorPair(Green,Blue),
      [COLOR_BAR_BORDER] = A_BOLD | ColorPair(Yellow,Blue),
      [COLOR_BAR_SHADOW] = ColorPair(Cyan,Blue),
      [COLOR_SWAP] = ColorPair(Red,Blue),
      [COLOR_GRAPH_1] = A_BOLD | ColorPair(Cyan,Blue),
      [COLOR_GRAPH_2] = ColorPair(Cyan,Blue),
      [COLOR_MEMORY_USED] = A_BOLD | ColorPair(Green,Blue),
      [COLOR_MEMORY_BUFFERS] = A_BOLD | ColorPair(Cyan,Blue),
      [COLOR_MEMORY_BUFFERS_TEXT] = A_BOLD | ColorPair(Cyan,Blue),
      [COLOR_MEMORY_CACHE] = A_BOLD | ColorPair(Yellow,Blue),
      [COLOR_LOAD_AVERAGE_FIFTEEN] = A_BOLD | ColorPair(Black,Blue),
      [COLOR_LOAD_AVERAGE_FIVE] = A_NORMAL | ColorPair(White,Blue),
      [COLOR_LOAD_AVERAGE_ONE] = A_BOLD | ColorPair(White,Blue),
      [COLOR_LOAD] = A_BOLD | ColorPair(White,Blue),
      [COLOR_HELP_BOLD] = A_BOLD | ColorPair(Cyan,Blue),
      [COLOR_CLOCK] = ColorPair(White,Blue),
      [COLOR_CHECK_BOX] = ColorPair(Cyan,Blue),
      [COLOR_CHECK_MARK] = A_BOLD | ColorPair(White,Blue),
      [COLOR_CHECK_TEXT] = A_NORMAL | ColorPair(White,Blue),
      [COLOR_HOSTNAME] = ColorPair(White,Blue),
      [COLOR_CPU_NICE] = A_BOLD | ColorPair(Cyan,Blue),
      [COLOR_CPU_NICE_TEXT] = A_BOLD | ColorPair(Cyan,Blue),
      [COLOR_CPU_NORMAL] = A_BOLD | ColorPair(Green,Blue),
      [COLOR_CPU_KERNEL] = A_BOLD | ColorPair(Red,Blue),
      [COLOR_CPU_IOWAIT] = A_BOLD | ColorPair(Blue,Blue),
      [COLOR_CPU_IRQ] = A_BOLD | ColorPair(Black,Blue),
      [COLOR_CPU_SOFTIRQ] = ColorPair(Black,Blue),
      [COLOR_CPU_STEAL] = ColorPair(White,Blue),
      [COLOR_CPU_GUEST] = ColorPair(White,Blue),
   },
   [COLORSCHEME_BLACKNIGHT] = {
      [COLOR_RESET_COLOR] = ColorPair(Cyan,Black),
      [COLOR_DEFAULT_COLOR] = ColorPair(Cyan,Black),
      [COLOR_FUNCTION_BAR] = ColorPair(Black,Green),
      [COLOR_FUNCTION_KEY] = ColorPair(Cyan,Black),
      [COLOR_PANEL_HEADER_FOCUS] = ColorPair(Black,Green),
      [COLOR_PANEL_HEADER_UNFOCUS] = ColorPair(Black,Green),
      [COLOR_PANEL_SELECTION_FOCUS] = ColorPair(Black,Cyan),
      [COLOR_PANEL_SELECTION_FOLLOW] = ColorPair(Black,Yellow),
      [COLOR_PANEL_SELECTION_UNFOCUS] = ColorPair(Black,White),
      [COLOR_FAILED_SEARCH] = ColorPair(Red,Cyan),
      [COLOR_UPTIME] = ColorPair(Green,Black),
      [COLOR_BATTERY] = ColorPair(Green,Black),
      [COLOR_LARGE_NUMBER] = A_BOLD | ColorPair(Red,Black),
      [COLOR_METER_TEXT] = ColorPair(Cyan,Black),
      [COLOR_METER_VALUE] = ColorPair(Green,Black),
      [COLOR_LED_COLOR] = ColorPair(Green,Black),
      [COLOR_TASKS_RUNNING] = A_BOLD | ColorPair(Green,Black),
      [COLOR_PROCESS] = ColorPair(Cyan,Black),
      [COLOR_PROCESS_SHADOW] = A_BOLD | ColorPairGrayBlack,
      [COLOR_PROCESS_TAG] = A_BOLD | ColorPair(Yellow,Black),
      [COLOR_PROCESS_MEGABYTES] = A_BOLD | ColorPair(Green,Black),
      [COLOR_PROCESS_BASENAME] = A_BOLD | ColorPair(Green,Black),
      [COLOR_PROCESS_TREE] = ColorPair(Cyan,Black),
      [COLOR_PROCESS_THREAD] = ColorPair(Green,Black),
      [COLOR_PROCESS_THREAD_BASENAME] = A_BOLD | ColorPair(Blue,Black),
      [COLOR_PROCESS_R_STATE] = ColorPair(Green,Black),
      [COLOR_PROCESS_D_STATE] = A_BOLD | ColorPair(Red,Black),
      [COLOR_PROCESS_HIGH_PRIORITY] = ColorPair(Red,Black),
      [COLOR_PROCESS_LOW_PRIORITY] = ColorPair(Green,Black),
      [COLOR_BAR_BORDER] = A_BOLD | ColorPair(Green,Black),
      [COLOR_BAR_SHADOW] = ColorPair(Cyan,Black),
      [COLOR_SWAP] = ColorPair(Red,Black),
      [COLOR_GRAPH_1] = A_BOLD | ColorPair(Green,Black),
      [COLOR_GRAPH_2] = ColorPair(Green,Black),
      [COLOR_MEMORY_USED] = ColorPair(Green,Black),
      [COLOR_MEMORY_BUFFERS] = ColorPair(Blue,Black),
      [COLOR_MEMORY_BUFFERS_TEXT] = A_BOLD | ColorPair(Blue,Black),
      [COLOR_MEMORY_CACHE] = ColorPair(Yellow,Black),
      [COLOR_LOAD_AVERAGE_FIFTEEN] = ColorPair(Green,Black),
      [COLOR_LOAD_AVERAGE_FIVE] = ColorPair(Green,Black),
      [COLOR_LOAD_AVERAGE_ONE] = A_BOLD | ColorPair(Green,Black),
      [COLOR_LOAD] = A_BOLD,
      [COLOR_HELP_BOLD] = A_BOLD | ColorPair(Cyan,Black),
      [COLOR_CLOCK] = ColorPair(Green,Black),
      [COLOR_CHECK_BOX] = ColorPair(Green,Black),
      [COLOR_CHECK_MARK] = A_BOLD | ColorPair(Green,Black),
      [COLOR_CHECK_TEXT] = ColorPair(Cyan,Black),
      [COLOR_HOSTNAME] = ColorPair(Green,Black),
      [COLOR_CPU_NICE] = ColorPair(Blue,Black),
      [COLOR_CPU_NICE_TEXT] = A_BOLD | ColorPair(Blue,Black),
      [COLOR_CPU_NORMAL] = ColorPair(Green,Black),
      [COLOR_CPU_KERNEL] = ColorPair(Red,Black),
      [COLOR_CPU_IOWAIT] = ColorPair(Yellow,Black),
      [COLOR_CPU_IRQ] = A_BOLD | ColorPair(Blue,Black),
      [COLOR_CPU_SOFTIRQ] = ColorPair(Blue,Black),
      [COLOR_CPU_STEAL] = ColorPair(Cyan,Black),
      [COLOR_CPU_GUEST] = ColorPair(Cyan,Black),
   },
   [COLORSCHEME_BROKENGRAY] = { 0 } // dynamically generated.
};

int CRT_cursorX = 0;

int CRT_scrollHAmount = 5;

int CRT_scrollWheelVAmount = 10;

char* CRT_termType;

// TODO move color scheme to Settings, perhaps?

int CRT_colorScheme = 0;

void *backtraceArray[128];

static void CRT_handleSIGTERM(int sgn) {
   (void) sgn;
   CRT_done();
   exit(0);
}

#if HAVE_SETUID_ENABLED

static int CRT_euid = -1;

static int CRT_egid = -1;

#define DIE(msg) do { CRT_done(); fprintf(stderr, msg); exit(1); } while(0)

void CRT_dropPrivileges() {
   CRT_egid = getegid();
   CRT_euid = geteuid();
   if (setegid(getgid()) == -1) {
      DIE("Fatal error: failed dropping group privileges.\n");
   }
   if (seteuid(getuid()) == -1) {
      DIE("Fatal error: failed dropping user privileges.\n");
   }
}

void CRT_restorePrivileges() {
   if (CRT_egid == -1 || CRT_euid == -1) {
      DIE("Fatal error: internal inconsistency.\n");
   }
   if (setegid(CRT_egid) == -1) {
      DIE("Fatal error: failed restoring group privileges.\n");
   }
   if (seteuid(CRT_euid) == -1) {
      DIE("Fatal error: failed restoring user privileges.\n");
   }
}

#else

/* Turn setuid operations into NOPs */

#ifndef CRT_dropPrivileges
#define CRT_dropPrivileges()
#define CRT_restorePrivileges()
#endif

#endif

// TODO: pass an instance of Settings instead.

void CRT_init(int delay, int colorScheme) {
   initscr();
   noecho();
   CRT_delay = delay;
   if (CRT_delay == 0) {
      CRT_delay = 1;
   }
   CRT_colors = CRT_colorSchemes[colorScheme];
   CRT_colorScheme = colorScheme;
   
   for (int i = 0; i < LAST_COLORELEMENT; i++) {
      unsigned int color = CRT_colorSchemes[COLORSCHEME_DEFAULT][i];
      CRT_colorSchemes[COLORSCHEME_BROKENGRAY][i] = color == (A_BOLD | ColorPairGrayBlack) ? ColorPair(White,Black) : color;
   }
   
   halfdelay(CRT_delay);
   nonl();
   intrflush(stdscr, false);
   keypad(stdscr, true);
   mouseinterval(0);
   curs_set(0);
   if (has_colors()) {
      start_color();
      CRT_hasColors = true;
   } else {
      CRT_hasColors = false;
   }
   CRT_termType = getenv("TERM");
   if (String_eq(CRT_termType, "linux"))
      CRT_scrollHAmount = 20;
   else
      CRT_scrollHAmount = 5;
   if (String_startsWith(CRT_termType, "xterm") || String_eq(CRT_termType, "vt220")) {
      define_key("\033[H", KEY_HOME);
      define_key("\033[F", KEY_END);
      define_key("\033[7~", KEY_HOME);
      define_key("\033[8~", KEY_END);
      define_key("\033OP", KEY_F(1));
      define_key("\033OQ", KEY_F(2));
      define_key("\033OR", KEY_F(3));
      define_key("\033OS", KEY_F(4));
      define_key("\033[11~", KEY_F(1));
      define_key("\033[12~", KEY_F(2));
      define_key("\033[13~", KEY_F(3));
      define_key("\033[14~", KEY_F(4));
      define_key("\033[17;2~", KEY_F(18));
      char sequence[3] = "\033a";
      for (char c = 'a'; c <= 'z'; c++) {
         sequence[1] = c;
         define_key(sequence, KEY_ALT('A' + (c - 'a')));
      }
   }
#ifndef DEBUG
   signal(11, CRT_handleSIGSEGV);
#endif
   signal(SIGTERM, CRT_handleSIGTERM);
   signal(SIGQUIT, CRT_handleSIGTERM);
   use_default_colors();
   if (!has_colors())
      CRT_colorScheme = 1;
   CRT_setColors(CRT_colorScheme);

   /* initialize locale */
   setlocale(LC_CTYPE, "");

#ifdef HAVE_LIBNCURSESW
   if(strcmp(nl_langinfo(CODESET), "UTF-8") == 0)
      CRT_utf8 = true;
   else
      CRT_utf8 = false;
#endif

   CRT_treeStr =
#ifdef HAVE_LIBNCURSESW
      CRT_utf8 ? CRT_treeStrUtf8 :
#endif
      CRT_treeStrAscii;

#if NCURSES_MOUSE_VERSION > 1
   mousemask(BUTTON1_RELEASED | BUTTON4_PRESSED | BUTTON5_PRESSED, NULL);
#else
   mousemask(BUTTON1_RELEASED, NULL);
#endif

}

void CRT_done() {
   curs_set(1);
   endwin();
}

void CRT_fatalError(const char* note) {
   char* sysMsg = strerror(errno);
   CRT_done();
   fprintf(stderr, "%s: %s\n", note, sysMsg);
   exit(2);
}

int CRT_readKey() {
   nocbreak();
   cbreak();
   nodelay(stdscr, FALSE);
   int ret = getch();
   halfdelay(CRT_delay);
   return ret;
}

void CRT_disableDelay() {
   nocbreak();
   cbreak();
   nodelay(stdscr, TRUE);
}

void CRT_enableDelay() {
   halfdelay(CRT_delay);
}

void CRT_setColors(int colorScheme) {
   CRT_colorScheme = colorScheme;

   for (int i = 0; i < 8; i++) {
      for (int j = 0; j < 8; j++) {
         if (ColorIndex(i,j) != ColorPairGrayBlack) {
            int bg = (colorScheme != COLORSCHEME_BLACKNIGHT)
                     ? (j==0 ? -1 : j)
                     : j;
            init_pair(ColorIndex(i,j), i, bg);
         }
      }
   }

   int grayBlackFg = COLORS > 8 ? 8 : 0;
   int grayBlackBg = (colorScheme != COLORSCHEME_BLACKNIGHT)
                     ? -1
                     : 0;
   init_pair(ColorIndexGrayBlack, grayBlackFg, grayBlackBg);

   CRT_colors = CRT_colorSchemes[colorScheme];
}
