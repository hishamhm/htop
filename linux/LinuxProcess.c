/*
htop - LinuxProcess.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Process.h"
#include "ProcessList.h"
#include "LinuxProcess.h"
#include "LinuxProcessList.h"
#include "Platform.h"
#include "CRT.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <errno.h>
#include <time.h>

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

#define PROCESS_FLAG_LINUX_IOPRIO   0x0100
#define PROCESS_FLAG_LINUX_OPENVZ   0x0200
#define PROCESS_FLAG_LINUX_VSERVER  0x0400
#define PROCESS_FLAG_LINUX_CGROUP   0x0800
#define PROCESS_FLAG_LINUX_OOM      0x1000

typedef enum UnsupportedProcessFields {
   FLAGS = 9,
   ITREALVALUE = 20,
   VSIZE = 22,
   RSS = 23,
   RLIM = 24,
   STARTCODE = 25,
   ENDCODE = 26,
   STARTSTACK = 27,
   KSTKESP = 28,
   KSTKEIP = 29,
   SIGNAL = 30,
   BLOCKED = 31,
   SSIGIGNORE = 32,
   SIGCATCH = 33,
   WCHAN = 34,
   NSWAP = 35,
   CNSWAP = 36,
   EXIT_SIGNAL = 37,
} UnsupportedProcessField;

typedef enum LinuxProcessFields {
   CMINFLT = 11,
   CMAJFLT = 13,
   UTIME = 14,
   STIME = 15,
   CUTIME = 16,
   CSTIME = 17,
   M_SHARE = 41,
   M_TRS = 42,
   M_DRS = 43,
   M_LRS = 44,
   M_DT = 45,
   #ifdef HAVE_OPENVZ
   CTID = 100,
   VPID = 101,
   #endif
   #ifdef HAVE_VSERVER
   VXID = 102,
   #endif
   #ifdef HAVE_TASKSTATS
   RCHAR = 103,
   WCHAR = 104,
   SYSCR = 105,
   SYSCW = 106,
   RBYTES = 107,
   WBYTES = 108,
   CNCLWB = 109,
   IO_READ_RATE = 110,
   IO_WRITE_RATE = 111,
   IO_RATE = 112,
   #endif
   #ifdef HAVE_CGROUP
   CGROUP = 113,
   #endif
   OOM = 114,
   IO_PRIORITY = 115,
   #ifdef HAVE_DELAYACCT
   PERCENT_CPU_DELAY = 116,
   PERCENT_IO_DELAY = 117,
   PERCENT_SWAP_DELAY = 118,
   #endif
   LAST_PROCESSFIELD = 119,
} LinuxProcessField;

#include "IOPriority.h"

typedef struct LinuxProcess_ {
   Process super;
   IOPriority ioPriority;
   unsigned long int cminflt;
   unsigned long int cmajflt;
   unsigned long long int utime;
   unsigned long long int stime;
   unsigned long long int cutime;
   unsigned long long int cstime;
   long m_share;
   long m_trs;
   long m_drs;
   long m_lrs;
   long m_dt;
   #ifdef HAVE_TASKSTATS
   unsigned long long io_rchar;
   unsigned long long io_wchar;
   unsigned long long io_syscr;
   unsigned long long io_syscw;
   unsigned long long io_read_bytes;
   unsigned long long io_write_bytes;
   unsigned long long io_cancelled_write_bytes;
   unsigned long long io_rate_read_time;
   unsigned long long io_rate_write_time;   
   double io_rate_read_bps;
   double io_rate_write_bps;
   #endif
   #ifdef HAVE_OPENVZ
   unsigned int ctid;
   unsigned int vpid;
   #endif
   #ifdef HAVE_VSERVER
   unsigned int vxid;
   #endif
   #ifdef HAVE_CGROUP
   char* cgroup;
   #endif
   unsigned int oom;
   char* ttyDevice;
   #ifdef HAVE_DELAYACCT
   unsigned long long int delay_read_time;
   unsigned long long cpu_delay_total;
   unsigned long long blkio_delay_total;
   unsigned long long swapin_delay_total;
   float cpu_delay_percent;
   float blkio_delay_percent;
   float swapin_delay_percent;
   #endif
} LinuxProcess;

}*/

