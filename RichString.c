/*
htop - RichString.c
(C) 2004,2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "RichString.h"

#include <stdlib.h>
#include <string.h>

#define RICHSTRING_MAXLEN 300

/*{
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

#define RichString_size(htop_this) ((htop_this)->chlen)
#define RichString_sizeVal(htop_this) ((htop_this).chlen)

#define RichString_begin(htop_this) RichString (htop_this); (htop_this).chlen = 0; (htop_this).chptr = (htop_this).chstr;
#define RichString_beginAllocated(htop_this) (htop_this).chlen = 0; (htop_this).chptr = (htop_this).chstr;
#define RichString_end(htop_this) RichString_prune(&(htop_this));

#ifdef HAVE_LIBNCURSESW
#define RichString_printVal(htop_this, y, x) mvadd_wchstr(y, x, (htop_this).chptr)
#define RichString_printoffnVal(htop_this, y, x, off, n) mvadd_wchnstr(y, x, (htop_this).chptr + off, n)
#define RichString_getCharVal(htop_this, i) ((htop_this).chptr[i].chars[0] & 255)
#define RichString_setChar(htop_this, at, ch) do{ (htop_this)->chptr[(at)].chars[0] = ch; } while(0)
#define CharType cchar_t
#else
#define RichString_printVal(htop_this, y, x) mvaddchstr(y, x, (htop_this).chptr)
#define RichString_printoffnVal(htop_this, y, x, off, n) mvaddchnstr(y, x, (htop_this).chptr + off, n)
#define RichString_getCharVal(htop_this, i) ((htop_this).chptr[i])
#define RichString_setChar(htop_this, at, ch) do{ (htop_this)->chptr[(at)] = ch; } while(0)
#define CharType chtype
#endif

typedef struct RichString_ {
   int chlen;
   CharType chstr[RICHSTRING_MAXLEN+1];
   CharType* chptr;
} RichString;

}*/

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#define charBytes(n) (sizeof(CharType) * (n)) 

static inline void RichString_setLen(RichString* htop_this, int len) {
   if (htop_this->chlen <= RICHSTRING_MAXLEN) {
      if (len > RICHSTRING_MAXLEN) {
         htop_this->chptr = malloc(charBytes(len+1));
         memcpy(htop_this->chptr, htop_this->chstr, charBytes(htop_this->chlen+1));
      }
   } else {
      if (len <= RICHSTRING_MAXLEN) {
         memcpy(htop_this->chstr, htop_this->chptr, charBytes(htop_this->chlen));
         free(htop_this->chptr);
         htop_this->chptr = htop_this->chstr;
      } else {
         htop_this->chptr = realloc(htop_this->chptr, charBytes(len+1));
      }
   }
   RichString_setChar(htop_this, len, 0);
   htop_this->chlen = len;
}

#ifdef HAVE_LIBNCURSESW

static inline void RichString_writeFrom(RichString* htop_this, int attrs, const char* data_c, int from, int len) {
   wchar_t data[len+1];
   len = mbstowcs(data, data_c, len);
   if (len<0)
      return;
   int newLen = from + len;
   RichString_setLen(htop_this, newLen);
   memset(&htop_this->chptr[from], 0, sizeof(CharType) * (newLen - from));
   for (int i = from, j = 0; i < newLen; i++, j++) {
      htop_this->chptr[i].chars[0] = data[j];
      htop_this->chptr[i].attr = attrs;
   }
   htop_this->chptr[newLen].chars[0] = 0;
}

inline void RichString_setAttrn(RichString* htop_this, int attrs, int start, int finish) {
   cchar_t* ch = htop_this->chptr + start;
   for (int i = start; i <= finish; i++) {
      ch->attr = attrs;
      ch++;
   }
}

int RichString_findChar(RichString* htop_this, char c, int start) {
   wchar_t wc = btowc(c);
   cchar_t* ch = htop_this->chptr + start;
   for (int i = start; i < htop_this->chlen; i++) {
      if (ch->chars[0] == wc)
         return i;
      ch++;
   }
   return -1;
}

#else

static inline void RichString_writeFrom(RichString* htop_this, int attrs, const char* data_c, int from, int len) {
   int newLen = from + len;
   RichString_setLen(htop_this, newLen);
   for (int i = from, j = 0; i < newLen; i++, j++)
      htop_this->chptr[i] = (isprint(data_c[j]) ? data_c[j] : '?') | attrs;
   htop_this->chptr[newLen] = 0;
}

void RichString_setAttrn(RichString* htop_this, int attrs, int start, int finish) {
   chtype* ch = htop_this->chptr + start;
   for (int i = start; i <= finish; i++) {
      *ch = (*ch & 0xff) | attrs;
      ch++;
   }
}

int RichString_findChar(RichString* htop_this, char c, int start) {
   chtype* ch = htop_this->chptr + start;
   for (int i = start; i < htop_this->chlen; i++) {
      if ((*ch & 0xff) == (chtype) c)
         return i;
      ch++;
   }
   return -1;
}

#endif

void RichString_prune(RichString* htop_this) {
   if (htop_this->chlen > RICHSTRING_MAXLEN)
      free(htop_this->chptr);
   htop_this->chptr = htop_this->chstr;
   htop_this->chlen = 0;
}

void RichString_setAttr(RichString* htop_this, int attrs) {
   RichString_setAttrn(htop_this, attrs, 0, htop_this->chlen - 1);
}

void RichString_append(RichString* htop_this, int attrs, const char* data) {
   RichString_writeFrom(htop_this, attrs, data, htop_this->chlen, strlen(data));
}

void RichString_appendn(RichString* htop_this, int attrs, const char* data, int len) {
   RichString_writeFrom(htop_this, attrs, data, htop_this->chlen, len);
}

void RichString_write(RichString* htop_this, int attrs, const char* data) {
   RichString_writeFrom(htop_this, attrs, data, 0, strlen(data));
}
