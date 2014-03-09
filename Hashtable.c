/*
htop - Hashtable.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Hashtable.h"

#include <stdlib.h>
#include <assert.h>

/*{
#include <stdbool.h>

typedef struct Hashtable_ Hashtable;

typedef void(*Hashtable_PairFunction)(int, void*, void*);

typedef struct HashtableItem {
   unsigned int key;
   void* value;
   struct HashtableItem* next;
} HashtableItem;

struct Hashtable_ {
   int size;
   HashtableItem** buckets;
   int items;
   bool owner;
};
}*/

#ifdef DEBUG

static bool Hashtable_isConsistent(Hashtable* htop_this) {
   int items = 0;
   for (int i = 0; i < htop_this->size; i++) {
      HashtableItem* bucket = htop_this->buckets[i];
      while (bucket) {
         items++;
         bucket = bucket->next;
      }
   }
   return items == htop_this->items;
}

int Hashtable_count(Hashtable* htop_this) {
   int items = 0;
   for (int i = 0; i < htop_this->size; i++) {
      HashtableItem* bucket = htop_this->buckets[i];
      while (bucket) {
         items++;
         bucket = bucket->next;
      }
   }
   assert(items == htop_this->items);
   return items;
}

#endif

static HashtableItem* HashtableItem_new(unsigned int key, void* value) {
   HashtableItem* htop_this;
   
   htop_this = (HashtableItem*) malloc(sizeof(HashtableItem));
   htop_this->key = key;
   htop_this->value = value;
   htop_this->next = NULL;
   return htop_this;
}

Hashtable* Hashtable_new(int size, bool owner) {
   Hashtable* htop_this;
   
   htop_this = (Hashtable*) malloc(sizeof(Hashtable));
   htop_this->items = 0;
   htop_this->size = size;
   htop_this->buckets = (HashtableItem**) calloc(size, sizeof(HashtableItem*));
   htop_this->owner = owner;
   assert(Hashtable_isConsistent(htop_this));
   return htop_this;
}

void Hashtable_delete(Hashtable* htop_this) {
   assert(Hashtable_isConsistent(htop_this));
   for (int i = 0; i < htop_this->size; i++) {
      HashtableItem* walk = htop_this->buckets[i];
      while (walk != NULL) {
         if (htop_this->owner)
            free(walk->value);
         HashtableItem* savedWalk = walk;
         walk = savedWalk->next;
         free(savedWalk);
      }
   }
   free(htop_this->buckets);
   free(htop_this);
}

void Hashtable_put(Hashtable* htop_this, unsigned int key, void* value) {
   unsigned int index = key % htop_this->size;
   HashtableItem** bucketPtr = &(htop_this->buckets[index]);
   while (true)
      if (*bucketPtr == NULL) {
         *bucketPtr = HashtableItem_new(key, value);
         htop_this->items++;
         break;
      } else if ((*bucketPtr)->key == key) {
         if (htop_this->owner)
            free((*bucketPtr)->value);
         (*bucketPtr)->value = value;
         break;
      } else
         bucketPtr = &((*bucketPtr)->next);
   assert(Hashtable_isConsistent(htop_this));
}

void* Hashtable_remove(Hashtable* htop_this, unsigned int key) {
   unsigned int index = key % htop_this->size;
   
   assert(Hashtable_isConsistent(htop_this));

   HashtableItem** bucket; 
   for (bucket = &(htop_this->buckets[index]); *bucket; bucket = &((*bucket)->next) ) {
      if ((*bucket)->key == key) {
         void* value = (*bucket)->value;
         HashtableItem* next = (*bucket)->next;
         free(*bucket);
         (*bucket) = next;
         htop_this->items--;
         if (htop_this->owner) {
            free(value);
            assert(Hashtable_isConsistent(htop_this));
            return NULL;
         } else {
            assert(Hashtable_isConsistent(htop_this));
            return value;
         }
      }
   }
   assert(Hashtable_isConsistent(htop_this));
   return NULL;
}

inline void* Hashtable_get(Hashtable* htop_this, unsigned int key) {
   unsigned int index = key % htop_this->size;
   HashtableItem* bucketPtr = htop_this->buckets[index];
   while (true) {
      if (bucketPtr == NULL) {
         assert(Hashtable_isConsistent(htop_this));
         return NULL;
      } else if (bucketPtr->key == key) {
         assert(Hashtable_isConsistent(htop_this));
         return bucketPtr->value;
      } else
         bucketPtr = bucketPtr->next;
   }
}

void Hashtable_foreach(Hashtable* htop_this, Hashtable_PairFunction f, void* userData) {
   assert(Hashtable_isConsistent(htop_this));
   for (int i = 0; i < htop_this->size; i++) {
      HashtableItem* walk = htop_this->buckets[i];
      while (walk != NULL) {
         f(walk->key, walk->value, userData);
         walk = walk->next;
      }
   }
   assert(Hashtable_isConsistent(htop_this));
}
