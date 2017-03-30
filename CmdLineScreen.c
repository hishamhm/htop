#include "CmdLineScreen.h"

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

typedef struct CmdLineScreen_ {
   InfoScreen super;
} CmdLineScreen;
}*/

InfoScreenClass CmdLineScreen_class = {
   .super = {
      .extends = Class(Object),
      .delete = CmdLineScreen_delete
   },
   .scan = CmdLineScreen_scan,
   .draw = CmdLineScreen_draw
};

CmdLineScreen* CmdLineScreen_new(Process* process) {
   CmdLineScreen* this = xMalloc(sizeof(CmdLineScreen));
   Object_setClass(this, Class(CmdLineScreen));
   return (CmdLineScreen*) InfoScreen_init(&this->super, process, NULL, LINES-3, " ");
}

void CmdLineScreen_delete(Object* this) {
   free(InfoScreen_done((InfoScreen*)this));
}

void CmdLineScreen_draw(InfoScreen* this) {
   char* title = xMalloc(COLS);
   snprintf(title, COLS - 4, "Full command line of process %d - %s", this->process->pid, this->process->comm);
   InfoScreen_drawTitled(this, "%s...", title);
   free(title);
}

void CmdLineScreen_addLine(InfoScreen* this, char* p, int line_offset, int len) {
   char *buf = xMalloc(len + 1);
   memcpy(buf, p - line_offset, len);
   buf[len] = '\0';
   InfoScreen_addLine(this, buf);
}

void CmdLineScreen_scan(InfoScreen* this) {
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
         CmdLineScreen_addLine(this, p, line_offset, len);
         line_offset -= len;
         last_spc = -1;
      }
   }

   CmdLineScreen_addLine(this, p, line_offset, line_offset);
   Panel_setSelected(panel, idx);
}
