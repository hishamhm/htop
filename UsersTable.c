/*
htop - UsersTable.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "UsersTable.h"

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <pwd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <assert.h>

/*{
#include "Hashtable.h"

typedef struct UsersTable_ {
   Hashtable* users;
} UsersTable;
}*/

UsersTable* UsersTable_new() {
   UsersTable* htop_this;
   htop_this = malloc(sizeof(UsersTable));
   htop_this->users = Hashtable_new(20, true);
   return htop_this;
}

void UsersTable_delete(UsersTable* htop_this) {
   Hashtable_delete(htop_this->users);
   free(htop_this);
}

char* UsersTable_getRef(UsersTable* htop_this, unsigned int uid) {
   char* name = (char*) (Hashtable_get(htop_this->users, uid));
   if (name == NULL) {
      struct passwd* userData = getpwuid(uid);
      if (userData != NULL) {
         name = strdup(userData->pw_name);
         Hashtable_put(htop_this->users, uid, name);
      }
   }
   return name;
}

inline void UsersTable_foreach(UsersTable* htop_this, Hashtable_PairFunction f, void* userData) {
   Hashtable_foreach(htop_this->users, f, userData);
}
