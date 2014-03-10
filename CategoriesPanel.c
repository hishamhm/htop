/*
htop - CategoriesPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "CategoriesPanel.h"

#include "AvailableMetersPanel.h"
#include "MetersPanel.h"
#include "DisplayOptionsPanel.h"
#include "ColumnsPanel.h"
#include "ColorsPanel.h"
#include "AvailableColumnsPanel.h"

#include <assert.h>
#include <stdlib.h>

/*{
#include "Panel.h"
#include "Settings.h"
#include "ScreenManager.h"

typedef struct CategoriesPanel_ {
   Panel super;

   Settings* settings;
   ScreenManager* scr;
} CategoriesPanel;

}*/

static const char* MetersFunctions[] = {"      ", "      ", "      ", "Type  ", "      ", "      ", "MoveUp", "MoveDn", "Remove", "Done  ", NULL};

static const char* AvailableMetersFunctions[] = {"      ", "      ", "      ", "      ", "Add L ", "Add R ", "      ", "      ", "      ", "Done  ", NULL};

static const char* DisplayOptionsFunctions[] = {"      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "Done  ", NULL};

static const char* ColumnsFunctions[] = {"      ", "      ", "      ", "      ", "      ", "      ", "MoveUp", "MoveDn", "Remove", "Done  ", NULL};

static const char* ColorsFunctions[] = {"      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "Done  ", NULL};

static const char* AvailableColumnsFunctions[] = {"      ", "      ", "      ", "      ", "Add   ", "      ", "      ", "      ", "      ", "Done  ", NULL};

static void CategoriesPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   CategoriesPanel* htop_this = (CategoriesPanel*) object;
   Panel_done(super);
   free(htop_this);
}

void CategoriesPanel_makeMetersPage(CategoriesPanel* htop_this) {
   Panel* leftMeters = (Panel*) MetersPanel_new(htop_this->settings, "Left column", htop_this->settings->header->leftMeters, htop_this->scr);
   Panel* rightMeters = (Panel*) MetersPanel_new(htop_this->settings, "Right column", htop_this->settings->header->rightMeters, htop_this->scr);
   Panel* availableMeters = (Panel*) AvailableMetersPanel_new(htop_this->settings, leftMeters, rightMeters, htop_this->scr);
   ScreenManager_add(htop_this->scr, leftMeters, FunctionBar_new(MetersFunctions, NULL, NULL), 20);
   ScreenManager_add(htop_this->scr, rightMeters, FunctionBar_new(MetersFunctions, NULL, NULL), 20);
   ScreenManager_add(htop_this->scr, availableMeters, FunctionBar_new(AvailableMetersFunctions, NULL, NULL), -1);
}

static void CategoriesPanel_makeDisplayOptionsPage(CategoriesPanel* htop_this) {
   Panel* displayOptions = (Panel*) DisplayOptionsPanel_new(htop_this->settings, htop_this->scr);
   ScreenManager_add(htop_this->scr, displayOptions, FunctionBar_new(DisplayOptionsFunctions, NULL, NULL), -1);
}

static void CategoriesPanel_makeColorsPage(CategoriesPanel* htop_this) {
   Panel* colors = (Panel*) ColorsPanel_new(htop_this->settings, htop_this->scr);
   ScreenManager_add(htop_this->scr, colors, FunctionBar_new(ColorsFunctions, NULL, NULL), -1);
}

static void CategoriesPanel_makeColumnsPage(CategoriesPanel* htop_this) {
   Panel* columns = (Panel*) ColumnsPanel_new(htop_this->settings, htop_this->scr);
   Panel* availableColumns = (Panel*) AvailableColumnsPanel_new(htop_this->settings, columns, htop_this->scr);
   ScreenManager_add(htop_this->scr, columns, FunctionBar_new(ColumnsFunctions, NULL, NULL), 20);
   ScreenManager_add(htop_this->scr, availableColumns, FunctionBar_new(AvailableColumnsFunctions, NULL, NULL), -1);
}

static HandlerResult CategoriesPanel_eventHandler(Panel* super, int ch) {
   CategoriesPanel* htop_this = (CategoriesPanel*) super;

   HandlerResult result = IGNORED;

   int selected = Panel_getSelectedIndex(super);
   switch (ch) {
      case EVENT_SETSELECTED:
         result = HANDLED;
         break;
      case KEY_UP:
      case KEY_CTRLP:
      case KEY_DOWN:
      case KEY_CTRLN:
      case KEY_NPAGE:
      case KEY_PPAGE:
      case KEY_HOME:
      case KEY_END: {
         int previous = selected;
         Panel_onKey(super, ch);
         selected = Panel_getSelectedIndex(super);
         if (previous != selected)
            result = HANDLED;
         break;
      }
      default:
         if (isalpha(ch))
            result = Panel_selectByTyping(super, ch);
         if (result == BREAK_LOOP)
            result = IGNORED;
         break;
   }

   if (result == HANDLED) {
      int size = ScreenManager_size(htop_this->scr);
      for (int i = 1; i < size; i++)
         ScreenManager_remove(htop_this->scr, 1);
      switch (selected) {
         case 0:
            CategoriesPanel_makeMetersPage(htop_this);
            break;
         case 1:
            CategoriesPanel_makeDisplayOptionsPage(htop_this);
            break;
         case 2:
            CategoriesPanel_makeColorsPage(htop_this);
            break;
         case 3:
            CategoriesPanel_makeColumnsPage(htop_this);
            break;
      }
   }

   return result;
}

PanelClass CategoriesPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = CategoriesPanel_delete
   },
   .eventHandler = CategoriesPanel_eventHandler
};

CategoriesPanel* CategoriesPanel_new(Settings* settings, ScreenManager* scr) {
   CategoriesPanel* htop_this = AllocThis(htop_this, CategoriesPanel);
   Panel* super = (Panel*) htop_this;
   Panel_init(super, 1, 1, 1, 1, Class(ListItem), true);

   htop_this->settings = settings;
   htop_this->scr = scr;
   Panel_setHeader(super, "Setup");
   Panel_add(super, (Object*) ListItem_new("Meters", 0));
   Panel_add(super, (Object*) ListItem_new("Display options", 0));
   Panel_add(super, (Object*) ListItem_new("Colors", 0));
   Panel_add(super, (Object*) ListItem_new("Columns", 0));
   return htop_this;
}