ProcessFieldData Process_fields[] = {
   [0] = { .name = "", .title = NULL, .description = NULL, .flags = 0, },
   [PID] = { .name = "PID", .title = "    PID ", .description = "Process/thread ID", .flags = 0, },
   [COMM] = { .name = "Command", .title = "Command ", .description = "Command line", .flags = 0, },
   [STATE] = { .name = "STATE", .title = "S ", .description = "Process state (S sleeping, R running, D disk, Z zombie, T traced, W paging)", .flags = 0, },
   [PPID] = { .name = "PPID", .title = "   PPID ", .description = "Parent process ID", .flags = 0, },
   [PGRP] = { .name = "PGRP", .title = "   PGRP ", .description = "Process group ID", .flags = 0, },
   [SESSION] = { .name = "SESSION", .title = "    SID ", .description = "Process's session ID", .flags = 0, },
   [TTY_NR] = { .name = "TTY_NR", .title = "TTY      ", .description = "Controlling terminal", .flags = 0, },
   [TPGID] = { .name = "TPGID", .title = "  TPGID ", .description = "Process ID of the fg process group of the controlling terminal", .flags = 0, },
   [FLAGS] = { .name = "FLAGS", .title = NULL, .description = NULL, .flags = 0, },
   [MINFLT] = { .name = "MINFLT", .title = "     MINFLT ", .description = "Number of minor faults which have not required loading a memory page from disk", .flags = 0, },
   [CMINFLT] = { .name = "CMINFLT", .title = "    CMINFLT ", .description = "Children processes' minor faults", .flags = 0, },
   [MAJFLT] = { .name = "MAJFLT", .title = "     MAJFLT ", .description = "Number of major faults which have required loading a memory page from disk", .flags = 0, },
   [CMAJFLT] = { .name = "CMAJFLT", .title = "    CMAJFLT ", .description = "Children processes' major faults", .flags = 0, },
   [UTIME] = { .name = "UTIME", .title = " UTIME+  ", .description = "User CPU time - time the process spent executing in user mode", .flags = 0, },
   [STIME] = { .name = "STIME", .title = " STIME+  ", .description = "System CPU time - time the kernel spent running system calls for this process", .flags = 0, },
   [CUTIME] = { .name = "CUTIME", .title = " CUTIME+ ", .description = "Children processes' user CPU time", .flags = 0, },
   [CSTIME] = { .name = "CSTIME", .title = " CSTIME+ ", .description = "Children processes' system CPU time", .flags = 0, },
   [PRIORITY] = { .name = "PRIORITY", .title = "PRI ", .description = "Kernel's internal priority for the process", .flags = 0, },
   [NICE] = { .name = "NICE", .title = " NI ", .description = "Nice value (the higher the value, the more it lets other processes take priority)", .flags = 0, },
   [ITREALVALUE] = { .name = "ITREALVALUE", .title = NULL, .description = NULL, .flags = 0, },
   [STARTTIME] = { .name = "STARTTIME", .title = "START ", .description = "Time the process was started", .flags = 0, },
   [VSIZE] = { .name = "VSIZE", .title = NULL, .description = NULL, .flags = 0, },
   [RSS] = { .name = "RSS", .title = NULL, .description = NULL, .flags = 0, },
   [RLIM] = { .name = "RLIM", .title = NULL, .description = NULL, .flags = 0, },
   [STARTCODE] = { .name = "STARTCODE", .title = NULL, .description = NULL, .flags = 0, },
   [ENDCODE] = { .name = "ENDCODE", .title = NULL, .description = NULL, .flags = 0, },
   [STARTSTACK] = { .name = "STARTSTACK", .title = NULL, .description = NULL, .flags = 0, },
   [KSTKESP] = { .name = "KSTKESP", .title = NULL, .description = NULL, .flags = 0, },
   [KSTKEIP] = { .name = "KSTKEIP", .title = NULL, .description = NULL, .flags = 0, },
   [SIGNAL] = { .name = "SIGNAL", .title = NULL, .description = NULL, .flags = 0, },
   [BLOCKED] = { .name = "BLOCKED", .title = NULL, .description = NULL, .flags = 0, },
   [SSIGIGNORE] = { .name = "SIGIGNORE", .title = NULL, .description = NULL, .flags = 0, },
   [SIGCATCH] = { .name = "SIGCATCH", .title = NULL, .description = NULL, .flags = 0, },
   [WCHAN] = { .name = "WCHAN", .title = NULL, .description = NULL, .flags = 0, },
   [NSWAP] = { .name = "NSWAP", .title = NULL, .description = NULL, .flags = 0, },
   [CNSWAP] = { .name = "CNSWAP", .title = NULL, .description = NULL, .flags = 0, },
   [EXIT_SIGNAL] = { .name = "EXIT_SIGNAL", .title = NULL, .description = NULL, .flags = 0, },
   [PROCESSOR] = { .name = "PROCESSOR", .title = "CPU ", .description = "Id of the CPU the process last executed on", .flags = 0, },
   [M_SIZE] = { .name = "M_SIZE", .title = " VIRT ", .description = "Total program size in virtual memory", .flags = 0, },
   [M_RESIDENT] = { .name = "M_RESIDENT", .title = "  RES ", .description = "Resident set size, size of the text and data sections, plus stack usage", .flags = 0, },
   [M_SHARE] = { .name = "M_SHARE", .title = "  SHR ", .description = "Size of the process's shared pages", .flags = 0, },
   [M_TRS] = { .name = "M_TRS", .title = " CODE ", .description = "Size of the text segment of the process", .flags = 0, },
   [M_DRS] = { .name = "M_DRS", .title = " DATA ", .description = "Size of the data segment plus stack usage of the process", .flags = 0, },
   [M_LRS] = { .name = "M_LRS", .title = " LIB ", .description = "The library size of the process", .flags = 0, },
   [M_DT] = { .name = "M_DT", .title = " DIRTY ", .description = "Size of the dirty pages of the process", .flags = 0, },
   [ST_UID] = { .name = "ST_UID", .title = " UID ", .description = "User ID of the process owner", .flags = 0, },
   [PERCENT_CPU] = { .name = "PERCENT_CPU", .title = "CPU% ", .description = "Percentage of the CPU time the process used in the last sampling", .flags = 0, },
   [PERCENT_MEM] = { .name = "PERCENT_MEM", .title = "MEM% ", .description = "Percentage of the memory the process is using, based on resident memory size", .flags = 0, },
   [USER] = { .name = "USER", .title = "USER      ", .description = "Username of the process owner (or user ID if name cannot be determined)", .flags = 0, },
   [TIME] = { .name = "TIME", .title = "  TIME+  ", .description = "Total time the process has spent in user and system time", .flags = 0, },
   [NLWP] = { .name = "NLWP", .title = "NLWP ", .description = "Number of threads in the process", .flags = 0, },
   [TGID] = { .name = "TGID", .title = "   TGID ", .description = "Thread group ID (i.e. process ID)", .flags = 0, },
#ifdef HAVE_OPENVZ
   [CTID] = { .name = "CTID", .title = "   CTID ", .description = "OpenVZ container ID (a.k.a. virtual environment ID)", .flags = PROCESS_FLAG_LINUX_OPENVZ, },
   [VPID] = { .name = "VPID", .title = " VPID ", .description = "OpenVZ process ID", .flags = PROCESS_FLAG_LINUX_OPENVZ, },
#endif
#ifdef HAVE_VSERVER
   [VXID] = { .name = "VXID", .title = " VXID ", .description = "VServer process ID", .flags = PROCESS_FLAG_LINUX_VSERVER, },
#endif
#ifdef HAVE_TASKSTATS
   [RCHAR] = { .name = "RCHAR", .title = "    RD_CHAR ", .description = "Number of bytes the process has read", .flags = PROCESS_FLAG_IO, },
   [WCHAR] = { .name = "WCHAR", .title = "    WR_CHAR ", .description = "Number of bytes the process has written", .flags = PROCESS_FLAG_IO, },
   [SYSCR] = { .name = "SYSCR", .title = "    RD_SYSC ", .description = "Number of read(2) syscalls for the process", .flags = PROCESS_FLAG_IO, },
   [SYSCW] = { .name = "SYSCW", .title = "    WR_SYSC ", .description = "Number of write(2) syscalls for the process", .flags = PROCESS_FLAG_IO, },
   [RBYTES] = { .name = "RBYTES", .title = "  IO_RBYTES ", .description = "Bytes of read(2) I/O for the process", .flags = PROCESS_FLAG_IO, },
   [WBYTES] = { .name = "WBYTES", .title = "  IO_WBYTES ", .description = "Bytes of write(2) I/O for the process", .flags = PROCESS_FLAG_IO, },
   [CNCLWB] = { .name = "CNCLWB", .title = "  IO_CANCEL ", .description = "Bytes of cancelled write(2) I/O", .flags = PROCESS_FLAG_IO, },
   [IO_READ_RATE] = { .name = "IO_READ_RATE", .title = "  DISK READ ", .description = "The I/O rate of read(2) in bytes per second for the process", .flags = PROCESS_FLAG_IO, },
   [IO_WRITE_RATE] = { .name = "IO_WRITE_RATE", .title = " DISK WRITE ", .description = "The I/O rate of write(2) in bytes per second for the process", .flags = PROCESS_FLAG_IO, },
   [IO_RATE] = { .name = "IO_RATE", .title = "   DISK R/W ", .description = "Total I/O rate in bytes per second", .flags = PROCESS_FLAG_IO, },
#endif
#ifdef HAVE_CGROUP
   [CGROUP] = { .name = "CGROUP", .title = "    CGROUP ", .description = "Which cgroup the process is in", .flags = PROCESS_FLAG_LINUX_CGROUP, },
#endif
   [OOM] = { .name = "OOM", .title = "    OOM ", .description = "OOM (Out-of-Memory) killer score", .flags = PROCESS_FLAG_LINUX_OOM, },
   [IO_PRIORITY] = { .name = "IO_PRIORITY", .title = "IO ", .description = "I/O priority", .flags = PROCESS_FLAG_LINUX_IOPRIO, },
#ifdef HAVE_DELAYACCT
   [PERCENT_CPU_DELAY] = { .name = "PERCENT_CPU_DELAY", .title = "CPUD% ", .description = "CPU delay %", .flags = 0, },
   [PERCENT_IO_DELAY] = { .name = "PERCENT_IO_DELAY", .title = "IOD% ", .description = "Block I/O delay %", .flags = 0, },
   [PERCENT_SWAP_DELAY] = { .name = "PERCENT_SWAP_DELAY", .title = "SWAPD% ", .description = "Swapin delay %", .flags = 0, },
#endif
   [LAST_PROCESSFIELD] = { .name = "*** report bug! ***", .title = NULL, .description = NULL, .flags = 0, },
};

