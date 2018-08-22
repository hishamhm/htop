/*
htop - AixProcessList.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ProcessList.h"
#include "AixProcess.h"
#include "AixProcessList.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/proc.h>
#include <procinfo.h>

/*{

typedef struct AixProcessList_ {
   ProcessList super;

} AixProcessList;

#ifndef Process_isKernelThread
#define Process_isKernelThread(_process) (_process->pgrp == 0)
#endif

#ifndef Process_isUserlandThread
#define Process_isUserlandThread(_process) (_process->pid != _process->tgid)
#endif

}*/

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidWhiteList, uid_t userId) {
   ProcessList* this = xCalloc(1, sizeof(ProcessList));
   ProcessList_init(this, Class(Process), usersTable, pidWhiteList, userId);
   
   return this;
}

void ProcessList_delete(ProcessList* this) {
   ProcessList_done(this);
   free(this);
}

static char *AixProcessList_readProcessName (struct procentry64 *pe) {
	char argvbuf [256]; // completely arbitrary
	if (getargs (pe, sizeof (struct procentry64), argvbuf, 256)) {
		// returns argv entries seperated by NULs, so we can just use the first NUL
		return xStrdup (argvbuf);
	} else {
		return xStrdup (pe->pi_comm);
	}
}

void ProcessList_goThroughEntries(ProcessList* super) {
    AixProcessList* apl = (AixProcessList*)super;
    Settings* settings = super->settings;
    bool hideKernelThreads = settings->hideKernelThreads;
    bool hideUserlandThreads = settings->hideUserlandThreads;
    bool preExisting;
    Process *proc;
    AixProcess *ap;
    /* getprocs stuff */
    struct procentry64 *pes;
    struct procentry64 *pe;
    pid_t pid;
    int count, i;
    struct tm date;
    struct timeval tv;

    // 1000000 is what IBM ps uses; instead of rerunning getprocs with
    // a PID cookie, get one big clump
    pid = 1;
    count = getprocs64 (NULL, 0, NULL, 0, &pid, 1000000);
    if (count < 1) {
        fprintf (stderr, "htop: ProcessList_goThroughEntries failed; during count: %s\n", strerror (errno));
	_exit (1);
    }
    pes = xCalloc (count, sizeof (struct procentry64));
    pid = 1;
    count = getprocs64 (pes, sizeof (struct procentry64), NULL, 0, &pid, count);
    if (count < 1) {
        fprintf (stderr, "htop: ProcessList_goThroughEntries failed; during listing\n");
	_exit (1);
    }

    for (i = 0; i < count; i++) {
        proc = ProcessList_getProcess(super, pe->pi_pid, &preExisting, (Process_New) AixProcess_new);
        pe = pes + i;
        ap = (AixProcess*) proc;

	proc->show = ! ((hideKernelThreads && Process_isKernelThread(proc))
                    || (hideUserlandThreads && Process_isUserlandThread(proc)));

        if (!preExisting) {
            proc->ppid = pe->pi_ppid;
            /* XXX: tpgid? */
            proc->tgid = pe->pi_pid;
            proc->session = pe->pi_sid;
            proc->tty_nr = pe->pi_ttyd;
            proc->pgrp = pe->pi_pgrp;
            proc->st_uid = pe->pi_uid;
            proc->starttime_ctime = pe->pi_start;
            proc->user = UsersTable_getRef(super->usersTable, proc->st_uid);
            ProcessList_add((ProcessList*)super, proc);
            proc->comm = AixProcessList_readProcessName (pe);
            (void) localtime_r((time_t*) &pe->pi_start, &date);
            strftime(proc->starttime_show, 7, ((proc->starttime_ctime > tv.tv_sec - 86400) ? "%R " : "%b%d "), &date);
	} else {
            if (settings->updateProcessNames) {
                free(proc->comm);
                proc->comm = AixProcessList_readProcessName (pe);
            }
        }

       ap->cid = pe->pi_cid;
       //proc->m_size = pi->pi_drss;
       //proc->m_resident = kproc->p_vm_rssize;
       //proc->percent_mem = (proc->m_resident * PAGE_SIZE_KB) / (double)(this->totalMem) * 100.0;
       //proc->percent_cpu = CLAMP(getpcpu(kproc), 0.0, this->cpuCount*100.0);
       proc->nlwp = pe->pi_thcount;
       //proc->time = kproc->p_rtime_sec + ((kproc->p_rtime_usec + 500000) / 10);
       proc->nice = pe->pi_nice;
       //proc->time = kproc->p_rtime_sec + ((kproc->p_rtime_usec + 500000) / 1000000);
       //proc->time *= 100;
       proc->priority = pe->pi_ppri;

       switch (pe->pi_state) {
           case SIDL:    proc->state = 'I'; break;
           case SRUN:    proc->state = 'R'; break;
           case SSLEEP:  proc->state = 'S'; break;
           case SSTOP:   proc->state = 'T'; break;
           case SZOMB:   proc->state = 'Z'; break;
           case SACTIVE: proc->state = 'P'; break;
           case SSWAP:   proc->state = 'W'; break;
           default:      proc->state = '?';
       }

       if (Process_isKernelThread(proc)) {
          super->kernelThreads++;
       }

       super->totalTasks++;
       // SRUN ('R') means runnable, not running
       if (proc->state == 'P') {
          super->runningTasks++;
       }

	proc->updated = true;
    }

    proc = ProcessList_getProcess(super, 1, &preExisting, AixProcess_new);
    free (pes);
}
