/*
htop - OpenFilesScreen.c
(C) 2005-2006 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "OpenFilesScreen.h"

#include "CRT.h"
#include "ProcessList.h"
#include "ListItem.h"
#include "IncSet.h"
#include "String.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

/*{
#include "Process.h"
#include "Panel.h"
#include "FunctionBar.h"

typedef struct OpenFiles_ProcessData_ {
   char* data[256];
   struct OpenFiles_FileData_* files;
   int error;
} OpenFiles_ProcessData;

typedef struct OpenFiles_FileData_ {
   char* data[256];
   struct OpenFiles_FileData_* next;
} OpenFiles_FileData;

typedef struct OpenFilesScreen_ {
   Process* process;
   pid_t pid;
   Panel* display;
   FunctionBar* bar;
} OpenFilesScreen;

}*/

static const char* ofsFunctions[] = {"Search ", "Filter ", "Refresh", "Done   ", NULL};

static const char* ofsKeys[] = {"F3", "F4", "F5", "Esc"};

static int ofsEvents[] = {KEY_F(3), KEY_F(4), KEY_F(5), 27};

OpenFilesScreen* OpenFilesScreen_new(Process* process) {
   OpenFilesScreen* htop_this = (OpenFilesScreen*) malloc(sizeof(OpenFilesScreen));
   htop_this->process = process;
   htop_this->display = Panel_new(0, 1, COLS, LINES-3, false, Class(ListItem));
   if (Process_isThread(process))
      htop_this->pid = process->tgid;
   else
      htop_this->pid = process->pid;
   return htop_this;
}

void OpenFilesScreen_delete(OpenFilesScreen* htop_this) {
   Panel_delete((Object*)htop_this->display);
   free(htop_this);
}

static void OpenFilesScreen_draw(OpenFilesScreen* htop_this, IncSet* inc) {
   attrset(CRT_colors[METER_TEXT]);
   mvhline(0, 0, ' ', COLS);
   mvprintw(0, 0, "Snapshot of files open in process %d - %s", htop_this->pid, htop_this->process->comm);
   attrset(CRT_colors[DEFAULT_COLOR]);
   Panel_draw(htop_this->display, true);
   IncSet_drawBar(inc);
}

static OpenFiles_ProcessData* OpenFilesScreen_getProcessData(pid_t pid) {
   char command[1025];
   snprintf(command, 1024, "lsof -P -p %d -F 2> /dev/null", pid);
   FILE* fd = popen(command, "r");
   OpenFiles_ProcessData* pdata = calloc(1, sizeof(OpenFiles_ProcessData));
   OpenFiles_FileData* fdata = NULL;
   OpenFiles_ProcessData* item = pdata;
   bool anyRead = false;
   if (!fd) {
      pdata->error = 127;
      return pdata;
   }
   while (!feof(fd)) {
      int cmd = fgetc(fd);
      if (cmd == EOF && !anyRead)
         break;
      anyRead = true;
      char* entry = malloc(1024);
      if (!fgets(entry, 1024, fd)) {
         free(entry);
         break;
      }
      char* newline = strrchr(entry, '\n');
      *newline = '\0';
      if (cmd == 'f') {
         OpenFiles_FileData* nextFile = calloc(1, sizeof(OpenFiles_ProcessData));
         if (fdata == NULL) {
            pdata->files = nextFile;
         } else {
            fdata->next = nextFile;
         }
         fdata = nextFile;
         item = (OpenFiles_ProcessData*) fdata;
      }
      item->data[cmd] = entry;
   }
   pdata->error = pclose(fd);
   return pdata;
}

static inline void addLine(const char* line, Vector* lines, Panel* panel, const char* incFilter) {
   Vector_add(lines, (Object*) ListItem_new(line, 0));
   if (!incFilter || String_contains_i(line, incFilter))
      Panel_add(panel, (Object*)Vector_get(lines, Vector_size(lines)-1));
}

