/*
htop - BatteryMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.

This meter written by Ian P. Hands (iphands@gmail.com, ihands@redhat.com).
*/

#include "BatteryMeter.h"

#include "Battery.h"
#include "ProcessList.h"
#include "CRT.h"
#include "StringUtils.h"
#include "Platform.h"

#include <string.h>
#include <stdlib.h>

/*{
#include "Meter.h"

typedef enum ACPresence_ {
   AC_ABSENT,
   AC_PRESENT,
   AC_ERROR
} ACPresence;
}*/

int BatteryMeter_attributes[] = {
   BATTERY
};

static void BatteryMeter_updateValues(Meter * this, char *buffer, int len) {
   ACPresence isOnAC;
   double percent;
   const char *unknownText = "%.1f%%",
      *onAcText = "%.1f%%(A/C)",
      *onBatteryText = "%.1f%%(bat)";

   Battery_getData(&percent, &isOnAC);

   if (percent == -1) {
      this->values[0] = 0;
      xSnprintf(buffer, len, "n/a");
      return;
   }

   this->values[0] = percent;
   this->values[1] = isOnAC;

   if (isOnAC == AC_PRESENT) {
      xSnprintf(buffer, len, onAcText, percent);
   } else if (isOnAC == AC_ABSENT) {
      xSnprintf(buffer, len, onBatteryText, percent);
   } else {
      xSnprintf(buffer, len, unknownText, percent);
   }

   return;
}

static void BatteryMeter_display(Object* cast, RichString* out) {
   ACPresence isOnAC;
   double percent;
   char buffer[28];
   size_t len = sizeof(buffer);
   const char *unknownText = "%.1f%%",
      *onAcText = "%.1f%% (Running on A/C)",
      *onBatteryText = "%.1f%% (Running on battery)";
   int color = CRT_colors[BATTERY];

   Meter* this = (Meter*)cast;

   percent = this->values[0];
   isOnAC = this->values[1];

   RichString_prune(out);

   if (isOnAC == AC_PRESENT) {
      xSnprintf(buffer, len, onAcText, percent);
   } else if (isOnAC == AC_ABSENT) {
      if (percent <= 5) {
         color = CRT_colors[BATTERY_CRIT];
      } else if (percent <= 10 ) {
         color = CRT_colors[BATTERY_WARN];
      }
      xSnprintf(buffer, len, onBatteryText, percent);
   } else {
      xSnprintf(buffer, len, unknownText, percent);
   }

   RichString_append(out, color, buffer);
}

MeterClass BatteryMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = BatteryMeter_display
   },
   .updateValues = BatteryMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 1,
   .total = 100.0,
   .attributes = BatteryMeter_attributes,
   .name = "Battery",
   .uiName = "Battery",
   .caption = "Battery: "
};
