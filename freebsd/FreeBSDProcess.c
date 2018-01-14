/*
htop - FreeBSDProcess.c
(C) 2015 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Process.h"
#include "ProcessList.h"
#include "FreeBSDProcess.h"
#include "FreeBSDProcessList.h"
#include "Platform.h"
#include "CRT.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>

/*{

#include <kvm.h>
#include <sys/param.h>
#include <sys/jail.h>
#include <sys/uio.h>
#include <sys/resource.h>

#define JAIL_ERRMSGLEN	1024
char jail_errmsg[JAIL_ERRMSGLEN];

typedef enum FreeBSDProcessFields {
   // Add platform-specific fields here, with ids >= 100
   JID   = 100,
   JAIL  = 101,
   LAST_PROCESSFIELD = 102,
} FreeBSDProcessField;

typedef struct FreeBSDProcess_ {
   Process super;
   int   kernel;
   int   jid;
   char* jname;
} FreeBSDProcess;

typedef struct FreeBSDProcessScanData_ {
   int pageSizeKb;
   int kernelFScale;
   struct kinfo_proc* kproc;
} FreeBSDProcessScanData;

}*/

ObjectClass FreeBSDProcess_class = {
   .extends = Class(Process),
   .display = Process_display,
   .delete = Process_delete,
   .compare = FreeBSDProcess_compare
};

ProcessFieldData Process_fields[] = {
   [0] = { .name = "", .title = NULL, .description = NULL, .flags = 0, },
   [PID] = { .name = "PID", .title = "    PID ", .description = "Process/thread ID", .flags = 0, },
   [COMM] = { .name = "Command", .title = "Command ", .description = "Command line", .flags = 0, },
   [STATE] = { .name = "STATE", .title = "S ", .description = "Process state (S sleeping, R running, D disk, Z zombie, T traced, W paging)", .flags = 0, },
   [PPID] = { .name = "PPID", .title = "   PPID ", .description = "Parent process ID", .flags = 0, },
   [PGRP] = { .name = "PGRP", .title = "   PGRP ", .description = "Process group ID", .flags = 0, },
   [SESSION] = { .name = "SESSION", .title = "    SID ", .description = "Process's session ID", .flags = 0, },
   [TTY_NR] = { .name = "TTY_NR", .title = "    TTY ", .description = "Controlling terminal", .flags = 0, },
   [TPGID] = { .name = "TPGID", .title = "  TPGID ", .description = "Process ID of the fg process group of the controlling terminal", .flags = 0, },
   [MINFLT] = { .name = "MINFLT", .title = "     MINFLT ", .description = "Number of minor faults which have not required loading a memory page from disk", .flags = 0, },
   [MAJFLT] = { .name = "MAJFLT", .title = "     MAJFLT ", .description = "Number of major faults which have required loading a memory page from disk", .flags = 0, },
   [PRIORITY] = { .name = "PRIORITY", .title = "PRI ", .description = "Kernel's internal priority for the process", .flags = 0, },
   [NICE] = { .name = "NICE", .title = " NI ", .description = "Nice value (the higher the value, the more it lets other processes take priority)", .flags = 0, },
   [STARTTIME] = { .name = "STARTTIME", .title = "START ", .description = "Time the process was started", .flags = 0, },

   [PROCESSOR] = { .name = "PROCESSOR", .title = "CPU ", .description = "Id of the CPU the process last executed on", .flags = 0, },
   [M_SIZE] = { .name = "M_SIZE", .title = " VIRT ", .description = "Total program size in virtual memory", .flags = 0, },
   [M_RESIDENT] = { .name = "M_RESIDENT", .title = "  RES ", .description = "Resident set size, size of the text and data sections, plus stack usage", .flags = 0, },
   [ST_UID] = { .name = "ST_UID", .title = " UID ", .description = "User ID of the process owner", .flags = 0, },
   [PERCENT_CPU] = { .name = "PERCENT_CPU", .title = "CPU% ", .description = "Percentage of the CPU time the process used in the last sampling", .flags = 0, },
   [PERCENT_MEM] = { .name = "PERCENT_MEM", .title = "MEM% ", .description = "Percentage of the memory the process is using, based on resident memory size", .flags = 0, },
   [USER] = { .name = "USER", .title = "USER      ", .description = "Username of the process owner (or user ID if name cannot be determined)", .flags = 0, },
   [TIME] = { .name = "TIME", .title = "  TIME+  ", .description = "Total time the process has spent in user and system time", .flags = 0, },
   [NLWP] = { .name = "NLWP", .title = "NLWP ", .description = "Number of threads in the process", .flags = 0, },
   [TGID] = { .name = "TGID", .title = "   TGID ", .description = "Thread group ID (i.e. process ID)", .flags = 0, },
   [JID] = { .name = "JID", .title = "    JID ", .description = "Jail prison ID", .flags = 0, },
   [JAIL] = { .name = "JAIL", .title = "JAIL        ", .description = "Jail prison name", .flags = 0, },
   [LAST_PROCESSFIELD] = { .name = "*** report bug! ***", .title = NULL, .description = NULL, .flags = 0, },
};

