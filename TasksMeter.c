/*
htop - TasksMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "TasksMeter.h"

#include "ProcessList.h"
#include "CRT.h"

/*{
#include "Meter.h"
}*/

int TasksMeter_attributes[] = {
   TASKS_RUNNING
};

static void TasksMeter_setValues(Meter* htop_this, char* buffer, int len) {
   ProcessList* pl = htop_this->pl;
   htop_this->total = pl->totalTasks;
   htop_this->values[0] = pl->runningTasks;
   snprintf(buffer, len, "%d/%d", (int) htop_this->values[0], (int) htop_this->total);
}

static void TasksMeter_display(Object* cast, RichString* out) {
   Meter* htop_this = (Meter*)cast;
   ProcessList* pl = htop_this->pl;
   char buffer[20];
   sprintf(buffer, "%d", (int)(htop_this->total - pl->userlandThreads - pl->kernelThreads));
   RichString_write(out, CRT_colors[METER_VALUE], buffer);
   int threadValueColor = CRT_colors[METER_VALUE];
   int threadCaptionColor = CRT_colors[METER_TEXT];
   if (pl->highlightThreads) {
      threadValueColor = CRT_colors[PROCESS_THREAD_BASENAME];
      threadCaptionColor = CRT_colors[PROCESS_THREAD];
   }
   if (!pl->hideUserlandThreads) {
      RichString_append(out, CRT_colors[METER_TEXT], ", ");
      sprintf(buffer, "%d", (int)pl->userlandThreads);
      RichString_append(out, threadValueColor, buffer);
      RichString_append(out, threadCaptionColor, " thr");
   }
   if (!pl->hideKernelThreads) {
      RichString_append(out, CRT_colors[METER_TEXT], ", ");
      sprintf(buffer, "%d", (int)pl->kernelThreads);
      RichString_append(out, threadValueColor, buffer);
      RichString_append(out, threadCaptionColor, " kthr");
   }
   RichString_append(out, CRT_colors[METER_TEXT], "; ");
   sprintf(buffer, "%d", (int)htop_this->values[0]);
   RichString_append(out, CRT_colors[TASKS_RUNNING], buffer);
   RichString_append(out, CRT_colors[METER_TEXT], " running");
}

MeterClass TasksMeter_class = {
   .super = {
      .extends = Class(Meter),
      .htop_delete = Meter_delete,
      .display = TasksMeter_display,
   },
   .setValues = TasksMeter_setValues, 
   .defaultMode = TEXT_METERMODE,
   .total = 100.0,
   .attributes = TasksMeter_attributes, 
   .name = "Tasks",
   .uiName = "Task counter",
   .caption = "Tasks: "
};
