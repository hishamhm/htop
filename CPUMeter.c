/*
htop - CPUMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "CPUMeter.h"

#include "CRT.h"
#include "ProcessList.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*{
#include "Meter.h"
}*/

int CPUMeter_attributes[] = {
   CPU_NICE, CPU_NORMAL, CPU_KERNEL, CPU_IRQ, CPU_SOFTIRQ, CPU_STEAL, CPU_GUEST, CPU_IOWAIT
};

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

static void CPUMeter_init(Meter* htop_this) {
   int cpu = htop_this->param;
   if (htop_this->pl->cpuCount > 1) {
      char caption[10];
      sprintf(caption, "%-3d", ProcessList_cpuId(htop_this->pl, cpu - 1));
      Meter_setCaption(htop_this, caption);
   }
   if (htop_this->param == 0)
      Meter_setCaption(htop_this, "Avg");
}

static void CPUMeter_setValues(Meter* htop_this, char* buffer, int size) {
   ProcessList* pl = htop_this->pl;
   int cpu = htop_this->param;
   if (cpu > htop_this->pl->cpuCount) {
      snprintf(buffer, size, "absent");
      return;
   }
   CPUData* cpuData = &(pl->cpus[cpu]);
   double total = (double) ( cpuData->totalPeriod == 0 ? 1 : cpuData->totalPeriod);
   double percent;
   double* v = htop_this->values;
   v[0] = cpuData->nicePeriod / total * 100.0;
   v[1] = cpuData->userPeriod / total * 100.0;
   if (pl->detailedCPUTime) {
      v[2] = cpuData->systemPeriod / total * 100.0;
      v[3] = cpuData->irqPeriod / total * 100.0;
      v[4] = cpuData->softIrqPeriod / total * 100.0;
      v[5] = cpuData->stealPeriod / total * 100.0;
      v[6] = cpuData->guestPeriod / total * 100.0;
      v[7] = cpuData->ioWaitPeriod / total * 100.0;
      Meter_setItems(htop_this, 8);
      if (pl->accountGuestInCPUMeter) {
         percent = v[0]+v[1]+v[2]+v[3]+v[4]+v[5]+v[6];
      } else {
         percent = v[0]+v[1]+v[2]+v[3]+v[4];
      }       
   } else {
      v[2] = cpuData->systemAllPeriod / total * 100.0;
      v[3] = (cpuData->stealPeriod + cpuData->guestPeriod) / total * 100.0;
      Meter_setItems(htop_this, 4);
      percent = v[0]+v[1]+v[2]+v[3];
   }
   percent = MIN(100.0, MAX(0.0, percent));      
   if (isnan(percent)) percent = 0.0;
   snprintf(buffer, size, "%5.1f%%", percent);
}

static void CPUMeter_display(Object* cast, RichString* out) {
   char buffer[50];
   Meter* htop_this = (Meter*)cast;
   RichString_prune(out);
   if (htop_this->param > htop_this->pl->cpuCount) {
      RichString_append(out, CRT_colors[METER_TEXT], "absent");
      return;
   }
   sprintf(buffer, "%5.1f%% ", htop_this->values[1]);
   RichString_append(out, CRT_colors[METER_TEXT], ":");
   RichString_append(out, CRT_colors[CPU_NORMAL], buffer);
   if (htop_this->pl->detailedCPUTime) {
      sprintf(buffer, "%5.1f%% ", htop_this->values[2]);
      RichString_append(out, CRT_colors[METER_TEXT], "sy:");
      RichString_append(out, CRT_colors[CPU_KERNEL], buffer);
      sprintf(buffer, "%5.1f%% ", htop_this->values[0]);
      RichString_append(out, CRT_colors[METER_TEXT], "ni:");
      RichString_append(out, CRT_colors[CPU_NICE], buffer);
      sprintf(buffer, "%5.1f%% ", htop_this->values[3]);
      RichString_append(out, CRT_colors[METER_TEXT], "hi:");
      RichString_append(out, CRT_colors[CPU_IRQ], buffer);
      sprintf(buffer, "%5.1f%% ", htop_this->values[4]);
      RichString_append(out, CRT_colors[METER_TEXT], "si:");
      RichString_append(out, CRT_colors[CPU_SOFTIRQ], buffer);
      if (htop_this->values[5]) {
         sprintf(buffer, "%5.1f%% ", htop_this->values[5]);
         RichString_append(out, CRT_colors[METER_TEXT], "st:");
         RichString_append(out, CRT_colors[CPU_STEAL], buffer);
      }
      if (htop_this->values[6]) {
         sprintf(buffer, "%5.1f%% ", htop_this->values[6]);
         RichString_append(out, CRT_colors[METER_TEXT], "gu:");
         RichString_append(out, CRT_colors[CPU_GUEST], buffer);
      }
      sprintf(buffer, "%5.1f%% ", htop_this->values[7]);
      RichString_append(out, CRT_colors[METER_TEXT], "wa:");
      RichString_append(out, CRT_colors[CPU_IOWAIT], buffer);
   } else {
      sprintf(buffer, "%5.1f%% ", htop_this->values[2]);
      RichString_append(out, CRT_colors[METER_TEXT], "sys:");
      RichString_append(out, CRT_colors[CPU_KERNEL], buffer);
      sprintf(buffer, "%5.1f%% ", htop_this->values[0]);
      RichString_append(out, CRT_colors[METER_TEXT], "low:");
      RichString_append(out, CRT_colors[CPU_NICE], buffer);
      if (htop_this->values[3]) {
         sprintf(buffer, "%5.1f%% ", htop_this->values[3]);
         RichString_append(out, CRT_colors[METER_TEXT], "vir:");
         RichString_append(out, CRT_colors[CPU_GUEST], buffer);
      }
   }
}

