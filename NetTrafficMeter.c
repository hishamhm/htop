#include "NetTrafficMeter.h"
#include "Meter.h"
#include "ProcessList.h"

#include <stdio.h>
#include <stdlib.h>
#include <curses.h>
#include <string.h>
#include <math.h>

#include "debug.h"
#include <assert.h>

int NetTrafficMeter_attributes[] = {
        CPU_NORMAL, CPU_NICE
};

static void NetTrafficMeter_init(Meter* this) {
   ProcessList* pl = this->pl;
   NICData* nicData = &(pl->nics[this->param]);
   
   if (nicData->name == NULL)
   {
      Meter_setCaption(this, "NIC");
      return;
   }
   
   char buffer[4];
   memset((void*)&buffer, 0, sizeof(char)*4);
   int namelen = strlen(nicData->name);
   if (namelen > 3)
   {
      buffer[0] = nicData->name[namelen - 3];
      buffer[1] = nicData->name[namelen - 2];
      buffer[2] = nicData->name[namelen - 1];
   }
   else
   {
      snprintf(buffer, 3, "%s", nicData->name);
   }
   Meter_setCaption(this, buffer);
}

static void NetTrafficMeter_updateValues(Meter* this, char* buffer, int size) {
   ProcessList* pl = this->pl;
   NICData* nicData = &(pl->nics[this->param]);
   
   if (strncmp(this->caption, "NIC", 3) == 0)
      NetTrafficMeter_init(this);

   float gbit = 134217728.0; // 1 Gbit
   //if (pl->nic100mbps)
   //   gbit = 13107200.0; // 100 Mbit if wanted
   
   float ninp = (float)nicData->receivedPeriod / ((float)pl->delay / 10.0);
   float noutp = (float)nicData->transmittedPeriod / ((float)pl->delay / 10.0);
   
   float nin = (ninp / 1024.0 / 1024.0);
   float nout = (noutp / 1024.0 / 1024.0);
   
   this->values[0] = (ninp / gbit) * 100.0;
   this->values[1] = (noutp / gbit) * 100.0;
   snprintf(buffer, size, "R%2.2f, T%2.2fMB/s", nin, nout);
}

/*
static void NetTrafficMeter_setValues(Meter* this, char* buffer, int size) {
   ProcessList* pl = this->pl;
   NICData* nicData = &(pl->nics[this->param]);
   
   if (strncmp(this->caption, "NIC", 3) == 0)
      NetTrafficMeter_init(this);

   float gbit = 134217728.0; // 1 Gbit
   if (pl->nic100mbps)
      gbit = 13107200.0; // 100 Mbit if wanted
   
   float ninp = (float)nicData->receivedPeriod / ((float)pl->delay / 10.0);
   float noutp = (float)nicData->transmittedPeriod / ((float)pl->delay / 10.0);
   
   float nin = (ninp / 1024.0 / 1024.0);
   float nout = (noutp / 1024.0 / 1024.0);
   
   this->values[0] = (ninp / gbit) * 100.0;
   this->values[1] = (noutp / gbit) * 100.0;
   snprintf(buffer, size, "R%2.2f, T%2.2fMB/s", nin, nout);
}
*/

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

/*
MeterType NetTrafficMeter = {
   .setValues = NetTrafficMeter_setValues, 
   .mode = BAR_METERMODE,
   .items = 2,
   .total = 100.0,
   .attributes = NetTrafficMeter_attributes, 
   .name = "nettraffic",
   .uiName = "Network Traffic",
   .caption = "eth",
   .init = NetTrafficMeter_init
};
*/
