/*
htop - ProcessList.c
(C) 2004,2005 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ProcessList.h"

#include "CRT.h"
#include "String.h"

#include <sys/time.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <stdbool.h>
#include <stdarg.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

/*{
#include "Vector.h"
#include "Hashtable.h"
#include "UsersTable.h"
#include "Panel.h"
#include "Process.h"

#ifndef PROCDIR
#define PROCDIR "/proc"
#endif

#ifndef PROCSTATFILE
#define PROCSTATFILE PROCDIR "/stat"
#endif

#ifndef PROCMEMINFOFILE
#define PROCMEMINFOFILE PROCDIR "/meminfo"
#endif

#ifndef MAX_NAME
#define MAX_NAME 128
#endif

#ifndef MAX_READ
#define MAX_READ 2048
#endif

#ifndef ProcessList_cpuId
#define ProcessList_cpuId(pl, cpu) ((pl)->countCPUsFromZero ? (cpu) : (cpu)+1)
#endif

typedef enum TreeStr_ {
   TREE_STR_HORZ,
   TREE_STR_VERT,
   TREE_STR_RTEE,
   TREE_STR_BEND,
   TREE_STR_TEND,
   TREE_STR_OPEN,
   TREE_STR_SHUT,
   TREE_STR_COUNT
} TreeStr;

typedef struct CPUData_ {
   unsigned long long int totalTime;
   unsigned long long int userTime;
   unsigned long long int systemTime;
   unsigned long long int systemAllTime;
   unsigned long long int idleAllTime;
   unsigned long long int idleTime;
   unsigned long long int niceTime;
   unsigned long long int ioWaitTime;
   unsigned long long int irqTime;
   unsigned long long int softIrqTime;
   unsigned long long int stealTime;
   unsigned long long int guestTime;
   
   unsigned long long int totalPeriod;
   unsigned long long int userPeriod;
   unsigned long long int systemPeriod;
   unsigned long long int systemAllPeriod;
   unsigned long long int idleAllPeriod;
   unsigned long long int idlePeriod;
   unsigned long long int nicePeriod;
   unsigned long long int ioWaitPeriod;
   unsigned long long int irqPeriod;
   unsigned long long int softIrqPeriod;
   unsigned long long int stealPeriod;
   unsigned long long int guestPeriod;
} CPUData;

typedef struct ProcessList_ {
   Vector* processes;
   Vector* processes2;
   Hashtable* processTable;
   UsersTable* usersTable;

   Panel* panel;
   int following;
   bool userOnly;
   uid_t userId;
   const char* incFilter;
   Hashtable* pidWhiteList;

   int cpuCount;
   int totalTasks;
   int userlandThreads;
   int kernelThreads;
   int runningTasks;

   #ifdef HAVE_LIBHWLOC
   hwloc_topology_t topology;
   bool topologyOk;
   #endif
   CPUData* cpus;

   unsigned long long int totalMem;
   unsigned long long int usedMem;
   unsigned long long int freeMem;
   unsigned long long int sharedMem;
   unsigned long long int buffersMem;
   unsigned long long int cachedMem;
   unsigned long long int totalSwap;
   unsigned long long int usedSwap;
   unsigned long long int freeSwap;

   int flags;
   ProcessField* fields;
   ProcessField sortKey;
   int direction;
   bool hideThreads;
   bool shadowOtherUsers;
   bool showThreadNames;
   bool showingThreadNames;
   bool hideKernelThreads;
   bool hideUserlandThreads;
   bool treeView;
   bool highlightBaseName;
   bool highlightMegabytes;
   bool highlightThreads;
   bool detailedCPUTime;
   bool countCPUsFromZero;
   bool updateProcessNames;
   bool accountGuestInCPUMeter;
   const char **treeStr;

} ProcessList;

}*/

static ProcessField defaultHeaders[] = { PID, USER, PRIORITY, NICE, M_SIZE, M_RESIDENT, M_SHARE, STATE, PERCENT_CPU, PERCENT_MEM, TIME, COMM, 0 };

const char *ProcessList_treeStrAscii[TREE_STR_COUNT] = {
   "-", // TREE_STR_HORZ
   "|", // TREE_STR_VERT
   "`", // TREE_STR_RTEE
   "`", // TREE_STR_BEND
   ",", // TREE_STR_TEND
   "+", // TREE_STR_OPEN
   "-", // TREE_STR_SHUT
};

const char *ProcessList_treeStrUtf8[TREE_STR_COUNT] = {
   "\xe2\x94\x80", // TREE_STR_HORZ ─
   "\xe2\x94\x82", // TREE_STR_VERT │
   "\xe2\x94\x9c", // TREE_STR_RTEE ├
   "\xe2\x94\x94", // TREE_STR_BEND └
   "\xe2\x94\x8c", // TREE_STR_TEND ┌
   "+",            // TREE_STR_OPEN +
   "\xe2\x94\x80", // TREE_STR_SHUT ─
};

