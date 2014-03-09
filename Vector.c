/*
htop - Vector.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Vector.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/*{
#include "Object.h"

#define swap(a_,x_,y_) do{ void* tmp_ = a_[x_]; a_[x_] = a_[y_]; a_[y_] = tmp_; }while(0)

#ifndef DEFAULT_SIZE
#define DEFAULT_SIZE -1
#endif

typedef struct Vector_ {
   Object **array;
   ObjectClass* type;
   int arraySize;
   int growthRate;
   int items;
   bool owner;
} Vector;

}*/

Vector* Vector_new(ObjectClass* type, bool owner, int size) {
   Vector* htop_this;

   if (size == DEFAULT_SIZE)
      size = 10;
   htop_this = (Vector*) malloc(sizeof(Vector));
   htop_this->growthRate = size;
   htop_this->array = (Object**) calloc(size, sizeof(Object*));
   htop_this->arraySize = size;
   htop_this->items = 0;
   htop_this->type = type;
   htop_this->owner = owner;
   return htop_this;
}

void Vector_delete(Vector* htop_this) {
   if (htop_this->owner) {
      for (int i = 0; i < htop_this->items; i++)
         if (htop_this->array[i])
            Object_delete(htop_this->array[i]);
   }
   free(htop_this->array);
   free(htop_this);
}

#ifdef DEBUG

static inline bool Vector_isConsistent(Vector* htop_this) {
   assert(htop_this->items <= htop_this->arraySize);
   if (htop_this->owner) {
      for (int i = 0; i < htop_this->items; i++)
         if (htop_this->array[i] && !Object_isA(htop_this->array[i], htop_this->type))
            return false;
      return true;
   } else {
      return true;
   }
}

int Vector_count(Vector* htop_this) {
   int items = 0;
   for (int i = 0; i < htop_this->items; i++) {
      if (htop_this->array[i])
         items++;
   }
   assert(items == htop_this->items);
   return items;
}

#endif

void Vector_prune(Vector* htop_this) {
   assert(Vector_isConsistent(htop_this));
   int i;

   if (htop_this->owner) {
      for (i = 0; i < htop_this->items; i++)
         if (htop_this->array[i]) {
            Object_delete(htop_this->array[i]);
            //htop_this->array[i] = NULL;
         }
   }
   htop_this->items = 0;
}

static int comparisons = 0;

static int partition(Object** array, int left, int right, int pivotIndex, Object_Compare compare) {
   void* pivotValue = array[pivotIndex];
   swap(array, pivotIndex, right);
   int storeIndex = left;
   for (int i = left; i < right; i++) {
      comparisons++;
      if (compare(array[i], pivotValue) <= 0) {
         swap(array, i, storeIndex);
         storeIndex++;
      }
   }
   swap(array, storeIndex, right);
   return storeIndex;
}

static void quickSort(Object** array, int left, int right, Object_Compare compare) {
   if (left >= right)
      return;
   int pivotIndex = (left+right) / 2;
   int pivotNewIndex = partition(array, left, right, pivotIndex, compare);
   quickSort(array, left, pivotNewIndex - 1, compare);
   quickSort(array, pivotNewIndex + 1, right, compare);
}

// If I were to use only one sorting algorithm for both cases, it would probably be htop_this one:
/*

static void combSort(Object** array, int left, int right, Object_Compare compare) {
   int gap = right - left;
   bool swapped = true;
   while ((gap > 1) || swapped) {
      if (gap > 1) {
         gap = (int)((double)gap / 1.247330950103979);
      }
      swapped = false;
      for (int i = left; gap + i <= right; i++) {
         comparisons++;
         if (compare(array[i], array[i+gap]) > 0) {
            swap(array, i, i+gap);
            swapped = true;
         }
      }
   }
}

*/

static void insertionSort(Object** array, int left, int right, Object_Compare compare) {
   for (int i = left+1; i <= right; i++) {
      void* t = array[i];
      int j = i - 1;
      while (j >= left) {
         comparisons++;
         if (compare(array[j], t) <= 0)
            break;
         array[j+1] = array[j];
         j--;
      }
      array[j+1] = t;
   }
}

void Vector_quickSort(Vector* htop_this) {
   assert(htop_this->type->compare);
   assert(Vector_isConsistent(htop_this));
   quickSort(htop_this->array, 0, htop_this->items - 1, htop_this->type->compare);
   assert(Vector_isConsistent(htop_this));
}

void Vector_insertionSort(Vector* htop_this) {
   assert(htop_this->type->compare);
   assert(Vector_isConsistent(htop_this));
   insertionSort(htop_this->array, 0, htop_this->items - 1, htop_this->type->compare);
   assert(Vector_isConsistent(htop_this));
}

