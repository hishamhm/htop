/*
htop - LinuxProcessList.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "LinuxProcessList.h"
#include "LinuxProcess.h"
#include "CRT.h"
#include "StringUtils.h"
#include <errno.h>
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

#ifdef HAVE_DELAYACCT
#include <netlink/attr.h>
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <netlink/socket.h>
#include <netlink/msg.h>
#include <linux/taskstats.h>
#endif

/*{

#include "ProcessList.h"

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

typedef struct TtyDriver_ {
   char* path;
   unsigned int major;
   unsigned int minorFrom;
   unsigned int minorTo;
} TtyDriver;

typedef struct LinuxProcessScanData_ {
   const char* name;
   const char* dirname;
   pid_t mainProcess;
   double period;
   time_t nowSec;
   unsigned long long nowMsec;
   #ifdef HAVE_DELAYACCT
   struct nl_sock *netlink_socket;
   int netlink_family;
   #endif
} LinuxProcessScanData;

typedef struct LinuxProcessList_ {
   ProcessList super;

   CPUData* cpus;
   TtyDriver* ttyDrivers;

   LinuxProcessScanData psd;
} LinuxProcessList;

#ifndef PROCDIR
#define PROCDIR "/proc"
#endif

#ifndef PROCSTATFILE
#define PROCSTATFILE PROCDIR "/stat"
#endif

#ifndef PROCMEMINFOFILE
#define PROCMEMINFOFILE PROCDIR "/meminfo"
#endif

#ifndef PROCTTYDRIVERSFILE
#define PROCTTYDRIVERSFILE PROCDIR "/tty/drivers"
#endif

#ifndef PROC_LINE_LENGTH
#define PROC_LINE_LENGTH 512
#endif

}*/

#ifndef CLAMP
#define CLAMP(x,low,high) (((x)>(high))?(high):(((x)<(low))?(low):(x)))
#endif

static int sortTtyDrivers(const void* va, const void* vb) {
   TtyDriver* a = (TtyDriver*) va;
   TtyDriver* b = (TtyDriver*) vb;
   return (a->major == b->major) ? (a->minorFrom - b->minorFrom) : (a->major - b->major);
}

static void LinuxProcessList_initTtyDrivers(LinuxProcessList* this) {
   TtyDriver* ttyDrivers;
   int fd = open(PROCTTYDRIVERSFILE, O_RDONLY);
   if (fd == -1)
      return;
   char* buf = NULL;
   int bufSize = MAX_READ;
   int bufLen = 0;
   for(;;) {
      buf = realloc(buf, bufSize);
      int size = xRead(fd, buf + bufLen, MAX_READ);
      if (size <= 0) {
         buf[bufLen] = '\0';
         close(fd);
         break;
      }
      bufLen += size;
      bufSize += MAX_READ;
   }
   if (bufLen == 0) {
      free(buf);
      return;
   }
   int numDrivers = 0;
   int allocd = 10;
   ttyDrivers = malloc(sizeof(TtyDriver) * allocd);
   char* at = buf;
   while (*at != '\0') {
      at = strchr(at, ' ');    // skip first token
      while (*at == ' ') at++; // skip spaces
      char* token = at;        // mark beginning of path
      at = strchr(at, ' ');    // find end of path
      *at = '\0'; at++;        // clear and skip
      ttyDrivers[numDrivers].path = strdup(token); // save
      while (*at == ' ') at++; // skip spaces
      token = at;              // mark beginning of major
      at = strchr(at, ' ');    // find end of major
      *at = '\0'; at++;        // clear and skip
      ttyDrivers[numDrivers].major = atoi(token); // save
      while (*at == ' ') at++; // skip spaces
      token = at;              // mark beginning of minorFrom
      while (*at >= '0' && *at <= '9') at++; //find end of minorFrom
      if (*at == '-') {        // if has range
         *at = '\0'; at++;        // clear and skip
         ttyDrivers[numDrivers].minorFrom = atoi(token); // save
         token = at;              // mark beginning of minorTo
         at = strchr(at, ' ');    // find end of minorTo
         *at = '\0'; at++;        // clear and skip
         ttyDrivers[numDrivers].minorTo = atoi(token); // save
      } else {                 // no range
         *at = '\0'; at++;        // clear and skip
         ttyDrivers[numDrivers].minorFrom = atoi(token); // save
         ttyDrivers[numDrivers].minorTo = atoi(token); // save
      }
      at = strchr(at, '\n');   // go to end of line
      at++;                    // skip
      numDrivers++;
      if (numDrivers == allocd) {
         allocd += 10;
         ttyDrivers = realloc(ttyDrivers, sizeof(TtyDriver) * allocd);
      }
   }
   free(buf);
   numDrivers++;
   ttyDrivers = realloc(ttyDrivers, sizeof(TtyDriver) * numDrivers);
   ttyDrivers[numDrivers - 1].path = NULL;
   qsort(ttyDrivers, numDrivers - 1, sizeof(TtyDriver), sortTtyDrivers);
   this->ttyDrivers = ttyDrivers;
}