static ssize_t xread(int fd, void *buf, size_t count) {
  // Read some bytes. Retry on EINTR and when we don't get as many bytes as we requested.
  size_t alreadyRead = 0;
  start:;
  ssize_t res = read(fd, buf, count);
  if (res == -1 && errno == EINTR) goto start;
  if (res > 0) {
    buf = ((char*)buf)+res;
    count -= res;
    alreadyRead += res;
  }
  if (res == -1) return -1;
  if (count == 0 || res == 0) return alreadyRead;
  goto start;
}

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidWhiteList) {
   ProcessList* htop_this;
   htop_this = calloc(1, sizeof(ProcessList));
   htop_this->processes = Vector_new(Class(Process), true, DEFAULT_SIZE);
   htop_this->processTable = Hashtable_new(140, false);
   htop_this->usersTable = usersTable;
   htop_this->pidWhiteList = pidWhiteList;
   
   /* tree-view auxiliary buffers */
   htop_this->processes2 = Vector_new(Class(Process), true, DEFAULT_SIZE);
   
   FILE* file = fopen(PROCSTATFILE, "r");
   if (file == NULL) {
      CRT_fatalError("Cannot open " PROCSTATFILE);
   }
   char buffer[256];
   int cpus = -1;
   do {
      cpus++;
      fgets(buffer, 255, file);
   } while (String_startsWith(buffer, "cpu"));
   fclose(file);
   htop_this->cpuCount = cpus - 1;

#ifdef HAVE_LIBHWLOC
   htop_this->topologyOk = false;
   int topoErr = hwloc_topology_init(&htop_this->topology);
   if (topoErr == 0) {
      topoErr = hwloc_topology_load(htop_this->topology);
      htop_this->topologyOk = true;
   }
#endif
   htop_this->cpus = calloc(cpus, sizeof(CPUData));

   for (int i = 0; i < cpus; i++) {
      htop_this->cpus[i].totalTime = 1;
      htop_this->cpus[i].totalPeriod = 1;
   }

   htop_this->fields = calloc(LAST_PROCESSFIELD+1, sizeof(ProcessField));
   // TODO: turn 'fields' into a Vector,
   // (and ProcessFields into proper objects).
   htop_this->flags = 0;
   for (int i = 0; defaultHeaders[i]; i++) {
      htop_this->fields[i] = defaultHeaders[i];
      htop_this->fields[i] |= Process_fieldFlags[defaultHeaders[i]];
   }
   htop_this->sortKey = PERCENT_CPU;
   htop_this->direction = 1;
   htop_this->hideThreads = false;
   htop_this->shadowOtherUsers = false;
   htop_this->showThreadNames = false;
   htop_this->showingThreadNames = false;
   htop_this->hideKernelThreads = false;
   htop_this->hideUserlandThreads = false;
   htop_this->treeView = false;
   htop_this->highlightBaseName = false;
   htop_this->highlightMegabytes = false;
   htop_this->detailedCPUTime = false;
   htop_this->countCPUsFromZero = false;
   htop_this->updateProcessNames = false;
   htop_this->treeStr = NULL;
   htop_this->following = -1;

   if (CRT_utf8)
      htop_this->treeStr = CRT_utf8 ? ProcessList_treeStrUtf8 : ProcessList_treeStrAscii;

   return htop_this;
}

void ProcessList_delete(ProcessList* htop_this) {
   Hashtable_delete(htop_this->processTable);
   Vector_delete(htop_this->processes);
   Vector_delete(htop_this->processes2);
   free(htop_this->cpus);
   free(htop_this->fields);
   free(htop_this);
}

void ProcessList_setPanel(ProcessList* htop_this, Panel* panel) {
   htop_this->panel = panel;
}

void ProcessList_invertSortOrder(ProcessList* htop_this) {
   if (htop_this->direction == 1)
      htop_this->direction = -1;
   else
      htop_this->direction = 1;
}

void ProcessList_printHeader(ProcessList* htop_this, RichString* header) {
   RichString_prune(header);
   ProcessField* fields = htop_this->fields;
   for (int i = 0; fields[i]; i++) {
      const char* field = Process_fieldTitles[fields[i]];
      if (!htop_this->treeView && htop_this->sortKey == fields[i])
         RichString_append(header, CRT_colors[PANEL_HIGHLIGHT_FOCUS], field);
      else
         RichString_append(header, CRT_colors[PANEL_HEADER_FOCUS], field);
   }
}

