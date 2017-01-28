#ifdef EMSCRIPTEN
#include <emscripten.h>
#endif
#include <iostream>

#include "lrdb/server.hpp"

typedef std::unique_ptr<lrdb::server> server_ptr;

int lrdb_activate(lua_State* L)
{
	server_ptr* server = (server_ptr*)lua_touserdata(L, lua_upvalueindex(1));
	if (lua_isnumber(L, 1))
	{
		server->reset(new lrdb::server((int16_t)lua_tonumber(L, 1)));
		(*server)->reset(L);
	}
	return 0;
}
int lrdb_deactivate(lua_State* L)
{
	server_ptr* server = (server_ptr*)lua_touserdata(L, lua_upvalueindex(1));
	server->reset();
	return 0;
}
int lrdb_destruct(lua_State* L)
{
	server_ptr* server = (server_ptr*)lua_touserdata(L, lua_upvalueindex(1));
	server->~server_ptr();
	return 0;
}

#if defined(_WIN32) || defined(_WIN64)
extern "C" __declspec(dllexport)
#else
extern "C" __attribute__((visibility("default")))
#endif
int luaopen_lrdb_server(lua_State* L)
{
	luaL_dostring(L, "debug=nil");
	lua_createtable(L,0,3);
	int smeta = lua_gettop(L);
	void* server = lua_newuserdata(L, sizeof(server_ptr));
	new(server) server_ptr();

	int sserver = lua_gettop(L);
	lua_pushvalue(L, sserver);
	lua_pushcclosure(L,&lrdb_activate,1);
	lua_setfield(L, smeta,"activate");
	lua_pushvalue(L, sserver);
	lua_pushcclosure(L, &lrdb_deactivate, 1);
	lua_setfield(L, smeta, "deactivate");
	lua_pushvalue(L, sserver);
	lua_pushcclosure(L, &lrdb_deactivate, 1);
	lua_setfield(L, smeta, "__gc");

	lua_pushvalue(L, smeta);
	return 1;	
}