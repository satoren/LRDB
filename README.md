# Lua Remote DeBugger

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

## Embedded to your host program

### Requirements
  * Lua 5.1 to 5.3 (recommended: 5.3)
  * C++11 compiler
  
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
### Build Lua module
```
mkdir build
cd build
cmake ../ -DLUA=lua-5.3
cmake --build --target lrdb_server
```
Generated lrdb_server.so or lrdb_server.dll
### Use Lua module
```lua
lrdb = require("lrdb_server")
lrdb.activate(21110) --21110 is using port number. waiting for connection by debug client.

--debuggee lua code
dofile("luascript.lua");

lrdb.deactivate() --deactivate debug server if you want.
```

## Visual Studio Code Extension setting
search ``lrdb`` and install at Visual Studio Code

launch.json example:
```json
{
    "version": "0.1.0",
    "configurations": [
        {
            "type": "lua",
            "request": "attach",
            "name": "Lua Attach",
            "host": "localhost",
            "port": 21110,
            "sourceRoot": "${workspaceRoot}",
            "stopOnEntry": true
        }
    ]
}
```

If you want Lua and C++ debug in VSCode 

launch.json example:
```json
{
    "version": "0.1.0",
    "configurations": [
        {
            "type": "lua",
            "request": "attach",
            "name": "Lua Attach",
            "host": "localhost",
            "port": 21110,
            "sourceRoot": "${workspaceRoot}"
        },
        {
            "name": "C++ Launch",
            "type": "cppdbg",
            "request": "launch",
            "program": "Your Lua host program",
            "args": []
            "cwd": "${workspaceRoot}"
            .....
        }
    ],
	"compounds": [
		{
			"name": "Lua + C++",
			"configurations": ["C++ Launch" , "Lua Attach"]
		}
	]
}
```
