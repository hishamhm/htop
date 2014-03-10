/*
htop - Panel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Panel.h"

#include "CRT.h"
#include "RichString.h"
#include "ListItem.h"
#include "String.h"

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

//#link curses

/*{
#include "Object.h"
#include "Vector.h"

typedef struct Panel_ Panel;

typedef enum HandlerResult_ {
   HANDLED,
   IGNORED,
   BREAK_LOOP
} HandlerResult;

#define EVENT_SETSELECTED -1

typedef HandlerResult(*Panel_EventHandler)(Panel*, int);

typedef struct PanelClass_ {
   const ObjectClass super;
   const Panel_EventHandler eventHandler;
} PanelClass;

#define As_Panel(htop_this_)                ((PanelClass*)((htop_this_)->super.klass))
#define Panel_eventHandlerFn(htop_this_)    As_Panel(htop_this_)->eventHandler
#define Panel_eventHandler(htop_this_, ev_) As_Panel(htop_this_)->eventHandler((Panel*)(htop_this_), ev_)

struct Panel_ {
   Object super;
   PanelClass* class;
   int x, y, w, h;
   WINDOW* window;
   Vector* items;
   int selected;
   int scrollV, scrollH;
   int scrollHAmount;
   int oldSelected;
   bool needsRedraw;
   RichString header;
   char* eventHandlerBuffer;
};

}*/

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

#define KEY_CTRLN      0016            /* control-n key */
#define KEY_CTRLP      0020            /* control-p key */
#define KEY_CTRLF      0006            /* control-f key */
#define KEY_CTRLB      0002            /* control-b key */

PanelClass Panel_class = {
   .super = {
      .extends = Class(Object),
      .htop_delete = Panel_delete
   },
   .eventHandler = Panel_selectByTyping
};

Panel* Panel_new(int x, int y, int w, int h, bool owner, ObjectClass* type) {
   Panel* htop_this;
   htop_this = malloc(sizeof(Panel));
   Object_setClass(htop_this, Class(Panel));
   Panel_init(htop_this, x, y, w, h, type, owner);
   return htop_this;
}

void Panel_delete(Object* cast) {
   Panel* htop_this = (Panel*)cast;
   Panel_done(htop_this);
   free(htop_this);
}

void Panel_init(Panel* htop_this, int x, int y, int w, int h, ObjectClass* type, bool owner) {
   htop_this->x = x;
   htop_this->y = y;
   htop_this->w = w;
   htop_this->h = h;
   htop_this->eventHandlerBuffer = NULL;
   htop_this->items = Vector_new(type, owner, DEFAULT_SIZE);
   htop_this->scrollV = 0;
   htop_this->scrollH = 0;
   htop_this->selected = 0;
   htop_this->oldSelected = 0;
   htop_this->needsRedraw = true;
   RichString_beginAllocated(htop_this->header);
   if (String_eq(CRT_termType, "linux"))
      htop_this->scrollHAmount = 20;
   else
      htop_this->scrollHAmount = 5;
}

void Panel_done(Panel* htop_this) {
   assert (htop_this != NULL);
   free(htop_this->eventHandlerBuffer);
   Vector_delete(htop_this->items);
   RichString_end(htop_this->header);
}

RichString* Panel_getHeader(Panel* htop_this) {
   assert (htop_this != NULL);

   htop_this->needsRedraw = true;
   return &(htop_this->header);
}

inline void Panel_setHeader(Panel* htop_this, const char* header) {
   RichString_write(&(htop_this->header), CRT_colors[PANEL_HEADER_FOCUS], header);
   htop_this->needsRedraw = true;
}

void Panel_move(Panel* htop_this, int x, int y) {
   assert (htop_this != NULL);

   htop_this->x = x;
   htop_this->y = y;
   htop_this->needsRedraw = true;
}

void Panel_resize(Panel* htop_this, int w, int h) {
   assert (htop_this != NULL);

   if (RichString_sizeVal(htop_this->header) > 0)
      h--;
   htop_this->w = w;
   htop_this->h = h;
   htop_this->needsRedraw = true;
}

void Panel_prune(Panel* htop_this) {
   assert (htop_this != NULL);

   Vector_prune(htop_this->items);
   htop_this->scrollV = 0;
   htop_this->selected = 0;
   htop_this->oldSelected = 0;
   htop_this->needsRedraw = true;
}

void Panel_add(Panel* htop_this, Object* o) {
   assert (htop_this != NULL);

   Vector_add(htop_this->items, o);
   htop_this->needsRedraw = true;
}

void Panel_insert(Panel* htop_this, int i, Object* o) {
   assert (htop_this != NULL);

   Vector_insert(htop_this->items, i, o);
   htop_this->needsRedraw = true;
}

void Panel_set(Panel* htop_this, int i, Object* o) {
   assert (htop_this != NULL);

   Vector_set(htop_this->items, i, o);
}