static void ProcessList_add(ProcessList* htop_this, Process* p) {
   assert(Vector_indexOf(htop_this->processes, p, Process_pidCompare) == -1);
   assert(Hashtable_get(htop_this->processTable, p->pid) == NULL);
   
   Vector_add(htop_this->processes, p);
   Hashtable_put(htop_this->processTable, p->pid, p);
   
   assert(Vector_indexOf(htop_this->processes, p, Process_pidCompare) != -1);
   assert(Hashtable_get(htop_this->processTable, p->pid) != NULL);
   assert(Hashtable_count(htop_this->processTable) == Vector_count(htop_this->processes));
}

static void ProcessList_remove(ProcessList* htop_this, Process* p) {
   assert(Vector_indexOf(htop_this->processes, p, Process_pidCompare) != -1);
   assert(Hashtable_get(htop_this->processTable, p->pid) != NULL);
   Process* pp = Hashtable_remove(htop_this->processTable, p->pid);
   assert(pp == p); (void)pp;
   unsigned int pid = p->pid;
   int idx = Vector_indexOf(htop_this->processes, p, Process_pidCompare);
   assert(idx != -1);
   if (idx >= 0) Vector_remove(htop_this->processes, idx);
   assert(Hashtable_get(htop_this->processTable, pid) == NULL); (void)pid;
   assert(Hashtable_count(htop_this->processTable) == Vector_count(htop_this->processes));
}

Process* ProcessList_get(ProcessList* htop_this, int idx) {
   return (Process*) (Vector_get(htop_this->processes, idx));
}

int ProcessList_size(ProcessList* htop_this) {
   return (Vector_size(htop_this->processes));
}

static void ProcessList_buildTree(ProcessList* htop_this, pid_t pid, int level, int indent, int direction, bool show) {
   Vector* children = Vector_new(Class(Process), false, DEFAULT_SIZE);

   for (int i = Vector_size(htop_this->processes) - 1; i >= 0; i--) {
      Process* process = (Process*) (Vector_get(htop_this->processes, i));
      if (process->tgid == pid || (process->tgid == process->pid && process->ppid == pid)) {
         process = (Process*) (Vector_take(htop_this->processes, i));
         Vector_add(children, process);
      }
   }
   int size = Vector_size(children);
   for (int i = 0; i < size; i++) {
      Process* process = (Process*) (Vector_get(children, i));
      if (!show)
         process->show = false;
      int s = htop_this->processes2->items;
      if (direction == 1)
         Vector_add(htop_this->processes2, process);
      else
         Vector_insert(htop_this->processes2, 0, process);
      assert(htop_this->processes2->items == s+1); (void)s;
      int nextIndent = indent | (1 << level);
      ProcessList_buildTree(htop_this, process->pid, level+1, (i < size - 1) ? nextIndent : indent, direction, show ? process->showChildren : false);
      if (i == size - 1)
         process->indent = -nextIndent;
      else
         process->indent = nextIndent;
   }
   Vector_delete(children);
}

void ProcessList_sort(ProcessList* htop_this) {
   if (!htop_this->treeView) {
      Vector_insertionSort(htop_this->processes);
   } else {
      // Save settings
      int direction = htop_this->direction;
      int sortKey = htop_this->sortKey;
      // Sort by PID
      htop_this->sortKey = PID;
      htop_this->direction = 1;
      Vector_quickSort(htop_this->processes);
      // Restore settings
      htop_this->sortKey = sortKey;
      htop_this->direction = direction;
      // Take PID 1 as root and add to the new listing
      int vsize = Vector_size(htop_this->processes);
      Process* init = (Process*) (Vector_take(htop_this->processes, 0));
      // This assertion crashes on hardened kernels.
      // I wonder how well tree view works on those systems.
      // assert(init->pid == 1);
      init->indent = 0;
      Vector_add(htop_this->processes2, init);
      // Recursively empty list
      ProcessList_buildTree(htop_this, init->pid, 0, 0, direction, true);
      // Add leftovers
      while (Vector_size(htop_this->processes)) {
         Process* p = (Process*) (Vector_take(htop_this->processes, 0));
         p->indent = 0;
         Vector_add(htop_this->processes2, p);
         ProcessList_buildTree(htop_this, p->pid, 0, 0, direction, p->showChildren);
      }
      assert(Vector_size(htop_this->processes2) == vsize); (void)vsize;
      assert(Vector_size(htop_this->processes) == 0);
      // Swap listings around
      Vector* t = htop_this->processes;
      htop_this->processes = htop_this->processes2;
      htop_this->processes2 = t;
   }
}

