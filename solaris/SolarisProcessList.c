/*
htop - SolarisProcessList.c
(C) 2014 Hisham H. Muhammad
(C) 2017 Guy M. Broome
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ProcessList.h"
#include "SolarisProcessList.h"
#include "SolarisProcess.h"

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/user.h>
#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <procfs.h>
#include <errno.h>
#include <pwd.h>
#include <dirent.h>
#include <math.h>

#define MAXCMDLINE 255

/*{

#include <kstat.h>
#include <sys/param.h>
#include <sys/zone.h>
#include <sys/uio.h>
#include <sys/resource.h>

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

static void setCommand(Process* process, const char* command, int len) {
   if (process->comm && process->commLen >= len) {
      strncpy(process->comm, command, len + 1);
   } else {
      free(process->comm);
      process->comm = xStrdup(command);
   }
   process->commLen = len;
}

static void setZoneName(kstat_ctl_t* kd, SolarisProcess* sproc) {
  if ( sproc->zoneid == 0 ) {
     strncpy( sproc->zname, "global    ", 11);
  } else if ( kd == NULL ) {
     strncpy( sproc->zname, "unknown   ", 11);
  } else {
     kstat_t* ks = kstat_lookup( kd, "zones", sproc->zoneid, NULL );
     strncpy( sproc->zname, ks->ks_name, strlen(ks->ks_name) );
  }
} 


ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidWhiteList, uid_t userId) {
   SolarisProcessList* spl = xCalloc(1, sizeof(SolarisProcessList));
   ProcessList* pl = (ProcessList*) spl;
   ProcessList_init(pl, Class(SolarisProcess), usersTable, pidWhiteList, userId);
   int kchain = 0;
   kstat_t *ncpu_handle;
   kstat_named_t *ks_ncpus;

   spl->kd = kstat_open();
   if (spl->kd     != NULL) { ncpu_handle = kstat_lookup(spl->kd,"unix",0,"system_misc"); }
   if (ncpu_handle != NULL) { kchain = kstat_read(spl->kd,ncpu_handle,NULL); }
   if (kchain      != -1  ) { ks_ncpus = kstat_data_lookup(ncpu_handle,"ncpus"); }
   if (ks_ncpus    != NULL) {
      pl->cpuCount = (uint32_t)ks_ncpus->value.ui32;
   } else {
      pl->cpuCount = 1;
   }

   if (pl->cpuCount == 1 ) {
      spl->cpus = xRealloc(spl->cpus, sizeof(CPUData));
   } else {
      spl->cpus = xRealloc(spl->cpus, (pl->cpuCount + 1) * sizeof(CPUData));
   }

   return pl;
}

static inline void SolarisProcessList_scanCPUTime(ProcessList* pl) {
   const SolarisProcessList* spl = (SolarisProcessList*) pl;
   int cpus   = pl->cpuCount;
   kstat_t *cpuinfo;
   int kchain = 0;
   kstat_named_t *idletime, *intrtime, *krnltime, *usertime;
   double idlebuf = 0;
   double intrbuf = 0;
   double krnlbuf = 0;
   double userbuf = 0;
   uint64_t totaltime = 0;
   int arrskip = 0;

   assert(cpus > 0);

   if (cpus > 1) {
       // Store values for the stats loop one extra element up in the array
       // to leave room for the average to be calculated afterwards
       arrskip++;
   }
#define DEBUGCPU 17
   // Calculate per-CPU statistics first
   for (int i = 0; i < cpus; i++) {
      if (spl->kd != NULL) { cpuinfo = kstat_lookup(spl->kd,"cpu",i,"sys"); }
      if (cpuinfo != NULL) { kchain = kstat_read(spl->kd,cpuinfo,NULL); }
      if (kchain  != -1  ) {
         idletime = kstat_data_lookup(cpuinfo,"cpu_nsec_idle");
         intrtime = kstat_data_lookup(cpuinfo,"cpu_nsec_intr");
         krnltime = kstat_data_lookup(cpuinfo,"cpu_nsec_kernel");
         usertime = kstat_data_lookup(cpuinfo,"cpu_nsec_user");
      }

      assert( (idletime != NULL) && (intrtime != NULL)
           && (krnltime != NULL) && (usertime != NULL) );

      CPUData* cpuData          = &(spl->cpus[i+arrskip]);
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

void ProcessList_delete(ProcessList* this) {
   const SolarisProcessList* spl = (SolarisProcessList*) this;
   if (spl->kd) kstat_close(spl->kd);
   free(spl->cpus);
   ProcessList_done(this);
   free(this);
}

void ProcessList_goThroughEntries(ProcessList* this) {
    SolarisProcessList* spl = (SolarisProcessList*) this;
    Settings* settings = this->settings;
    bool hideKernelThreads = settings->hideKernelThreads;
    bool hideUserlandThreads = settings->hideUserlandThreads;
    DIR* dir;
    struct dirent* entry;
    struct timeval tv;
    time_t curTime;
    char*  name;
    int    pid;
    bool   preExisting = false;
    bool isIdleProcess = false;
    Process* proc;
    Process* parent = NULL;
    SolarisProcess* sproc;
    psinfo_t _psinfo;
    pstatus_t _pstatus;
    prusage_t _prusage;
    char filename[MAX_NAME+1];
    FILE *fp;

    SolarisProcessList_scanCPUTime(this);

    dir = opendir(PROCDIR); 
    if (!dir) return false;
    while ((entry = readdir(dir)) != NULL) {
        name = entry->d_name;
        pid  = atoi(name);
        proc = ProcessList_getProcess(this, pid, &preExisting, (Process_New) SolarisProcess_new);
        proc->tgid = parent ? parent->pid : pid;
        proc->show = true; 	
        sproc = (SolarisProcess *) proc;
        xSnprintf(filename, MAX_NAME, "%s/%s/psinfo", PROCDIR, name);
	fp   = fopen(filename, "r");
	if ( fp == NULL ) continue;
	fread(&_psinfo,sizeof(psinfo_t),1,fp);
	fclose(fp);
	xSnprintf(filename, MAX_NAME, "%s/%s/status", PROCDIR, name);
	fp   = fopen(filename, "r");
	if ( fp != NULL ) {
	    fread(&_pstatus,sizeof(pstatus_t),1,fp);
	}
	fclose(fp);
	xSnprintf(filename, MAX_NAME, "%s/%s/usage", PROCDIR, name);
	fp   = fopen(filename,"r");
	if ( fp == NULL ) continue;
	fread(&_prusage,sizeof(prusage_t),1,fp);
	fclose(fp);

        if(!preExisting) {
	    sproc->kernel          = false;
             proc->pid             = _psinfo.pr_pid;
             proc->ppid            = _psinfo.pr_ppid;
             proc->tgid            = _psinfo.pr_pid;
            sproc->zoneid          = _psinfo.pr_zoneid;
             proc->tty_nr          = _psinfo.pr_ttydev;
             proc->pgrp            = _psinfo.pr_pgid;
	     // WARNING: These Solaris 'percentages' are actually 16-bit BINARY FRACTIONS
	     // where 1.0 = 0x8000.
             proc->percent_cpu     = ((uint16_t)_psinfo.pr_pctcpu/(double)65535)*(double)100.0;
             proc->percent_mem     = ((uint16_t)_psinfo.pr_pctmem/(double)65535)*(double)100.0;
	     // End warning
             proc->st_uid          = _psinfo.pr_euid;
             proc->user            = UsersTable_getRef(this->usersTable, proc->st_uid);
             proc->nlwp            = _psinfo.pr_nlwp;
             proc->starttime_ctime = _psinfo.pr_start.tv_sec;
	     proc->session         = _pstatus.pr_sid;
             setCommand(proc,_psinfo.pr_fname,PRFNSZ);
	     setZoneName(spl->kd,sproc);
	     proc->majflt          = _prusage.pr_majf;
	     proc->minflt          = _prusage.pr_minf; 
	     proc->m_resident      = (_psinfo.pr_rssize)/8;
	     proc->m_size          = (_psinfo.pr_size)/8;
             proc->priority        = _psinfo.pr_lwp.pr_pri;
             proc->nice            = _psinfo.pr_lwp.pr_nice;
             proc->processor       = _psinfo.pr_lwp.pr_onpro;
             proc->state           = _psinfo.pr_lwp.pr_sname;
	     proc->time            = _psinfo.pr_time.tv_sec;
             ProcessList_add(this, proc);
	} else {
	     proc->ppid            = _psinfo.pr_ppid;
	    sproc->zoneid          = _psinfo.pr_zoneid;
	     // WARNING: These Solaris 'percentages' are actually 16-bit BINARY FRACTIONS
	     // where 1.0 = 0x8000.
	     proc->percent_cpu     = ((uint16_t)_psinfo.pr_pctcpu/(double)65535)*(double)100.0;
	     proc->percent_mem     = ((uint16_t)_psinfo.pr_pctmem/(double)65535)*(double)100.0;
             // End warning
	     proc->st_uid          = _psinfo.pr_euid;
	     proc->pgrp            = _psinfo.pr_pgid;
	     proc->nlwp            = _psinfo.pr_nlwp;
	     proc->user            = UsersTable_getRef(this->usersTable, proc->st_uid);
             setCommand(proc,_psinfo.pr_fname,PRFNSZ);
	     setZoneName(spl->kd,sproc);
             proc->majflt          = _prusage.pr_majf;
             proc->minflt          = _prusage.pr_minf;
             proc->m_resident      = (_psinfo.pr_rssize)/8;
             proc->m_size          = (_psinfo.pr_size)/8;
             proc->priority        = _psinfo.pr_lwp.pr_pri;
             proc->nice            = _psinfo.pr_lwp.pr_nice;
             proc->processor       = _psinfo.pr_lwp.pr_onpro;
             proc->state           = _psinfo.pr_lwp.pr_sname;
             proc->time            = _psinfo.pr_time.tv_sec;
	}
        if (proc->percent_cpu > 0.1) {
            if ( strcmp("sleep", proc->comm) == 0 ) {
                isIdleProcess = true;
            }
        }
        if (isnan(proc->percent_cpu)) proc->percent_cpu = 0.0;
	if (proc->state == 'O')
		this->runningTasks++;
        this->totalTasks++;
	proc->updated = true;
    }    
    closedir(dir);
}

void SolarisProcessList_scan(ProcessList* this) {
   (void) this;
   // stub!
}

