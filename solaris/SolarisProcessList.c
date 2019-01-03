/*
htop - SolarisProcessList.c
(C) 2014 Hisham H. Muhammad
(C) 2017,2018 Guy M. Broome
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ProcessList.h"
#include "SolarisProcess.h"
#include "SolarisProcessList.h"

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/user.h>
#include <err.h>
#include <limits.h>
#include <string.h>
#include <procfs.h>
#include <errno.h>
#include <pwd.h>
#include <math.h>
#include <time.h>

#define MAXCMDLINE 255

/*{

#include <kstat.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <sys/resource.h>
#include <sys/sysconf.h>
#include <sys/sysinfo.h>
#include <sys/swap.h>

#define ZONE_ERRMSGLEN 1024
char zone_errmsg[ZONE_ERRMSGLEN];

typedef struct CPUData_ {
   double userPercent;
   double nicePercent;
   double systemPercent;
   double irqPercent;
   double idlePercent;
   double systemAllPercent;
   uint64_t luser;
   uint64_t lkrnl;
   uint64_t lintr;
   uint64_t lidle;
} CPUData;

typedef struct SolarisProcessList_ {
   ProcessList super;
   kstat_ctl_t* kd;
   CPUData* cpus;
} SolarisProcessList;

}*/

char* SolarisProcessList_readZoneName(kstat_ctl_t* kd, SolarisProcess* sproc) {
  char* zname;
  if ( sproc->zoneid == 0 ) {
     zname = xStrdup("global    ");
  } else if ( kd == NULL ) {
     zname = xStrdup("unknown   ");
  } else {
     kstat_t* ks = kstat_lookup( kd, "zones", sproc->zoneid, NULL );
     zname = xStrdup(ks->ks_name);
  }
  return zname;
}

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidWhiteList, uid_t userId) {
   SolarisProcessList* spl = xCalloc(1, sizeof(SolarisProcessList));
   ProcessList* pl = (ProcessList*) spl;
   ProcessList_init(pl, Class(SolarisProcess), usersTable, pidWhiteList, userId);
   spl->kd = NULL;
   pl->cpuCount = 0;

   // Failing to obtain a kstat handle is is fatal
   if ( (spl->kd = kstat_open()) == NULL ) {
      fprintf(stderr, "\nUnable to open kstat handle.\n");
      abort();
   }

   // ...as is failing to access sysconf data
   if ( (pl->cpuCount = sysconf(_SC_NPROCESSORS_ONLN)) <= 0 ) {
      fprintf(stderr, "\nThe sysconf() system call does not seem to be working.\n");
      abort();
   }

   // The extra "cpu" for spl->cpus > 1 is to store aggregate data for all CPUs
   if (pl->cpuCount == 1 ) {
      spl->cpus = xRealloc(spl->cpus, sizeof(CPUData));
   } else {
      spl->cpus = xRealloc(spl->cpus, (pl->cpuCount + 1) * sizeof(CPUData));
   }

   return pl;
}

