/*
htop - LoadAverageMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "LoadAverageMeter.h"

#include "CRT.h"
#include "Platform.h"

#include <stdlib.h>

/*{
#include "Meter.h"
}*/

int LoadAverageMeter_attributes[] = {
   LOAD_AVERAGE_FIFTEEN, LOAD_AVERAGE_FIVE, LOAD_AVERAGE_ONE
};

int LoadMeter_attributes[] = { LOAD };

static void LoadAverageMeter_setValues(Meter* this, char* buffer, int size) {
   Platform_getLoadAverage(&this->values[2], &this->values[1], &this->values[0]);
   snprintf(buffer, size, "%.2f/%.2f/%.2f", this->values[2], this->values[1], this->values[0]);
}

static void LoadAverage3Meter_setValues(Meter* this, char* buffer, int size) {
   double tmp;
   switch(this->param){
      case 2:
        Platform_getLoadAverage(&tmp,&tmp,&this->values[0]);
        break;
      case 1:
        Platform_getLoadAverage(&tmp,&this->values[1],&tmp);
        break;
      case 0:
        Platform_getLoadAverage(&this->values[2],&tmp,&tmp);
        break;
   }
   snprintf(buffer, size, "%.2f", this->values[2-this->param]);
}

static void LoadAverageMeter_init(Meter* this) {
   int cpus = this->pl->cpuCount;
// Load agerage should be lower than #CPUs 3 <= 1,5,15mins
   this->total = 3*cpus;
}

static void LoadAverage3Meter_init(Meter* this) {
   if (!this->drawData)
      this->drawData = calloc(3, sizeof(Meter*));
   Meter** meters = (Meter**) this->drawData;
   for (int i = 0; i < 3; i++) {
      if (!meters[i])
         meters[i] = Meter_new(this->pl, i, (MeterClass*) Class(LoadAverage1Meter));
      Meter_init(meters[i]);
      if(i>0) Meter_setCaption(meters[i],"");
   }
}

static void LoadAverage1Meter_init(Meter* this) {
   int cpus = this->pl->cpuCount;
   this->total = cpus;
}

static void LoadAverage3Meter_draw(Meter* this, int x, int y, int w) {
   Meter** meters = (Meter**) this->drawData;
// 4 = captionLen + 1, form Metter.c
   int wid = (w-4)/3;
   int corr = (w-4)%3;
   meters[0]->draw(meters[0], x, y,wid+4);
   meters[1]->draw(meters[1], x+1*wid, y,wid+4+(corr==2?1:0));
   meters[2]->draw(meters[2], x+2*wid+(corr==2?1:0), y,wid+4+(corr>=1?1:0));
}

static void LoadAverageMeter_display(Object* cast, RichString* out) {
   Meter* this = (Meter*)cast;
   char buffer[20];
   sprintf(buffer, "%.2f ", this->values[2]);
   RichString_write(out, CRT_colors[LOAD_AVERAGE_FIFTEEN], buffer);
   sprintf(buffer, "%.2f ", this->values[1]);
   RichString_append(out, CRT_colors[LOAD_AVERAGE_FIVE], buffer);
   sprintf(buffer, "%.2f ", this->values[0]);
   RichString_append(out, CRT_colors[LOAD_AVERAGE_ONE], buffer);
}

static void LoadMeter_init(Meter* this) {
   int cpus = this->pl->cpuCount;
// Load agerage should be lower than #CPUs
   this->total = cpus;
}

static void LoadMeter_setValues(Meter* this, char* buffer, int size) {
   double five, fifteen;
   Platform_getLoadAverage(&this->values[0], &five, &fifteen);
// no rescale
//   if (this->values[0] > this->total) {
//      this->total = this->values[0];
//   }
   snprintf(buffer, size, "%.2f", this->values[0]);
}

static void LoadMeter_display(Object* cast, RichString* out) {
   Meter* this = (Meter*)cast;
   char buffer[20];
   sprintf(buffer, "%.2f ", ((Meter*)this)->values[0]);
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
   .total = 3.0,
   .attributes = LoadAverageMeter_attributes,
   .name = "LoadAverage",
   .uiName = "Load average",
   .description = "Load averages: 15 minutes, 5 minutes, 1 minute",
   .caption = "Load average: ",
   .init = LoadAverageMeter_init
};

MeterClass LoadAverage3Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = LoadAverageMeter_display,
   },
   .defaultMode = CUSTOM_METERMODE,
   .maxItems = 3,
   .total = 3.0,
   .attributes = LoadAverageMeter_attributes,
   .name = "LoadAverage3",
   .uiName = "Load average 3c",
   .description = "Load averages (3 col): 15 minutes, 5 minutes, 1 minute",
   .caption = "Load average: ",
   .draw = LoadAverage3Meter_draw,
   .init = LoadAverage3Meter_init
};

MeterClass LoadAverage1Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = LoadAverageMeter_display,
   },
   .setValues = LoadAverage3Meter_setValues, 
   .defaultMode = BAR_METERMODE,
   .maxItems = 3,
   .total = 1.0,
   .attributes = LoadAverageMeter_attributes,
   .name = "LoadAverage",
   .uiName = "Load average",
   .description = "Load averages: 15 minutes, 5 minutes, 1 minute",
   .caption = "Load average: ",
   .init = LoadAverage1Meter_init
};

MeterClass LoadMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = LoadMeter_display,
   },
   .setValues = LoadMeter_setValues, 
   .defaultMode = TEXT_METERMODE,
   .total = 1.0,
   .attributes = LoadMeter_attributes,
   .name = "Load",
   .uiName = "Load",
   .description = "Load: average of ready processes in the last minute",
   .caption = "Load: ",
   .init = LoadMeter_init
};