Object* Panel_get(Panel* htop_this, int i) {
   assert (htop_this != NULL);

   return Vector_get(htop_this->items, i);
}

Object* Panel_remove(Panel* htop_this, int i) {
   assert (htop_this != NULL);

   htop_this->needsRedraw = true;
   Object* removed = Vector_remove(htop_this->items, i);
   if (htop_this->selected > 0 && htop_this->selected >= Vector_size(htop_this->items))
      htop_this->selected--;
   return removed;
}

Object* Panel_getSelected(Panel* htop_this) {
   assert (htop_this != NULL);
   if (Vector_size(htop_this->items) > 0)
      return Vector_get(htop_this->items, htop_this->selected);
   else
      return NULL;
}

void Panel_moveSelectedUp(Panel* htop_this) {
   assert (htop_this != NULL);

   Vector_moveUp(htop_this->items, htop_this->selected);
   if (htop_this->selected > 0)
      htop_this->selected--;
}

void Panel_moveSelectedDown(Panel* htop_this) {
   assert (htop_this != NULL);

   Vector_moveDown(htop_this->items, htop_this->selected);
   if (htop_this->selected + 1 < Vector_size(htop_this->items))
      htop_this->selected++;
}

int Panel_getSelectedIndex(Panel* htop_this) {
   assert (htop_this != NULL);

   return htop_this->selected;
}

int Panel_size(Panel* htop_this) {
   assert (htop_this != NULL);

   return Vector_size(htop_this->items);
}

void Panel_setSelected(Panel* htop_this, int selected) {
   assert (htop_this != NULL);

   selected = MAX(0, MIN(Vector_size(htop_this->items) - 1, selected));
   htop_this->selected = selected;
   if (Panel_eventHandlerFn(htop_this)) {
      Panel_eventHandler(htop_this, EVENT_SETSELECTED);
   }
}

void Panel_draw(Panel* htop_this, bool focus) {
   assert (htop_this != NULL);

   int itemCount = Vector_size(htop_this->items);
   int scrollH = htop_this->scrollH;
   int y = htop_this->y; int x = htop_this->x;
   int first = htop_this->scrollV;
   if (itemCount > htop_this->h && first > itemCount - htop_this->h) {
      first = itemCount - htop_this->h;
      htop_this->scrollV = first;
   }
   int last = MIN(itemCount, first + MIN(itemCount, htop_this->h));
   if (htop_this->selected < first) {
      first = htop_this->selected;
      htop_this->scrollV = first;
      htop_this->needsRedraw = true;
   }
   if (htop_this->selected >= last) {
      last = MIN(itemCount, htop_this->selected + 1);
      first = MAX(0, last - htop_this->h);
      htop_this->scrollV = first;
      htop_this->needsRedraw = true;
   }
   assert(first >= 0);
   assert(last <= itemCount);

   int headerLen = RichString_sizeVal(htop_this->header);
   if (headerLen > 0) {
      int attr = focus
               ? CRT_colors[PANEL_HEADER_FOCUS]
               : CRT_colors[PANEL_HEADER_UNFOCUS];
      attrset(attr);
      mvhline(y, x, ' ', htop_this->w);
      if (scrollH < headerLen) {
         RichString_printoffnVal(htop_this->header, y, x, scrollH,
            MIN(headerLen - scrollH, htop_this->w));
      }
      attrset(CRT_colors[RESET_COLOR]);
      y++;
   }
   
   int highlight = focus
                 ? CRT_colors[PANEL_HIGHLIGHT_FOCUS]
                 : CRT_colors[PANEL_HIGHLIGHT_UNFOCUS];

   if (htop_this->needsRedraw) {

      for(int i = first, j = 0; j < htop_this->h && i < last; i++, j++) {
         Object* itemObj = Vector_get(htop_this->items, i);
         assert(itemObj); if(!itemObj) continue;
         RichString_begin(item);
         Object_display(itemObj, &item);
         int itemLen = RichString_sizeVal(item);
         int amt = MIN(itemLen - scrollH, htop_this->w);
         if (i == htop_this->selected) {
            attrset(highlight);
            RichString_setAttr(&item, highlight);
            mvhline(y + j, x+0, ' ', htop_this->w);
            if (amt > 0)
               RichString_printoffnVal(item, y+j, x+0, scrollH, amt);
            attrset(CRT_colors[RESET_COLOR]);
         } else {
            mvhline(y+j, x+0, ' ', htop_this->w);
            if (amt > 0)
               RichString_printoffnVal(item, y+j, x+0, scrollH, amt);
         }
         RichString_end(item);
      }
      for (int i = y + (last - first); i < y + htop_this->h; i++)
         mvhline(i, x+0, ' ', htop_this->w);
      htop_this->needsRedraw = false;

   } else {
      Object* oldObj = Vector_get(htop_this->items, htop_this->oldSelected);
      assert(oldObj);
      RichString_begin(old);
      Object_display(oldObj, &old);
      int oldLen = RichString_sizeVal(old);
      Object* newObj = Vector_get(htop_this->items, htop_this->selected);
      RichString_begin(new);
      Object_display(newObj, &new);
      int newLen = RichString_sizeVal(new);
      mvhline(y+ htop_this->oldSelected - htop_this->scrollV, x+0, ' ', htop_this->w);
      if (scrollH < oldLen)
         RichString_printoffnVal(old, y+htop_this->oldSelected - htop_this->scrollV, x,
            htop_this->scrollH, MIN(oldLen - scrollH, htop_this->w));
      attrset(highlight);
      mvhline(y+htop_this->selected - htop_this->scrollV, x+0, ' ', htop_this->w);
      RichString_setAttr(&new, highlight);
      if (scrollH < newLen)
         RichString_printoffnVal(new, y+htop_this->selected - htop_this->scrollV, x,
            htop_this->scrollH, MIN(newLen - scrollH, htop_this->w));
      attrset(CRT_colors[RESET_COLOR]);
      RichString_end(new);
      RichString_end(old);
   }
   htop_this->oldSelected = htop_this->selected;
   move(0, 0);
}