ProcessPidColumn Process_pidColumns[] = {
   { .id = JID, .label = "JID" },
   { .id = PID, .label = "PID" },
   { .id = PPID, .label = "PPID" },
   { .id = TPGID, .label = "TPGID" },
   { .id = TGID, .label = "TGID" },
   { .id = PGRP, .label = "PGRP" },
   { .id = SESSION, .label = "SID" },
   { .id = 0, .label = NULL },
};

Process* Process_new(Settings* settings) {
   FreeBSDProcess* this = xCalloc(1, sizeof(FreeBSDProcess));
   Object_setClass(this, Class(FreeBSDProcess));
   Process_init(&this->super, settings);
   return (Process*) this;
}

void Process_delete(Object* cast) {
   FreeBSDProcess* this = (FreeBSDProcess*) cast;
   Process_done((Process*)cast);
   free(this->jname);
   free(this);
}

void Process_writeField(Process* this, RichString* str, ProcessField field) {
   FreeBSDProcess* fp = (FreeBSDProcess*) this;
   char buffer[256]; buffer[255] = '\0';
   int attr = CRT_colors[DEFAULT_COLOR];
   int n = sizeof(buffer) - 1;
   switch ((int) field) {
   // add FreeBSD-specific fields here
   case JID: xSnprintf(buffer, n, Process_pidFormat, fp->jid); break;
   case JAIL:{
      xSnprintf(buffer, n, "%-11s ", fp->jname); break;
      if (buffer[11] != '\0') {
         buffer[11] = ' ';
         buffer[12] = '\0';
      }
      break;
   }
   default:
      Process_defaultWriteField(this, str, field);
      return;
   }
   RichString_append(str, attr, buffer);
}

long FreeBSDProcess_compare(const void* v1, const void* v2) {
   FreeBSDProcess *p1, *p2;
   Settings *settings = ((Process*)v1)->settings;
   if (settings->direction == 1) {
      p1 = (FreeBSDProcess*)v1;
      p2 = (FreeBSDProcess*)v2;
   } else {
      p2 = (FreeBSDProcess*)v1;
      p1 = (FreeBSDProcess*)v2;
   }
   switch ((int) settings->sortKey) {
   // add FreeBSD-specific fields here
   case JID:
      return (p1->jid - p2->jid);
   case JAIL:
      return strcmp(p1->jname ? p1->jname : "", p2->jname ? p2->jname : "");
   default:
      return Process_compare(v1, v2);
   }
}

char* FreeBSDProcess_readJailName(struct kinfo_proc* kproc) {
   int    jid;
   struct iovec jiov[6];
   char*  jname;
   char   jnamebuf[MAXHOSTNAMELEN];

   if (kproc->ki_jid != 0 ){
      memset(jnamebuf, 0, sizeof(jnamebuf));
      *(const void **)&jiov[0].iov_base = "jid";
      jiov[0].iov_len = sizeof("jid");
      jiov[1].iov_base = &kproc->ki_jid;
      jiov[1].iov_len = sizeof(kproc->ki_jid);
      *(const void **)&jiov[2].iov_base = "name";
      jiov[2].iov_len = sizeof("name");
      jiov[3].iov_base = jnamebuf;
      jiov[3].iov_len = sizeof(jnamebuf);
      *(const void **)&jiov[4].iov_base = "errmsg";
      jiov[4].iov_len = sizeof("errmsg");
      jiov[5].iov_base = jail_errmsg;
      jiov[5].iov_len = JAIL_ERRMSGLEN;
      jail_errmsg[0] = 0;
      jid = jail_get(jiov, 6, 0);
      if (jid < 0) {
         if (!jail_errmsg[0])
            xSnprintf(jail_errmsg, JAIL_ERRMSGLEN, "jail_get: %s", strerror(errno));
            return NULL;
      } else if (jid == kproc->ki_jid) {
         jname = xStrdup(jnamebuf);
         if (jname == NULL)
            strerror_r(errno, jail_errmsg, JAIL_ERRMSGLEN);
         return jname;
      } else {
         return NULL;
      }
   } else {
      jnamebuf[0]='-';
      jnamebuf[1]='\0';
      jname = xStrdup(jnamebuf);
   }
   return jname;
}

char* FreeBSDProcess_readProcessName(kvm_t* kd, struct kinfo_proc* kproc, int* basenameEnd) {
   char** argv = kvm_getargv(kd, kproc, 0);
   if (!argv) {
      return xStrdup(kproc->ki_comm);
   }
   int len = 0;
   for (int i = 0; argv[i]; i++) {
      len += strlen(argv[i]) + 1;
   }
   char* comm = xMalloc(len);
   char* at = comm;
   *basenameEnd = 0;
   for (int i = 0; argv[i]; i++) {
      at = stpcpy(at, argv[i]);
      if (!*basenameEnd) {
         *basenameEnd = at - comm;
      }
      *at = ' ';
      at++;
   }
   at--;
   *at = '\0';
   return comm;
}