ProcessPidColumn Process_pidColumns[] = {
   { .id = PID, .label = "PID" },
   { .id = PPID, .label = "PPID" },
   #ifdef HAVE_OPENVZ
   { .id = VPID, .label = "VPID" },
   #endif
   { .id = TPGID, .label = "TPGID" },
   { .id = TGID, .label = "TGID" },
   { .id = PGRP, .label = "PGRP" },
   { .id = SESSION, .label = "SID" },
   { .id = OOM, .label = "OOM" },
   { .id = 0, .label = NULL },
};

ObjectClass LinuxProcess_class = {
   .extends = Class(Process),
   .display = Process_display,
   .delete = Process_delete,
   .compare = LinuxProcess_compare
};

Process* Process_new(Settings* settings) {
   LinuxProcess* this = xCalloc(1, sizeof(LinuxProcess));
   Object_setClass(this, Class(LinuxProcess));
   Process_init(&this->super, settings);
   return (Process*) this;
}

void Process_delete(Object* cast) {
   LinuxProcess* this = (LinuxProcess*) cast;
   Process_done((Process*)cast);
#ifdef HAVE_CGROUP
   free(this->cgroup);
#endif
   free(this->ttyDevice);
   free(this);
}

/*
[1] Note that before kernel 2.6.26 a process that has not asked for
an io priority formally uses "none" as scheduling class, but the
io scheduler will treat such processes as if it were in the best
effort class. The priority within the best effort class will  be
dynamically  derived  from  the  cpu  nice level of the process:
io_priority = (cpu_nice + 20) / 5. -- From ionice(1) man page
*/
#define LinuxProcess_effectiveIOPriority(p_) (IOPriority_class(p_->ioPriority) == IOPRIO_CLASS_NONE ? IOPriority_tuple(IOPRIO_CLASS_BE, (p_->super.nice + 20) / 5) : p_->ioPriority)

