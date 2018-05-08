/*
htop - ProcessList.c
(C) 2004,2005 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ProcessList.h"
#include "Platform.h"

#include "CRT.h"
#include "StringUtils.h"

#include <stdlib.h>
#include <string.h>

/*{
#include "Vector.h"
#include "Hashtable.h"
#include "UsersTable.h"
#include "Panel.h"
#include "Process.h"
#include "Settings.h"

#ifdef HAVE_LIBHWLOC
#include <hwloc.h>
#endif

#ifndef MAX_NAME
#define MAX_NAME 128
#endif

#ifndef MAX_READ
#define MAX_READ 2048
#endif

typedef struct ProcessList_ {
   Settings* settings;

   Vector* processes;
   Vector* processes2;
   Hashtable* processTable;
   UsersTable* usersTable;

   Panel* panel;
   int following;
   uid_t userId;
   const char* incFilter;
   Hashtable* pidWhiteList;

   #ifdef HAVE_LIBHWLOC
   hwloc_topology_t topology;
   bool topologyOk;
   #endif

   int totalTasks;
   int runningTasks;
   int userlandThreads;
   int kernelThreads;

   unsigned long long int totalMem;
   unsigned long long int usedMem;
   unsigned long long int freeMem;
   unsigned long long int sharedMem;
   unsigned long long int buffersMem;
   unsigned long long int cachedMem;
   unsigned long long int totalSwap;
   unsigned long long int usedSwap;
   unsigned long long int freeSwap;

   int cpuCount;

} ProcessList;

ProcessList* ProcessList_new(UsersTable* ut, Hashtable* pidWhiteList, uid_t userId);
void ProcessList_delete(ProcessList* pl);
void ProcessList_goThroughEntries(ProcessList* pl);

}*/

ProcessList* ProcessList_init(ProcessList* this, ObjectClass* klass, UsersTable* usersTable, Hashtable* pidWhiteList, uid_t userId) {
   this->processes = Vector_new(klass, true, DEFAULT_SIZE);
   this->processTable = Hashtable_new(140, false);
   this->usersTable = usersTable;
   this->pidWhiteList = pidWhiteList;
   this->userId = userId;
   
   // tree-view auxiliary buffer
   this->processes2 = Vector_new(klass, true, DEFAULT_SIZE);
   
   // set later by platform-specific code
   this->cpuCount = 0;

#ifdef HAVE_LIBHWLOC
   this->topologyOk = false;
   int topoErr = hwloc_topology_init(&this->topology);
   if (topoErr == 0) {
      topoErr = hwloc_topology_load(this->topology);
   }
   if (topoErr == 0) {
      this->topologyOk = true;
   }
#endif

   this->following = -1;

   return this;
}

void ProcessList_done(ProcessList* this) {
#ifdef HAVE_LIBHWLOC
   if (this->topologyOk) {
      hwloc_topology_destroy(this->topology);
   }
#endif
   Hashtable_delete(this->processTable);
   Vector_delete(this->processes);
   Vector_delete(this->processes2);
}

void ProcessList_setPanel(ProcessList* this, Panel* panel) {
   this->panel = panel;
}

void ProcessList_printHeader(ProcessList* this, RichString* header) {
   RichString_prune(header);
   ProcessField* fields = this->settings->fields;
   for (int i = 0; fields[i]; i++) {
      const char* field = Process_fields[fields[i]].title;
      if (!field) field = "- ";
      if (!this->settings->treeView && this->settings->sortKey == fields[i])
         RichString_append(header, CRT_colors[PANEL_SELECTION_FOCUS], field);
      else
         RichString_append(header, CRT_colors[PANEL_HEADER_FOCUS], field);
   }
}

void ProcessList_add(ProcessList* this, Process* p) {
   assert(Vector_indexOf(this->processes, p, Process_pidCompare) == -1);
   assert(Hashtable_get(this->processTable, p->pid) == NULL);
   
   Vector_add(this->processes, p);
   Hashtable_put(this->processTable, p->pid, p);
   
   assert(Vector_indexOf(this->processes, p, Process_pidCompare) != -1);
   assert(Hashtable_get(this->processTable, p->pid) != NULL);
   assert(Hashtable_count(this->processTable) == Vector_count(this->processes));
}

void ProcessList_remove(ProcessList* this, Process* p) {
   assert(Vector_indexOf(this->processes, p, Process_pidCompare) != -1);
   assert(Hashtable_get(this->processTable, p->pid) != NULL);
   Process* pp = Hashtable_remove(this->processTable, p->pid);
   assert(pp == p); (void)pp;
   unsigned int pid = p->pid;
   int idx = Vector_indexOf(this->processes, p, Process_pidCompare);
   assert(idx != -1);
   if (idx >= 0) Vector_remove(this->processes, idx);
   assert(Hashtable_get(this->processTable, pid) == NULL); (void)pid;
   assert(Hashtable_count(this->processTable) == Vector_count(this->processes));
}

Process* ProcessList_get(ProcessList* this, int idx) {
   return (Process*) (Vector_get(this->processes, idx));
}