static bool ProcessList_readStatFile(Process *process, const char* dirname, const char* name, char* command) {
   char filename[MAX_NAME+1];
   snprintf(filename, MAX_NAME, "%s/%s/stat", dirname, name);
   int fd = open(filename, O_RDONLY);
   if (fd == -1)
      return false;

   static char buf[MAX_READ+1];

   int size = xread(fd, buf, MAX_READ);
   close(fd);
   if (!size) return false;
   buf[size] = '\0';

   assert(process->pid == atoi(buf));
   char *location = strchr(buf, ' ');
   if (!location) return false;

   location += 2;
   char *end = strrchr(location, ')');
   if (!end) return false;
   
   int commsize = end - location;
   memcpy(command, location, commsize);
   command[commsize] = '\0';
   location = end + 2;

   process->state = location[0];
   location += 2;
   process->ppid = strtol(location, &location, 10);
   location += 1;
   process->pgrp = strtoul(location, &location, 10);
   location += 1;
   process->session = strtoul(location, &location, 10);
   location += 1;
   process->tty_nr = strtoul(location, &location, 10);
   location += 1;
   process->tpgid = strtol(location, &location, 10);
   location += 1;
   process->flags = strtoul(location, &location, 10);
   location += 1;
   location = strchr(location, ' ')+1;
   location = strchr(location, ' ')+1;
   location = strchr(location, ' ')+1;
   location = strchr(location, ' ')+1;
   process->utime = strtoull(location, &location, 10);
   location += 1;
   process->stime = strtoull(location, &location, 10);
   location += 1;
   process->cutime = strtoull(location, &location, 10);
   location += 1;
   process->cstime = strtoull(location, &location, 10);
   location += 1;
   process->priority = strtol(location, &location, 10);
   location += 1;
   process->nice = strtol(location, &location, 10);
   location += 1;
   process->nlwp = strtol(location, &location, 10);
   location += 1;
   for (int i=0; i<17; i++) location = strchr(location, ' ')+1;
   process->exit_signal = strtol(location, &location, 10);
   location += 1;
   assert(location != NULL);
   process->processor = strtol(location, &location, 10);
   assert(location == NULL);

   return true;
}

static bool ProcessList_statProcessDir(Process* process, const char* dirname, char* name, time_t curTime) {
   char filename[MAX_NAME+1];
   filename[MAX_NAME] = '\0';

   snprintf(filename, MAX_NAME, "%s/%s", dirname, name);
   struct stat sstat;
   int statok = stat(filename, &sstat);
   if (statok == -1)
      return false;
   process->st_uid = sstat.st_uid;
  
   struct tm date;
   time_t ctime = sstat.st_ctime;
   process->starttime_ctime = ctime;
   (void) localtime_r((time_t*) &ctime, &date);
   strftime(process->starttime_show, 7, ((ctime > curTime - 86400) ? "%R " : "%b%d "), &date);
   
   return true;
}

#ifdef HAVE_TASKSTATS

static void ProcessList_readIoFile(Process* process, const char* dirname, char* name, unsigned long long now) {
   char filename[MAX_NAME+1];
   filename[MAX_NAME] = '\0';

   snprintf(filename, MAX_NAME, "%s/%s/io", dirname, name);
   int fd = open(filename, O_RDONLY);
   if (fd == -1)
      return;
   
   char buffer[1024];
   ssize_t buflen = xread(fd, buffer, 1023);
   close(fd);
   if (buflen < 1) return;
   buffer[buflen] = '\0';
   unsigned long long last_read = process->io_read_bytes;
   unsigned long long last_write = process->io_write_bytes;
   char *buf = buffer;
   char *line = NULL;
   while ((line = strsep(&buf, "\n")) != NULL) {
      switch (line[0]) {
      case 'r':
         if (line[1] == 'c' && strncmp(line+2, "har: ", 5) == 0)
            process->io_rchar = strtoull(line+7, NULL, 10);
         else if (strncmp(line+1, "ead_bytes: ", 11) == 0) {
            process->io_read_bytes = strtoull(line+12, NULL, 10);
            process->io_rate_read_bps = 
               ((double)(process->io_read_bytes - last_read))/(((double)(now - process->io_rate_read_time))/1000);
            process->io_rate_read_time = now;
         }
         break;
      case 'w':
         if (line[1] == 'c' && strncmp(line+2, "har: ", 5) == 0)
            process->io_wchar = strtoull(line+7, NULL, 10);
         else if (strncmp(line+1, "rite_bytes: ", 12) == 0) {
            process->io_write_bytes = strtoull(line+13, NULL, 10);
            process->io_rate_write_bps = 
               ((double)(process->io_write_bytes - last_write))/(((double)(now - process->io_rate_write_time))/1000);
            process->io_rate_write_time = now;
         }
         break;
      case 's':
         if (line[5] == 'r' && strncmp(line+1, "yscr: ", 6) == 0)
            process->io_syscr = strtoull(line+7, NULL, 10);
         else if (strncmp(line+1, "yscw: ", 6) == 0)
            sscanf(line, "syscw: %llu", &process->io_syscw);
            process->io_syscw = strtoull(line+7, NULL, 10);
         break;
      case 'c':
         if (strncmp(line+1, "ancelled_write_bytes: ", 22) == 0)
           process->io_cancelled_write_bytes = strtoull(line+23, NULL, 10);
      }
   }
}