#ifdef HAVE_DELAYACCT

static void LinuxProcessList_initNetlinkSocket(LinuxProcessList* this) {
   this->psd.netlink_socket = nl_socket_alloc();
   if (this->psd.netlink_socket == NULL) {
      return;
   }
   if (nl_connect(this->psd.netlink_socket, NETLINK_GENERIC) < 0) {
      return;
   }
   this->psd.netlink_family = genl_ctrl_resolve(this->psd.netlink_socket, TASKSTATS_GENL_NAME);
}

#endif

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidWhiteList, uid_t userId) {
   LinuxProcessList* this = xCalloc(1, sizeof(LinuxProcessList));
   ProcessList* pl = &(this->super);
   ProcessList_init(pl, Class(LinuxProcess), usersTable, pidWhiteList, userId);

   LinuxProcessList_initTtyDrivers(this);

   #ifdef HAVE_DELAYACCT
   LinuxProcessList_initNetlinkSocket(this);
   #endif

   // Update CPU count:
   FILE* file = fopen(PROCSTATFILE, "r");
   if (file == NULL) {
      CRT_fatalError("Cannot open " PROCSTATFILE);
   }
   char buffer[PROC_LINE_LENGTH + 1];
   int cpus = -1;
   do {
      cpus++;
      char * s = fgets(buffer, PROC_LINE_LENGTH, file);
      (void) s;
   } while (String_startsWith(buffer, "cpu"));
   fclose(file);

   pl->cpuCount = MAX(cpus - 1, 1);
   this->cpus = xCalloc(cpus, sizeof(CPUData));

   for (int i = 0; i < cpus; i++) {
      this->cpus[i].totalTime = 1;
      this->cpus[i].totalPeriod = 1;
   }

   return pl;
}

void ProcessList_delete(ProcessList* pl) {
   LinuxProcessList* this = (LinuxProcessList*) pl;
   ProcessList_done(pl);
   free(this->cpus);
   if (this->ttyDrivers) {
      for(int i = 0; this->ttyDrivers[i].path; i++) {
         free(this->ttyDrivers[i].path);
      }
      free(this->ttyDrivers);
   }
   #ifdef HAVE_DELAYACCT
   if (this->psd.netlink_socket) {
      nl_close(this->psd.netlink_socket);
      nl_socket_free(this->psd.netlink_socket);
   }
   #endif
   free(this);
}