static void AllCPUsMeter_getRange(Meter* htop_this, int* start, int* count) {
   int cpus = htop_this->pl->cpuCount;
   switch(Meter_name(htop_this)[0]) {
      default:
      case 'A': // All
         *start = 0;
         *count = cpus;
         break;
      case 'L': // First Half
         *start = 0;
         *count = (cpus+1) / 2;
         break;
      case 'R': // Second Half
         *start = (cpus+1) / 2;
         *count = cpus / 2;
         break;
   }
}

static void AllCPUsMeter_init(Meter* htop_this) {
   int cpus = htop_this->pl->cpuCount;
   if (!htop_this->drawData)
      htop_this->drawData = calloc(cpus, sizeof(Meter*));
   Meter** meters = (Meter**) htop_this->drawData;
   int start, count;
   AllCPUsMeter_getRange(htop_this, &start, &count);
   for (int i = 0; i < count; i++) {
      if (!meters[i])
         meters[i] = Meter_new(htop_this->pl, start+i+1, (MeterClass*) Class(CPUMeter));
      Meter_init(meters[i]);
   }
   if (htop_this->mode == 0)
      htop_this->mode = BAR_METERMODE;
   int h = Meter_modes[htop_this->mode]->h;
   if (strchr(Meter_name(htop_this), '2'))
      htop_this->h = h * ((count+1) / 2);
   else
      htop_this->h = h * count;
}

static void AllCPUsMeter_done(Meter* htop_this) {
   Meter** meters = (Meter**) htop_this->drawData;
   int start, count;
   AllCPUsMeter_getRange(htop_this, &start, &count);
   for (int i = 0; i < count; i++)
      Meter_delete((Object*)meters[i]);
}

static void AllCPUsMeter_updateMode(Meter* htop_this, int mode) {
   Meter** meters = (Meter**) htop_this->drawData;
   htop_this->mode = mode;
   int h = Meter_modes[mode]->h;
   int start, count;
   AllCPUsMeter_getRange(htop_this, &start, &count);
   for (int i = 0; i < count; i++) {
      Meter_setMode(meters[i], mode);
   }
   if (strchr(Meter_name(htop_this), '2'))
      htop_this->h = h * ((count+1) / 2);
   else
      htop_this->h = h * count;
}

static void DualColCPUsMeter_draw(Meter* htop_this, int x, int y, int w) {
   Meter** meters = (Meter**) htop_this->drawData;
   int start, count;
   AllCPUsMeter_getRange(htop_this, &start, &count);
   int height = (count+1)/2;
   int startY = y;
   for (int i = 0; i < height; i++) {
      meters[i]->draw(meters[i], x, y, (w-2)/2);
      y += meters[i]->h;
   }
   y = startY;
   for (int i = height; i < count; i++) {
      meters[i]->draw(meters[i], x+(w-1)/2+2, y, (w-2)/2);
      y += meters[i]->h;
   }
}

static void SingleColCPUsMeter_draw(Meter* htop_this, int x, int y, int w) {
   Meter** meters = (Meter**) htop_this->drawData;
   int start, count;
   AllCPUsMeter_getRange(htop_this, &start, &count);
   for (int i = 0; i < count; i++) {
      meters[i]->draw(meters[i], x, y, w);
      y += meters[i]->h;
   }
}

MeterClass CPUMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .setValues = CPUMeter_setValues, 
   .defaultMode = BAR_METERMODE,
   .maxItems = 8,
   .total = 100.0,
   .attributes = CPUMeter_attributes, 
   .name = "CPU",
   .uiName = "CPU",
   .caption = "CPU",
   .init = CPUMeter_init
};

MeterClass AllCPUsMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .attributes = CPUMeter_attributes, 
   .name = "AllCPUs",
   .uiName = "CPUs (1/1)",
   .caption = "CPU",
   .draw = SingleColCPUsMeter_draw,
   .init = AllCPUsMeter_init,
   .updateMode = AllCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

MeterClass AllCPUs2Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .attributes = CPUMeter_attributes, 
   .name = "AllCPUs2",
   .uiName = "CPUs (1&2/2)",
   .caption = "CPU",
   .draw = DualColCPUsMeter_draw,
   .init = AllCPUsMeter_init,
   .updateMode = AllCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

MeterClass LeftCPUsMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .attributes = CPUMeter_attributes, 
   .name = "LeftCPUs",
   .uiName = "CPUs (1/2)",
   .caption = "CPU",
   .draw = SingleColCPUsMeter_draw,
   .init = AllCPUsMeter_init,
   .updateMode = AllCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

MeterClass RightCPUsMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .attributes = CPUMeter_attributes, 
   .name = "RightCPUs",
   .uiName = "CPUs (2/2)",
   .caption = "CPU",
   .draw = SingleColCPUsMeter_draw,
   .init = AllCPUsMeter_init,
   .updateMode = AllCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

MeterClass LeftCPUs2Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .attributes = CPUMeter_attributes, 
   .name = "LeftCPUs2",
   .uiName = "CPUs (1&2/4)",
   .caption = "CPU",
   .draw = DualColCPUsMeter_draw,
   .init = AllCPUsMeter_init,
   .updateMode = AllCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

MeterClass RightCPUs2Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .attributes = CPUMeter_attributes, 
   .name = "RightCPUs2",
   .uiName = "CPUs (3&4/4)",
   .caption = "CPU",
   .draw = DualColCPUsMeter_draw,
   .init = AllCPUsMeter_init,
   .updateMode = AllCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

