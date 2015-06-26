/*
htop

Author : (c) nojhan <nojhan@nojhan.net>

Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include <string.h>

#include "ActiveUsersMeter.h"
#include "Platform.h"
#include "CRT.h"

/*{
#include "Meter.h"
}*/

int ActiveUsersMeter_attributes[] = {
    METER_VALUE
};

static void ActiveUsersMeter_setValues(Meter* this, char* buffer, int len) {

    /* Save settings. */
    int sortKey = this->pl->settings->sortKey;
    int direction = this->pl->settings->direction;

    /* Sort by CPU%. */
    this->pl->settings->sortKey = PERCENT_CPU;
    this->pl->settings->direction = 1;
    ProcessList_sort(this->pl);

    /* Restore settings. */
    this->pl->settings->sortKey = sortKey;
    this->pl->settings->direction = direction;

    /* If no active process were to be found. */
    snprintf( buffer, len, "" );

    /* There is active processes. */
    if( this->pl->runningTasks > 0 ) {

        /* Temporary buffer */
        char* prev_users = malloc(sizeof(char)*len);

        /* Fo all ACTIVE processes */
        for( int i=0; i < this->pl->runningTasks; ++i ) {

            if( strnlen(buffer, len) == len ) {
                /* Because if there is no null byte among the first n bytes of src,
                 * the string strncpy-ied will be non null-terminated. */
                buffer[len-1] = '\0';

                /* No need to parse more users. */
                break;

            } else {
                /* Get the i^th process CPU load. */
                Process* p = ProcessList_get( this->pl, i );

                /* If the user is not already showed. */
                if( strstr( buffer, p->user) == NULL) {
                    /* Add him/her to the list */
                    strncpy(prev_users, buffer, len);
                    snprintf( buffer, len, "%s %s", prev_users, p->user );
                }
            } // if max length
        } // for i in active processes

        free(prev_users);
    } // active process ?
}

MeterClass ActiveUsersMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .setValues = ActiveUsersMeter_setValues,
   .defaultMode = TEXT_METERMODE,
   .total = 100.0,
   .attributes = ActiveUsersMeter_attributes,
   .name = "Users",
   .uiName = "Active users",
   .caption = "Active users:"
};
