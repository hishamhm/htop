/*
htop - ListItem.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ListItem.h"

#include "CRT.h"
#include "String.h"
#include "RichString.h"

#include <string.h>
#include <assert.h>
#include <stdlib.h>

/*{
#include "Object.h"

typedef struct ListItem_ {
   Object super;
   char* value;
   int key;
} ListItem;

}*/

static void ListItem_delete(Object* cast) {
   ListItem* htop_this = (ListItem*)cast;
   free(htop_this->value);
   free(htop_this);
}

static void ListItem_display(Object* cast, RichString* out) {
   ListItem* htop_this = (ListItem*)cast;
   assert (htop_this != NULL);
   /*
   int len = strlen(htop_this->value)+1;
   char buffer[len+1];
   snprintf(buffer, len, "%s", htop_this->value);
   */
   RichString_write(out, CRT_colors[DEFAULT_COLOR], htop_this->value/*buffer*/);
}

ObjectClass ListItem_class = {
   .display = ListItem_display,
   .delete = ListItem_delete,
   .compare = ListItem_compare
};

ListItem* ListItem_new(const char* value, int key) {
   ListItem* htop_this = AllocThis(htop_this, ListItem);
   htop_this->value = strdup(value);
   htop_this->key = key;
   return htop_this;

}

void ListItem_append(ListItem* htop_this, const char* text) {
   int oldLen = strlen(htop_this->value);
   int textLen = strlen(text);
   int newLen = strlen(htop_this->value) + textLen;
   htop_this->value = realloc(htop_this->value, newLen + 1);
   memcpy(htop_this->value + oldLen, text, textLen);
   htop_this->value[newLen] = '\0';
}

const char* ListItem_getRef(ListItem* htop_this) {
   return htop_this->value;
}

int ListItem_compare(const void* cast1, const void* cast2) {
   ListItem* obj1 = (ListItem*) cast1;
   ListItem* obj2 = (ListItem*) cast2;
   return strcmp(obj1->value, obj2->value);
}