static inline void SolarisProcessList_scanCPUTime(ProcessList* pl) {
   const SolarisProcessList* spl = (SolarisProcessList*) pl;
   int cpus = pl->cpuCount;
   kstat_t *cpuinfo = NULL;
   int kchain = -1;
   kstat_named_t *idletime = NULL;
   kstat_named_t *intrtime = NULL;
   kstat_named_t *krnltime = NULL;
   kstat_named_t *usertime = NULL;
   double idlebuf = 0;
   double intrbuf = 0;
   double krnlbuf = 0;
   double userbuf = 0;
   uint64_t totaltime = 0;
   int arrskip = 0;

   // cpus > 0 covered in ProcessList_new()

   if (cpus > 1) {
       // Store values for the stats loop one extra element up in the array
       // to leave room for the average to be calculated afterwards
       arrskip++;
   }

   // Calculate per-CPU statistics first
   for (int i = 0; i < cpus; i++) {
      if ( (cpuinfo = kstat_lookup(spl->kd,"cpu",i,"sys")) != NULL) {
         if ( (kchain = kstat_read(spl->kd,cpuinfo,NULL)) != -1 ) {
            idletime = kstat_data_lookup(cpuinfo,"cpu_nsec_idle");
            intrtime = kstat_data_lookup(cpuinfo,"cpu_nsec_intr");
            krnltime = kstat_data_lookup(cpuinfo,"cpu_nsec_kernel");
            usertime = kstat_data_lookup(cpuinfo,"cpu_nsec_user");
         }
      }

      if (!((idletime != NULL) && (intrtime != NULL)
           && (krnltime != NULL) && (usertime != NULL))) {
         fprintf(stderr,"\nCalls to kstat do not appear to be working.\n");
         abort();
      }

      CPUData* cpuData = &(spl->cpus[i+arrskip]);
      totaltime = (idletime->value.ui64 - cpuData->lidle)
                + (intrtime->value.ui64 - cpuData->lintr)
                + (krnltime->value.ui64 - cpuData->lkrnl)
                + (usertime->value.ui64 - cpuData->luser);
      // Calculate percentages of deltas since last reading
      cpuData->userPercent      = ((usertime->value.ui64 - cpuData->luser) / (double)totaltime) * 100.0;
      cpuData->nicePercent      = (double)0.0; // Not implemented on Solaris
      cpuData->systemPercent    = ((krnltime->value.ui64 - cpuData->lkrnl) / (double)totaltime) * 100.0;
      cpuData->irqPercent       = ((intrtime->value.ui64 - cpuData->lintr) / (double)totaltime) * 100.0;
      cpuData->systemAllPercent = cpuData->systemPercent + cpuData->irqPercent;
      cpuData->idlePercent      = ((idletime->value.ui64 - cpuData->lidle) / (double)totaltime) * 100.0;
      // Store current values to use for the next round of deltas
      cpuData->luser            = usertime->value.ui64;
      cpuData->lkrnl            = krnltime->value.ui64;
      cpuData->lintr            = intrtime->value.ui64;
      cpuData->lidle            = idletime->value.ui64;
      // Accumulate the current percentages into buffers for later average calculation
      if (cpus > 1) {
         userbuf               += cpuData->userPercent;
         krnlbuf               += cpuData->systemPercent;
         intrbuf               += cpuData->irqPercent;
         idlebuf               += cpuData->idlePercent;
      }
   }
   
   if (cpus > 1) {
      CPUData* cpuData          = &(spl->cpus[0]);
      cpuData->userPercent      = userbuf / cpus;
      cpuData->nicePercent      = (double)0.0; // Not implemented on Solaris
      cpuData->systemPercent    = krnlbuf / cpus;
      cpuData->irqPercent       = intrbuf / cpus;
      cpuData->systemAllPercent = cpuData->systemPercent + cpuData->irqPercent;
      cpuData->idlePercent      = idlebuf / cpus;
   }
}

