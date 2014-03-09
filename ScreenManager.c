/*
htop - ScreenManager.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ScreenManager.h"

#include "Panel.h"
#include "Object.h"

#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include <stdbool.h>

/*{
#include "FunctionBar.h"
#include "Vector.h"
#include "Header.h"

typedef enum Orientation_ {
   VERTICAL,
   HORIZONTAL
} Orientation;

typedef struct ScreenManager_ {
   int x1;
   int y1;
   int x2;
   int y2;
   Orientation orientation;
   Vector* panels;
   Vector* fuBars;
   int panelCount;
   const FunctionBar* fuBar;
   const Header* header;
   time_t lastScan;
   bool owner;
   bool allowFocusChange;
} ScreenManager;

}*/

ScreenManager* ScreenManager_new(int x1, int y1, int x2, int y2, Orientation orientation, const Header* header, bool owner) {
   ScreenManager* htop_this;
   htop_this = malloc(sizeof(ScreenManager));
   htop_this->x1 = x1;
   htop_this->y1 = y1;
   htop_this->x2 = x2;
   htop_this->y2 = y2;
   htop_this->fuBar = NULL;
   htop_this->orientation = orientation;
   htop_this->panels = Vector_new(Class(Panel), owner, DEFAULT_SIZE);
   htop_this->fuBars = Vector_new(Class(FunctionBar), true, DEFAULT_SIZE);
   htop_this->panelCount = 0;
   htop_this->header = header;
   htop_this->owner = owner;
   htop_this->allowFocusChange = true;
   return htop_this;
}

void ScreenManager_delete(ScreenManager* htop_this) {
   Vector_delete(htop_this->panels);
   Vector_delete(htop_this->fuBars);
   free(htop_this);
}

inline int ScreenManager_size(ScreenManager* htop_this) {
   return htop_this->panelCount;
}

void ScreenManager_add(ScreenManager* htop_this, Panel* item, FunctionBar* fuBar, int size) {
   if (htop_this->orientation == HORIZONTAL) {
      int lastX = 0;
      if (htop_this->panelCount > 0) {
         Panel* last = (Panel*) Vector_get(htop_this->panels, htop_this->panelCount - 1);
         lastX = last->x + last->w + 1;
      }
      if (size > 0) {
         Panel_resize(item, size, LINES-htop_this->y1+htop_this->y2);
      } else {
         Panel_resize(item, COLS-htop_this->x1+htop_this->x2-lastX, LINES-htop_this->y1+htop_this->y2);
      }
      Panel_move(item, lastX, htop_this->y1);
   }
   // TODO: VERTICAL
   Vector_add(htop_this->panels, item);
   if (fuBar)
      Vector_add(htop_this->fuBars, fuBar);
   else
      Vector_add(htop_this->fuBars, FunctionBar_new(NULL, NULL, NULL));
   if (!htop_this->fuBar && fuBar) htop_this->fuBar = fuBar;
   item->needsRedraw = true;
   htop_this->panelCount++;
}

Panel* ScreenManager_remove(ScreenManager* htop_this, int idx) {
   assert(htop_this->panelCount > idx);
   Panel* panel = (Panel*) Vector_remove(htop_this->panels, idx);
   Vector_remove(htop_this->fuBars, idx);
   htop_this->fuBar = NULL;
   htop_this->panelCount--;
   return panel;
}

void ScreenManager_resize(ScreenManager* htop_this, int x1, int y1, int x2, int y2) {
   htop_this->x1 = x1;
   htop_this->y1 = y1;
   htop_this->x2 = x2;
   htop_this->y2 = y2;
   int panels = htop_this->panelCount;
   int lastX = 0;
   for (int i = 0; i < panels - 1; i++) {
      Panel* panel = (Panel*) Vector_get(htop_this->panels, i);
      Panel_resize(panel, panel->w, LINES-y1+y2);
      Panel_move(panel, lastX, y1);
      lastX = panel->x + panel->w + 1;
   }
   Panel* panel = (Panel*) Vector_get(htop_this->panels, panels-1);
   Panel_resize(panel, COLS-x1+x2-lastX, LINES-y1+y2);
   Panel_move(panel, lastX, y1);
}

