#include "NetTrafficMeter.h"
#include "Meter.h"
#include "ProcessList.h"
#include "CRT.h"
#include "Platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <curses.h>
#include <string.h>
#include <math.h>

#include <assert.h>

/*{
#include "Meter.h"
}*/

int NetTrafficMeter_attributes[] = {
   CPU_NORMAL, CPU_NICE
};

static void NetTrafficMeter_init(Meter* this) {
   Platform_setNICInit(this);
}

static void NetTrafficMeter_updateValues(Meter* this, char* buffer, int size) {
   
   if (strncmp(this->caption, "NIC", 3) == 0)
      NetTrafficMeter_init(this);

   double* nd = Platform_setNICValues(this);
   
   snprintf(buffer, size, "R%2.2f, T%2.2fMB/s", nd[0], nd[1]);
}

MeterClass NetTrafficMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .updateValues = NetTrafficMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .maxItems = 2,
   .total = 100.0,
   .attributes = NetTrafficMeter_attributes,
   .name = "nettraffic",
   .uiName = "Network Traffic",
   .caption = "Network Traffic: "
};
