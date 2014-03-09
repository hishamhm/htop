/*
htop - LoadAverageMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "LoadAverageMeter.h"

#include "CRT.h"

#include <assert.h>

/*{
#include "Meter.h"
}*/

int LoadAverageMeter_attributes[] = {
   LOAD_AVERAGE_FIFTEEN, LOAD_AVERAGE_FIVE, LOAD_AVERAGE_ONE
};

int LoadMeter_attributes[] = { LOAD };

static inline void LoadAverageMeter_scan(double* one, double* five, double* fifteen) {
   int activeProcs, totalProcs, lastProc;
   *one = 0; *five = 0; *fifteen = 0;
   FILE *fd = fopen(PROCDIR "/loadavg", "r");
   if (fd) {
      int total = fscanf(fd, "%32lf %32lf %32lf %32d/%32d %32d", one, five, fifteen,
         &activeProcs, &totalProcs, &lastProc);
      (void) total;
      assert(total == 6);
      fclose(fd);
   }
}

static void LoadAverageMeter_setValues(Meter* htop_this, char* buffer, int size) {
   LoadAverageMeter_scan(&htop_this->values[2], &htop_this->values[1], &htop_this->values[0]);
   snprintf(buffer, size, "%.2f/%.2f/%.2f", htop_this->values[2], htop_this->values[1], htop_this->values[0]);
}

static void LoadAverageMeter_display(Object* cast, RichString* out) {
   Meter* htop_this = (Meter*)cast;
   char buffer[20];
   sprintf(buffer, "%.2f ", htop_this->values[2]);
   RichString_write(out, CRT_colors[LOAD_AVERAGE_FIFTEEN], buffer);
   sprintf(buffer, "%.2f ", htop_this->values[1]);
   RichString_append(out, CRT_colors[LOAD_AVERAGE_FIVE], buffer);
   sprintf(buffer, "%.2f ", htop_this->values[0]);
   RichString_append(out, CRT_colors[LOAD_AVERAGE_ONE], buffer);
}

static void LoadMeter_setValues(Meter* htop_this, char* buffer, int size) {
   double five, fifteen;
   LoadAverageMeter_scan(&htop_this->values[0], &five, &fifteen);
   if (htop_this->values[0] > htop_this->total) {
      htop_this->total = htop_this->values[0];
   }
   snprintf(buffer, size, "%.2f", htop_this->values[0]);
}

static void LoadMeter_display(Object* cast, RichString* out) {
   Meter* htop_this = (Meter*)cast;
   char buffer[20];
   sprintf(buffer, "%.2f ", ((Meter*)htop_this)->values[0]);
   RichString_write(out, CRT_colors[LOAD], buffer);
}

MeterClass LoadAverageMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = LoadAverageMeter_display,
   },
   .setValues = LoadAverageMeter_setValues, 
   .defaultMode = TEXT_METERMODE,
   .maxItems = 3,
   .total = 100.0,
   .attributes = LoadAverageMeter_attributes,
   .name = "LoadAverage",
   .uiName = "Load average",
   .caption = "Load average: "
};

MeterClass LoadMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = LoadMeter_display,
   },
   .setValues = LoadMeter_setValues, 
   .defaultMode = TEXT_METERMODE,
   .total = 100.0,
   .attributes = LoadMeter_attributes,
   .name = "Load",
   .uiName = "Load",
   .caption = "Load: "
};
