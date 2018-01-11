/*
htop - AvailableColumnsPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "AvailableColumnsPanel.h"
#include "Platform.h"

#include "Header.h"
#include "ColumnsPanel.h"

#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

/*{
#include "Panel.h"
#include "ProcessList.h"

typedef struct AvailableColumnsPanel_ {
   Panel super;
   Panel* columns;
   #ifdef HAVE_LUA
   lua_State* L;
   #endif
} AvailableColumnsPanel;

}*/

static const char* const AvailableColumnsFunctions[] = {"      ", "      ", "      ", "      ", "Add   ", "      ", "      ", "      ", "      ", "Done  ", NULL};

static void AvailableColumnsPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   AvailableColumnsPanel* this = (AvailableColumnsPanel*) object;
   Panel_done(super);
   free(this);
}

static HandlerResult AvailableColumnsPanel_eventHandler(Panel* super, int ch) {
   AvailableColumnsPanel* this = (AvailableColumnsPanel*) super;
   int key = ((ListItem*) Panel_getSelected(super))->key;
   HandlerResult result = IGNORED;

   switch(ch) {
      case 13:
      case KEY_ENTER:
      case KEY_F(5):
      {
         int at = Panel_getSelectedIndex(this->columns);
         ListItem* item;
         #ifdef HAVE_LUA
         if (key > 1000) {

            lua_State* L = this->L;
            lua_getfield(L, LUA_REGISTRYINDEX, "htop_columns");
            lua_geti(L, -1, key - 1000);
            ProcessFieldData* pfd = (ProcessFieldData*) lua_touserdata(L, -1);
            item = ListItem_new(pfd->name, key);
            lua_pop(L, 2);
         } else
         #endif
         {
            item = ListItem_new(Process_fields[key].name, key);
         }
         Panel_insert(this->columns, at, (Object*) item);
         Panel_setSelected(this->columns, at+1);
         ColumnsPanel_update(this->columns);
         result = HANDLED;
         break;
      }
      default:
      {
         if (ch < 255 && isalpha(ch))
            result = Panel_selectByTyping(super, ch);
         break;
      }
   }
   return result;
}

PanelClass AvailableColumnsPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = AvailableColumnsPanel_delete
   },
   .eventHandler = AvailableColumnsPanel_eventHandler
};

AvailableColumnsPanel* AvailableColumnsPanel_new(Panel* columns, ProcessList* pl) {
   AvailableColumnsPanel* this = AllocThis(AvailableColumnsPanel);
   Panel* super = (Panel*) this;
   FunctionBar* fuBar = FunctionBar_new(AvailableColumnsFunctions, NULL, NULL);
   Panel_init(super, 1, 1, 1, 1, Class(ListItem), true, fuBar);

   Panel_setHeader(super, "Available Columns");

   for (int i = 1; i < Platform_numberOfFields; i++) {
      if (i != COMM && Process_fields[i].description) {
         char description[256];
         xSnprintf(description, sizeof(description), "%s - %s", Process_fields[i].name, Process_fields[i].description);
         Panel_add(super, (Object*) ListItem_new(description, i));
      }
   }
   #ifdef HAVE_LUA
   lua_State* L = pl->L;
   this->L = L;
   int top = lua_gettop(L);
   lua_getfield(L, LUA_REGISTRYINDEX, "htop_columns");
   lua_len(L, -1);
   int len = lua_tointeger(L, -1);
   lua_pop(L, 1);
   for (int a = 1; a <= len; a++) {
      lua_geti(L, -1, a);
      ProcessFieldData* pfd = (ProcessFieldData*) lua_touserdata(L, -1);
      char description[256];
      xSnprintf(description, sizeof(description), "%s - %s", pfd->name, pfd->description);
      Panel_add(super, (Object*) ListItem_new(description, 1000 + a));
      lua_pop(L, 1);
   }
   lua_settop(L, top);
   #endif
   this->columns = columns;
   return this;
}