void ScreenManager_run(ScreenManager* htop_this, Panel** lastFocus, int* lastKey) {
   bool quit = false;
   int focus = 0;
   
   Panel* panelFocus = (Panel*) Vector_get(htop_this->panels, focus);
   if (htop_this->fuBar)
      FunctionBar_draw(htop_this->fuBar, NULL);
   
   htop_this->lastScan = 0;

   int ch = 0;
   while (!quit) {
      int panels = htop_this->panelCount;
      if (htop_this->header) {
         time_t now = time(NULL);
         if (now > htop_this->lastScan) {
            ProcessList_scan(htop_this->header->pl);
            ProcessList_sort(htop_this->header->pl);
            htop_this->lastScan = now;
         }
         Header_draw(htop_this->header);
         ProcessList_rebuildPanel(htop_this->header->pl, false, false, false, false, NULL);
      }
      for (int i = 0; i < panels; i++) {
         Panel* panel = (Panel*) Vector_get(htop_this->panels, i);
         Panel_draw(panel, i == focus);
         if (i < panels) {
            if (htop_this->orientation == HORIZONTAL) {
               mvvline(panel->y, panel->x+panel->w, ' ', panel->h+1);
            }
         }
      }
      FunctionBar* bar = (FunctionBar*) Vector_get(htop_this->fuBars, focus);
      if (bar)
         htop_this->fuBar = bar;
      if (htop_this->fuBar)
         FunctionBar_draw(htop_this->fuBar, NULL);

      ch = getch();
      
      if (ch == KEY_MOUSE) {
         MEVENT mevent;
         int ok = getmouse(&mevent);
         if (ok == OK) {
            if (mevent.y == LINES - 1) {
               ch = FunctionBar_synthesizeEvent(htop_this->fuBar, mevent.x);
            } else {
               for (int i = 0; i < htop_this->panelCount; i++) {
                  Panel* panel = (Panel*) Vector_get(htop_this->panels, i);
                  if (mevent.x > panel->x && mevent.x <= panel->x+panel->w &&
                     mevent.y > panel->y && mevent.y <= panel->y+panel->h &&
                     (htop_this->allowFocusChange || panelFocus == panel) ) {
                     focus = i;
                     panelFocus = panel;
                     Panel_setSelected(panel, mevent.y - panel->y + panel->scrollV - 1);
                     break;
                  }
               }
            }
         }
      }
      
      if (Panel_eventHandlerFn(panelFocus)) {
         HandlerResult result = Panel_eventHandler(panelFocus, ch);
         if (result == HANDLED) {
            continue;
         } else if (result == BREAK_LOOP) {
            quit = true;
            continue;
         }
      }
      
      switch (ch) {
      case ERR:
         continue;
      case KEY_RESIZE:
      {
         ScreenManager_resize(htop_this, htop_this->x1, htop_this->y1, htop_this->x2, htop_this->y2);
         continue;
      }
      case KEY_LEFT:
      case KEY_CTRLB:
         if (!htop_this->allowFocusChange)
            break;
         tryLeft:
         if (focus > 0)
            focus--;
         panelFocus = (Panel*) Vector_get(htop_this->panels, focus);
         if (Panel_size(panelFocus) == 0 && focus > 0)
            goto tryLeft;
         break;
      case KEY_RIGHT:
      case KEY_CTRLF:
      case 9:
         if (!htop_this->allowFocusChange)
            break;
         tryRight:
         if (focus < htop_this->panelCount - 1)
            focus++;
         panelFocus = (Panel*) Vector_get(htop_this->panels, focus);
         if (Panel_size(panelFocus) == 0 && focus < htop_this->panelCount - 1)
            goto tryRight;
         break;
      case KEY_F(10):
      case 'q':
      case 27:
         quit = true;
         continue;
      default:
         Panel_onKey(panelFocus, ch);
         break;
      }
   }

   *lastFocus = panelFocus;
   *lastKey = ch;
}
