/*
htop - FunctionBar.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "FunctionBar.h"

#include "CRT.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

/*{
#include "Object.h"

typedef struct FunctionBar_ {
   Object super;
   int size;
   char** functions;
   char** keys;
   int* events;
   bool staticData;
} FunctionBar;

}*/

static const char* FunctionBar_FKeys[] = {"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", NULL};

static const char* FunctionBar_FLabels[] = {"      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", NULL};

static int FunctionBar_FEvents[] = {KEY_F(1), KEY_F(2), KEY_F(3), KEY_F(4), KEY_F(5), KEY_F(6), KEY_F(7), KEY_F(8), KEY_F(9), KEY_F(10)};

ObjectClass FunctionBar_class = {
   .htop_delete = FunctionBar_delete
};

FunctionBar* FunctionBar_new(const char** functions, const char** keys, int* events) {
   FunctionBar* htop_this = AllocThis(htop_this,FunctionBar);
   htop_this->functions = (char**) functions;
   if (keys && events) {
      htop_this->staticData = false; 
      htop_this->functions = malloc(sizeof(char*) * 15);
      htop_this->keys = malloc(sizeof(char*) * 15);
      htop_this->events = malloc(sizeof(int) * 15);
      int i = 0;
      while (i < 15 && functions[i]) {
         htop_this->functions[i] = strdup(functions[i]);
         htop_this->keys[i] = strdup(keys[i]);
         htop_this->events[i] = events[i];
         i++;
      }
      htop_this->size = i;
   } else {
      htop_this->staticData = true;
      htop_this->functions = (char**)( functions ? functions : FunctionBar_FLabels );
      htop_this->keys = (char**) FunctionBar_FKeys;
      htop_this->events = FunctionBar_FEvents;
      htop_this->size = 10;
   }
   return htop_this;
}

void FunctionBar_delete(Object* cast) {
   FunctionBar* htop_this = (FunctionBar*) cast;
   if (!htop_this->staticData) {
      for (int i = 0; i < htop_this->size; i++) {
         free(htop_this->functions[i]);
         free(htop_this->keys[i]);
      }
      free(htop_this->functions);
      free(htop_this->keys);
      free(htop_this->events);
   }
   free(htop_this);
}

void FunctionBar_setLabel(FunctionBar* htop_this, int event, const char* text) {
   assert(!htop_this->staticData);
   for (int i = 0; i < htop_this->size; i++) {
      if (htop_this->events[i] == event) {
         free(htop_this->functions[i]);
         htop_this->functions[i] = strdup(text);
         break;
      }
   }
}

void FunctionBar_draw(const FunctionBar* htop_this, char* buffer) {
   FunctionBar_drawAttr(htop_this, buffer, CRT_colors[FUNCTION_BAR]);
}

void FunctionBar_drawAttr(const FunctionBar* htop_this, char* buffer, int attr) {
   attrset(CRT_colors[FUNCTION_BAR]);
   mvhline(LINES-1, 0, ' ', COLS);
   int x = 0;
   for (int i = 0; i < htop_this->size; i++) {
      attrset(CRT_colors[FUNCTION_KEY]);
      mvaddstr(LINES-1, x, htop_this->keys[i]);
      x += strlen(htop_this->keys[i]);
      attrset(CRT_colors[FUNCTION_BAR]);
      mvaddstr(LINES-1, x, htop_this->functions[i]);
      x += strlen(htop_this->functions[i]);
   }
   if (buffer) {
      attrset(attr);
      mvaddstr(LINES-1, x, buffer);
      CRT_cursorX = x + strlen(buffer);
      curs_set(1);
   } else {
      curs_set(0);
   }
   attrset(CRT_colors[RESET_COLOR]);
}

int FunctionBar_synthesizeEvent(const FunctionBar* htop_this, int pos) {
   int x = 0;
   for (int i = 0; i < htop_this->size; i++) {
      x += strlen(htop_this->keys[i]);
      x += strlen(htop_this->functions[i]);
      if (pos < x) {
         return htop_this->events[i];
      }
   }
   return ERR;
}