#endif

static bool ProcessList_readStatmFile(Process* process, const char* dirname, const char* name) {
   char filename[MAX_NAME+1];
   snprintf(filename, MAX_NAME, "%s/%s/statm", dirname, name);
   int fd = open(filename, O_RDONLY);
   if (fd == -1)
      return false;
   char buf[256];
   ssize_t rres = xread(fd, buf, 255);
   close(fd);
   if (rres < 1) return false;

   char *p = buf;
   errno = 0;
   process->m_size = strtol(p, &p, 10); if (*p == ' ') p++;
   process->m_resident = strtol(p, &p, 10); if (*p == ' ') p++;
   process->m_share = strtol(p, &p, 10); if (*p == ' ') p++;
   process->m_trs = strtol(p, &p, 10); if (*p == ' ') p++;
   process->m_lrs = strtol(p, &p, 10); if (*p == ' ') p++;
   process->m_drs = strtol(p, &p, 10); if (*p == ' ') p++;
   process->m_dt = strtol(p, &p, 10);
   return (errno == 0);
}

#ifdef HAVE_OPENVZ

static void ProcessList_readOpenVZData(Process* process, const char* dirname, const char* name) {
   if (access("/proc/vz", R_OK) != 0) {
      process->vpid = process->pid;
      process->ctid = 0;
      return;
   }
   char filename[MAX_NAME+1];
   snprintf(filename, MAX_NAME, "%s/%s/stat", dirname, name);
   FILE* file = fopen(filename, "r");
   if (!file) 
      return;
   fscanf(file, 
      "%*32u %*32s %*1c %*32u %*32u %*32u %*32u %*32u %*32u %*32u "
      "%*32u %*32u %*32u %*32u %*32u %*32u %*32u %*32u "
      "%*32u %*32u %*32u %*32u %*32u %*32u %*32u %*32u "
      "%*32u %*32u %*32u %*32u %*32u %*32u %*32u %*32u "
      "%*32u %*32u %*32u %*32u %*32u %*32u %*32u %*32u "
      "%*32u %*32u %*32u %*32u %*32u %*32u %*32u "
      "%*32u %*32u %32u %32u",
      &process->vpid, &process->ctid);
   fclose(file);
}

#endif

#ifdef HAVE_CGROUP

static void ProcessList_readCGroupFile(Process* process, const char* dirname, const char* name) {
   char filename[MAX_NAME+1];
   snprintf(filename, MAX_NAME, "%s/%s/cgroup", dirname, name);
   FILE* file = fopen(filename, "r");
   if (!file) {
      process->cgroup = strdup("");
      return;
   }
   char buffer[256];
   char *ok = fgets(buffer, 255, file);
   if (ok) {
      char* trimmed = String_trim(buffer);
      int nFields;
      char** fields = String_split(trimmed, ':', &nFields);
      free(trimmed);
      if (nFields >= 3) {
         process->cgroup = strndup(fields[2] + 1, 10);
      } else {
         process->cgroup = strdup("");
      }
      String_freeArray(fields);
   }
   fclose(file);
}

#endif

#ifdef HAVE_VSERVER

static void ProcessList_readVServerData(Process* process, const char* dirname, const char* name) {
   char filename[MAX_NAME+1];
   snprintf(filename, MAX_NAME, "%s/%s/status", dirname, name);
   FILE* file = fopen(filename, "r");
   if (!file)
      return;
   char buffer[256];
   process->vxid = 0;
   while (fgets(buffer, 255, file)) {
      if (String_startsWith(buffer, "VxID:")) {
         int vxid;
         int ok = sscanf(buffer, "VxID:\t%d", &vxid);
         if (ok >= 1) {
            process->vxid = vxid;
         }
      }
      #if defined HAVE_ANCIENT_VSERVER
      else if (String_startsWith(buffer, "s_context:")) {
         int vxid;
         int ok = sscanf(buffer, "s_context:\t%d", &vxid);
         if (ok >= 1) {
            process->vxid = vxid;
         }
      }
      #endif
   }
   fclose(file);
}

#endif