int ProcessList_size(ProcessList* this) {
   return (Vector_size(this->processes));
}

static void blank_out_descendant_generations_ct(ProcessList * this) {
    for (int i = 0; i < Vector_size(this->processes); i++) {
        Process * process = (Process *) (Vector_get(this->processes, i));
        process->descendant_generations_ct = -1;
    }
}

static void fill_descendant_generations_ct_in_leaf_nodes(ProcessList * this) {
    unsigned int leaf_nodes = 0;
    unsigned int non_leaf_nodes = 0;
    for (int i = 0; i < Vector_size(this->processes); i++) {
        Process * p0 = (Process *) (Vector_get(this->processes, i));
        unsigned int p0_children = 0;
        for (int j = 0; j < Vector_size(this->processes); j++) {
            if (i != j) {
                Process * p1 = (Process *) (Vector_get(this->processes, j));
                if (p0->pid == p1->ppid) {
                    p0_children++;
                }
            }
        }
        p0->direct_children_ct = p0_children;
        if (p0_children == 0) {
            p0->descendant_generations_ct = 0;
            leaf_nodes++;
        } else {
            non_leaf_nodes++;
        }
    }
}

static unsigned int some_empty_descendant_generations_ct(ProcessList * this) {
    //   Member field `descendant_generations_ct` is NULL for yet some
    // processes.

    unsigned int returning = 0;
    for (int i = 0; i < Vector_size(this->processes); i++) {
        Process * process = (Process *) (Vector_get(this->processes, i));
        if (process->descendant_generations_ct == -1) {
            returning += 1;
        }
    }
    return returning;
}

static void fill_descendant_generations_ct(ProcessList * this) {
    //   XXX Missing tree-ish algorithm and likely so an appropriate
    // supporting data structure for a tree-ish algorithm.

    int last_filled_descendant_generations_ct;
    fill_descendant_generations_ct_in_leaf_nodes(this);
    last_filled_descendant_generations_ct = 0;
    unsigned int keep_iterating = true;
    do {
        //   Find a process with a final value of `cum_percent_cpu`.
        for (int i = 0; i < Vector_size(this->processes); i++) {
            Process * p0 = (Process *) (Vector_get(this->processes, i));
            if (p0->descendant_generations_ct ==
                    last_filled_descendant_generations_ct) {
                //   Find the parent process and add the cumulative
                // percent cpu of the child.
                for (int j = 0; j < Vector_size(this->processes); j++) {
                    if (i != j) {
                        Process * p1 = (Process *)
                                       (Vector_get(this->processes, j));
                        if (p0->ppid == p1->pid) {
                            p1->cum_percent_cpu += p0->cum_percent_cpu;
                            p1->descendant_generations_ct =
                                    p0->descendant_generations_ct + 1;
                        }
                    }
                }
            }
        }
        last_filled_descendant_generations_ct++;
        keep_iterating = some_empty_descendant_generations_ct(this);
    } while (keep_iterating > 0);
}

static void copy_results_to_second_list(ProcessList * this) {
    for (int i = 0; i < Vector_size(this->processes); i++) {
        Process * p0 = (Process *) (Vector_get(this->processes, i));
        for (int j = 0; j < Vector_size(this->processes2); j++) {
            Process * p1 = (Process *) (Vector_get(this->processes2, j));
            if (p0->pid == p1->pid) {
                p1->cum_percent_cpu = p0->cum_percent_cpu;
            }
        }
    }
}

static void compute_cumulatives(ProcessList * this) {
    blank_out_descendant_generations_ct(this);
    fill_descendant_generations_ct(this);
    copy_results_to_second_list(this);  //   XXX
}

static void ProcessList_buildTree(ProcessList* this, pid_t pid, int level, int indent, int direction, bool show) {
   Vector* children = Vector_new(Class(Process), false, DEFAULT_SIZE);

   for (int i = Vector_size(this->processes) - 1; i >= 0; i--) {
      Process* process = (Process*) (Vector_get(this->processes, i));
      if (process->show && Process_isChildOf(process, pid)) {
         process = (Process*) (Vector_take(this->processes, i));
         Vector_add(children, process);
      }
   }
   int size = Vector_size(children);
   for (int i = 0; i < size; i++) {
      Process* process = (Process*) (Vector_get(children, i));
      if (!show)
         process->show = false;
      int s = this->processes2->items;
      if (direction == 1)
         Vector_add(this->processes2, process);
      else
         Vector_insert(this->processes2, 0, process);
      assert(this->processes2->items == s+1); (void)s;
      int nextIndent = indent | (1 << level);
      ProcessList_buildTree(this, process->pid, level+1, (i < size - 1) ? nextIndent : indent, direction, show ? process->showChildren : false);
      if (i == size - 1)
         process->indent = -nextIndent;
      else
         process->indent = nextIndent;
   }
   Vector_delete(children);
}