static void Vector_checkArraySize(Vector* htop_this) {
   assert(Vector_isConsistent(htop_this));
   if (htop_this->items >= htop_this->arraySize) {
      //int i;
      //i = htop_this->arraySize;
      htop_this->arraySize = htop_this->items + htop_this->growthRate;
      htop_this->array = (Object**) realloc(htop_this->array, sizeof(Object*) * htop_this->arraySize);
      //for (; i < htop_this->arraySize; i++)
      //   htop_this->array[i] = NULL;
   }
   assert(Vector_isConsistent(htop_this));
}

void Vector_insert(Vector* htop_this, int idx, void* data_) {
   Object* data = data_;
   assert(idx >= 0);
   assert(idx <= htop_this->items);
   assert(Object_isA(data, htop_this->type));
   assert(Vector_isConsistent(htop_this));
   
   Vector_checkArraySize(htop_this);
   //assert(htop_this->array[htop_this->items] == NULL);
   for (int i = htop_this->items; i > idx; i--) {
      htop_this->array[i] = htop_this->array[i-1];
   }
   htop_this->array[idx] = data;
   htop_this->items++;
   assert(Vector_isConsistent(htop_this));
}

Object* Vector_take(Vector* htop_this, int idx) {
   assert(idx >= 0 && idx < htop_this->items);
   assert(Vector_isConsistent(htop_this));
   Object* removed = htop_this->array[idx];
   //assert (removed != NULL);
   htop_this->items--;
   for (int i = idx; i < htop_this->items; i++)
      htop_this->array[i] = htop_this->array[i+1];
   //htop_this->array[htop_this->items] = NULL;
   assert(Vector_isConsistent(htop_this));
   return removed;
}

Object* Vector_remove(Vector* htop_this, int idx) {
   Object* removed = Vector_take(htop_this, idx);
   if (htop_this->owner) {
      Object_delete(removed);
      return NULL;
   } else
      return removed;
}

void Vector_moveUp(Vector* htop_this, int idx) {
   assert(idx >= 0 && idx < htop_this->items);
   assert(Vector_isConsistent(htop_this));
   if (idx == 0)
      return;
   Object* temp = htop_this->array[idx];
   htop_this->array[idx] = htop_this->array[idx - 1];
   htop_this->array[idx - 1] = temp;
}

void Vector_moveDown(Vector* htop_this, int idx) {
   assert(idx >= 0 && idx < htop_this->items);
   assert(Vector_isConsistent(htop_this));
   if (idx == htop_this->items - 1)
      return;
   Object* temp = htop_this->array[idx];
   htop_this->array[idx] = htop_this->array[idx + 1];
   htop_this->array[idx + 1] = temp;
}

void Vector_set(Vector* htop_this, int idx, void* data_) {
   Object* data = data_;
   assert(idx >= 0);
   assert(Object_isA((Object*)data, htop_this->type));
   assert(Vector_isConsistent(htop_this));

   Vector_checkArraySize(htop_this);
   if (idx >= htop_this->items) {
      htop_this->items = idx+1;
   } else {
      if (htop_this->owner) {
         Object* removed = htop_this->array[idx];
         assert (removed != NULL);
         if (htop_this->owner) {
            Object_delete(removed);
         }
      }
   }
   htop_this->array[idx] = data;
   assert(Vector_isConsistent(htop_this));
}

#ifdef DEBUG

inline Object* Vector_get(Vector* htop_this, int idx) {
   assert(idx < htop_this->items);
   assert(Vector_isConsistent(htop_this));
   return htop_this->array[idx];
}

#else

#define Vector_get(v_, idx_) ((v_)->array[idx_])

#endif

inline int Vector_size(Vector* htop_this) {
   assert(Vector_isConsistent(htop_this));
   return htop_this->items;
}

/*

static void Vector_merge(Vector* htop_this, Vector* v2) {
   int i;
   assert(Vector_isConsistent(htop_this));
   
   for (i = 0; i < v2->items; i++)
      Vector_add(htop_this, v2->array[i]);
   v2->items = 0;
   Vector_delete(v2);
   assert(Vector_isConsistent(htop_this));
}

*/

void Vector_add(Vector* htop_this, void* data_) {
   Object* data = data_;
   assert(Object_isA((Object*)data, htop_this->type));
   assert(Vector_isConsistent(htop_this));
   int i = htop_this->items;
   Vector_set(htop_this, htop_this->items, data);
   assert(htop_this->items == i+1); (void)(i);
   assert(Vector_isConsistent(htop_this));
}

inline int Vector_indexOf(Vector* htop_this, void* search_, Object_Compare compare) {
   Object* search = search_;
   assert(Object_isA((Object*)search, htop_this->type));
   assert(compare);
   assert(Vector_isConsistent(htop_this));
   for (int i = 0; i < htop_this->items; i++) {
      Object* o = (Object*)htop_this->array[i];
      assert(o);
      if (compare(search, o) == 0)
         return i;
   }
   return -1;
}