static bool ProcessList_readCmdlineFile(Process* process, const char* dirname, const char* name) {
   if (Process_isKernelThread(process))
      return true;

   char filename[MAX_NAME+1];
   snprintf(filename, MAX_NAME, "%s/%s/cmdline", dirname, name);
   int fd = open(filename, O_RDONLY);
   if (fd == -1)
      return false;
         
   char command[4096+1]; // max cmdline length on Linux
   int amtRead = xread(fd, command, sizeof(command) - 1);
   close(fd);
   if (amtRead > 0) {
      for (int i = 0; i < amtRead; i++)
         if (command[i] == '\0' || command[i] == '\n') {
            command[i] = ' ';
         }
   }
   command[amtRead] = '\0';
   free(process->comm);
   process->comm = strdup(command);

   return true;
}


static bool ProcessList_processEntries(ProcessList* htop_this, const char* dirname, Process* parent, double period, struct timeval tv) {
   DIR* dir;
   struct dirent* entry;

   time_t curTime = tv.tv_sec;
   #ifdef HAVE_TASKSTATS
   unsigned long long now = tv.tv_sec*1000+tv.tv_usec/1000;
   #endif

   dir = opendir(dirname);
   if (!dir) return false;
   int cpus = htop_this->cpuCount;
   bool hideKernelThreads = htop_this->hideKernelThreads;
   bool hideUserlandThreads = htop_this->hideUserlandThreads;
   while ((entry = readdir(dir)) != NULL) {
      char* name = entry->d_name;

      // The RedHat kernel hides threads with a dot.
      // I believe htop_this is non-standard.
      if ((!htop_this->hideThreads) && name[0] == '.') {
         name++;
      }

      // Just skip all non-number directories.
      if (name[0] <= '0' || name[0] >= '9')
         continue;

      // filename is a number: process directory
      int pid = atoi(name);
     
      if (parent && pid == parent->pid)
         continue;

      if (pid <= 0) 
         continue;

      Process* process = NULL;
      Process* existingProcess = (Process*) Hashtable_get(htop_this->processTable, pid);

      if (existingProcess) {
         assert(Vector_indexOf(htop_this->processes, existingProcess, Process_pidCompare) != -1);
         process = existingProcess;
         assert(process->pid == pid);
      } else {
         process = Process_new(htop_this);
         assert(process->comm == NULL);
         process->pid = pid;
         process->tgid = parent ? parent->pid : pid;
      }

      char subdirname[MAX_NAME+1];
      snprintf(subdirname, MAX_NAME, "%s/%s/task", dirname, name);
      ProcessList_processEntries(htop_this, subdirname, process, period, tv);

      #ifdef HAVE_TASKSTATS
      if (htop_this->flags & PROCESS_FLAG_IO)
         ProcessList_readIoFile(process, dirname, name, now);
      #endif

      if (! ProcessList_readStatmFile(process, dirname, name))
         goto errorReadingProcess;

      process->show = ! ((hideKernelThreads && Process_isKernelThread(process)) || (hideUserlandThreads && Process_isUserlandThread(process)));

      char command[MAX_NAME+1];
      unsigned long long int lasttimes = (process->utime + process->stime);
      if (! ProcessList_readStatFile(process, dirname, name, command))
         goto errorReadingProcess;
      if (htop_this->flags & PROCESS_FLAG_IOPRIO)
         Process_updateIOPriority(process);
      float percent_cpu = (process->utime + process->stime - lasttimes) / period * 100.0;
      process->percent_cpu = MAX(MIN(percent_cpu, cpus*100.0), 0.0);
      if (isnan(process->percent_cpu)) process->percent_cpu = 0.0;
      process->percent_mem = (process->m_resident * PAGE_SIZE_KB) / (double)(htop_this->totalMem) * 100.0;

      if(!existingProcess) {

         if (! ProcessList_statProcessDir(process, dirname, name, curTime))
            goto errorReadingProcess;

         process->user = UsersTable_getRef(htop_this->usersTable, process->st_uid);

         #ifdef HAVE_OPENVZ
         if (htop_this->flags & PROCESS_FLAG_OPENVZ)
            ProcessList_readOpenVZData(process, dirname, name);
         #endif

         #ifdef HAVE_CGROUP
         if (htop_this->flags & PROCESS_FLAG_CGROUP)
            ProcessList_readCGroupFile(process, dirname, name);
         #endif
         
         #ifdef HAVE_VSERVER
         if (htop_this->flags & PROCESS_FLAG_VSERVER)
            ProcessList_readVServerData(process, dirname, name);
         #endif
         
         if (! ProcessList_readCmdlineFile(process, dirname, name))
            goto errorReadingProcess;

         ProcessList_add(htop_this, process);
      } else {
         if (htop_this->updateProcessNames) {
            if (! ProcessList_readCmdlineFile(process, dirname, name))
               goto errorReadingProcess;
         }
      }

      if (process->state == 'Z') {
         free(process->comm);
         process->comm = strdup(command);
      } else if (Process_isThread(process)) {
         if (htop_this->showThreadNames || Process_isKernelThread(process) || process->state == 'Z') {
            free(process->comm);
            process->comm = strdup(command);
         } else if (htop_this->showingThreadNames) {
            if (! ProcessList_readCmdlineFile(process, dirname, name))
               goto errorReadingProcess;
         }
         if (Process_isKernelThread(process)) {
            htop_this->kernelThreads++;
         } else {
            htop_this->userlandThreads++;
         }
      }

      htop_this->totalTasks++;
      if (process->state == 'R')
         htop_this->runningTasks++;
      process->updated = true;

      continue;

      // Exception handler.
      errorReadingProcess: {
         if (process->comm) {
            free(process->comm);
            process->comm = NULL;
         }
         if (existingProcess)
            ProcessList_remove(htop_this, process);
         else
            Process_delete((Object*)process);
      }
   }
   closedir(dir);
   return true;
}

