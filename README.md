# Lua Remote DeBugger

Licensed under [Boost Software License](http://www.boost.org/LICENSE_1_0.txt)

[![Build Status](https://travis-ci.org/satoren/LRDB.svg?branch=master)](https://travis-ci.org/satoren/LRDB)
[![Coverage Status](https://coveralls.io/repos/github/satoren/LRDB/badge.svg?branch=master)](https://coveralls.io/github/satoren/LRDB?branch=master)

## Fork changes

This fork is modified to use absolute paths instead of relative in communication with remote debugger.

This fork works only with similarly modified vscode-lrdb extension - [kapecp/vscode-lrdb](https://github.com/kapecp/vscode-lrdb).

## Introduction

LRDB is Debugger for Lua programing language.


Currentry debug client is [Visual Studio Code extension](https://marketplace.visualstudio.com/items?itemName=satoren.lrdb) only.

Command line interface debugger is not implemented.


## Features

* Breakpoints with conditional and hit counts.
* Step over, step in, step out
* Display Local,Upvalue,Global values
* Watches,Eval on Debug Console
* Remote debugging over TCP network


## Requirements
  * Lua 5.1 to 5.3 (recommended: 5.3)
  * C++11 compiler

## Embedded to your host program

LRDB is header only library

### include path
  - LRDB/include
  - LRDB/third_party/asio/asio/include or boost.asio with -DLRDB_USE_BOOST_ASIO
  - LRDB/third_party/picojson

### code
```C++
#include "lrdb/server.hpp"
...

  int listen_port = 21110;//listen tcp port for debugger interface

  lua_State* L = luaL_newstate();//create lua state

  lrdb::server debug_server(listen_port);
  debug_server.reset(L);//assign debug server to lua state(Required before script load)

  bool ret = luaL_dofile(L, luafilepath);

  debug_server.reset(); //unassign debug server (Required before lua_close )
  lua_close(L);
```

## Lua module
If you using standalone Lua. you can use lua c mocule.

### Build and Install with [LuaRocks](https://luarocks.org/)
```
luarocks install lrdb
```

### Build module your self
```
mkdir build
cd build
cmake ../ -DLUA=lua-5.3
cmake --build --target lrdb_server
```
Generated lrdb_server.so or lrdb_server.dll


### Use module
```lua
lrdb = require("lrdb_server")
lrdb.activate(21110) --21110 is using port number. waiting for connection by debug client.

--debuggee lua code
dofile("luascript.lua");

lrdb.deactivate() --deactivate debug server if you want.
```

## Visual Studio Code Extension
https://github.com/satoren/vscode-lrdb