char* LinuxProcessList_updateTtyDevice(LinuxProcessList* this, unsigned int tty_nr) {
   TtyDriver* ttyDrivers = this->ttyDrivers;
   unsigned int maj = major(tty_nr);
   unsigned int min = minor(tty_nr);

   int i = -1;
   for (;;) {
      i++;
      if ((!ttyDrivers[i].path) || maj < ttyDrivers[i].major) {
         break;
      } 
      if (maj > ttyDrivers[i].major) {
         continue;
      }
      if (min < ttyDrivers[i].minorFrom) {
         break;
      } 
      if (min > ttyDrivers[i].minorTo) {
         continue;
      }
      unsigned int idx = min - ttyDrivers[i].minorFrom;
      struct stat sstat;
      char* fullPath;
      for(;;) {
         asprintf(&fullPath, "%s/%d", ttyDrivers[i].path, idx);
         int err = stat(fullPath, &sstat);
         if (err == 0 && major(sstat.st_rdev) == maj && minor(sstat.st_rdev) == min) return fullPath;
         free(fullPath);
         asprintf(&fullPath, "%s%d", ttyDrivers[i].path, idx);
         err = stat(fullPath, &sstat);
         if (err == 0 && major(sstat.st_rdev) == maj && minor(sstat.st_rdev) == min) return fullPath;
         free(fullPath);
         if (idx == min) break;
         idx = min;
      }
      int err = stat(ttyDrivers[i].path, &sstat);
      if (err == 0 && tty_nr == sstat.st_rdev) return strdup(ttyDrivers[i].path);
   }
   char* out;
   asprintf(&out, "/dev/%u:%u", maj, min);
   return out;
}

static inline void LinuxProcessList_scanMemoryInfo(ProcessList* this) {
   unsigned long long int swapFree = 0;
   unsigned long long int shmem = 0;
   unsigned long long int sreclaimable = 0;

   FILE* file = fopen(PROCMEMINFOFILE, "r");
   if (file == NULL) {
      CRT_fatalError("Cannot open " PROCMEMINFOFILE);
   }
   char buffer[128];
   while (fgets(buffer, 128, file)) {

      #define tryRead(label, variable) (String_startsWith(buffer, label) && sscanf(buffer + strlen(label), " %32llu kB", variable))
      switch (buffer[0]) {
      case 'M':
         if (tryRead("MemTotal:", &this->totalMem)) {}
         else if (tryRead("MemFree:", &this->freeMem)) {}
         else if (tryRead("MemShared:", &this->sharedMem)) {}
         break;
      case 'B':
         if (tryRead("Buffers:", &this->buffersMem)) {}
         break;
      case 'C':
         if (tryRead("Cached:", &this->cachedMem)) {}
         break;
      case 'S':
         switch (buffer[1]) {
         case 'w':
            if (tryRead("SwapTotal:", &this->totalSwap)) {}
            else if (tryRead("SwapFree:", &swapFree)) {}
            break;
         case 'h':
            if (tryRead("Shmem:", &shmem)) {}
            break;
         case 'R':
            if (tryRead("SReclaimable:", &sreclaimable)) {}
            break;
         }
         break;
      }
      #undef tryRead
   }

   this->usedMem = this->totalMem - this->freeMem;
   this->cachedMem = this->cachedMem + sreclaimable - shmem;
   this->usedSwap = this->totalSwap - swapFree;
   fclose(file);
}