IOPriority LinuxProcess_updateIOPriority(LinuxProcess* this) {
   IOPriority ioprio = 0;
// Other OSes masquerading as Linux (NetBSD?) don't have this syscall
#ifdef SYS_ioprio_get
   ioprio = syscall(SYS_ioprio_get, IOPRIO_WHO_PROCESS, this->super.pid);
#endif
   this->ioPriority = ioprio;
   return ioprio;
}

bool LinuxProcess_setIOPriority(LinuxProcess* this, IOPriority ioprio) {
// Other OSes masquerading as Linux (NetBSD?) don't have this syscall
#ifdef SYS_ioprio_set
   syscall(SYS_ioprio_set, IOPRIO_WHO_PROCESS, this->super.pid, ioprio);
#endif
   return (LinuxProcess_updateIOPriority(this) == ioprio);
}

#ifdef HAVE_DELAYACCT
void LinuxProcess_printDelay(float delay_percent, char* buffer, int n) {
  if (delay_percent == -1LL) {
    xSnprintf(buffer, n, " N/A  ");
  } else {
    xSnprintf(buffer, n, "%4.1f  ", delay_percent);
  }
}
#endif

void Process_writeField(Process* this, RichString* str, ProcessField field) {
   LinuxProcess* lp = (LinuxProcess*) this;
   bool coloring = this->settings->highlightMegabytes;
   char buffer[256]; buffer[255] = '\0';
   int attr = CRT_colors[DEFAULT_COLOR];
   int n = sizeof(buffer) - 1;
   switch ((int)field) {
   case TTY_NR: {
      if (lp->ttyDevice) {
         xSnprintf(buffer, n, "%-9s", lp->ttyDevice + 5 /* skip "/dev/" */);
      } else {
         attr = CRT_colors[PROCESS_SHADOW];
         xSnprintf(buffer, n, "?        ");
      }
      break;
   }
   case CMINFLT: Process_colorNumber(str, lp->cminflt, coloring); return;
   case CMAJFLT: Process_colorNumber(str, lp->cmajflt, coloring); return;
   case M_DRS: Process_humanNumber(str, lp->m_drs * PAGE_SIZE_KB, coloring); return;
   case M_DT: Process_humanNumber(str, lp->m_dt * PAGE_SIZE_KB, coloring); return;
   case M_LRS: Process_humanNumber(str, lp->m_lrs * PAGE_SIZE_KB, coloring); return;
   case M_TRS: Process_humanNumber(str, lp->m_trs * PAGE_SIZE_KB, coloring); return;
   case M_SHARE: Process_humanNumber(str, lp->m_share * PAGE_SIZE_KB, coloring); return;
   case UTIME: Process_printTime(str, lp->utime); return;
   case STIME: Process_printTime(str, lp->stime); return;
   case CUTIME: Process_printTime(str, lp->cutime); return;
   case CSTIME: Process_printTime(str, lp->cstime); return;
   #ifdef HAVE_TASKSTATS
   case RCHAR:  Process_colorNumber(str, lp->io_rchar, coloring); return;
   case WCHAR:  Process_colorNumber(str, lp->io_wchar, coloring); return;
   case SYSCR:  Process_colorNumber(str, lp->io_syscr, coloring); return;
   case SYSCW:  Process_colorNumber(str, lp->io_syscw, coloring); return;
   case RBYTES: Process_colorNumber(str, lp->io_read_bytes, coloring); return;
   case WBYTES: Process_colorNumber(str, lp->io_write_bytes, coloring); return;
   case CNCLWB: Process_colorNumber(str, lp->io_cancelled_write_bytes, coloring); return;
   case IO_READ_RATE:  Process_outputRate(str, buffer, n, lp->io_rate_read_bps, coloring); return;
   case IO_WRITE_RATE: Process_outputRate(str, buffer, n, lp->io_rate_write_bps, coloring); return;
   case IO_RATE: {
      double totalRate = (lp->io_rate_read_bps != -1)
                       ? (lp->io_rate_read_bps + lp->io_rate_write_bps)
                       : -1;
      Process_outputRate(str, buffer, n, totalRate, coloring); return;
   }
   #endif
   #ifdef HAVE_OPENVZ
   case CTID: xSnprintf(buffer, n, "%7u ", lp->ctid); break;
   case VPID: xSnprintf(buffer, n, Process_pidFormat, lp->vpid); break;
   #endif
   #ifdef HAVE_VSERVER
   case VXID: xSnprintf(buffer, n, "%5u ", lp->vxid); break;
   #endif
   #ifdef HAVE_CGROUP
   case CGROUP: xSnprintf(buffer, n, "%-10s ", lp->cgroup); break;
   #endif
   case OOM: xSnprintf(buffer, n, Process_pidFormat, lp->oom); break;
   case IO_PRIORITY: {
      int klass = IOPriority_class(lp->ioPriority);
      if (klass == IOPRIO_CLASS_NONE) {
         // see note [1] above
         xSnprintf(buffer, n, "B%1d ", (int) (this->nice + 20) / 5);
      } else if (klass == IOPRIO_CLASS_BE) {
         xSnprintf(buffer, n, "B%1d ", IOPriority_data(lp->ioPriority));
      } else if (klass == IOPRIO_CLASS_RT) {
         attr = CRT_colors[PROCESS_HIGH_PRIORITY];
         xSnprintf(buffer, n, "R%1d ", IOPriority_data(lp->ioPriority));
      } else if (klass == IOPRIO_CLASS_IDLE) {
         attr = CRT_colors[PROCESS_LOW_PRIORITY]; 
         xSnprintf(buffer, n, "id ");
      } else {
         xSnprintf(buffer, n, "?? ");
      }
      break;
   }
   #ifdef HAVE_DELAYACCT
   case PERCENT_CPU_DELAY: LinuxProcess_printDelay(lp->cpu_delay_percent, buffer, n); break;
   case PERCENT_IO_DELAY: LinuxProcess_printDelay(lp->blkio_delay_percent, buffer, n); break;
   case PERCENT_SWAP_DELAY: LinuxProcess_printDelay(lp->swapin_delay_percent, buffer, n); break;
   #endif
   default:
      Process_defaultWriteField((Process*)this, str, field);
      return;
   }
   RichString_append(str, attr, buffer);
}

