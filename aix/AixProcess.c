/*
htop - AixProcess.c
(C) 2015 Hisham H. Muhammad
(C) 2018 Calvin Buckley
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Process.h"
#include "AixProcess.h"
#include "CRT.h"
#include <stdlib.h>

/*{
#include "Settings.h"
#include <sys/corralid.h> // cid_t 

#define Process_delete AixProcess_delete

typedef enum AixProcessFields {
   // Add platform-specific fields here, with ids >= 100
   WPAR_ID = 100,
   LAST_PROCESSFIELD = 101,
} AixProcessField;

typedef struct AixProcess_ {
   Process super;
   int kernel;
   cid_t cid; // WPAR ID
   unsigned long long int stime, utime; // System/User time (stored sep for calculations)
} AixProcess;

#ifndef Process_isKernelThread
#define Process_isKernelThread(_process) (_process->kernel == 1)
#endif

#ifndef Process_isUserlandThread
// XXX
#define Process_isUserlandThread(_process) (false)
#endif

}*/

ProcessClass AixProcess_class = {
   .super = {
      .extends = Class(Process),
      .display = Process_display,
      .delete = Process_delete,
      .compare = AixProcess_compare
   },
   .writeField = (Process_WriteField) AixProcess_writeField,
};

ProcessFieldData Process_fields[] = {
   [0] = {
      .name = "",
      .title = NULL,
      .description = NULL,
      .flags = 0, },
   [PID] = {
      .name = "PID",
      .title = "    PID ",
      .description = "Process/thread ID",
      .flags = 0, },
   [COMM] = {
      .name = "Command",
      .title = "Command ",
      .description = "Command line",
      .flags = 0, },
   [STATE] = {
      .name = "STATE",
      .title = "S ",
      .description = "Process state (S sleeping, R running, D disk, Z zombie, T traced, W paging)",
      .flags = 0, },
   [PPID] = {
      .name = "PPID",
      .title = "   PPID ",
      .description = "Parent process ID",
      .flags = 0, },
   [PGRP] = {
      .name = "PGRP",
      .title = "   PGRP ",
      .description = "Process group ID",
      .flags = 0, },
   [SESSION] = {
      .name = "SESSION",
      .title = "   SESN ",
      .description = "Process's session ID",
      .flags = 0, },
   [TTY_NR] = {
      .name = "TTY_NR",
      .title = "    TTY ",
      .description = "Controlling terminal",
      .flags = 0, },
   [TPGID] = {
      .name = "TPGID",
      .title = "  TPGID ",
      .description = "Process ID of the fg process group of the controlling terminal",
      .flags = 0, },
   [MINFLT] = {
      .name = "MINFLT",
      .title = "     MINFLT ",
      .description = "Number of minor faults which have not required loading a memory page from disk",
      .flags = 0, },
   [MAJFLT] = {
      .name = "MAJFLT",
      .title = "     MAJFLT ",
      .description = "Number of major faults which have required loading a memory page from disk",
      .flags = 0, },
   [PRIORITY] = {
      .name = "PRIORITY",
      .title = "PRI ",
      .description = "Kernel's internal priority for the process",
      .flags = 0, },
   [NICE] = {
      .name = "NICE",
      .title = " NI ",
      .description = "Nice value (the higher the value, the more it lets other processes take priority)",
      .flags = 0, },
   [STARTTIME] = {
      .name = "STARTTIME",
      .title = "START ",
      .description = "Time the process was started",
      .flags = 0, },
   [PROCESSOR] = {
      .name = "PROCESSOR",
      .title = "CPU ",
      .description = "Id of the CPU the process last executed on",
      .flags = 0, },
   [M_SIZE] = {
      .name = "M_SIZE",
      .title = " VIRT ",
      .description = "Total program size in virtual memory",
      .flags = 0, },
   [M_RESIDENT] = {
      .name = "M_RESIDENT",
      .title = "  RES ",
      .description = "Resident set size, size of the text and data sections, plus stack usage",
      .flags = 0, },
   [ST_UID] = {
      .name = "ST_UID",
      .title = " UID ",
      .description = "User ID of the process owner",
      .flags = 0, },
   [PERCENT_CPU] = {
      .name = "PERCENT_CPU",
      .title = "CPU% ",
      .description = "Percentage of the CPU time the process used in the last sampling",
      .flags = 0, },
   [PERCENT_MEM] = {
      .name = "PERCENT_MEM",
      .title = "MEM% ",
      .description = "Percentage of the memory the process is using, based on resident memory size",
      .flags = 0, },
   [USER] = {
      .name = "USER",
      .title = "USER      ",
      .description = "Username of the process owner (or user ID if name cannot be determined)",
      .flags = 0, },
   [TIME] = {
      .name = "TIME",
      .title = "  TIME+  ",
      .description = "Total time the process has spent in user and system time",
      .flags = 0, },
   [NLWP] = {
      .name = "NLWP",
      .title = "NLWP ",
      .description = "Number of threads in the process",
      .flags = 0, },
   [TGID] = {
      .name = "TGID",
      .title = "   TGID ",
      .description = "Thread group ID (i.e. process ID)",
      .flags = 0, },
   [WPAR_ID] = {
      .name = "WPAR",
      .title = "   WPAR ",
      .description = "Workload Partition ID",
      .flags = 0, },
   [LAST_PROCESSFIELD] = {
      .name = "*** report bug! ***",
      .title = NULL,
      .description = NULL,
      .flags = 0, },
};

ProcessPidColumn Process_pidColumns[] = {
   { .id = PID, .label = "PID" },
   { .id = PPID, .label = "PPID" },
   { .id = TPGID, .label = "TPGID" },
   { .id = TGID, .label = "TGID" },
   { .id = PGRP, .label = "PGRP" },
   { .id = SESSION, .label = "SESN" },
   { .id = 0, .label = NULL },
};

AixProcess* AixProcess_new(Settings* settings) {
   AixProcess* this = xCalloc(1, sizeof(AixProcess));
   Object_setClass(this, Class(AixProcess));
   Process_init(&this->super, settings);
   return this;
}

void AixProcess_delete(Object* cast) {
   AixProcess* this = (AixProcess*) cast;
   Process_done((Process*)cast);
   // free platform-specific fields here
   free(this);
}

void AixProcess_writeField(Process* this, RichString* str, ProcessField field) {
   AixProcess* fp = (AixProcess*) this;
   char buffer[256]; buffer[255] = '\0';
   int attr = CRT_colors[DEFAULT_COLOR];
   int n = sizeof(buffer) - 1;
   switch ((int) field) {
   // add Aix-specific fields here
   case WPAR_ID: xSnprintf(buffer, n, Process_pidFormat, fp->cid); break;
   default:
      Process_writeField(this, str, field);
      return;
   }
   RichString_append(str, attr, buffer);
}

long AixProcess_compare(const void* v1, const void* v2) {
   AixProcess *p1, *p2;
   Settings *settings = ((Process*)v1)->settings;
   if (settings->direction == 1) {
      p1 = (AixProcess*)v1;
      p2 = (AixProcess*)v2;
   } else {
      p2 = (AixProcess*)v1;
      p1 = (AixProcess*)v2;
   }
   switch ((int) settings->sortKey) {
   // add Aix-specific fields here
   case WPAR_ID:
      return (p1->cid - p2->cid);
   default:
      return Process_compare(v1, v2);
   }
}