static void OpenFilesScreen_scan(OpenFilesScreen* htop_this, Vector* lines, IncSet* inc) {
   Panel* panel = htop_this->display;
   int idx = Panel_getSelectedIndex(panel);
   Panel_prune(panel);
   OpenFiles_ProcessData* pdata = OpenFilesScreen_getProcessData(htop_this->pid);
   if (pdata->error == 127) {
      addLine("Could not execute 'lsof'. Please make sure it is available in your $PATH.", lines, panel, IncSet_filter(inc));
   } else if (pdata->error == 1) {
      addLine("Failed listing open files.", lines, panel, IncSet_filter(inc));
   } else {
      OpenFiles_FileData* fdata = pdata->files;
      while (fdata) {
         char entry[1024];
         sprintf(entry, "%5s %4s %10s %10s %10s %s",
            fdata->data['f'] ? fdata->data['f'] : "",
            fdata->data['t'] ? fdata->data['t'] : "",
            fdata->data['D'] ? fdata->data['D'] : "",
            fdata->data['s'] ? fdata->data['s'] : "",
            fdata->data['i'] ? fdata->data['i'] : "",
            fdata->data['n'] ? fdata->data['n'] : "");
         addLine(entry, lines, panel, IncSet_filter(inc));
         for (int i = 0; i < 255; i++)
            if (fdata->data[i])
               free(fdata->data[i]);
         OpenFiles_FileData* old = fdata;
         fdata = fdata->next;
         free(old);
      }
      for (int i = 0; i < 255; i++)
         if (pdata->data[i])
            free(pdata->data[i]);
   }
   free(pdata);
   Vector_insertionSort(lines);
   Vector_insertionSort(panel->items);
   Panel_setSelected(panel, idx);
}

void OpenFilesScreen_run(OpenFilesScreen* htop_this) {
   Panel* panel = htop_this->display;
   Panel_setHeader(panel, "   FD TYPE     DEVICE       SIZE       NODE NAME");

   FunctionBar* bar = FunctionBar_new(ofsFunctions, ofsKeys, ofsEvents);
   IncSet* inc = IncSet_new(bar);
   
   Vector* lines = Vector_new(panel->items->type, true, DEFAULT_SIZE);

   OpenFilesScreen_scan(htop_this, lines, inc);
   OpenFilesScreen_draw(htop_this, inc);
   
   bool looping = true;
   while (looping) {
   
      Panel_draw(panel, true);
      
      if (inc->active)
         move(LINES-1, CRT_cursorX);
      int ch = getch();
      
      if (ch == KEY_MOUSE) {
         MEVENT mevent;
         int ok = getmouse(&mevent);
         if (ok == OK)
            if (mevent.y >= panel->y && mevent.y < LINES - 1) {
               Panel_setSelected(panel, mevent.y - panel->y + panel->scrollV);
               ch = 0;
            } if (mevent.y == LINES - 1)
               ch = FunctionBar_synthesizeEvent(inc->bar, mevent.x);
      }

      if (inc->active) {
         IncSet_handleKey(inc, ch, panel, IncSet_getListItemValue, lines);
         continue;
      }

      switch(ch) {
      case ERR:
         continue;
      case KEY_F(3):
      case '/':
         IncSet_activate(inc, INC_SEARCH);
         break;
      case KEY_F(4):
      case '\\':
         IncSet_activate(inc, INC_FILTER);
         break;
      case KEY_F(5):
         clear();
         OpenFilesScreen_scan(htop_this, lines, inc);
         OpenFilesScreen_draw(htop_this, inc);
         break;
      case '\014': // Ctrl+L
         clear();
         OpenFilesScreen_draw(htop_this, inc);
         break;
      case 'q':
      case 27:
      case KEY_F(10):
         looping = false;
         break;
      case KEY_RESIZE:
         Panel_resize(panel, COLS, LINES-2);
         OpenFilesScreen_draw(htop_this, inc);
         break;
      default:
         Panel_onKey(panel, ch);
      }
   }

   Vector_delete(lines);
   FunctionBar_delete((Object*)bar);
   IncSet_delete(inc);
}