long LinuxProcess_compare(const void* v1, const void* v2) {
   LinuxProcess *p1, *p2;
   Settings *settings = ((Process*)v1)->settings;
   if (settings->ss->direction == 1) {
      p1 = (LinuxProcess*)v1;
      p2 = (LinuxProcess*)v2;
   } else {
      p2 = (LinuxProcess*)v1;
      p1 = (LinuxProcess*)v2;
   }
   long long diff;
   switch ((int)settings->ss->sortKey) {
   case M_DRS:
      return (p2->m_drs - p1->m_drs);
   case M_DT:
      return (p2->m_dt - p1->m_dt);
   case M_LRS:
      return (p2->m_lrs - p1->m_lrs);
   case M_TRS:
      return (p2->m_trs - p1->m_trs);
   case M_SHARE:
      return (p2->m_share - p1->m_share);
   case UTIME:  diff = p2->utime - p1->utime; goto test_diff;
   case CUTIME: diff = p2->cutime - p1->cutime; goto test_diff;
   case STIME:  diff = p2->stime - p1->stime; goto test_diff;
   case CSTIME: diff = p2->cstime - p1->cstime; goto test_diff;
   #ifdef HAVE_TASKSTATS
   case RCHAR:  diff = p2->io_rchar - p1->io_rchar; goto test_diff;
   case WCHAR:  diff = p2->io_wchar - p1->io_wchar; goto test_diff;
   case SYSCR:  diff = p2->io_syscr - p1->io_syscr; goto test_diff;
   case SYSCW:  diff = p2->io_syscw - p1->io_syscw; goto test_diff;
   case RBYTES: diff = p2->io_read_bytes - p1->io_read_bytes; goto test_diff;
   case WBYTES: diff = p2->io_write_bytes - p1->io_write_bytes; goto test_diff;
   case CNCLWB: diff = p2->io_cancelled_write_bytes - p1->io_cancelled_write_bytes; goto test_diff;
   case IO_READ_RATE:  diff = p2->io_rate_read_bps - p1->io_rate_read_bps; goto test_diff;
   case IO_WRITE_RATE: diff = p2->io_rate_write_bps - p1->io_rate_write_bps; goto test_diff;
   case IO_RATE: diff = (p2->io_rate_read_bps + p2->io_rate_write_bps) - (p1->io_rate_read_bps + p1->io_rate_write_bps); goto test_diff;
   #endif
   #ifdef HAVE_OPENVZ
   case CTID:
      return (p2->ctid - p1->ctid);
   case VPID:
      return (p2->vpid - p1->vpid);
   #endif
   #ifdef HAVE_VSERVER
   case VXID:
      return (p2->vxid - p1->vxid);
   #endif
   #ifdef HAVE_CGROUP
   case CGROUP:
      return strcmp(p1->cgroup ? p1->cgroup : "", p2->cgroup ? p2->cgroup : "");
   #endif
   case OOM:
      return (p2->oom - p1->oom);
   #ifdef HAVE_DELAYACCT
   case PERCENT_CPU_DELAY:
      return (p2->cpu_delay_percent > p1->cpu_delay_percent ? 1 : -1);
   case PERCENT_IO_DELAY:
      return (p2->blkio_delay_percent > p1->blkio_delay_percent ? 1 : -1);
   case PERCENT_SWAP_DELAY:
      return (p2->swapin_delay_percent > p1->swapin_delay_percent ? 1 : -1);
   #endif
   case IO_PRIORITY:
      return LinuxProcess_effectiveIOPriority(p1) - LinuxProcess_effectiveIOPriority(p2);
   default:
      return Process_compare(v1, v2);
   }
   test_diff:
   return (diff > 0) ? 1 : (diff < 0 ? -1 : 0);
}

#ifdef HAVE_TASKSTATS

