/*
htop - AixProcessList.c
(C) 2014 Hisham H. Muhammad
(C) 2018 Calvin Buckley
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
#define Process_isKernelThread(_process) (_process->kernel == 1)
#endif

#ifndef Process_isUserlandThread
// XXX
#define Process_isUserlandThread(_process) (false)
#endif

}*/

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidWhiteList, uid_t userId) {
   AixProcessList* apl = xCalloc(1, sizeof(AixProcessList));
   ProcessList* this = (ProcessList*) apl;
   ProcessList_init(this, Class(AixProcess), usersTable, pidWhiteList, userId);

   this->cpuCount = sysconf (_SC_NPROCESSORS_CONF);
   this->totalMem = sysconf (_SC_AIX_REALMEM);

   return apl;
}

void ProcessList_delete(ProcessList* this) {
   ProcessList_done(this);
   free(this);
}

static char *AixProcessList_readProcessName (struct procentry64 *pe) {
	char argvbuf [0x100000]; // completely arbitrary
	int i;
	// if empty fall back
	if (getargs (pe, sizeof (struct procentry64), argvbuf, 0x100000) == 0 && *argvbuf) {
		// args are seperated by NUL, double NUL terminates
		for (i = 0; i < 0x100000; i++) {
			if (argvbuf [i] == '\0' && argvbuf [i + 1] != '\0')
				argvbuf [i] = ' ';
		}
		return xStrdup (argvbuf);
	}
	return xStrdup (pe->pi_comm);
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
    time_t t, pt;

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

    t = time (NULL);
    for (i = 0; i < count; i++) {
        pe = pes + i;
        proc = ProcessList_getProcess(super, pe->pi_pid, &preExisting, (Process_New) AixProcess_new);
        ap = (AixProcess*) proc;

	proc->show = ! ((hideKernelThreads && Process_isKernelThread(ap))
                    || (hideUserlandThreads && Process_isUserlandThread(ap)));

        if (!preExisting) {
            ap->kernel = pe->pi_flags & SKPROC ? 1 : 0;
            proc->pid = pe->pi_pid;
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
            // copy so localtime_r works properly
            pt = pe->pi_start;
            (void) localtime_r((time_t*) &pt, &date);
            strftime(proc->starttime_show, 7, ((proc->starttime_ctime > t - 86400) || 0 ? "%R " : "%b%d "), &date);
	} else {
            if (settings->updateProcessNames) {
                free(proc->comm);
                proc->comm = AixProcessList_readProcessName (pe);
            }
        }

       ap->cid = pe->pi_cid;
       // XXX: are the numbers here right? I think these are based on pages or 1K?
       proc->m_size = pe->pi_drss;
       proc->m_resident = pe->pi_ru.ru_maxrss;
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

       if (Process_isKernelThread(ap)) {
          super->kernelThreads++;
       }

       super->totalTasks++;
       // SRUN ('R') means runnable, not running
       if (proc->state == 'P') {
          super->runningTasks++;
       }

	proc->updated = true;
    }

    free (pes);
}
