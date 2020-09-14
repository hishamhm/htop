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

void CommandScreen_addLine(InfoScreen* this, char* p, int line_offset, int len) {
   char *line = xMalloc(len + 1);
   memcpy(line, p - line_offset, len);
   line[len] = '\0';
   InfoScreen_addLine(this, line);
   free(line);
}

void CommandScreen_scan(InfoScreen* this) {
   Panel* panel = this->display;
   int idx = MAX(Panel_getSelectedIndex(panel), 0);

   Panel_prune(panel);

   char *p = this->process->comm;
   int line_offset = 0, last_spc = -1, len;
   for (; *p; p++, line_offset++) {
      if (*p == ' ' || *p == '\t') {
         last_spc = line_offset;
      }

      if (line_offset == COLS) {
         len = (last_spc == -1) ? line_offset : last_spc;
         CommandScreen_addLine(this, p, line_offset, len);
         line_offset -= len;
         last_spc = -1;
      }
   }

   CommandScreen_addLine(this, p, line_offset, line_offset);
   Panel_setSelected(panel, idx);
}