static inline double LinuxProcessList_scanCPUTime(LinuxProcessList* this) {

   FILE* file = fopen(PROCSTATFILE, "r");
   if (file == NULL) {
      CRT_fatalError("Cannot open " PROCSTATFILE);
   }
   int cpus = this->super.cpuCount;
   assert(cpus > 0);
   for (int i = 0; i <= cpus; i++) {
      char buffer[PROC_LINE_LENGTH + 1];
      unsigned long long int usertime, nicetime, systemtime, idletime;
      unsigned long long int ioWait, irq, softIrq, steal, guest, guestnice;
      ioWait = irq = softIrq = steal = guest = guestnice = 0;
      // Depending on your kernel version,
      // 5, 7, 8 or 9 of these fields will be set.
      // The rest will remain at zero.
      char* ok = fgets(buffer, PROC_LINE_LENGTH, file);
      if (!ok) buffer[0] = '\0';
      if (i == 0)
         sscanf(buffer,   "cpu  %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu",         &usertime, &nicetime, &systemtime, &idletime, &ioWait, &irq, &softIrq, &steal, &guest, &guestnice);
      else {
         int cpuid;
         sscanf(buffer, "cpu%4d %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu", &cpuid, &usertime, &nicetime, &systemtime, &idletime, &ioWait, &irq, &softIrq, &steal, &guest, &guestnice);
         assert(cpuid == i - 1);
      }
      // Guest time is already accounted in usertime
      usertime = usertime - guest;
      nicetime = nicetime - guestnice;
      // Fields existing on kernels >= 2.6
      // (and RHEL's patched kernel 2.4...)
      unsigned long long int idlealltime = idletime + ioWait;
      unsigned long long int systemalltime = systemtime + irq + softIrq;
      unsigned long long int virtalltime = guest + guestnice;
      unsigned long long int totaltime = usertime + nicetime + systemalltime + idlealltime + steal + virtalltime;
      CPUData* cpuData = &(this->cpus[i]);
      // Since we do a subtraction (usertime - guest) and cputime64_to_clock_t()
      // used in /proc/stat rounds down numbers, it can lead to a case where the
      // integer overflow.
      #define WRAP_SUBTRACT(a,b) (a > b) ? a - b : 0
      cpuData->userPeriod = WRAP_SUBTRACT(usertime, cpuData->userTime);
      cpuData->nicePeriod = WRAP_SUBTRACT(nicetime, cpuData->niceTime);
      cpuData->systemPeriod = WRAP_SUBTRACT(systemtime, cpuData->systemTime);
      cpuData->systemAllPeriod = WRAP_SUBTRACT(systemalltime, cpuData->systemAllTime);
      cpuData->idleAllPeriod = WRAP_SUBTRACT(idlealltime, cpuData->idleAllTime);
      cpuData->idlePeriod = WRAP_SUBTRACT(idletime, cpuData->idleTime);
      cpuData->ioWaitPeriod = WRAP_SUBTRACT(ioWait, cpuData->ioWaitTime);
      cpuData->irqPeriod = WRAP_SUBTRACT(irq, cpuData->irqTime);
      cpuData->softIrqPeriod = WRAP_SUBTRACT(softIrq, cpuData->softIrqTime);
      cpuData->stealPeriod = WRAP_SUBTRACT(steal, cpuData->stealTime);
      cpuData->guestPeriod = WRAP_SUBTRACT(virtalltime, cpuData->guestTime);
      cpuData->totalPeriod = WRAP_SUBTRACT(totaltime, cpuData->totalTime);
      #undef WRAP_SUBTRACT
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
   double period = (double)this->cpus[0].totalPeriod / cpus;
   fclose(file);
   return period;
}

static bool LinuxProcessList_recurseProcTree(LinuxProcessList* this, const char* dirname, pid_t mainProcess, LinuxProcessScanData* psd) {
   ProcessList* pl = (ProcessList*) this;
   Settings* settings = pl->settings;

   DIR* dir = opendir(dirname);
   if (!dir) return false;

   for (struct dirent* entry; (entry = readdir(dir)) != NULL; ) {
      char* name = entry->d_name;

      // The RedHat kernel hides threads with a dot.
      // I believe this is non-standard.
      if ((!settings->hideThreads) && name[0] == '.') {
         name++;
      }
      // Just skip all non-number directories.
      if (name[0] < '0' || name[0] > '9') {
         continue;
      }
      // filename is a number: process directory
      int pid = atoi(name);
     
      if ((mainProcess != 0 && pid == mainProcess) || (pid <= 0))
         continue;

      char subdirname[MAX_NAME+1];
      xSnprintf(subdirname, MAX_NAME, "%s/%s/task", dirname, name);
      LinuxProcessList_recurseProcTree(this, subdirname, pid, psd);

      psd->name = name;
      psd->dirname = dirname;
      psd->mainProcess = mainProcess;
      ProcessList_scanProcess(pl, pid, (ProcessScanData*) psd);
   }
   closedir(dir);
   return true;
}

void ProcessList_goThroughEntries(ProcessList* super) {
   LinuxProcessList* this = (LinuxProcessList*) super;

   LinuxProcessList_scanMemoryInfo(super);
   double period = LinuxProcessList_scanCPUTime(this);
   
   struct timeval tv;
   gettimeofday(&tv, NULL);

   this->psd.period = period;
   this->psd.nowSec = tv.tv_sec;
   this->psd.nowMsec = tv.tv_sec * 1000LL + tv.tv_usec / 1000LL;

   LinuxProcessList_recurseProcTree(this, PROCDIR, 0, &(this->psd));
}
