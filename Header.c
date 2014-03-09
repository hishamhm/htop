/*
htop - Header.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Header.h"

#include "CRT.h"
#include "CPUMeter.h"
#include "MemoryMeter.h"
#include "SwapMeter.h"
#include "TasksMeter.h"
#include "LoadAverageMeter.h"
#include "UptimeMeter.h"
#include "BatteryMeter.h"
#include "ClockMeter.h"
#include "HostnameMeter.h"
#include "String.h"

#include <assert.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

/*{
#include "ProcessList.h"
#include "Meter.h"

typedef enum HeaderSide_ {
   LEFT_HEADER,
   RIGHT_HEADER
} HeaderSide;

typedef struct Header_ {
   Vector* leftMeters;
   Vector* rightMeters;
   ProcessList* pl;
   bool margin;
   int height;
   int pad;
} Header;

}*/

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

Header* Header_new(ProcessList* pl) {
   Header* htop_this = calloc(1, sizeof(Header));
   htop_this->leftMeters = Vector_new(Class(Meter), true, DEFAULT_SIZE);
   htop_this->rightMeters = Vector_new(Class(Meter), true, DEFAULT_SIZE);
   htop_this->margin = true;
   htop_this->pl = pl;
   return htop_this;
}

void Header_delete(Header* htop_this) {
   Vector_delete(htop_this->leftMeters);
   Vector_delete(htop_this->rightMeters);
   free(htop_this);
}

void Header_createMeter(Header* htop_this, char* name, HeaderSide side) {
   Vector* meters = side == LEFT_HEADER
                       ? htop_this->leftMeters
                       : htop_this->rightMeters;

   char* paren = strchr(name, '(');
   int param = 0;
   if (paren) {
      int ok = sscanf(paren, "(%d)", &param);
      if (!ok) param = 0;
      *paren = '\0';
   }
   for (MeterClass** type = Meter_types; *type; type++) {
      if (String_eq(name, (*type)->name)) {
         Vector_add(meters, Meter_new(htop_this->pl, param, *type));
         break;
      }
   }
}

void Header_setMode(Header* htop_this, int i, MeterModeId mode, HeaderSide side) {
   Vector* meters = side == LEFT_HEADER
                       ? htop_this->leftMeters
                       : htop_this->rightMeters;

   if (i >= Vector_size(meters))
      return;
   Meter* meter = (Meter*) Vector_get(meters, i);
   Meter_setMode(meter, mode);
}

Meter* Header_addMeter(Header* htop_this, MeterClass* type, int param, HeaderSide side) {
   Vector* meters = side == LEFT_HEADER
                       ? htop_this->leftMeters
                       : htop_this->rightMeters;

   Meter* meter = Meter_new(htop_this->pl, param, type);
   Vector_add(meters, meter);
   return meter;
}

int Header_size(Header* htop_this, HeaderSide side) {
   Vector* meters = side == LEFT_HEADER
                       ? htop_this->leftMeters
                       : htop_this->rightMeters;

   return Vector_size(meters);
}

char* Header_readMeterName(Header* htop_this, int i, HeaderSide side) {
   Vector* meters = side == LEFT_HEADER
                       ? htop_this->leftMeters
                       : htop_this->rightMeters;
   Meter* meter = (Meter*) Vector_get(meters, i);

   int nameLen = strlen(Meter_name(meter));
   int len = nameLen + 100;
   char* name = malloc(len);
   strncpy(name, Meter_name(meter), nameLen);
   name[nameLen] = '\0';
   if (meter->param)
      snprintf(name + nameLen, len - nameLen, "(%d)", meter->param);

   return name;
}

MeterModeId Header_readMeterMode(Header* htop_this, int i, HeaderSide side) {
   Vector* meters = side == LEFT_HEADER
                       ? htop_this->leftMeters
                       : htop_this->rightMeters;

   Meter* meter = (Meter*) Vector_get(meters, i);
   return meter->mode;
}

void Header_defaultMeters(Header* htop_this, int cpuCount) {
   if (cpuCount > 8) {
      Vector_add(htop_this->leftMeters, Meter_new(htop_this->pl, 0, (MeterClass*) Class(LeftCPUs2Meter)));
      Vector_add(htop_this->rightMeters, Meter_new(htop_this->pl, 0, (MeterClass*) Class(RightCPUs2Meter)));
   } else if (cpuCount > 4) {
      Vector_add(htop_this->leftMeters, Meter_new(htop_this->pl, 0, (MeterClass*) Class(LeftCPUsMeter)));
      Vector_add(htop_this->rightMeters, Meter_new(htop_this->pl, 0, (MeterClass*) Class(RightCPUsMeter)));
   } else {
      Vector_add(htop_this->leftMeters, Meter_new(htop_this->pl, 0, (MeterClass*) Class(AllCPUsMeter)));
   }
   Vector_add(htop_this->leftMeters, Meter_new(htop_this->pl, 0, (MeterClass*) Class(MemoryMeter)));
   Vector_add(htop_this->leftMeters, Meter_new(htop_this->pl, 0, (MeterClass*) Class(SwapMeter)));
   Vector_add(htop_this->rightMeters, Meter_new(htop_this->pl, 0, (MeterClass*) Class(TasksMeter)));
   Vector_add(htop_this->rightMeters, Meter_new(htop_this->pl, 0, (MeterClass*) Class(LoadAverageMeter)));
   Vector_add(htop_this->rightMeters, Meter_new(htop_this->pl, 0, (MeterClass*) Class(UptimeMeter)));
}

void Header_reinit(Header* htop_this) {
   for (int i = 0; i < Vector_size(htop_this->leftMeters); i++) {
      Meter* meter = (Meter*) Vector_get(htop_this->leftMeters, i);
      if (Meter_initFn(meter))
         Meter_init(meter);
   }
   for (int i = 0; i < Vector_size(htop_this->rightMeters); i++) {
      Meter* meter = (Meter*) Vector_get(htop_this->rightMeters, i);
      if (Meter_initFn(meter))
         Meter_init(meter);
   }
}

void Header_draw(const Header* htop_this) {
   int height = htop_this->height;
   int pad = htop_this->pad;
   attrset(CRT_colors[RESET_COLOR]);
   for (int y = 0; y < height; y++) {
      mvhline(y, 0, ' ', COLS);
   }
   for (int y = (pad / 2), i = 0; i < Vector_size(htop_this->leftMeters); i++) {
      Meter* meter = (Meter*) Vector_get(htop_this->leftMeters, i);
      meter->draw(meter, pad, y, COLS / 2 - (pad * 2 - 1) - 1);
      y += meter->h;
   }
   for (int y = (pad / 2), i = 0; i < Vector_size(htop_this->rightMeters); i++) {
      Meter* meter = (Meter*) Vector_get(htop_this->rightMeters, i);
      meter->draw(meter, COLS / 2 + pad, y, COLS / 2 - (pad * 2 - 1) - 1);
      y += meter->h;
   }
}

int Header_calculateHeight(Header* htop_this) {
   int pad = htop_this->margin ? 2 : 0;
   int leftHeight = pad;
   int rightHeight = pad;

   for (int i = 0; i < Vector_size(htop_this->leftMeters); i++) {
      Meter* meter = (Meter*) Vector_get(htop_this->leftMeters, i);
      leftHeight += meter->h;
   }
   for (int i = 0; i < Vector_size(htop_this->rightMeters); i++) {
      Meter* meter = (Meter*) Vector_get(htop_this->rightMeters, i);
      rightHeight += meter->h;
   }
   htop_this->pad = pad;
   htop_this->height = MAX(leftHeight, rightHeight);
   return htop_this->height;
}
