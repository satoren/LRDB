package = "LRDB"
version = "0.1.7-1"
source = {
   url = "git+https://github.com/satoren/LRDB",
   tag = "0.1.6"
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
    "lua >= 5.1, < 5.4"
}
build = {
   type = "cmake",
   cmake = 'cmake_minimum_required(VERSION 2.8)\n include(luarocks_cmake.txt)',
   variables ={LUA_LIBRARY_DIRS="$(LUA_LIBDIR)",LUA_INCLUDE_DIRS="$(LUA_INCDIR)",LUA_LIBRARIES="$(LUALIB)",INSTALL_LIBDIR="$(LIBDIR)"}
}