/*
htop - MetersPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "MetersPanel.h"

#include <stdlib.h>
#include <assert.h>

/*{
#include "Panel.h"
#include "Settings.h"
#include "ScreenManager.h"

typedef struct MetersPanel_ {
   Panel super;

   Settings* settings;
   Vector* meters;
   ScreenManager* scr;
} MetersPanel;

}*/

static void MetersPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   MetersPanel* htop_this = (MetersPanel*) object;
   Panel_done(super);
   free(htop_this);
}

static HandlerResult MetersPanel_eventHandler(Panel* super, int ch) {
   MetersPanel* htop_this = (MetersPanel*) super;
   
   int selected = Panel_getSelectedIndex(super);
   HandlerResult result = IGNORED;

   switch(ch) {
      case 0x0a:
      case 0x0d:
      case KEY_ENTER:
      case KEY_F(4):
      case 't':
      {
         Meter* meter = (Meter*) Vector_get(htop_this->meters, selected);
         int mode = meter->mode + 1;
         if (mode == LAST_METERMODE) mode = 1;
         Meter_setMode(meter, mode);
         Panel_set(super, selected, (Object*) Meter_toListItem(meter));
         result = HANDLED;
         break;
      }
      case KEY_F(7):
      case '[':
      case '-':
      {
         Vector_moveUp(htop_this->meters, selected);
         Panel_moveSelectedUp(super);
         result = HANDLED;
         break;
      }
      case KEY_F(8):
      case ']':
      case '+':
      {
         Vector_moveDown(htop_this->meters, selected);
         Panel_moveSelectedDown(super);
         result = HANDLED;
         break;
      }
      case KEY_F(9):
      case KEY_DC:
      {
         if (selected < Vector_size(htop_this->meters)) {
            Vector_remove(htop_this->meters, selected);
            Panel_remove(super, selected);
         }
         result = HANDLED;
         break;
      }
   }
   if (result == HANDLED) {
      Header* header = htop_this->settings->header;
      htop_this->settings->changed = true;
      Header_calculateHeight(header);
      Header_draw(header);
      ScreenManager_resize(htop_this->scr, htop_this->scr->x1, header->height, htop_this->scr->x2, htop_this->scr->y2);
   }
   return result;
}

PanelClass MetersPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = MetersPanel_delete
   },
   .eventHandler = MetersPanel_eventHandler
};

MetersPanel* MetersPanel_new(Settings* settings, const char* header, Vector* meters, ScreenManager* scr) {
   MetersPanel* htop_this = AllocThis(htop_this,MetersPanel);
   Panel* super = (Panel*) htop_this;
   Panel_init(super, 1, 1, 1, 1, Class(ListItem), true);

   htop_this->settings = settings;
   htop_this->meters = meters;
   htop_this->scr = scr;
   Panel_setHeader(super, header);
   for (int i = 0; i < Vector_size(meters); i++) {
      Meter* meter = (Meter*) Vector_get(meters, i);
      Panel_add(super, (Object*) Meter_toListItem(meter));
   }
   return htop_this;
}
