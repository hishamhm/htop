/*
htop - Object.c
(C) 2004-2012 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Object.h"

/*{
#include "RichString.h"

typedef struct Object_ Object;

typedef void(*Object_Display)(Object*, RichString*);
typedef int(*Object_Compare)(const void*, const void*);
typedef void(*Object_Delete)(Object*);

#define Object_getClass(obj_)         ((Object*)(obj_))->klass
#define Object_setClass(obj_, class_) Object_getClass(obj_) = (ObjectClass*) class_

#define Object_delete(obj_)           Object_getClass(obj_)->htop_delete((Object*)(obj_))
#define Object_displayFn(obj_)        Object_getClass(obj_)->display
#define Object_display(obj_, str_)    Object_getClass(obj_)->display((Object*)(obj_), str_)
#define Object_compare(obj_, other_)  Object_getClass(obj_)->compare((const void*)(obj_), other_)

#define Class(class_)                 ((ObjectClass*)(&(class_ ## _class)))

#define AllocThis(var, class_) (class_*) malloc(sizeof(class_)); Object_setClass(var, Class(class_));
 
typedef struct ObjectClass_ {
   const void* extends;
   const Object_Display display;
   const Object_Delete htop_delete;
   const Object_Compare compare;
} ObjectClass;

struct Object_ {
   ObjectClass* klass;
};

}*/

ObjectClass Object_class = {
   .extends = NULL
};

#ifdef DEBUG

bool Object_isA(Object* o, const ObjectClass* klass) {
   if (!o)
      return false;
   const ObjectClass* type = o->klass;
   while (type) {
      if (type == klass)
         return true;
      type = type->extends;
   }
   return false;
}

#endif