static void LinuxProcess_readIoFile(LinuxProcess* process, const char* dirname, const char* name, unsigned long long now) {
   char filename[MAX_NAME+1];
   filename[MAX_NAME] = '\0';

   xSnprintf(filename, MAX_NAME, "%s/%s/io", dirname, name);
   int fd = open(filename, O_RDONLY);
   if (fd == -1) {
      process->io_rate_read_bps = -1;
      process->io_rate_write_bps = -1;
      process->io_rchar = -1LL;
      process->io_wchar = -1LL;
      process->io_syscr = -1LL;
      process->io_syscw = -1LL;
      process->io_read_bytes = -1LL;
      process->io_write_bytes = -1LL;
      process->io_cancelled_write_bytes = -1LL;
      process->io_rate_read_time = -1LL;
      process->io_rate_write_time = -1LL;
      return;
   }
   
   char buffer[1024];
   ssize_t buflen = xRead(fd, buffer, 1023);
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
         if (line[5] == 'r' && strncmp(line+1, "yscr: ", 6) == 0) {
            process->io_syscr = strtoull(line+7, NULL, 10);
         } else if (strncmp(line+1, "yscw: ", 6) == 0) {
            process->io_syscw = strtoull(line+7, NULL, 10);
         }
         break;
      case 'c':
         if (strncmp(line+1, "ancelled_write_bytes: ", 22) == 0) {
           process->io_cancelled_write_bytes = strtoull(line+23, NULL, 10);
        }
      }
   }
}

#endif

static double jiffy = 0.0;

static inline unsigned long long LinuxProcess_adjustTime(unsigned long long t) {
   if(jiffy == 0.0) jiffy = sysconf(_SC_CLK_TCK);
   double jiffytime = 1.0 / jiffy;
   return (unsigned long long) t * jiffytime * 100;
}

static bool LinuxProcess_readStatFile(Process *process, const char* dirname, const char* name, char* command, int* commLen) {
   LinuxProcess* lp = (LinuxProcess*) process;
   char filename[MAX_NAME+1];
   xSnprintf(filename, MAX_NAME, "%s/%s/stat", dirname, name);
   int fd = open(filename, O_RDONLY);
   if (fd == -1)
      return false;

   static char buf[MAX_READ+1];

   int size = xRead(fd, buf, MAX_READ);
   close(fd);
   if (size <= 0) return false;
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
   *commLen = commsize;
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
   process->minflt = strtoull(location, &location, 10);
   location += 1;
   lp->cminflt = strtoull(location, &location, 10);
   location += 1;
   process->majflt = strtoull(location, &location, 10);
   location += 1;
   lp->cmajflt = strtoull(location, &location, 10);
   location += 1;
   lp->utime = LinuxProcess_adjustTime(strtoull(location, &location, 10));
   location += 1;
   lp->stime = LinuxProcess_adjustTime(strtoull(location, &location, 10));
   location += 1;
   lp->cutime = LinuxProcess_adjustTime(strtoull(location, &location, 10));
   location += 1;
   lp->cstime = LinuxProcess_adjustTime(strtoull(location, &location, 10));
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
   
   process->time = lp->utime + lp->stime;
   
   return true;
}


