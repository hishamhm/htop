/*
htop - AvailableMetersPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "AvailableMetersPanel.h"

#include "CPUMeter.h"
#include "Header.h"
#include "ListItem.h"

#include <assert.h>
#include <stdlib.h>

/*{
#include "Settings.h"
#include "Panel.h"
#include "ScreenManager.h"

typedef struct AvailableMetersPanel_ {
   Panel super;

   Settings* settings;
   Panel* leftPanel;
   Panel* rightPanel;
   ScreenManager* scr;
} AvailableMetersPanel;

}*/

static void AvailableMetersPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   AvailableMetersPanel* htop_this = (AvailableMetersPanel*) object;
   Panel_done(super);
   free(htop_this);
}

static inline void AvailableMetersPanel_addHeader(Header* header, Panel* panel, MeterClass* type, int param, HeaderSide side) {
   Meter* meter = (Meter*) Header_addMeter(header, type, param, side);
   Panel_add(panel, (Object*) Meter_toListItem(meter));
}

static HandlerResult AvailableMetersPanel_eventHandler(Panel* super, int ch) {
   AvailableMetersPanel* htop_this = (AvailableMetersPanel*) super;
   Header* header = htop_this->settings->header;
   
   ListItem* selected = (ListItem*) Panel_getSelected(super);
   int param = selected->key & 0xff;
   int type = selected->key >> 16;
   HandlerResult result = IGNORED;

   switch(ch) {
      case KEY_F(5):
      case 'l':
      case 'L':
      {
         AvailableMetersPanel_addHeader(header, htop_this->leftPanel, Meter_types[type], param, LEFT_HEADER);
         result = HANDLED;
         break;
      }
      case KEY_F(6):
      case 'r':
      case 'R':
      {
         AvailableMetersPanel_addHeader(header, htop_this->rightPanel, Meter_types[type], param, RIGHT_HEADER);
         result = HANDLED;
         break;
      }
   }
   if (result == HANDLED) {
      htop_this->settings->changed = true;
      Header_calculateHeight(header);
      Header_draw(header);
      ScreenManager_resize(htop_this->scr, htop_this->scr->x1, header->height, htop_this->scr->x2, htop_this->scr->y2);
   }
   return result;
}

PanelClass AvailableMetersPanel_class = {
   .super = {
      .extends = Class(Panel),
      .htop_delete = AvailableMetersPanel_delete
   },
   .eventHandler = AvailableMetersPanel_eventHandler
};

AvailableMetersPanel* AvailableMetersPanel_new(Settings* settings, Panel* leftMeters, Panel* rightMeters, ScreenManager* scr) {
   AvailableMetersPanel* htop_this = AllocThis(htop_this, AvailableMetersPanel);
   Panel* super = (Panel*) htop_this;
   Panel_init(super, 1, 1, 1, 1, Class(ListItem), true);
   
   htop_this->settings = settings;
   htop_this->leftPanel = leftMeters;
   htop_this->rightPanel = rightMeters;
   htop_this->scr = scr;

   Panel_setHeader(super, "Available meters");
   for (int i = 1; Meter_types[i]; i++) {
      MeterClass* type = Meter_types[i];
      if (type != &CPUMeter_class) {
         Panel_add(super, (Object*) ListItem_new(type->uiName, i << 16));
      }
   }
   MeterClass* type = &CPUMeter_class;
   int cpus = settings->pl->cpuCount;
   if (cpus > 1) {
      Panel_add(super, (Object*) ListItem_new("CPU average", 0));
      for (int i = 1; i <= cpus; i++) {
         char buffer[50];
         sprintf(buffer, "%s %d", type->uiName, i);
         Panel_add(super, (Object*) ListItem_new(buffer, i));
      }
   } else {
      Panel_add(super, (Object*) ListItem_new("CPU", 1));
   }
   return htop_this;
}
