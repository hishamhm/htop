/*
htop - Affinity.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Affinity.h"

#include <stdlib.h>

/*{

typedef struct Affinity_ {
   int size;
   int used;
   int* cpus;
} Affinity;

}*/

Affinity* Affinity_new() {
   Affinity* htop_this = calloc(1, sizeof(Affinity));
   htop_this->size = 8;
   htop_this->cpus = calloc(htop_this->size, sizeof(int));
   return htop_this;
}

void Affinity_delete(Affinity* htop_this) {
   free(htop_this->cpus);
   free(htop_this);
}

void Affinity_add(Affinity* htop_this, int id) {
   if (htop_this->used == htop_this->size) {
      htop_this->size *= 2;
      htop_this->cpus = realloc(htop_this->cpus, sizeof(int) * htop_this->size);
   }
   htop_this->cpus[htop_this->used] = id;
   htop_this->used++;
}

