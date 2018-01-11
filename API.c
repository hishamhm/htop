
#include "config.h"
#include "API.h"
#include "Process.h"

#ifdef HAVE_LUA

/*{

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define check_field(_name, _type, _err)    \
   do {                                    \
      int typ = lua_getfield(L, 1, _name); \
      if (typ != _type) {                  \
         lua_settop(L, 0);                 \
         lua_pushnil(L);                   \
         lua_pushliteral(L, _err);         \
         return 2;                         \
      }                                    \
      lua_pop(L, 1);                       \
   } while(0)

#define assign_string(_var, _idx, _field)  \
   do {                                    \
      lua_getfield(L, _idx, _field);       \
      _var = xStrdup(lua_tostring(L, -1)); \
      lua_pop(L, 1);                       \
   } while(0)

}*/

int API_newColumn(lua_State* L) {
   luaL_checktype(L, 1, LUA_TTABLE);

   check_field("name",        LUA_TSTRING,   "expected 'name' field of type string");
   check_field("description", LUA_TSTRING,   "expected 'description' field of type string");
   check_field("header",      LUA_TSTRING,   "expected 'header' field of type string");
   check_field("format",      LUA_TSTRING,   "expected 'format' field of type string");
   check_field("run",         LUA_TFUNCTION, "expected 'run' field of type function");
   
   lua_getfield(L, LUA_REGISTRYINDEX, "htop_columns");
   lua_len(L, -1);
   int len = lua_tonumber(L, -1);
   lua_pop(L, 1);
   ProcessFieldData* pfd = lua_newuserdata(L, sizeof(ProcessFieldData));

   assign_string(pfd->name,        1, "name");
   assign_string(pfd->description, 1, "description");
   assign_string(pfd->title,       1, "header");
   assign_string(pfd->format,      1, "format");

   lua_seti(L, -2, len + 1);
   lua_settop(L, 0);
   lua_pushboolean(L, 1);
   return 1;
}

static luaL_Reg API_functions[] = {
   { .name = "new_column", .func = API_newColumn },
   { .name = NULL, .func = NULL },
};

int API_new(lua_State* L) {
   lua_settop(L, 0);

   lua_newtable(L);
   lua_setfield(L, LUA_REGISTRYINDEX, "htop_columns");

   lua_newtable(L);
   luaL_setfuncs(L, API_functions, 0);
   return 1;   
}

#endif
