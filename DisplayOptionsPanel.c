/*
htop - DisplayOptionsPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "DisplayOptionsPanel.h"

#include "CheckItem.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/*{
#include "Panel.h"
#include "Settings.h"
#include "ScreenManager.h"

typedef struct DisplayOptionsPanel_ {
   Panel super;

   Settings* settings;
   ScreenManager* scr;
} DisplayOptionsPanel;

}*/

static void DisplayOptionsPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   DisplayOptionsPanel* htop_this = (DisplayOptionsPanel*) object;
   Panel_done(super);
   free(htop_this);
}

static HandlerResult DisplayOptionsPanel_eventHandler(Panel* super, int ch) {
   DisplayOptionsPanel* htop_this = (DisplayOptionsPanel*) super;
   
   HandlerResult result = IGNORED;
   CheckItem* selected = (CheckItem*) Panel_getSelected(super);

   switch(ch) {
   case 0x0a:
   case 0x0d:
   case KEY_ENTER:
   case KEY_MOUSE:
   case ' ':
      CheckItem_set(selected, ! (CheckItem_get(selected)) );
      result = HANDLED;
   }

   if (result == HANDLED) {
      htop_this->settings->changed = true;
      Header* header = htop_this->settings->header;
      Header_calculateHeight(header);
      Header_reinit(header);
      Header_draw(header);
      ScreenManager_resize(htop_this->scr, htop_this->scr->x1, header->height, htop_this->scr->x2, htop_this->scr->y2);
   }
   return result;
}

PanelClass DisplayOptionsPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = DisplayOptionsPanel_delete
   },
   .eventHandler = DisplayOptionsPanel_eventHandler
};

DisplayOptionsPanel* DisplayOptionsPanel_new(Settings* settings, ScreenManager* scr) {
   DisplayOptionsPanel* htop_this = AllocThis(htop_this, DisplayOptionsPanel);
   Panel* super = (Panel*) htop_this;
   Panel_init(super, 1, 1, 1, 1, Class(CheckItem), true);

   htop_this->settings = settings;
   htop_this->scr = scr;

   Panel_setHeader(super, "Display options");
   Panel_add(super, (Object*) CheckItem_new(strdup("Tree view"), &(settings->pl->treeView), false));
   Panel_add(super, (Object*) CheckItem_new(strdup("Shadow other users' processes"), &(settings->pl->shadowOtherUsers), false));
   Panel_add(super, (Object*) CheckItem_new(strdup("Hide kernel threads"), &(settings->pl->hideKernelThreads), false));
   Panel_add(super, (Object*) CheckItem_new(strdup("Hide userland threads"), &(settings->pl->hideUserlandThreads), false));
   Panel_add(super, (Object*) CheckItem_new(strdup("Display threads in a different color"), &(settings->pl->highlightThreads), false));
   Panel_add(super, (Object*) CheckItem_new(strdup("Show custom thread names"), &(settings->pl->showThreadNames), false));
   Panel_add(super, (Object*) CheckItem_new(strdup("Highlight program \"basename\""), &(settings->pl->highlightBaseName), false));
   Panel_add(super, (Object*) CheckItem_new(strdup("Highlight large numbers in memory counters"), &(settings->pl->highlightMegabytes), false));
   Panel_add(super, (Object*) CheckItem_new(strdup("Leave a margin around header"), &(settings->header->margin), false));
   Panel_add(super, (Object*) CheckItem_new(strdup("Detailed CPU time (System/IO-Wait/Hard-IRQ/Soft-IRQ/Steal/Guest)"), &(settings->pl->detailedCPUTime), false));
   Panel_add(super, (Object*) CheckItem_new(strdup("Count CPUs from 0 instead of 1"), &(settings->pl->countCPUsFromZero), false));
   Panel_add(super, (Object*) CheckItem_new(strdup("Update process names on every refresh"), &(settings->pl->updateProcessNames), false));
   Panel_add(super, (Object*) CheckItem_new(strdup("Add guest time in CPU meter percentage"), &(settings->pl->accountGuestInCPUMeter), false));
   return htop_this;
}
