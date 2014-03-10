/*
htop - CheckItem.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "CheckItem.h"

#include "CRT.h"

#include <assert.h>
#include <stdlib.h>

/*{
#include "Object.h"

typedef struct CheckItem_ {
   Object super;
   char* text;
   bool value;
   bool* ref;
} CheckItem;

}*/

static void CheckItem_delete(Object* cast) {
   CheckItem* htop_this = (CheckItem*)cast;
   assert (htop_this != NULL);

   free(htop_this->text);
   free(htop_this);
}

static void CheckItem_display(Object* cast, RichString* out) {
   CheckItem* htop_this = (CheckItem*)cast;
   assert (htop_this != NULL);
   RichString_write(out, CRT_colors[CHECK_BOX], "[");
   if (CheckItem_get(htop_this))
      RichString_append(out, CRT_colors[CHECK_MARK], "x");
   else
      RichString_append(out, CRT_colors[CHECK_MARK], " ");
   RichString_append(out, CRT_colors[CHECK_BOX], "] ");
   RichString_append(out, CRT_colors[CHECK_TEXT], htop_this->text);
}

ObjectClass CheckItem_class = {
   .display = CheckItem_display,
   .delete = CheckItem_delete
};

CheckItem* CheckItem_new(char* text, bool* ref, bool value) {
   CheckItem* htop_this = AllocThis(htop_this, CheckItem);
   htop_this->text = text;
   htop_this->value = value;
   htop_this->ref = ref;
   return htop_this;
}

void CheckItem_set(CheckItem* htop_this, bool value) {
   if (htop_this->ref) 
      *(htop_this->ref) = value;
   else
      htop_this->value = value;
}

bool CheckItem_get(CheckItem* htop_this) {
   if (htop_this->ref) 
      return *(htop_this->ref);
   else
      return htop_this->value;
}