void ProcessList_scan(ProcessList* htop_this) {
   unsigned long long int usertime, nicetime, systemtime, systemalltime, idlealltime, idletime, totaltime, virtalltime;
   unsigned long long int swapFree = 0;

   FILE* file = fopen(PROCMEMINFOFILE, "r");
   if (file == NULL) {
      CRT_fatalError("Cannot open " PROCMEMINFOFILE);
   }
   int cpus = htop_this->cpuCount;
   {
      char buffer[128];
      while (fgets(buffer, 128, file)) {
   
         switch (buffer[0]) {
         case 'M':
            if (String_startsWith(buffer, "MemTotal:"))
               sscanf(buffer, "MemTotal: %llu kB", &htop_this->totalMem);
            else if (String_startsWith(buffer, "MemFree:"))
               sscanf(buffer, "MemFree: %llu kB", &htop_this->freeMem);
            else if (String_startsWith(buffer, "MemShared:"))
               sscanf(buffer, "MemShared: %llu kB", &htop_this->sharedMem);
            break;
         case 'B':
            if (String_startsWith(buffer, "Buffers:"))
               sscanf(buffer, "Buffers: %llu kB", &htop_this->buffersMem);
            break;
         case 'C':
            if (String_startsWith(buffer, "Cached:"))
               sscanf(buffer, "Cached: %llu kB", &htop_this->cachedMem);
            break;
         case 'S':
            if (String_startsWith(buffer, "SwapTotal:"))
               sscanf(buffer, "SwapTotal: %llu kB", &htop_this->totalSwap);
            if (String_startsWith(buffer, "SwapFree:"))
               sscanf(buffer, "SwapFree: %llu kB", &swapFree);
            break;
         }
      }
   }

   htop_this->usedMem = htop_this->totalMem - htop_this->freeMem;
   htop_this->usedSwap = htop_this->totalSwap - swapFree;
   fclose(file);

   file = fopen(PROCSTATFILE, "r");
   if (file == NULL) {
      CRT_fatalError("Cannot open " PROCSTATFILE);
   }
   for (int i = 0; i <= cpus; i++) {
      char buffer[256];
      int cpuid;
      unsigned long long int ioWait, irq, softIrq, steal, guest, guestnice;
      ioWait = irq = softIrq = steal = guest = guestnice = 0;
      // Dependending on your kernel version,
      // 5, 7, 8 or 9 of these fields will be set.
      // The rest will remain at zero.
      fgets(buffer, 255, file);
      if (i == 0)
         sscanf(buffer, "cpu  %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu", &usertime, &nicetime, &systemtime, &idletime, &ioWait, &irq, &softIrq, &steal, &guest, &guestnice);
      else {
         sscanf(buffer, "cpu%d %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu", &cpuid, &usertime, &nicetime, &systemtime, &idletime, &ioWait, &irq, &softIrq, &steal, &guest, &guestnice);
         assert(cpuid == i - 1);
      }
      // Guest time is already accounted in usertime
      usertime = usertime - guest;
      nicetime = nicetime - guestnice;
      // Fields existing on kernels >= 2.6
      // (and RHEL's patched kernel 2.4...)
      idlealltime = idletime + ioWait;
      systemalltime = systemtime + irq + softIrq;
      virtalltime = guest + guestnice;
      totaltime = usertime + nicetime + systemalltime + idlealltime + steal + virtalltime;
      CPUData* cpuData = &(htop_this->cpus[i]);
      assert (usertime >= cpuData->userTime);
      assert (nicetime >= cpuData->niceTime);
      assert (systemtime >= cpuData->systemTime);
      assert (idletime >= cpuData->idleTime);
      assert (totaltime >= cpuData->totalTime);
      assert (systemalltime >= cpuData->systemAllTime);
      assert (idlealltime >= cpuData->idleAllTime);
      assert (ioWait >= cpuData->ioWaitTime);
      assert (irq >= cpuData->irqTime);
      assert (softIrq >= cpuData->softIrqTime);
      assert (steal >= cpuData->stealTime);
      assert (virtalltime >= cpuData->guestTime);
      cpuData->userPeriod = usertime - cpuData->userTime;
      cpuData->nicePeriod = nicetime - cpuData->niceTime;
      cpuData->systemPeriod = systemtime - cpuData->systemTime;
      cpuData->systemAllPeriod = systemalltime - cpuData->systemAllTime;
      cpuData->idleAllPeriod = idlealltime - cpuData->idleAllTime;
      cpuData->idlePeriod = idletime - cpuData->idleTime;
      cpuData->ioWaitPeriod = ioWait - cpuData->ioWaitTime;
      cpuData->irqPeriod = irq - cpuData->irqTime;
      cpuData->softIrqPeriod = softIrq - cpuData->softIrqTime;
      cpuData->stealPeriod = steal - cpuData->stealTime;
      cpuData->guestPeriod = virtalltime - cpuData->guestTime;
      cpuData->totalPeriod = totaltime - cpuData->totalTime;
      cpuData->userTime = usertime;
      cpuData->niceTime = nicetime;
      cpuData->systemTime = systemtime;
      cpuData->systemAllTime = systemalltime;
      cpuData->idleAllTime = idlealltime;
      cpuData->idleTime = idletime;
      cpuData->ioWaitTime = ioWait;
      cpuData->irqTime = irq;
      cpuData->softIrqTime = softIrq;
      cpuData->stealTime = steal;
      cpuData->guestTime = virtalltime;
      cpuData->totalTime = totaltime;
   }
   double period = (double)htop_this->cpus[0].totalPeriod / cpus; fclose(file);

   // mark all process as "dirty"
   for (int i = 0; i < Vector_size(htop_this->processes); i++) {
      Process* p = (Process*) Vector_get(htop_this->processes, i);
      p->updated = false;
   }
   
   htop_this->totalTasks = 0;
   htop_this->userlandThreads = 0;
   htop_this->kernelThreads = 0;
   htop_this->runningTasks = 0;

   struct timeval tv;
   gettimeofday(&tv, NULL);
   ProcessList_processEntries(htop_this, PROCDIR, NULL, period, tv);
   
   htop_this->showingThreadNames = htop_this->showThreadNames;
   
   for (int i = Vector_size(htop_this->processes) - 1; i >= 0; i--) {
      Process* p = (Process*) Vector_get(htop_this->processes, i);
      if (p->updated == false)
         ProcessList_remove(htop_this, p);
      else
         p->updated = false;
   }

}