static bool LinuxProcess_statProcessDir(Process* process, const char* dirname, const char* name, time_t curTime) {
   char filename[MAX_NAME+1];
   filename[MAX_NAME] = '\0';

   xSnprintf(filename, MAX_NAME, "%s/%s", dirname, name);
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

static bool LinuxProcess_readStatmFile(LinuxProcess* process, const char* dirname, const char* name) {
   char filename[MAX_NAME+1];
   xSnprintf(filename, MAX_NAME, "%s/%s/statm", dirname, name);
   int fd = open(filename, O_RDONLY);
   if (fd == -1)
      return false;
   char buf[PROC_LINE_LENGTH + 1];
   ssize_t rres = xRead(fd, buf, PROC_LINE_LENGTH);
   close(fd);
   if (rres < 1) return false;

   char *p = buf;
   errno = 0;
   process->super.m_size = strtol(p, &p, 10); if (*p == ' ') p++;
   process->super.m_resident = strtol(p, &p, 10); if (*p == ' ') p++;
   process->m_share = strtol(p, &p, 10); if (*p == ' ') p++;
   process->m_trs = strtol(p, &p, 10); if (*p == ' ') p++;
   process->m_lrs = strtol(p, &p, 10); if (*p == ' ') p++;
   process->m_drs = strtol(p, &p, 10); if (*p == ' ') p++;
   process->m_dt = strtol(p, &p, 10);
   return (errno == 0);
}

#ifdef HAVE_OPENVZ

static void LinuxProcess_readOpenVZData(LinuxProcess* process, const char* dirname, const char* name) {
   if ( (access("/proc/vz", R_OK) != 0)) {
      process->vpid = process->super.pid;
      process->ctid = 0;
      return;
   }
   char filename[MAX_NAME+1];
   xSnprintf(filename, MAX_NAME, "%s/%s/stat", dirname, name);
   FILE* file = fopen(filename, "r");
   if (!file)
      return;
   (void) fscanf(file,
      "%*32u %*32s %*1c %*32u %*32u %*32u %*32u %*32u %*32u %*32u "
      "%*32u %*32u %*32u %*32u %*32u %*32u %*32u %*32u "
      "%*32u %*32u %*32u %*32u %*32u %*32u %*32u %*32u "
      "%*32u %*32u %*32u %*32u %*32u %*32u %*32u %*32u "
      "%*32u %*32u %*32u %*32u %*32u %*32u %*32u %*32u "
      "%*32u %*32u %*32u %*32u %*32u %*32u %*32u "
      "%*32u %*32u %32u %32u",
      &process->vpid, &process->ctid);
   fclose(file);
   return;
}

#endif

#ifdef HAVE_CGROUP

static void LinuxProcess_readCGroupFile(LinuxProcess* process, const char* dirname, const char* name) {
   char filename[MAX_NAME+1];
   xSnprintf(filename, MAX_NAME, "%s/%s/cgroup", dirname, name);
   FILE* file = fopen(filename, "r");
   if (!file) {
      process->cgroup = xStrdup("");
      return;
   }
   char output[PROC_LINE_LENGTH + 1];
   output[0] = '\0';
   char* at = output;
   int left = PROC_LINE_LENGTH;
   while (!feof(file) && left > 0) {
      char buffer[PROC_LINE_LENGTH + 1];
      char *ok = fgets(buffer, PROC_LINE_LENGTH, file);
      if (!ok) break;
      char* group = strchr(buffer, ':');
      if (!group) break;
      if (at != output) {
         *at = ';';
         at++;
         left--;
      }
      int wrote = snprintf(at, left, "%s", group);
      left -= wrote;
   }
   fclose(file);
   free(process->cgroup);
   process->cgroup = xStrdup(output);
}

#endif

#ifdef HAVE_VSERVER

static void LinuxProcess_readVServerData(LinuxProcess* process, const char* dirname, const char* name) {
   char filename[MAX_NAME+1];
   xSnprintf(filename, MAX_NAME, "%s/%s/status", dirname, name);
   FILE* file = fopen(filename, "r");
   if (!file)
      return;
   char buffer[PROC_LINE_LENGTH + 1];
   process->vxid = 0;
   while (fgets(buffer, PROC_LINE_LENGTH, file)) {
      if (String_startsWith(buffer, "VxID:")) {
         int vxid;
         int ok = sscanf(buffer, "VxID:\t%32d", &vxid);
         if (ok >= 1) {
            process->vxid = vxid;
         }
      }
      #if defined HAVE_ANCIENT_VSERVER
      else if (String_startsWith(buffer, "s_context:")) {
         int vxid;
         int ok = sscanf(buffer, "s_context:\t%32d", &vxid);
         if (ok >= 1) {
            process->vxid = vxid;
         }
      }
      #endif
   }
   fclose(file);
}

#endif

#ifdef HAVE_DELAYACCT

static int handleNetlinkMsg(struct nl_msg *nlmsg, void *linuxProcess) {
   struct nlmsghdr *nlhdr;
   struct nlattr *nlattrs[TASKSTATS_TYPE_MAX + 1];
   struct nlattr *nlattr;
   struct taskstats *stats;
   int rem;
   unsigned long long int timeDelta;
   LinuxProcess* lp = (LinuxProcess*) linuxProcess;

   nlhdr = nlmsg_hdr(nlmsg);

   if (genlmsg_parse(nlhdr, 0, nlattrs, TASKSTATS_TYPE_MAX, NULL) < 0) {
      return NL_SKIP;
   }

   if ((nlattr = nlattrs[TASKSTATS_TYPE_AGGR_PID]) || (nlattr = nlattrs[TASKSTATS_TYPE_NULL])) {
      stats = nla_data(nla_next(nla_data(nlattr), &rem));
      assert(lp->super.pid == stats->ac_pid);
      timeDelta = (stats->ac_etime*1000 - lp->delay_read_time);
      #define BOUNDS(x) isnan(x) ? 0.0 : (x > 100) ? 100.0 : x;
      #define DELTAPERC(x,y) BOUNDS((float) (x - y) / timeDelta * 100);
      lp->cpu_delay_percent = DELTAPERC(stats->cpu_delay_total, lp->cpu_delay_total);
      lp->blkio_delay_percent = DELTAPERC(stats->blkio_delay_total, lp->blkio_delay_total);
      lp->swapin_delay_percent = DELTAPERC(stats->swapin_delay_total, lp->swapin_delay_total);
      #undef DELTAPERC
      #undef BOUNDS
      lp->swapin_delay_total = stats->swapin_delay_total;
      lp->blkio_delay_total = stats->blkio_delay_total;
      lp->cpu_delay_total = stats->cpu_delay_total;
      lp->delay_read_time = stats->ac_etime*1000;
   }
   return NL_OK;
}

static void LinuxProcess_readDelayAcctData(LinuxProcess* process, LinuxProcessScanData* lpsd) {
   struct nl_msg *msg;

   if (nl_socket_modify_cb(lpsd->netlink_socket, NL_CB_VALID, NL_CB_CUSTOM, handleNetlinkMsg, process) < 0) {
      return;
   }

   if (! (msg = nlmsg_alloc())) {
      return;
   }

   if (! genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, lpsd->netlink_family, 0, NLM_F_REQUEST, TASKSTATS_CMD_GET, TASKSTATS_VERSION)) {
      nlmsg_free(msg);
   }

   if (nla_put_u32(msg, TASKSTATS_CMD_ATTR_PID, process->super.pid) < 0) {
      nlmsg_free(msg);
   }

   if (nl_send_sync(lpsd->netlink_socket, msg) < 0) {
      process->swapin_delay_percent = -1LL;
      process->blkio_delay_percent = -1LL;
      process->cpu_delay_percent = -1LL;
      return;
   }
   
   if (nl_recvmsgs_default(lpsd->netlink_socket) < 0) {
      return;
   }
}

#endif

static void LinuxProcess_readOomData(LinuxProcess* process, const char* dirname, const char* name) {
   char filename[MAX_NAME+1];
   xSnprintf(filename, MAX_NAME, "%s/%s/oom_score", dirname, name);
   FILE* file = fopen(filename, "r");
   if (!file) {
      return;
   }
   char buffer[PROC_LINE_LENGTH + 1];
   if (fgets(buffer, PROC_LINE_LENGTH, file)) {
      unsigned int oom;
      int ok = sscanf(buffer, "%32u", &oom);
      if (ok >= 1) {
         process->oom = oom;
      }
   }
   fclose(file);
}

