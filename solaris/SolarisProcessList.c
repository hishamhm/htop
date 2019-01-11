/*
htop - SolarisProcessList.c
(C) 2014 Hisham H. Muhammad
(C) 2017-2019 Guy M. Broome
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
#include <sys/vm_usage.h>
#include <sys/systeminfo.h>

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
   zoneid_t this_zone;
   size_t zmaxmem;
   size_t sysusedmem;
   char* karch;
   uint_t kbitness;
   char* earch;
   uint_t ebitness;
} SolarisProcessList;

}*/

// Used in case htop is 32-bit but we're on a 64-bit kernel
// in which case it is needed to correctly cast zone memory
// usage info
typedef struct htop_vmusage64 {
	id_t vmu_zoneid;
	uint_t vmu_type;
	id_t vmu_id;
	int alignment_padding;
	uint64_t vmu_rss_all;
	uint64_t vmu_rss_private;
	uint64_t vmu_rss_shared;
	uint64_t vmu_swap_all;
	uint64_t vmu_swap_private;
	uint64_t vmu_swap_shared;
} htop_vmusage64_t;

static uint_t get_bitness(const char *isa) {
   if (strcmp(isa, "sparc") == 0 || strcmp(isa, "i386") == 0) return (32);
   if (strcmp(isa, "sparcv9") == 0 || strcmp(isa, "amd64") == 0) return (64);
   return (0);
}

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

   // Get the zone of the running htop process.
   spl->this_zone = getzoneid();

   spl->karch = (char *)calloc(8,sizeof(char));
   spl->earch = (char *)calloc(8,sizeof(char));

   // Various info on the architecture of the kernel and the current binary
   // which is needed to correctly get zone memory usage and limit info
   if (sysinfo(SI_ARCHITECTURE_K,spl->karch,8) == -1) {
      fprintf(stderr, "\nUnable to determine kernel architecture.\n");
      abort();
   }

   if ((spl->kbitness = get_bitness(spl->karch)) == 0) {
      fprintf(stderr, "\nUnable to determine kernel bitness.\n");
      abort();
   }

   if (sysinfo(SI_ARCHITECTURE_NATIVE,spl->earch,8) == -1) {
      fprintf(stderr, "\nUnable to determine architecture of this program.\n");
      abort();
   }

   if ((spl->ebitness = get_bitness(spl->earch)) == 0) {
      fprintf(stderr, "\nUnable to determine bitness of this program.\n");
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
   kstat_named_t       *freemem_pgs = NULL;
   struct swaptable    *sl = NULL;
   struct swapent      *swapdev = NULL;
   uint64_t            totalswap = 0;
   uint64_t            totalfree = 0;
   uint64_t            zramcap = 0;
   int                 nswap = 0;
   char                *spath = NULL; 
   char                *spathbase = NULL;
   htop_vmusage64_t    *vmu_vals64 = NULL;
   vmusage_t           *vmu_vals = NULL;
   size_t              nvmu_vals = 1;
   size_t              real_sys_used = 0;
   int                 ret;

   // Part 1 - physical memory
   // This is done very differently for global vs. non-global zones, because
   // the method needed for non-global, while capable of reporting system-wide
   // usage, also provides much more limited detail.

   if ( (meminfo = kstat_lookup(spl->kd,"unix",0,"system_pages")) != NULL) {
      if ( (ksrphyserr = kstat_read(spl->kd,meminfo,NULL)) != -1) {
         totalmem_pgs   = kstat_data_lookup( meminfo, "physmem" );
         freemem_pgs    = kstat_data_lookup( meminfo, "pagesfree" );

         if (spl->this_zone == 0) {
            // htop is running in the global zone, so get system-wide memory stats
            pl->totalMem   = totalmem_pgs->value.ui64 * PAGE_SIZE_KB;
            pl->usedMem    = (totalmem_pgs->value.ui64 - freemem_pgs->value.ui64) * PAGE_SIZE_KB;
            // Not sure how to implement these on Solaris - suggestions welcome!
            spl->zmaxmem = 0;
            spl->sysusedmem  = 0;
         } else {
            // htop is running in a non-global zone, so only report mem stats for this zone
            pl->totalMem    = totalmem_pgs->value.ui64 * PAGE_SIZE_KB;
            spl->zmaxmem    = 0;
            real_sys_used   = (totalmem_pgs->value.ui64 - freemem_pgs->value.ui64) * PAGE_SIZE_KB;
            vmu_vals        = (vmusage_t *)calloc(1,sizeof(vmusage_t));
            vmu_vals64      = (htop_vmusage64_t *)calloc(1,sizeof(htop_vmusage64_t));

            if ( spl->kbitness == spl->ebitness ) {
               // htop is kernel-native bitness, 32 or 64
               getvmusage(VMUSAGE_ZONE, 0, vmu_vals, &nvmu_vals);
               pl->usedMem  = vmu_vals[0].vmu_rss_all / 1024; // Returned in bytes, should be KiB for htop
            } else if ( spl->kbitness == 64 ) {
               // htop is not kernel native bitness, e.g. 32-bit htop with a 64-bit kernel
               getvmusage(VMUSAGE_ZONE, 0, vmu_vals64, &nvmu_vals);
               pl->usedMem  = vmu_vals64[0].vmu_rss_all / 1024; // Returned in bytes, should be KiB for htop
            } else {
               // Huh?  64-bit app on a 32-bit kernel?  Nope.  Maybe it's 2030 and 128-bit architectures
               // are now a thing?
               pl->usedMem  = 0;
            }

            ret = zone_getattr(spl->this_zone,ZONE_ATTR_PHYS_MCAP,&zramcap,sizeof(zramcap));
            if ( ret < 0 ) zramcap = 0;

            spl->zmaxmem = zramcap / 1024;
            if ( real_sys_used > spl->zmaxmem ) {
               spl->sysusedmem = real_sys_used - spl->zmaxmem;
            } else {
               spl->sysusedmem = 0;
            }

            free(vmu_vals);
            free(vmu_vals64);
         }
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
   free(spl->earch);
   free(spl->karch);
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
   ProcessList *pl = (ProcessList*) listptr;
   SolarisProcessList *spl = (SolarisProcessList*) listptr;
   struct timeval tv;
   struct tm date;
   bool preExisting;
   pid_t getpid;
   int perr = -1;
   int psferr = -1;

   // Setup for using pseudo-PIDs in the htop process table while
   // displaying the real PIDs in user output, since LWPs don't have
   // unique PIDs on Solaris or illumos
   // NOTE: LWPIDs greater than 1023 on a given process will not be
   //   listed by htop, due to size limits of the pid_t field.  I
   //   don't currently see any way around this.  Suggestions welcome.
   id_t lwpid_real = _lwpsinfo->pr_lwpid;
   if (lwpid_real > 1023) return 0;
   pid_t lwpid   = (_psinfo->pr_pid * 1024) + lwpid_real;
   bool onMasterLWP = (_lwpsinfo->pr_lwpid == _psinfo->pr_lwp.pr_lwpid);
   if (onMasterLWP) {
      // Left-shifting the top level PID, while subordinate
      // LWPs have that base plus the LWPID as their "htop PID."
      // This gives us unique PIDs per-LWP for the htop PID table
      // _with correct sorting_ at the cost of the 1023 LWP limit.
      getpid = _psinfo->pr_pid * 1024;
   } else {
      getpid = lwpid;
   } 

   // Can't do this until we have done the pseudo-PID setup above
   Process *proc             = ProcessList_getProcess(pl, getpid, &preExisting, (Process_New) SolarisProcess_new);
   SolarisProcess *sproc     = (SolarisProcess*) proc;

   // Grab a read-only process handle.  Only used for getting
   // process security flags at the moment, and that only works
   // on illumos.
#ifdef PRSECFLAGS_VERSION_1
   struct ps_prochandle *ph  = Pgrab(_psinfo->pr_pid,PGRAB_RDONLY,&perr);
   prsecflags_t *psf         = NULL;
   if (!perr) {
      psferr = Psecflags(ph,&psf);
   }
#endif
   gettimeofday(&tv, NULL);

   // For new and existing entries in the htop proc table for all LWPs
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
   sproc->sol_tty_nr        = _psinfo->pr_ttydev;
   proc->m_resident         = _psinfo->pr_rssize/PAGE_SIZE_KB;
   proc->m_size             = _psinfo->pr_size/PAGE_SIZE_KB;
   proc->user               = UsersTable_getRef(pl->usersTable, proc->st_uid);
#ifdef PRSECFLAGS_VERSION_1 // illumos only
   if (!psferr) {
      sproc->esecflags      = psf->pr_effective;
      Psecflags_free(psf);
   } else {
      sproc->esecflags      = PROC_SEC_UNAVAIL;
   }
   if (!perr) Pfree(ph);
#endif
   // For new htop proc table entries only, for all LWPs
   if (!preExisting) {
      sproc->realpid        = _psinfo->pr_pid;
      sproc->lwpid          = lwpid_real;
      sproc->zoneid         = _psinfo->pr_zoneid;
      sproc->zname          = SolarisProcessList_readZoneName(spl->kd,sproc); 
      proc->comm            = xStrdup(_psinfo->pr_fname);
      proc->commLen         = strnlen(_psinfo->pr_fname,PRFNSZ);
      sproc->dmodel         = _psinfo->pr_dmodel;
   }

   // For new and existing entries in the htop proc table, but only for rep. LWP 
   if (onMasterLWP) {
      proc->ppid            = (_psinfo->pr_ppid * 1024);
      proc->tgid            = (_psinfo->pr_ppid * 1024);
      sproc->realppid       = _psinfo->pr_ppid;
      // See note above (in common section) about this BINARY FRACTION
      proc->percent_cpu     = ((uint16_t)_psinfo->pr_pctcpu/(double)32768)*(double)100.0;
      proc->time            = (_psinfo->pr_time.tv_sec * 100) + (_psinfo->pr_time.tv_nsec / 10000000);
      // For existing htop proc table entries, and only for rep. LWP
      if(!preExisting) {
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
   } else {
   // For new and existing entries in the htop proc table, but only for non-rep. LWPs
      proc->percent_cpu        = ((uint16_t)_lwpsinfo->pr_pctcpu/(double)32768)*(double)100.0;
      proc->time               = (_lwpsinfo->pr_time.tv_sec * 100) + (_lwpsinfo->pr_time.tv_nsec / 10000000);
      // For existing proc table entries, but only for non-rep. LWPs
      if (!preExisting) {
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
   }

   // For new entries only, for all LWPs
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

   return 0;
}

void ProcessList_goThroughEntries(ProcessList* this) {
   SolarisProcessList_scanCPUTime(this);
   SolarisProcessList_scanMemoryInfo(this);
   this->kernelThreads = 1;
   proc_walk(&SolarisProcessList_walkproc, this, PR_WALK_LWP);
}

