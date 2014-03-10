/*
htop - IOPriorityPanel.c
(C) 2004-2012 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "IOPriorityPanel.h"

/*{
#include "Panel.h"
#include "IOPriority.h"
#include "ListItem.h"
}*/

Panel* IOPriorityPanel_new(IOPriority currPrio) {
   Panel*  htop_this = Panel_new(1, 1, 1, 1, true, Class(ListItem));

   Panel_setHeader( htop_this, "IO Priority:");
   Panel_add( htop_this, (Object*) ListItem_new("None (based on nice)", IOPriority_None));
   if (currPrio == IOPriority_None) Panel_setSelected( htop_this, 0);
   struct { int klass; const char* name; } classes[] = {
      { .klass = IOPRIO_CLASS_RT, .name = "Realtime" },
      { .klass = IOPRIO_CLASS_BE, .name = "Best-effort" },
      { .klass = 0, .name = NULL }
   };
   for (int c = 0; classes[c].name; c++) {
      for (int i = 0; i < 8; i++) {
         char name[50];
         snprintf(name, sizeof(name)-1, "%s %d %s", classes[c].name, i, i == 0 ? "(High)" : (i == 7 ? "(Low)" : ""));
         IOPriority ioprio = IOPriority_tuple(classes[c].klass, i);
         Panel_add( htop_this, (Object*) ListItem_new(name, ioprio));
         if (currPrio == ioprio) Panel_setSelected( htop_this, Panel_size( htop_this) - 1);
      }
   }
   Panel_add( htop_this, (Object*) ListItem_new("Idle", IOPriority_Idle));
   if (currPrio == IOPriority_Idle) Panel_setSelected( htop_this, Panel_size( htop_this) - 1);
   return  htop_this;
}

IOPriority IOPriorityPanel_getIOPriority(Panel*  htop_this) {
   return (IOPriority) ( ((ListItem*) Panel_getSelected( htop_this))->key );
}