void ProcessList_sort(ProcessList* this) {
   if (!this->settings->treeView) {
      Vector_insertionSort(this->processes);
   } else {
      // Save settings
      int direction = this->settings->direction;
      int sortKey = this->settings->sortKey;
      // Sort by PID
      this->settings->sortKey = PID;
      this->settings->direction = 1;
      Vector_quickSort(this->processes);
      // Restore settings
      this->settings->sortKey = sortKey;
      this->settings->direction = direction;
      int vsize = Vector_size(this->processes);
      // Find all processes whose parent is not visible
      int size;
      while ((size = Vector_size(this->processes))) {
         int i;
         for (i = 0; i < size; i++) {
            Process* process = (Process*)(Vector_get(this->processes, i));
            // Immediately consume not shown processes
            if (!process->show) {
               process = (Process*)(Vector_take(this->processes, i));
               process->indent = 0;
               Vector_add(this->processes2, process);
               compute_cumulatives(this);  //   XXX
               ProcessList_buildTree(this, process->pid, 0, 0, direction, false);
               break;
            }
            pid_t ppid = Process_getParentPid(process);
            // Bisect the process vector to find parent
            int l = 0, r = size;
            // If PID corresponds with PPID (e.g. "kernel_task" (PID:0, PPID:0)
            // on Mac OS X 10.11.6) cancel bisecting and regard this process as
            // root.
            if (process->pid == ppid)
               r = 0;
            while (l < r) {
               int c = (l + r) / 2;
               pid_t pid = ((Process*)(Vector_get(this->processes, c)))->pid;
               if (ppid == pid) {
                  break;
               } else if (ppid < pid) {
                  r = c;
               } else {
                  l = c + 1;
               }
            }
            // If parent not found, then construct the tree with this root
            if (l >= r) {
               process = (Process*)(Vector_take(this->processes, i));
               process->indent = 0;
               Vector_add(this->processes2, process);
               compute_cumulatives(this);  //   XXX
               ProcessList_buildTree(this, process->pid, 0, 0, direction, process->showChildren);
               break;
            }
         }
         // There should be no loop in the process tree
         assert(i < size);
      }
      assert(Vector_size(this->processes2) == vsize); (void)vsize;
      assert(Vector_size(this->processes) == 0);
      // Swap listings around
      Vector* t = this->processes;
      this->processes = this->processes2;
      this->processes2 = t;
   }
}


ProcessField ProcessList_keyAt(ProcessList* this, int at) {
   int x = 0;
   ProcessField* fields = this->settings->fields;
   ProcessField field;
   for (int i = 0; (field = fields[i]); i++) {
      const char* title = Process_fields[field].title;
      if (!title) title = "- ";
      int len = strlen(title);
      if (at >= x && at <= x + len) {
         return field;
      }
      x += len;
   }
   return COMM;
}

void ProcessList_expandTree(ProcessList* this) {
   int size = Vector_size(this->processes);
   for (int i = 0; i < size; i++) {
      Process* process = (Process*) Vector_get(this->processes, i);
      process->showChildren = true;
   }
}

void ProcessList_rebuildPanel(ProcessList* this) {
   const char* incFilter = this->incFilter;

   int currPos = Panel_getSelectedIndex(this->panel);
   pid_t currPid = this->following != -1 ? this->following : 0;
   int currScrollV = this->panel->scrollV;

   Panel_prune(this->panel);
   int size = ProcessList_size(this);
   int idx = 0;
   for (int i = 0; i < size; i++) {
      bool hidden = false;
      Process* p = ProcessList_get(this, i);

      if ( (!p->show)
         || (this->userId != (uid_t) -1 && (p->st_uid != this->userId))
         || (incFilter && !(String_contains_i(p->comm, incFilter)))
         || (this->pidWhiteList && !Hashtable_get(this->pidWhiteList, p->tgid)) )
         hidden = true;

      if (!hidden) {
         Panel_set(this->panel, idx, (Object*)p);
         if ((this->following == -1 && idx == currPos) || (this->following != -1 && p->pid == currPid)) {
            Panel_setSelected(this->panel, idx);
            this->panel->scrollV = currScrollV;
         }
         idx++;
      }
   }
}

Process* ProcessList_getProcess(ProcessList* this, pid_t pid, bool* preExisting, Process_New constructor) {
   Process* proc = (Process*) Hashtable_get(this->processTable, pid);
   *preExisting = proc;
   if (proc) {
      assert(Vector_indexOf(this->processes, proc, Process_pidCompare) != -1);
      assert(proc->pid == pid);
   } else {
      proc = constructor(this->settings);
      assert(proc->comm == NULL);
      proc->pid = pid;
   }
   return proc;
}

void ProcessList_scan(ProcessList* this) {

   // mark all process as "dirty"
   for (int i = 0; i < Vector_size(this->processes); i++) {
      Process* p = (Process*) Vector_get(this->processes, i);
      p->updated = false;
      p->show = true;
   }

   this->totalTasks = 0;
   this->userlandThreads = 0;
   this->kernelThreads = 0;
   this->runningTasks = 0;

   ProcessList_goThroughEntries(this);
   
   for (int i = Vector_size(this->processes) - 1; i >= 0; i--) {
      Process* p = (Process*) Vector_get(this->processes, i);
      if (p->updated == false)
         ProcessList_remove(this, p);
      else
         p->updated = false;
   }
}
