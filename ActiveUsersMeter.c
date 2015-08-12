/*
htop

Author : (c) nojhan <nojhan@nojhan.net>

Released under the GNU GPL v2 or later,
see the COPYING file in the source distribution for its full text.
*/


/**********************************************************************
 * This text meter shows the names of users with active processes.
 * Users are categorized in 3 classes: high, middle and low activity.
 * Users are displayed in descending order of activity,
 * with different colors.
 * Users shown in higher levels are not shown in lower ones, even if
 * they have active processes in those levels.
 *
 * Bounds of the level classes are given as percentages,
 * set in gather_data.
 * Data are stored in the Meter.strings array.
 **********************************************************************/


#include <string.h>
#include <stdlib.h>

#include "ActiveUsersMeter.h"
#include "RichString.h"
#include "Platform.h"
#include "CRT.h"

/* This will be included in the automagically generated header ActiveUsersMeter.h */
/*{
#include "Meter.h"

typedef enum {
   ACTIVE_LEVEL_HIGH = 0,
   ACTIVE_LEVEL_MID = 1,
   ACTIVE_LEVEL_LOW = 2
} ActiveUsers_levels;

}*/



/**********************************************************************
 * Public data structures
 **********************************************************************/


/* There is 3 (see class.maxItems) data buffers.
 * Those attributes are used in the colors management. */
int ActiveUsersMeter_attributes[] = {
    ACTIVE_USERS_HIGH,
    ACTIVE_USERS_MID,
    ACTIVE_USERS_LOW
};



/**********************************************************************
 * Private functions
 **********************************************************************/


/* Is the given user in one of the data buffers? */
static int user_in( const char* user, char** strings, int maxItems ) {
    assert(user != NULL);
    for( int i = 0; i < maxItems; ++i ) {
        /* if there is no string already existing, just skip.
         * If the user is found in a existing string, then it's a match. */
        if(strings[i] != NULL && strstr( strings[i], user ) != NULL ) {
            return true;
        }
    }
    return false;
}


/* Find users active within the given bounds
 * and that are not already shown in one of the strings
 * (because we do not want a user to be shown in all activity level,
 * only the higher one, if this function is called in descending order)
 * and print those ones in the buffer. */
static void active_users(struct ProcessList_* pl, const int min_cpu, const int max_cpu, char** strings, int maxItems, char* buffer, int len) {

    /* Save settings. */
    const int sortKey = pl->settings->sortKey;
    const int direction = pl->settings->direction;

    /* Sort by CPU%. */
    pl->settings->sortKey = PERCENT_CPU;
    pl->settings->direction = 1;
    ProcessList_sort(pl);

    /* Restore settings. */
    pl->settings->sortKey = sortKey;
    pl->settings->direction = direction;

    /* If no active process were to be found. */
    snprintf( buffer, len, "%s", "" );

    /* There is active processes. */
    if( pl->runningTasks > 0 ) {

        /* Temporary buffer */
        char* prev_users = malloc(sizeof(char)*len);

        /* Fo all ACTIVE processes */
        for( int i=0; i < pl->runningTasks; ++i ) {

            Process* p = ProcessList_get( pl, i );
            const float p_cpu = p->percent_cpu;

            if( p_cpu > max_cpu ) {
                /* Just skip higher values. */
                continue;

            } else if( p_cpu <= min_cpu ) {
                /* Process list is sorted on CPU, if we go beyond the min,
                 * there is no need to continue. */
                break;

            } else { /* min_cpu < p_cpu <= max_cpu */

                // if( strnlen(buffer, len) == len ) { // FIXME unknown function? WTF
                if( strlen(buffer) == (size_t)len ) {
                    /* Because if there is no null byte among the first n bytes of src,
                     * the string strncpy-ied will be non null-terminated. */
                    buffer[len-1] = '\0';

                    /* No need to parse more users. */
                    break;

                } else {

                    /* If the user is not already showed,
                     * either in the currently built buffer
                     * or in the other buffers. */
                    if( !user_in(p->user, strings, maxItems) && strstr( buffer, p->user) == NULL ) {
                        /* Add him/her to the list */
                        strncpy(prev_users, buffer, len);
                        snprintf( buffer, len, "%s %s", prev_users, p->user );
                    }
                } // if max length
            } // if in bounds
        } // for i in active processes

        free(prev_users);
    } // active process ?
}


/* Gather the users active in the given bounds (but not elsewhere)
 * and save them in the meter data field for the given level. */
static void activity( Meter* this, int level, const int min_cpu, const int max_cpu ) {

    MeterClass* type = (MeterClass*) Class(ActiveUsersMeter);

    assert(type->maxItems > level);
    free( this->strings[level] );
    char* users = calloc( METER_BUFFER_LEN, sizeof(char) );
    assert( users != NULL );
    active_users( this->pl, min_cpu, max_cpu, this->strings, type->maxItems, users, METER_BUFFER_LEN);
    this->strings[level] = users;
}


/* Check the size of the string for the given activity level
 * and write it in the colored string at the given position.
 * Trim the user string if necessary.
 * Return current_pos + length of the written string. */
static int add_users( Meter* this, int level, int current_pos, RichString* out ) {

   char* users = this->strings[level];
   size_t len = strlen( users ); // FIXME use strnlen
   /* trim string if necessary */
   if( current_pos+len >= RICHSTRING_MAXLEN ) {
       users[current_pos+len-1] = '\0';
       len = strlen(users); // FIXME use strnlen
       assert(current_pos+len == RICHSTRING_MAXLEN);
   }
   RichString_writeFrom(out, CRT_colors[ActiveUsersMeter_attributes[level]], users, current_pos, len);
   return current_pos+len;
}



/**********************************************************************
 * Interfaces
 **********************************************************************/


/* Build the 3 active users strings and store them as data. */
static void gather_data(Meter* this, char* buffer, int len) {
    /* NOTE: buffer/len are not used because data are saved in Meter.strings. */

    activity( this, ACTIVE_LEVEL_HIGH, 95, 100 );
    activity( this, ACTIVE_LEVEL_MID , 50,  95 );
    activity( this, ACTIVE_LEVEL_LOW ,  0,  50 );
    /* If you change the levels bounds,
     * do not forget to update the description beyond. */
}


/* Readability macro for the display function.
 * Call add_users and immediately return if the output is full. */
#define ACTIVE_USERS_ADD(level) add_users( this, level, len, out ); if( len == RICHSTRING_MAXLEN ) { return; }

/* Concatenate active users data and write them as a colored string. */
static void display(Object* cast, RichString* out) {
   Meter* this = (Meter*)cast;
   int len = 0;

   len = ACTIVE_USERS_ADD( ACTIVE_LEVEL_HIGH );
   len = ACTIVE_USERS_ADD( ACTIVE_LEVEL_MID  );
   len = ACTIVE_USERS_ADD( ACTIVE_LEVEL_LOW  );

   /* All strings should have been displayed. */
   assert( len == strlen(this->strings[ACTIVE_LEVEL_HIGH]) /* FIXME use strnlen */
                 +strlen(this->strings[ACTIVE_LEVEL_MID ])
                 +strlen(this->strings[ACTIVE_LEVEL_LOW ]) );
}


/* Note: we use .strings to store data, not .values
 *       but both have .maxItems size. */
MeterClass ActiveUsersMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = display,
   },
   .setValues = gather_data,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 3,
   .total = 100.0,
   .attributes = ActiveUsersMeter_attributes,
   .name = "Users",
   .uiName = "Active users",
   .description = "Active users: >95%, 95-50%, <50%",
   .caption = "Users:"
};
