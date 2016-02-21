#include "InfoScreen.h"

#include "config.h"
#include "Object.h"
#include "CRT.h"
#include "IncSet.h"
#include "ListItem.h"
#include "Platform.h"
#include "StringUtils.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

/*{
#include "Process.h"
#include "Panel.h"
#include "FunctionBar.h"
#include "IncSet.h"

typedef struct InfoScreen_ InfoScreen;

typedef void(*InfoScreen_Scan)(InfoScreen*);
typedef void(*InfoScreen_Draw)(InfoScreen*);
typedef void(*InfoScreen_OnErr)(InfoScreen*);
typedef bool(*InfoScreen_OnKey)(InfoScreen*, int);

typedef struct InfoScreenClass_ {
   ObjectClass super;
   const InfoScreen_Scan scan;
   const InfoScreen_Draw draw;
   const InfoScreen_OnErr onErr;
   const InfoScreen_OnKey onKey;
} InfoScreenClass;

#define As_InfoScreen(this_)          ((InfoScreenClass*)(((InfoScreen*)(this_))->super.klass))
#define InfoScreen_scan(this_)        As_InfoScreen(this_)->scan((InfoScreen*)(this_))
#define InfoScreen_draw(this_)        As_InfoScreen(this_)->draw((InfoScreen*)(this_))
#define InfoScreen_onErr(this_)       As_InfoScreen(this_)->onErr((InfoScreen*)(this_))
#define InfoScreen_onKey(this_, ch_)  As_InfoScreen(this_)->onKey((InfoScreen*)(this_), ch_)

struct InfoScreen_ {
   Object super;
   Process* process;
   Panel* display;
   FunctionBar* bar;
   IncSet* inc;
   Vector* lines;
};
}*/

static const char* InfoScreenFunctions[] = {"Search ", "Filter ", "Refresh", "Done   ", NULL};

static const char* InfoScreenKeys[] = {"F3", "F4", "F5", "Esc"};

static int InfoScreenEvents[] = {KEY_F(3), KEY_F(4), KEY_F(5), 27};

InfoScreen* InfoScreen_init(InfoScreen* this, Process* process, FunctionBar* bar, int height, char* panelHeader) {
   this->process = process;
   if (!bar) {
      bar = FunctionBar_new(InfoScreenFunctions, InfoScreenKeys, InfoScreenEvents);
   }
   this->display = Panel_new(0, 1, COLS, height, false, Class(ListItem), bar);
   this->inc = IncSet_new(bar);
   this->lines = Vector_new(this->display->items->type, true, DEFAULT_SIZE);
   Panel_setHeader(this->display, panelHeader);
   return this;
}

InfoScreen* InfoScreen_done(InfoScreen* this) {
   Panel_delete((Object*)this->display);
   IncSet_delete(this->inc);
   Vector_delete(this->lines);
   return this;
}

void InfoScreen_drawTitled(InfoScreen* this, char* fmt, ...) {
   va_list ap;
   va_start(ap, fmt);
   attrset(CRT_colors[METER_TEXT]);
   mvhline(0, 0, ' ', COLS);
   wmove(stdscr, 0, 0);
   vw_printw(stdscr, fmt, ap);
   attrset(CRT_colors[DEFAULT_COLOR]);
   Panel_draw(this->display, true);
   IncSet_drawBar(this->inc);
   va_end(ap);
}

void InfoScreen_addLine(InfoScreen* this, const char* line) {
   Vector_add(this->lines, (Object*) ListItem_new(line, 0));
   const char* incFilter = IncSet_filter(this->inc);
   if (!incFilter || String_contains_i(line, incFilter))
      Panel_add(this->display, (Object*)Vector_get(this->lines, Vector_size(this->lines)-1));
}

void InfoScreen_appendLine(InfoScreen* this, const char* line) {
   ListItem* last = (ListItem*)Vector_get(this->lines, Vector_size(this->lines)-1);
   ListItem_append(last, line);
   const char* incFilter = IncSet_filter(this->inc);
   if (incFilter && Panel_get(this->display, Panel_size(this->display)-1) != (Object*)last && String_contains_i(line, incFilter))
      Panel_add(this->display, (Object*)last);
}

void InfoScreen_run(InfoScreen* this) {
   Panel* panel = this->display;

   if (As_InfoScreen(this)->scan) InfoScreen_scan(this);
   InfoScreen_draw(this);

   bool looping = true;
   while (looping) {

      Panel_draw(panel, true);

      if (this->inc->active)
         move(LINES-1, CRT_cursorX);
      ESCDELAY = 25;
      int ch = getch();
      
      if (ch == ERR) {
         if (As_InfoScreen(this)->onErr) {
            InfoScreen_onErr(this);
            continue;
         }
      }

      if (ch == KEY_MOUSE) {
         MEVENT mevent;
         int ok = getmouse(&mevent);
         if (ok == OK)
            if (mevent.y >= panel->y && mevent.y < LINES - 1) {
               Panel_setSelected(panel, mevent.y - panel->y + panel->scrollV);
               ch = 0;
            } if (mevent.y == LINES - 1)
               ch = IncSet_synthesizeEvent(this->inc, mevent.x);
      }

      if (this->inc->active) {
         IncSet_handleKey(this->inc, ch, panel, IncSet_getListItemValue, this->lines);
         continue;
      }
      
      switch(ch) {
      case ERR:
         continue;
      case KEY_F(3):
      case '/':
         IncSet_activate(this->inc, INC_SEARCH, panel);
         break;
      case KEY_F(4):
      case '\\':
         IncSet_activate(this->inc, INC_FILTER, panel);
         break;
      case KEY_F(5):
         clear();
         if (As_InfoScreen(this)->scan) InfoScreen_scan(this);
         InfoScreen_draw(this);
         break;
      case '\014': // Ctrl+L
         clear();
         InfoScreen_draw(this);
         break;
      case 'q':
      case 27:
      case KEY_F(10):
         looping = false;
         break;
      case KEY_RESIZE:
         Panel_resize(panel, COLS, LINES-2);
         InfoScreen_draw(this);
         break;
      default:
         if (As_InfoScreen(this)->onKey && InfoScreen_onKey(this, ch)) {
            continue;
         }
         Panel_onKey(panel, ch);
      }
   }
}