static inline void SolarisProcessList_scanMemoryInfo(ProcessList* pl) {
   SolarisProcessList* spl = (SolarisProcessList*) pl;
   kstat_t             *meminfo = NULL;
   int                 ksrphyserr = -1;
   kstat_named_t       *totalmem_pgs = NULL;
   kstat_named_t       *lockedmem_pgs = NULL;
   kstat_named_t       *pages = NULL;
   struct swaptable    *sl = NULL;
   struct swapent      *swapdev = NULL;
   uint64_t            totalswap = 0;
   uint64_t            totalfree = 0;
   int                 nswap = 0;
   char                *spath = NULL; 
   char                *spathbase = NULL;

   // Part 1 - physical memory
   if ( (meminfo = kstat_lookup(spl->kd,"unix",0,"system_pages")) != NULL) {
      if ( (ksrphyserr = kstat_read(spl->kd,meminfo,NULL)) != -1) {
         totalmem_pgs   = kstat_data_lookup( meminfo, "physmem" );
         lockedmem_pgs  = kstat_data_lookup( meminfo, "pageslocked" );
         pages          = kstat_data_lookup( meminfo, "pagestotal" );

         pl->totalMem   = totalmem_pgs->value.ui64 * PAGE_SIZE_KB;
         pl->usedMem    = lockedmem_pgs->value.ui64 * PAGE_SIZE_KB;
         // Not sure how to implement this on Solaris - suggestions welcome!
         pl->cachedMem  = 0;     
         // Not really "buffers" but the best Solaris analogue that I can find to
         // "memory in use but not by programs or the kernel itself"
         pl->buffersMem = (totalmem_pgs->value.ui64 - pages->value.ui64) * PAGE_SIZE_KB;
       }
   }
   
   // Part 2 - swap
   if ( (nswap = swapctl(SC_GETNSWP, NULL)) > 0) {
      if ( (sl = xMalloc((nswap * sizeof(swapent_t)) + sizeof(int))) != NULL) {
         if ( (spathbase = xMalloc( nswap * MAXPATHLEN )) != NULL) { 
            spath = spathbase;
            swapdev = sl->swt_ent;
            for (int i = 0; i < nswap; i++, swapdev++) {
               swapdev->ste_path = spath;
               spath += MAXPATHLEN;
            }
         }
         sl->swt_n = nswap;
      }
   }
   if ( (nswap = swapctl(SC_LIST, sl)) > 0) { 
      swapdev = sl->swt_ent;
      for (int i = 0; i < nswap; i++, swapdev++) {
         totalswap += swapdev->ste_pages;
         totalfree += swapdev->ste_free;
      }
   }
   free(spathbase);
   free(sl);
   pl->totalSwap = totalswap * PAGE_SIZE_KB;
   pl->usedSwap  = pl->totalSwap - (totalfree * PAGE_SIZE_KB); 
}

void ProcessList_delete(ProcessList* pl) {
   SolarisProcessList* spl = (SolarisProcessList*) pl;
   ProcessList_done(pl);
   free(spl->cpus);
   kstat_close(spl->kd);
   free(spl);
}

/* NOTE: the following is a callback function of type proc_walk_f
 *       and MUST conform to the appropriate definition in order
 *       to work.  See libproc(3LIB) on a Solaris or Illumos
 *       system for more info.
 */ 

