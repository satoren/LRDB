# Lua Remote DeBugger

## Introduction
Lua Remote Debugger 


Currentry client is VSCode extension only.
command line interface debugger is not implemented.


## Features

* Breakpoints
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
  - LRDB/third_party/asio/asio/include
  - LRDB/third_party/picojson

### code
```C++
  int listen_port = 21110;//listen tcp port for debugger interface

  lua_State* L = luaL_newstate();//create lua state

  lrdb::server debug_server(listen_port);
  debug_server.reset(L);//assign debug server to lua state(Required before script load)

  bool ret = luaL_dofile(L, luafilepath);

  debug_server.reset(); //unassign debug server (Required before lua_close )
  lua_close(L);
```


## Visual Studio Code Extension
search ``lrdb`` and install at Visual Studio Code

launch.json example:
```
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
Exaple
```
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
            "program": "Your C++ with Lua host program",
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
