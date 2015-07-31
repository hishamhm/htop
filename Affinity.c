/*
htop - Affinity.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Affinity.h"

#include <stdlib.h>

#ifdef HAVE_LIBHWLOC
#include <hwloc/linux.h>
#elif HAVE_NATIVE_AFFINITY
#include <sched.h>
#endif

/*{
#include "Process.h"
#include "ProcessList.h"

typedef struct Affinity_ {
   ProcessList* pl;
   int size;
   int used;
   int* cpus;
} Affinity;

}*/

Affinity* Affinity_new(ProcessList* pl) {
   Affinity* this = calloc(1, sizeof(Affinity));
   this->size = 8;
   this->cpus = calloc(this->size, sizeof(int));
   this->pl = pl;
   return this;
}

void Affinity_delete(Affinity* this) {
   free(this->cpus);
   free(this);
}

void Affinity_add(Affinity* this, int id) {
   if (this->used == this->size) {
      this->size *= 2;
      int* tmp_cpus = (int*) realloc(this->cpus, sizeof(int) * this->size);
      assert(tmp_cpus != NULL);
      this->cpus = tmp_cpus;
   }
   this->cpus[this->used] = id;
   this->used++;
}


#ifdef HAVE_LIBHWLOC

Affinity* Affinity_get(Process* proc, ProcessList* pl) {
   hwloc_cpuset_t cpuset = hwloc_bitmap_alloc();
   bool ok = (hwloc_linux_get_tid_cpubind(pl->topology, proc->pid, cpuset) == 0);
   Affinity* affinity = NULL;
   if (ok) {
      affinity = Affinity_new(pl);
      if (hwloc_bitmap_last(cpuset) == -1) {
         for (int i = 0; i < pl->cpuCount; i++) {
            Affinity_add(affinity, i);
         }
      } else {
         unsigned int id;
         hwloc_bitmap_foreach_begin(id, cpuset);
            Affinity_add(affinity, id);
         hwloc_bitmap_foreach_end();
      }
   }
   hwloc_bitmap_free(cpuset);
   return affinity;
}

bool Affinity_set(Process* proc, Affinity* this) {
   hwloc_cpuset_t cpuset = hwloc_bitmap_alloc();
   for (int i = 0; i < affinity->used; i++) {
      hwloc_bitmap_set(cpuset, affinity->cpus[i]);
   }
   bool ok = (hwloc_linux_set_tid_cpubind(this->pl->topology, proc->pid, cpuset) == 0);
   hwloc_bitmap_free(cpuset);
   return ok;
}

#elif HAVE_NATIVE_AFFINITY

Affinity* Affinity_get(Process* proc, ProcessList* pl) {
   cpu_set_t cpuset;
   bool ok = (sched_getaffinity(proc->pid, sizeof(cpu_set_t), &cpuset) == 0);
   if (!ok) return NULL;
   Affinity* affinity = Affinity_new(pl);
   for (int i = 0; i < pl->cpuCount; i++) {
      if (CPU_ISSET(i, &cpuset))
         Affinity_add(affinity, i);
   }
   return affinity;
}

bool Affinity_set(Process* proc, Affinity* this) {
   cpu_set_t cpuset;
   CPU_ZERO(&cpuset);
   for (int i = 0; i < this->used; i++) {
      CPU_SET(this->cpus[i], &cpuset);
   }
   bool ok = (sched_setaffinity(proc->pid, sizeof(unsigned long), &cpuset) == 0);
   return ok;
}

#endif