bool Panel_onKey(Panel* htop_this, int key) {
   assert (htop_this != NULL);
   switch (key) {
   case KEY_DOWN:
   case KEY_CTRLN:
      if (htop_this->selected + 1 < Vector_size(htop_this->items))
         htop_this->selected++;
      return true;
   case KEY_UP:
   case KEY_CTRLP:
      if (htop_this->selected > 0)
         htop_this->selected--;
      return true;
   #ifdef KEY_C_DOWN
   case KEY_C_DOWN:
      if (htop_this->selected + 1 < Vector_size(htop_this->items)) {
         htop_this->selected++;
         if (htop_this->scrollV < Vector_size(htop_this->items) - htop_this->h) {
            htop_this->scrollV++;
            htop_this->needsRedraw = true;
         }
      }
      return true;
   #endif
   #ifdef KEY_C_UP
   case KEY_C_UP:
      if (htop_this->selected > 0) {
         htop_this->selected--;
         if (htop_this->scrollV > 0) {
            htop_this->scrollV--;
            htop_this->needsRedraw = true;
         }
      }
      return true;
   #endif
   case KEY_LEFT:
   case KEY_CTRLB:
      if (htop_this->scrollH > 0) {
         htop_this->scrollH -= 5;
         htop_this->needsRedraw = true;
      }
      return true;
   case KEY_RIGHT:
   case KEY_CTRLF:
      htop_this->scrollH += 5;
      htop_this->needsRedraw = true;
      return true;
   case KEY_PPAGE:
      htop_this->selected -= (htop_this->h - 1);
      htop_this->scrollV -= (htop_this->h - 1);
      if (htop_this->selected < 0)
         htop_this->selected = 0;
      if (htop_this->scrollV < 0)
         htop_this->scrollV = 0;
      htop_this->needsRedraw = true;
      return true;
   case KEY_NPAGE:
      htop_this->selected += (htop_this->h - 1);
      int size = Vector_size(htop_this->items);
      if (htop_this->selected >= size)
         htop_this->selected = size - 1;
      htop_this->scrollV += (htop_this->h - 1);
      if (htop_this->scrollV >= MAX(0, size - htop_this->h))
         htop_this->scrollV = MAX(0, size - htop_this->h - 1);
      htop_this->needsRedraw = true;
      return true;
   case KEY_HOME:
      htop_this->selected = 0;
      return true;
   case KEY_END:
      htop_this->selected = Vector_size(htop_this->items) - 1;
      return true;
   }
   return false;
}


HandlerResult Panel_selectByTyping(Panel* htop_this, int ch) {
   int size = Panel_size(htop_this);
   if (!htop_this->eventHandlerBuffer)
      htop_this->eventHandlerBuffer = calloc(100, 1);

   if (isalnum(ch)) {
      int len = strlen(htop_this->eventHandlerBuffer);
      if (len < 99) {
         htop_this->eventHandlerBuffer[len] = ch;
         htop_this->eventHandlerBuffer[len+1] = '\0';
      }
      for (int try = 0; try < 2; try++) {
         len = strlen(htop_this->eventHandlerBuffer);
         for (int i = 0; i < size; i++) {
            char* cur = ((ListItem*) Panel_get(htop_this, i))->value;
            while (*cur == ' ') cur++;
            if (strncasecmp(cur, htop_this->eventHandlerBuffer, len) == 0) {
               Panel_setSelected(htop_this, i);
               return HANDLED;
            }
         }
         htop_this->eventHandlerBuffer[0] = ch;
         htop_this->eventHandlerBuffer[1] = '\0';
      }
      return HANDLED;
   } else if (ch != ERR) {
      htop_this->eventHandlerBuffer[0] = '\0';
   }
   if (ch == 13) {
      return BREAK_LOOP;
   }
   return IGNORED;
}
