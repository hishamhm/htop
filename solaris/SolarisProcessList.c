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
} CPUData;

typedef struct SolarisProcessList_ {
   ProcessList super;
   kstat_ctl_t* kd;
   int zfsArcEnabled;
   unsigned long long int memWire;
   unsigned long long int memActive;
   unsigned long long int memInactive;
   unsigned long long int memFree;
   unsigned long long int memZfsArc;
   CPUData* cpus;
   unsigned long   *cp_time_o;
   unsigned long   *cp_time_n;
   unsigned long  *cp_times_o;
   unsigned long  *cp_times_n;
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

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidWhiteList, uid_t userId) {
   SolarisProcessList* spl = xCalloc(1, sizeof(SolarisProcessList));
   ProcessList* pl = (ProcessList*) spl;
   ProcessList_init(pl, Class(SolarisProcess), usersTable, pidWhiteList, userId);
//   spl->kd = kstat_open();
//   kstat_t *ncpu_handle = kstat_lookup(spl->kd,"unix",0,"system_misc");
//   kstat_named_t *ks_ncpus = kstat_data_lookup(ncpu_handle,"ncpus");
//   if ( ks_ncpus->value.i32 > 0 ) {
//	   pl->cpuCount = ks_ncpus->value.i32;
//   } else {
//	   pl->cpuCount = 1;
//   }

   return pl;
}

void ProcessList_delete(ProcessList* this) {
   const SolarisProcessList* spl = (SolarisProcessList*) this;
//   if (spl->kd) kstat_close(spl->kd);
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
             proc->percent_cpu     = (_psinfo.pr_pctcpu)/100;
             proc->percent_mem     = (_psinfo.pr_pctmem)/100;
             proc->st_uid          = _psinfo.pr_euid;
             proc->user            = UsersTable_getRef(this->usersTable, proc->st_uid);
             proc->nlwp            = _psinfo.pr_nlwp;
             proc->starttime_ctime = _psinfo.pr_start.tv_sec;
	     proc->session         = _pstatus.pr_sid;
             setCommand(proc,_psinfo.pr_fname,PRFNSZ);
	     proc->majflt          = _prusage.pr_majf;
	     proc->minflt          = _prusage.pr_minf; 
             ProcessList_add(this, proc);
	    sproc->zname           = "unknown";
	     proc->m_resident      = (_psinfo.pr_rssize)/8;
	     proc->m_size          = (_psinfo.pr_size)/8;
             proc->priority        = _psinfo.pr_lwp.pr_pri;
             proc->nice            = _psinfo.pr_lwp.pr_nice;
             proc->processor       = _psinfo.pr_lwp.pr_onpro;
             proc->state           = _psinfo.pr_lwp.pr_sname;
	     proc->time            = _psinfo.pr_time.tv_sec;
        } else {
	     proc->ppid            = _psinfo.pr_ppid;
	    sproc->zoneid          = _psinfo.pr_zoneid;
	     proc->percent_cpu     = (_psinfo.pr_pctcpu)/100;
	     proc->percent_mem     = (_psinfo.pr_pctmem)/100;
	     proc->st_uid          = _psinfo.pr_euid;
	     proc->nlwp            = _psinfo.pr_nlwp;
	     proc->user            = UsersTable_getRef(this->usersTable, proc->st_uid);
             setCommand(proc,_psinfo.pr_fname,PRFNSZ);
	    sproc->zname           = "unknown";
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