static bool LinuxProcess_readCmdlineFile(Process* process, const char* dirname, const char* name) {
   if (Process_isKernelThread(process))
      return true;

   char filename[MAX_NAME+1];
   xSnprintf(filename, MAX_NAME, "%s/%s/cmdline", dirname, name);
   int fd = open(filename, O_RDONLY);
   if (fd == -1)
      return false;
         
   char command[4096+1]; // max cmdline length on Linux
   int amtRead = xRead(fd, command, sizeof(command) - 1);
   close(fd);
   int tokenEnd = 0; 
   int lastChar = 0;
   if (amtRead <= 0) {
      return false;
   }
   for (int i = 0; i < amtRead; i++) {
      if (command[i] == '\0' || command[i] == '\n') {
         if (tokenEnd == 0) {
            tokenEnd = i;
         }
         command[i] = ' ';
      } else {
         lastChar = i;
      }
   }
   if (tokenEnd == 0) {
      tokenEnd = amtRead;
   }
   command[lastChar + 1] = '\0';
   process->basenameOffset = tokenEnd;
   Process_setCommand(process, command, lastChar);

   return true;
}

bool Process_update(Process* proc, bool isNew, ProcessList* pl, ProcessScanData* psd) {
   LinuxProcess* lp = (LinuxProcess*) proc;
   LinuxProcessList* lpl = (LinuxProcessList*) pl;
   LinuxProcessScanData* lpsd = (LinuxProcessScanData*) psd;
   Settings* settings = proc->settings;
   const char* dirname = lpsd->dirname;
   const char* name = lpsd->name;

   char command[MAX_NAME+1];
   unsigned long long int lasttimes = (lp->utime + lp->stime);
   int commLen = 0;
   unsigned int tty_nr = proc->tty_nr;

   if (! LinuxProcess_readStatFile(proc, dirname, name, command, &commLen)) {
      return false;
   }

   #ifdef HAVE_TASKSTATS
   if (settings->flags & PROCESS_FLAG_IO)
      LinuxProcess_readIoFile(lp, dirname, name, lpsd->nowMsec);
   #endif

   if (! LinuxProcess_readStatmFile(lp, dirname, name)) {
      return false;
   }

   if (tty_nr != proc->tty_nr && lpl->ttyDrivers) {
      free(lp->ttyDevice);
      lp->ttyDevice = LinuxProcessList_updateTtyDevice(lpl, proc->tty_nr);
   }
   if (settings->flags & PROCESS_FLAG_LINUX_IOPRIO)
      LinuxProcess_updateIOPriority(lp);
   float percent_cpu = (lp->utime + lp->stime - lasttimes) / lpsd->period * 100.0;
   proc->percent_cpu = CLAMP(percent_cpu, 0.0, pl->cpuCount * 100.0);
   if (isnan(proc->percent_cpu)) proc->percent_cpu = 0.0;
   proc->percent_mem = (proc->m_resident * PAGE_SIZE_KB) / (double)(pl->totalMem) * 100.0;

   if(isNew) {
      proc->tgid = lpsd->mainProcess ? lpsd->mainProcess : proc->pid;
      proc->threadFlags = ((proc->pgrp == 0) ? PROCESS_KERNEL_THREAD : 0)
                        | ((proc->pid != proc->tgid) ? PROCESS_USERLAND_THREAD : 0);

      if (! LinuxProcess_statProcessDir(proc, dirname, name, lpsd->nowSec)) {
         return false;
      }
      #ifdef HAVE_OPENVZ
      if (settings->flags & PROCESS_FLAG_LINUX_OPENVZ) {
         LinuxProcess_readOpenVZData(lp, dirname, name);
      }
      #endif
      #ifdef HAVE_VSERVER
      if (settings->flags & PROCESS_FLAG_LINUX_VSERVER) {
         LinuxProcess_readVServerData(lp, dirname, name);
      }
      #endif
      if (! LinuxProcess_readCmdlineFile(proc, dirname, name)) {
         return false;
      }
   } else {
      if (settings->updateProcessNames && proc->state != 'Z') {
         if (! LinuxProcess_readCmdlineFile(proc, dirname, name)) {
            return false;
         }
      }
   }

   #ifdef HAVE_CGROUP
   if (settings->flags & PROCESS_FLAG_LINUX_CGROUP)
      LinuxProcess_readCGroupFile(lp, dirname, name);
   #endif

   #ifdef HAVE_DELAYACCT
   LinuxProcess_readDelayAcctData(lp, lpsd);
   #endif
   
   if (settings->flags & PROCESS_FLAG_LINUX_OOM)
      LinuxProcess_readOomData(lp, dirname, name);

   if (proc->state == 'Z' && (proc->basenameOffset == 0)) {
      proc->basenameOffset = -1;
      Process_setCommand(proc, command, commLen);
   } else if (Process_isThread(proc)) {
      if (settings->showThreadNames || Process_isKernelThread(proc) || (proc->state == 'Z' && proc->basenameOffset == 0)) {
         proc->basenameOffset = -1;
         Process_setCommand(proc, command, commLen);
      } else if (settings->showThreadNames) {
         if (! LinuxProcess_readCmdlineFile(proc, dirname, name)) {
            return false;
         }
      }
   }
   return true;
}
