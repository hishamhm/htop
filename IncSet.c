/*
htop - IncSet.c
(C) 2005-2012 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "IncSet.h"
#include "String.h"
#include "Panel.h"
#include "ListItem.h"
#include "CRT.h"
#include <string.h>
#include <stdlib.h>

/*{

#include "FunctionBar.h"
#include "Panel.h"
#include <stdbool.h>

#define INCMODE_MAX 40

typedef enum {
   INC_SEARCH = 0,
   INC_FILTER = 1
} IncType;

#define IncSet_filter(inc_) (inc_->filtering ? inc_->modes[INC_FILTER].buffer : NULL)

typedef struct IncMode_ {
   char buffer[INCMODE_MAX];
   int index;
   FunctionBar* bar;
   bool isFilter;
} IncMode;

typedef struct IncSet_ {
   IncMode modes[2];
   IncMode* active;
   FunctionBar* bar;
   FunctionBar* defaultBar;
   bool filtering;
} IncSet;

typedef const char* (*IncMode_GetPanelValue)(Panel*, int);

}*/

static void IncMode_reset(IncMode* mode) {
   mode->index = 0;
   mode->buffer[0] = 0;
}

static const char* searchFunctions[] = {"Next  ", "Cancel ", " Search: ", NULL};
static const char* searchKeys[] = {"F3", "Esc", "  "};
static int searchEvents[] = {KEY_F(3), 27, ERR};

static inline void IncMode_initSearch(IncMode* search) {
   memset(search, 0, sizeof(IncMode));
   search->bar = FunctionBar_new(searchFunctions, searchKeys, searchEvents);
   search->isFilter = false;
}

static const char* filterFunctions[] = {"Done  ", "Clear ", " Filter: ", NULL};
static const char* filterKeys[] = {"Enter", "Esc", "  "};
static int filterEvents[] = {13, 27, ERR};

static inline void IncMode_initFilter(IncMode* filter) {
   memset(filter, 0, sizeof(IncMode));
   filter->bar = FunctionBar_new(filterFunctions, filterKeys, filterEvents);
   filter->isFilter = true;
}

static inline void IncMode_done(IncMode* mode) {
   FunctionBar_delete((Object*)mode->bar);
}

IncSet* IncSet_new(FunctionBar* bar) {
   IncSet* htop_this = calloc(1, sizeof(IncSet));
   IncMode_initSearch(&(htop_this->modes[INC_SEARCH]));
   IncMode_initFilter(&(htop_this->modes[INC_FILTER]));
   htop_this->active = NULL;
   htop_this->filtering = false;
   htop_this->bar = bar;
   htop_this->defaultBar = bar;
   return htop_this;
}

void IncSet_delete(IncSet* htop_this) {
   IncMode_done(&(htop_this->modes[0]));
   IncMode_done(&(htop_this->modes[1]));
   free(htop_this);
}

static void updateWeakPanel(IncSet* htop_this, Panel* panel, Vector* lines) {
   Object* selected = Panel_getSelected(panel);
   Panel_prune(panel);
   if (htop_this->filtering) {
      int n = 0;
      const char* incFilter = htop_this->modes[INC_FILTER].buffer;
      for (int i = 0; i < Vector_size(lines); i++) {
         ListItem* line = (ListItem*)Vector_get(lines, i);
         if (String_contains_i(line->value, incFilter)) {
            Panel_add(panel, (Object*)line);
            if (selected == (Object*)line) Panel_setSelected(panel, n);
            n++;
         }
      }
   } else {
      for (int i = 0; i < Vector_size(lines); i++) {
         Object* line = Vector_get(lines, i);
         Panel_add(panel, line);
         if (selected == line) Panel_setSelected(panel, i);
      }
   }
}

static void search(IncMode* mode, Panel* panel, IncMode_GetPanelValue getPanelValue) {
   int size = Panel_size(panel);
   bool found = false;
   for (int i = 0; i < size; i++) {
      if (String_contains_i(getPanelValue(panel, i), mode->buffer)) {
         Panel_setSelected(panel, i);
         found = true;
         break;
      }
   }
   if (found)
      FunctionBar_draw(mode->bar, mode->buffer);
   else
      FunctionBar_drawAttr(mode->bar, mode->buffer, CRT_colors[FAILED_SEARCH]);
}

bool IncSet_handleKey(IncSet* htop_this, int ch, Panel* panel, IncMode_GetPanelValue getPanelValue, Vector* lines) {
   if (ch == ERR)
      return true;
   IncMode* mode = htop_this->active;
   int size = Panel_size(panel);
   bool filterChange = false;
   bool doSearch = true;
   if (ch == KEY_F(3)) {
      if (size == 0) return true;
      int here = Panel_getSelectedIndex(panel);
      int i = here;
      for(;;) {
         i++;
         if (i == size) i = 0;
         if (i == here) break;
         if (String_contains_i(getPanelValue(panel, i), mode->buffer)) {
            Panel_setSelected(panel, i);
            break;
         }
      }
      doSearch = false;
   } else if (isprint((char)ch) && (mode->index < INCMODE_MAX)) {
      mode->buffer[mode->index] = ch;
      mode->index++;
      mode->buffer[mode->index] = 0;
      if (mode->isFilter) {
         filterChange = true;
         if (mode->index == 1) htop_this->filtering = true;
      }
   } else if ((ch == KEY_BACKSPACE || ch == 127) && (mode->index > 0)) {
      mode->index--;
      mode->buffer[mode->index] = 0;
      if (mode->isFilter) {
         filterChange = true;
         if (mode->index == 0) {
            htop_this->filtering = false;
            IncMode_reset(mode);
         }
      }
   } else {
      if (mode->isFilter) {
         filterChange = true;
         if (ch == 27) {
            htop_this->filtering = false;
            IncMode_reset(mode);
         }
      } else {
         IncMode_reset(mode);
      }
      htop_this->active = NULL;
      htop_this->bar = htop_this->defaultBar;
      FunctionBar_draw(htop_this->defaultBar, NULL);
      doSearch = false;
   }
   if (doSearch) {
      search(mode, panel, getPanelValue);
   }
   if (filterChange && lines) {
      updateWeakPanel(htop_this, panel, lines);
   }
   return filterChange;
}

const char* IncSet_getListItemValue(Panel* panel, int i) {
   ListItem* l = (ListItem*) Panel_get(panel, i);
   if (l)
      return l->value;
   return "";
}

void IncSet_activate(IncSet* htop_this, IncType type) {
   htop_this->active = &(htop_this->modes[type]);
   htop_this->bar = htop_this->active->bar;
   FunctionBar_draw(htop_this->active->bar, htop_this->active->buffer);
}

void IncSet_drawBar(IncSet* htop_this) {
   FunctionBar_draw(htop_this->bar, htop_this->active ? htop_this->active->buffer : NULL);
}
