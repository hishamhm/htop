/*
htop - unsupported/Platform.c
(C) 2014 Hisham H. Muhammad
(C) 2015 David C. Hunt
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Platform.h"
#include "CPUMeter.h"
#include "MemoryMeter.h"
#include "SwapMeter.h"
#include "TasksMeter.h"
#include "LoadAverageMeter.h"
#include "ClockMeter.h"
#include "HostnameMeter.h"
#include "UptimeMeter.h"

/*{
#include "Action.h"
#include "BatteryMeter.h"
#include "SignalsPanel.h"
#include "SolarisProcess.h"
}*/

#include <utmpx.h>
#include <sys/loadavg.h>
#include <string.h>
#include <kstat.h>

double plat_loadavg[3] = {0};

#define MINORBITS        20
#define MINORMASK        ((1U << MINORBITS) - 1)

unsigned int major(dev_t dev) {
   return ((unsigned int) ((dev) >> MINORBITS ));
}

unsigned int minor(dev_t dev) {
   return ((unsigned int) ((dev) & MINORMASK ));
}

unsigned int mkdev(unsigned int ma, unsigned int mi) {
   return (((ma) << MINORBITS) | (mi));
}

const SignalItem Platform_signals[] = {
   { .name = " 0 Cancel",      .number =  0 },
   { .name = " 1 SIGHUP",      .number =  1 },
   { .name = " 2 SIGINT",      .number =  2 },
   { .name = " 3 SIGQUIT",     .number =  3 },
   { .name = " 4 SIGILL",      .number =  4 },
   { .name = " 5 SIGTRAP",     .number =  5 },
   { .name = " 6 SIGABRT/IOT", .number =  6 },
   { .name = " 7 SIGEMT",      .number =  7 },
   { .name = " 8 SIGFPE",      .number =  8 },
   { .name = " 9 SIGKILL",     .number =  9 },
   { .name = "10 SIGBUS",      .number = 10 },
   { .name = "11 SIGSEGV",     .number = 11 },
   { .name = "12 SIGSYS",      .number = 12 },
   { .name = "13 SIGPIPE",     .number = 13 },
   { .name = "14 SIGALRM",     .number = 14 },
   { .name = "15 SIGTERM",     .number = 15 },
   { .name = "16 SIGUSR1",     .number = 16 },
   { .name = "17 SIGUSR2",     .number = 17 },
   { .name = "18 SIGCHLD/CLD", .number = 18 },
   { .name = "19 SIGPWR",      .number = 19 },
   { .name = "20 WIGWINCH",    .number = 20 },
   { .name = "21 SIGURG",      .number = 21 },
   { .name = "22 SIGPOLL/IO",  .number = 22 },
   { .name = "23 SIGSTOP",     .number = 23 },
   { .name = "24 SIGTSTP",     .number = 24 },
   { .name = "25 SIGCONT",     .number = 25 },
   { .name = "26 SIGTTIN",     .number = 26 },
   { .name = "27 SIGTTOU",     .number = 27 },
   { .name = "28 SIGVTALRM",   .number = 28 },
   { .name = "29 SIGPROF",     .number = 29 },
   { .name = "30 SIGXCPU",     .number = 30 },
   { .name = "31 SIGXFSZ",     .number = 31 },
   { .name = "32 SIGWAITING",  .number = 32 },
   { .name = "33 SIGLWP",      .number = 33 },
   { .name = "34 SIGFREEZE",   .number = 34 },
   { .name = "35 SIGTHAW",     .number = 35 },
   { .name = "36 SIGCANCEL",   .number = 36 },
   { .name = "37 SIGLOST",     .number = 37 },
   { .name = "38 SIGXRES",     .number = 38 },
   { .name = "39 SIGJVM1",     .number = 39 },
   { .name = "40 SIGJVM2",     .number = 40 },
   { .name = "41 SIGINFO",     .number = 41 },
};

const unsigned int Platform_numberOfSignals = sizeof(Platform_signals)/sizeof(SignalItem);

ProcessField Platform_defaultFields[] = { PID, USER, PRIORITY, NICE, M_SIZE, M_RESIDENT, STATE, PERCENT_CPU, PERCENT_MEM, TIME, COMM, 0 };

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
   [100] = { .name = "*** report bug! ***", .title = NULL, .description = NULL, .flags = 0, },
};

MeterClass* Platform_meterTypes[] = {
   &CPUMeter_class,
   &ClockMeter_class,
   &LoadAverageMeter_class,
   &LoadMeter_class,
   &MemoryMeter_class,
   &SwapMeter_class,
   &TasksMeter_class,
   &BatteryMeter_class,
   &HostnameMeter_class,
   &UptimeMeter_class,
   &AllCPUsMeter_class,
   &AllCPUs2Meter_class,
   &LeftCPUsMeter_class,
   &RightCPUsMeter_class,
   &LeftCPUs2Meter_class,
   &RightCPUs2Meter_class,
   &BlankMeter_class,
   NULL
};

void Platform_setBindings(Htop_Action* keys) {
   (void) keys;
}

int Platform_numberOfFields = 100;

extern char Process_pidFormat[20];

ProcessPidColumn Process_pidColumns[] = {
   { .id = 0, .label = NULL },
};

int Platform_getUptime() {
	int boot_time = 0;
	int curr_time = time(NULL);   
	struct utmpx * ent;

	while (( ent = getutxent() )) {
		if ( !strcmp("system boot", ent->ut_line )) {
			boot_time = ent->ut_tv.tv_sec;
		}      
	}

	endutxent();

	return (curr_time-boot_time);
}

void Platform_getLoadAverage(double* one, double* five, double* fifteen) {
   getloadavg( plat_loadavg, 3 );
   *one = plat_loadavg[LOADAVG_1MIN];
   *five = plat_loadavg[LOADAVG_5MIN];
   *fifteen = plat_loadavg[LOADAVG_15MIN];
}

int Platform_getMaxPid() {
   kstat_ctl_t    *kc;
   kstat_t	  *ksp;
   int            *kstat_pidmax_p;
   int            kstat_pidmax;
   char ks_module[] = "unix";
   int  ks_instance = 0;
   char ks_name[] ="var";
   char ks_vname[] = "v_proc";

   kc = kstat_open();
   ksp = kstat_lookup(kc,ks_module,ks_instance,ks_name);
// Apparently this is returning NULL for some reason...
   kstat_pidmax_p = kstat_data_lookup(ksp,ks_vname);
   if( kstat_pidmax_p != NULL ) {
      kstat_pidmax = *kstat_pidmax_p;
   } else {
      kstat_pidmax = 32778;
   } 
   kstat_close(kc);
   return kstat_pidmax;
}

double Platform_setCPUValues(Meter* this, int cpu) {
   (void) this;
   (void) cpu;
   return 0.0;
}

void Platform_setMemoryValues(Meter* this) {
   (void) this;
}

void Platform_setSwapValues(Meter* this) {
   (void) this;
}

bool Process_isThread(Process* this) {
   (void) this;
   return false;
}

char* Platform_getProcessEnv(pid_t pid) {
   (void) pid;
   return NULL;
}
