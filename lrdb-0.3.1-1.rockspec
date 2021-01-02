package = "LRDB"
version = "0.3.1-1"
source = {
   url = "git+https://github.com/satoren/LRDB",
   tag = "v0.3.1"
}
description = {
    summary = "Remote Debugger for Lua",
    detailed = [[
        Remote debugger for Lua with Visual Studio Code extension.
        https://marketplace.visualstudio.com/items?itemName=satoren.lrdb
   ]],
   homepage = "https://github.com/satoren/LRDB",
   license = "Boost"
}
dependencies = {
    "lua >= 5.1"
}

build = {
   type = "cmake",
   cmake = 'cmake_minimum_required(VERSION 2.8)\n include(luarocks_cmake.txt)',
   variables ={CFLAGS="$(CFLAGS)",
   LIBFLAG="$(LIBFLAG)",
   LUA="$(LUA)",
   LUALIB="$(LUALIB)",
   LUA_BINDIR="$(LUA_BINDIR)",
   LUADIR="$(LUADIR)",
   LUA_LIBDIR="$(LUA_LIBDIR)",
   LIBDIR="$(LIBDIR)",
   LUA_INCDIR="$(LUA_INCDIR)",
   CMAKE_INSTALL_PREFIX="$(PREFIX)"}
}