ProcessField ProcessList_keyAt(ProcessList* htop_this, int at) {
   int x = 0;
   ProcessField* fields = htop_this->fields;
   ProcessField field;
   for (int i = 0; (field = fields[i]); i++) {
      int len = strlen(Process_fieldTitles[field]);
      if (at >= x && at <= x + len) {
         return field;
      }
      x += len;
   }
   return COMM;
}

void ProcessList_expandTree(ProcessList* htop_this) {
   int size = Vector_size(htop_this->processes);
   for (int i = 0; i < size; i++) {
      Process* process = (Process*) Vector_get(htop_this->processes, i);
      process->showChildren = true;
   }
}

void ProcessList_rebuildPanel(ProcessList* htop_this, bool flags, int following, bool userOnly, uid_t userId, const char* incFilter) {
   if (!flags) {
      following = htop_this->following;
      userOnly = htop_this->userOnly;
      userId = htop_this->userId;
      incFilter = htop_this->incFilter;
   } else {
      htop_this->following = following;
      htop_this->userOnly = userOnly;
      htop_this->userId = userId;
      htop_this->incFilter = incFilter;
   }

   int currPos = Panel_getSelectedIndex(htop_this->panel);
   pid_t currPid = following != -1 ? following : 0;
   int currScrollV = htop_this->panel->scrollV;

   Panel_prune(htop_this->panel);
   int size = ProcessList_size(htop_this);
   int idx = 0;
   for (int i = 0; i < size; i++) {
      bool hidden = false;
      Process* p = ProcessList_get(htop_this, i);

      if ( (!p->show)
         || (userOnly && (p->st_uid != userId))
         || (incFilter && !(String_contains_i(p->comm, incFilter)))
         || (htop_this->pidWhiteList && !Hashtable_get(htop_this->pidWhiteList, p->pid)) )
         hidden = true;

      if (!hidden) {
         Panel_set(htop_this->panel, idx, (Object*)p);
         if ((following == -1 && idx == currPos) || (following != -1 && p->pid == currPid)) {
            Panel_setSelected(htop_this->panel, idx);
            htop_this->panel->scrollV = currScrollV;
         }
         idx++;
      }
   }
}