bool Process_update(Process* proc, bool isNew, ProcessList* pl, ProcessScanData* psd) {
   FreeBSDProcess* fp = (FreeBSDProcess*) proc;
   FreeBSDProcessList* fpl = (FreeBSDProcessList*) pl;
   FreeBSDProcessScanData* fpsd = (FreeBSDProcessScanData*) psd;

   struct kinfo_proc* kproc = ((FreeBSDProcessScanData*)psd)->kproc;

   if (isNew) {
      fp->jid = kproc->ki_jid;
      proc->pid = kproc->ki_pid;
      if ( ! ((kproc->ki_pid == 0) || (kproc->ki_pid == 1) ) && kproc->ki_flag & P_SYSTEM)
        fp->kernel = 1;
      else
        fp->kernel = 0;
      proc->ppid = kproc->ki_ppid;
      proc->tpgid = kproc->ki_tpgid;
      proc->tgid = kproc->ki_pid;
      proc->session = kproc->ki_sid;
      proc->tty_nr = kproc->ki_tdev;
      proc->pgrp = kproc->ki_pgid;
      proc->st_uid = kproc->ki_uid;
      proc->starttime_ctime = kproc->ki_start.tv_sec;
      proc->comm = FreeBSDProcess_readProcessName(fpl->kd, kproc, &proc->basenameOffset);
      fp->jname = FreeBSDProcess_readJailName(kproc);
      proc->threadFlags = ((fp->kernel == 1) ? PROCESS_KERNEL_THREAD : 0)
                        | ((proc->pid != proc->tgid) ? PROCESS_USERLAND_THREAD : 0);
   } else {
      if(fp->jid != kproc->ki_jid) {
         // process can enter jail anytime
         fp->jid = kproc->ki_jid;
         free(fp->jname);
         fp->jname = FreeBSDProcess_readJailName(kproc);
      }
      if (proc->ppid != kproc->ki_ppid) {
         // if there are reapers in the system, process can get reparented anytime
         proc->ppid = kproc->ki_ppid;
      }
      if(proc->st_uid != kproc->ki_uid) {
         // some processes change users (eg. to lower privs)
         proc->st_uid = kproc->ki_uid;
      }
      if (proc->settings->updateProcessNames) {
         free(proc->comm);
         proc->comm = FreeBSDProcess_readProcessName(fpl->kd, kproc, &proc->basenameOffset);
      }
   }

   // from FreeBSD source /src/usr.bin/top/machine.c
   proc->m_size = kproc->ki_size / 1024 / fpsd->pageSizeKb;
   proc->m_resident = kproc->ki_rssize;
   proc->percent_mem = (proc->m_resident * PAGE_SIZE_KB) / (double)(pl->totalMem) * 100.0;
   proc->nlwp = kproc->ki_numthreads;
   proc->time = (kproc->ki_runtime + 5000) / 10000;

   proc->percent_cpu = 100.0 * ((double)kproc->ki_pctcpu / (double)fpsd->kernelFScale);
   proc->percent_mem = 100.0 * (proc->m_resident * PAGE_SIZE_KB) / (double)(pl->totalMem);

//   if (proc->percent_cpu > 0.1) {
//      // system idle process should own all CPU time left regardless of CPU count
//      if ( strcmp("idle", kproc->ki_comm) == 0 ) {
//         isIdleProcess = true;
//      }
//   }

   proc->priority = kproc->ki_pri.pri_level - PZERO;

   if (strcmp("intr", kproc->ki_comm) == 0 && kproc->ki_flag & P_SYSTEM) {
      proc->nice = 0; //@etosan: intr kernel process (not thread) has weird nice value
   } else if (kproc->ki_pri.pri_class == PRI_TIMESHARE) {
      proc->nice = kproc->ki_nice - NZERO;
   } else if (PRI_IS_REALTIME(kproc->ki_pri.pri_class)) {
      proc->nice = PRIO_MIN - 1 - (PRI_MAX_REALTIME - kproc->ki_pri.pri_level);
   } else {
      proc->nice = PRIO_MAX + 1 + kproc->ki_pri.pri_level - PRI_MIN_IDLE;
   }

   switch (kproc->ki_stat) {
   case SIDL:   proc->state = 'I'; break;
   case SRUN:   proc->state = 'R'; break;
   case SSLEEP: proc->state = 'S'; break;
   case SSTOP:  proc->state = 'T'; break;
   case SZOMB:  proc->state = 'Z'; break;
   case SWAIT:  proc->state = 'D'; break;
   case SLOCK:  proc->state = 'L'; break;
   default:     proc->state = '?';
   }
   
   return proc;
}
