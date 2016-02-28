/*
htop - IncSet.c
(C) 2005-2012 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "IncSet.h"
#include "StringUtils.h"
#include "Panel.h"
#include "ListItem.h"
#include "CRT.h"
#include <string.h>
#include <stdlib.h>

/*{

#include "FunctionBar.h"
#include "Panel.h"
#include <regex.h>
#include <stdbool.h>

#define INCMODE_MAX 40

typedef enum {
   INC_SEARCH = 0,
   INC_FILTER = 1
} IncType;

#define IncSet_filter(inc_) (inc_->filtering ? inc_->modes[INC_FILTER].buffer : NULL)

typedef struct IncMode_ {
   char buffer[INCMODE_MAX+1];
   int index;
   FunctionBar* bar;
   bool isFilter;
} IncMode;

typedef struct IncSet_ {
   IncMode modes[2];
   IncMode* active;
   FunctionBar* defaultBar;
   bool filtering;
   bool regex;
   bool exclude;
   bool regexAlloc;
   char* rawFilter;
   regex_t regexFilter;
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

static const char* filterFunctions[] = {"Done  ", "Clear ", "!Regexp ", "Filter Out ", " Filter: ", NULL};
static const char* filterKeys[] = {"Enter", "Esc", "F5", "F6", "  "};
static const char* regexLabels[] = {"Regexp  ", "!Regexp "};
static const char* excludeLabels[] = {"Filter Out ", "Filter In  "};
static int filterEvents[] = {13, 27, KEY_F(5), KEY_F(6), ERR};

static inline void IncMode_initFilter(IncMode* filter) {
   memset(filter, 0, sizeof(IncMode));
   filter->bar = FunctionBar_new(filterFunctions, filterKeys, filterEvents);
   filter->isFilter = true;
}

static inline void IncMode_done(IncMode* mode) {
   FunctionBar_delete(mode->bar);
}

IncSet* IncSet_new(FunctionBar* bar) {
   IncSet* this = xCalloc(1, sizeof(IncSet));
   IncMode_initSearch(&(this->modes[INC_SEARCH]));
   IncMode_initFilter(&(this->modes[INC_FILTER]));
   this->active = NULL;
   this->filtering = false;
   this->defaultBar = bar;
   this->rawFilter = NULL;
   this->regex = true;
   this->exclude = false;
   this->regexAlloc = false;   
   return this;
}

static void freeFilter(IncSet* this);

void IncSet_delete(IncSet* this) {
   IncMode_done(&(this->modes[0]));
   IncMode_done(&(this->modes[1]));
   freeFilter(this);
   free(this);
}


static void updateBarLabels(IncSet* this) {
   if (this->active == &(this->modes[INC_FILTER])) {
      IncMode* filterMode = &(this->modes[INC_FILTER]);
      FunctionBar_setLabel(filterMode->bar, KEY_F(5), regexLabels[this->regex ? 1 : 0]);
      FunctionBar_setLabel(filterMode->bar, KEY_F(6), excludeLabels[this->exclude ? 1 : 0]);
   }
}

static bool testWithFilter(IncSet* this, const char* testSubject) {
   if (!this || !this->filtering) {
      return true;
   }
   if (this->regexAlloc) {
      return regexec(&this->regexFilter, testSubject, 0, NULL, 0) ? this->exclude : !this->exclude;
   }
   return  String_contains_i(testSubject, this->rawFilter ? this->rawFilter : "" ) ? !this->exclude : this->exclude;
}

bool IncSet_filterTest(IncSet* this, const char* str) {
      return !this || !this->filtering || testWithFilter(this, str);
}

static void freeFilter(IncSet* this) {
   if (this->regexAlloc) {
      regfree(&this->regexFilter);
      this->regexAlloc = false;
   }
   if (this->rawFilter) {
      free(this->rawFilter);
      this->rawFilter = NULL;
   }
}

static void compileFilter(IncSet* this, const char* filterStr) {
   if (this->rawFilter) {
      freeFilter(this);
   }
   if (filterStr) {
      this->rawFilter = xStrdup(filterStr);
      if (this->regex) {
         int result = regcomp(&this->regexFilter, this->rawFilter, REG_EXTENDED|REG_NOSUB|REG_ICASE);
         if (!result) {
            this->regexAlloc = true;
         }
      }
   }
}

static void updateWeakPanel(IncSet* this, Panel* panel, Vector* lines) {
   Object* selected = Panel_getSelected(panel);
   Panel_prune(panel);
   if (this->filtering) {
      int n = 0;
      const char* incFilter = this->modes[INC_FILTER].buffer;
      for (int i = 0; i < Vector_size(lines); i++) {
         ListItem* line = (ListItem*)Vector_get(lines, i);
         if (testWithFilter(this, line->value)) {
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

static void search(IncSet* this, IncMode* mode, Panel* panel, IncMode_GetPanelValue getPanelValue) {
   int size = Panel_size(panel);
   bool found = false;
   for (int i = 0; i < size; i++) {
      if (testWithFilter(this, getPanelValue(panel, i))) {
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

bool IncSet_handleKey(IncSet* this, int ch, Panel* panel, IncMode_GetPanelValue getPanelValue, Vector* lines) {
   if (ch == ERR)
      return true;
   IncMode* mode = this->active;
   int size = Panel_size(panel);
   bool filterChanged = false;
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
   } else if (ch == KEY_F(5)) {
      if (mode->isFilter) {
         filterChanged = true;
         this->regex = !this->regex;
         updateBarLabels(this);
      }
   } else if (ch == KEY_F(6)) {
      if (mode->isFilter) {
         filterChanged = true;
         this->exclude = !this->exclude;
         updateBarLabels(this);
      }      
   } else if (ch < 255 && isprint((char)ch) && (mode->index < INCMODE_MAX)) {
      mode->buffer[mode->index] = ch;
      mode->index++;
      mode->buffer[mode->index] = 0;
      if (mode->isFilter) {         
         filterChanged = true;
         if (mode->index == 1) this->filtering = true;
      }
   } else if ((ch == KEY_BACKSPACE || ch == 127) && (mode->index > 0)) {
      mode->index--;
      mode->buffer[mode->index] = 0;
      if (mode->isFilter) {
         filterChanged = true;
         if (mode->index == 0) {
            this->filtering = false;
            freeFilter(this);
            IncMode_reset(mode);
         }
      }
   } else if (ch == KEY_RESIZE) {
     Panel_resize(panel, COLS, LINES-panel->y-1);
   } else {
      if (mode->isFilter) {
         filterChanged = true;
         if (ch == 27) {
            this->filtering = false;
            freeFilter(this);
            IncMode_reset(mode);
         }
      } else {
         IncMode_reset(mode);
      }
      this->active = NULL;
      Panel_setDefaultBar(panel);
      FunctionBar_draw(this->defaultBar, NULL);
      doSearch = false;
   }
   if (filterChanged) {
      compileFilter(this, mode->buffer);
   }
   if (doSearch) {
      search(this, mode, panel, getPanelValue);
   }
   if (filterChanged && lines) {
      updateWeakPanel(this, panel, lines);
   }
   return filterChanged;
}

const char* IncSet_getListItemValue(Panel* panel, int i) {
   ListItem* l = (ListItem*) Panel_get(panel, i);
   if (l)
      return l->value;
   return "";
}

void IncSet_activate(IncSet* this, IncType type, Panel* panel) {
   this->active = &(this->modes[type]);
   updateBarLabels(this);
   FunctionBar_draw(this->active->bar, this->active->buffer);
   panel->currentBar = this->active->bar;
}

void IncSet_drawBar(IncSet* this) {
   if (this->active) {
      FunctionBar_draw(this->active->bar, this->active->buffer);
   } else {
      FunctionBar_draw(this->defaultBar, NULL);
   }
}

int IncSet_synthesizeEvent(IncSet* this, int x) {
   if (this->active) {
      return FunctionBar_synthesizeEvent(this->active->bar, x);
   } else {
      return FunctionBar_synthesizeEvent(this->defaultBar, x);
   }
}
