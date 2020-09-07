/* Do not edit this file. It was automatically generated. */

#ifndef HEADER_RichString
#define HEADER_RichString
/*
htop - RichString.h
(C) 2004,2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#define RICHSTRING_MAXLEN 350

#include "config.h"
#include <ctype.h>

#include <assert.h>
#ifdef HAVE_NCURSESW_CURSES_H
#include <ncursesw/curses.h>
#elif HAVE_NCURSES_NCURSES_H
#include <ncurses/ncurses.h>
#elif HAVE_NCURSES_CURSES_H
#include <ncurses/curses.h>
#elif HAVE_NCURSES_H
#include <ncurses.h>
#elif HAVE_CURSES_H
#include <curses.h>
#endif

#ifdef HAVE_LIBNCURSESW
#include <wctype.h>
#endif

#define RichString_size(this) ((this)->chlen)
#define RichString_sizeVal(this) ((this).chlen)

#define RichString_begin(this) RichString (this); memset(&this, 0, sizeof(RichString)); (this).chptr = (this).chstr;
#define RichString_beginAllocated(this) memset(&this, 0, sizeof(RichString)); (this).chptr = (this).chstr;
#define RichString_end(this) RichString_prune(&(this));

#ifdef HAVE_LIBNCURSESW
#define RichString_printVal(this, y, x) mvadd_wchstr(y, x, (this).chptr)
#define RichString_printoffnVal(this, y, x, off, n) mvadd_wchnstr(y, x, (this).chptr + off, n)
#define RichString_getCharVal(this, i) ((this).chptr[i].chars[0] & 255)
#define RichString_setChar(this, at, ch) do{ (this)->chptr[(at)] = (CharType) { .chars = { ch, 0 } }; } while(0)
#define CharType cchar_t
#else
#define RichString_printVal(this, y, x) mvaddchstr(y, x, (this).chptr)
#define RichString_printoffnVal(this, y, x, off, n) mvaddchnstr(y, x, (this).chptr + off, n)
#define RichString_getCharVal(this, i) ((this).chptr[i])
#define RichString_setChar(this, at, ch) do{ (this)->chptr[(at)] = ch; } while(0)
#define CharType chtype
#endif

typedef struct RichString_ {
   int chlen;
   CharType* chptr;
   CharType chstr[RICHSTRING_MAXLEN+1];
} RichString;


#ifndef CLAMP
#define CLAMP(x,low,high) (((x)>(high))?(high):(((x)<(low))?(low):(x)))
#endif

#define charBytes(n) (sizeof(CharType) * (n))

#define RichString_setLen(this, len) do{ if(len < RICHSTRING_MAXLEN && this->chlen < RICHSTRING_MAXLEN) { RichString_setChar(this,len,0); this->chlen=len; } else RichString_extendLen(this,len); }while(0)

#ifdef HAVE_LIBNCURSESW

extern void RichString_setAttrn(RichString* this, int attrs, int start, int finish);

extern int RichString_findChar(RichString* this, char c, int start);

#else

extern void RichString_setAttrn(RichString* this, int attrs, int start, int finish);

extern int RichString_findChar(RichString* this, char c, int start);

#endif

extern void RichString_prune(RichString* this);

extern void RichString_setAttr(RichString* this, int attrs);

extern void RichString_append(RichString* this, int attrs, const char* data);

extern void RichString_appendn(RichString* this, int attrs, const char* data, int len);

extern void RichString_write(RichString* this, int attrs, const char* data);

#endif