int SolarisProcessList_walkproc(psinfo_t *_psinfo, lwpsinfo_t *_lwpsinfo, void *listptr) {
   struct timeval tv;
   struct tm date;
   bool preExisting;
   pid_t getpid;

   // Setup process list
   ProcessList *pl = (ProcessList*) listptr;
   SolarisProcessList *spl = (SolarisProcessList*) listptr;

   id_t lwpid_real = _lwpsinfo->pr_lwpid;
   if (lwpid_real > 1023) return 0;
   pid_t lwpid   = (_psinfo->pr_pid * 1024) + lwpid_real;
   bool onMasterLWP = (_lwpsinfo->pr_lwpid == _psinfo->pr_lwp.pr_lwpid);
   if (onMasterLWP) {
      getpid = _psinfo->pr_pid * 1024;
   } else {
      getpid = lwpid;
   } 
   Process *proc             = ProcessList_getProcess(pl, getpid, &preExisting, (Process_New) SolarisProcess_new);
   SolarisProcess *sproc     = (SolarisProcess*) proc;

   gettimeofday(&tv, NULL);

   // Common code pass 1
   proc->show               = false;
   sproc->taskid            = _psinfo->pr_taskid;
   sproc->projid            = _psinfo->pr_projid;
   sproc->poolid            = _psinfo->pr_poolid;
   sproc->contid            = _psinfo->pr_contract;
   proc->priority           = _lwpsinfo->pr_pri;
   proc->nice               = _lwpsinfo->pr_nice;
   proc->processor          = _lwpsinfo->pr_onpro;
   proc->state              = _lwpsinfo->pr_sname;
   // NOTE: This 'percentage' is a 16-bit BINARY FRACTIONS where 1.0 = 0x8000
   // Source: https://docs.oracle.com/cd/E19253-01/816-5174/proc-4/index.html
   // (accessed on 18 November 2017)
   proc->percent_mem        = ((uint16_t)_psinfo->pr_pctmem/(double)32768)*(double)100.0;
   proc->st_uid             = _psinfo->pr_euid;
   proc->pgrp               = _psinfo->pr_pgid;
   proc->nlwp               = _psinfo->pr_nlwp;
   proc->tty_nr             = _psinfo->pr_ttydev;
   proc->m_resident         = _psinfo->pr_rssize/PAGE_SIZE_KB;
   proc->m_size             = _psinfo->pr_size/PAGE_SIZE_KB;

   if (!preExisting) {
      sproc->realpid        = _psinfo->pr_pid;
      sproc->lwpid          = lwpid_real;
      sproc->zoneid         = _psinfo->pr_zoneid;
      sproc->zname          = SolarisProcessList_readZoneName(spl->kd,sproc); 
      proc->user            = UsersTable_getRef(pl->usersTable, proc->st_uid);
      proc->comm            = xStrdup(_psinfo->pr_fname);
      proc->commLen         = strnlen(_psinfo->pr_fname,PRFNSZ);
   }

   // End common code pass 1

   if (onMasterLWP) { // Are we on the representative LWP?
      proc->ppid            = (_psinfo->pr_ppid * 1024);
      proc->tgid            = (_psinfo->pr_ppid * 1024);
      sproc->realppid       = _psinfo->pr_ppid;
      // See note above (in common section) about this BINARY FRACTION
      proc->percent_cpu     = ((uint16_t)_psinfo->pr_pctcpu/(double)32768)*(double)100.0;
      proc->time            = _psinfo->pr_time.tv_sec;
      if(!preExisting) { // Tasks done only for NEW processes
         sproc->is_lwp = false;
         proc->starttime_ctime = _psinfo->pr_start.tv_sec;
      }

      // Update proc and thread counts based on settings
      if (sproc->kernel && !pl->settings->hideKernelThreads) {
         pl->kernelThreads += proc->nlwp;
         pl->totalTasks += proc->nlwp+1;
         if (proc->state == 'O') pl->runningTasks++;
      } else if (!sproc->kernel) {
         if (proc->state == 'O') pl->runningTasks++;
         if (pl->settings->hideUserlandThreads) {
            pl->totalTasks++;
         } else {
            pl->userlandThreads += proc->nlwp;
            pl->totalTasks += proc->nlwp+1;
         }
      }
      proc->show = !(pl->settings->hideKernelThreads && sproc->kernel);
   } else { // We are not in the master LWP, so jump to the LWP handling code
      proc->percent_cpu        = ((uint16_t)_lwpsinfo->pr_pctcpu/(double)32768)*(double)100.0;
      proc->time               = _lwpsinfo->pr_time.tv_sec;
      if (!preExisting) { // Tasks done only for NEW LWPs
         sproc->is_lwp         = true; 
         proc->basenameOffset  = -1;
         proc->ppid            = _psinfo->pr_pid * 1024;
         proc->tgid            = _psinfo->pr_pid * 1024;
         sproc->realppid       = _psinfo->pr_pid;
         proc->starttime_ctime = _lwpsinfo->pr_start.tv_sec;
      }

      // Top-level process only gets this for the representative LWP
      if (sproc->kernel  && !pl->settings->hideKernelThreads)   proc->show = true;
      if (!sproc->kernel && !pl->settings->hideUserlandThreads) proc->show = true;
   } // Top-level LWP or subordinate LWP

   // Common code pass 2

   if (!preExisting) {
      if ((sproc->realppid <= 0) && !(sproc->realpid <= 1)) {
         sproc->kernel = true;
      } else {
         sproc->kernel = false;
      }
      (void) localtime_r((time_t*) &proc->starttime_ctime, &date);
      strftime(proc->starttime_show, 7, ((proc->starttime_ctime > tv.tv_sec - 86400) ? "%R " : "%b%d "), &date);
      ProcessList_add(pl, proc);
   }
   proc->updated = true;

   // End common code pass 2

   return 0;
}

void ProcessList_goThroughEntries(ProcessList* this) {
   SolarisProcessList_scanCPUTime(this);
   SolarisProcessList_scanMemoryInfo(this);
   this->kernelThreads = 1;
   proc_walk(&SolarisProcessList_walkproc, this, PR_WALK_LWP);
}

