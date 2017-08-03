#include "CommandScreen.h"

#include "config.h"
#include "CRT.h"
#include "IncSet.h"
#include "ListItem.h"
#include "Platform.h"
#include "StringUtils.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*{
#include "InfoScreen.h"

typedef struct CommandScreen_ {
   InfoScreen super;
} CommandScreen;
}*/

InfoScreenClass CommandScreen_class = {
   .super = {
      .extends = Class(Object),
      .delete = CommandScreen_delete
   },
   .scan = CommandScreen_scan,
   .draw = CommandScreen_draw
};

CommandScreen* CommandScreen_new(Process* process) {
   CommandScreen* this = xMalloc(sizeof(CommandScreen));
   Object_setClass(this, Class(CommandScreen));
   return (CommandScreen*) InfoScreen_init(&this->super, process, NULL, LINES-3, " ");
}

void CommandScreen_delete(Object* this) {
   free(InfoScreen_done((InfoScreen*)this));
}

void CommandScreen_draw(InfoScreen* this) {
   char* title = xMalloc(COLS + 1);
   snprintf(title, COLS - 4, "Command of process %d - %s", this->process->pid, this->process->comm);
   InfoScreen_drawTitled(this, "%s...", title);
   free(title);
}

void CommandScreen_scan(InfoScreen* this) {
   Panel* panel = this->display;
   int idx = MAX(Panel_getSelectedIndex(panel), 0);

   Panel_prune(panel);

   char *buf = xMalloc(COLS + 1);
   char *cmd = this->process->comm;
   int total = strlen(cmd), curr = 0, len;

   while (curr < total) {
      len = MIN(COLS, total - curr);
      memcpy(buf, cmd + curr, len);
      buf[len] = '\0';
      InfoScreen_addLine(this, buf);
      curr += len;
   }

   free(buf);
   Panel_setSelected(panel, idx);
}
