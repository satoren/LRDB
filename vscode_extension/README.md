# Lua Remote DeBugger for Visual Studio Code

## Introduction

This extension is debug Lua programs with Visual Studio Code.

![Lua Debug](https://raw.githubusercontent.com/satoren/LRDB/master/vscode_extension/images/lrdb.gif)

## Features

Debug your Lua program.

* Breakpoints
* Step over, step in, step out
* Display Local,Upvalue,Global values
* Watches,Eval on Debug Console
* Debug with embedded Lua interpreter(Lua 5.3.3)
* Remote debugging over TCP network


If you want embedded debug server to your host program, Please see [this page](https://github.com/satoren/LRDB)

## Extension Settings

launch.json example:
```
{
    "version": "0.1.0",
    "configurations": [
        {
            "type": "lua",
            "request": "launch",
            "name": "Lua Launch",
            "program": "${file}",
            "cwd": "${workspaceRoot}",
            "stopOnEntry": true
        }
    ]
}
```


## Known Issues


## Release Notes

### 0.1.6
- ``null`` to ``nil`` at watch and variable view

### 0.1.4
- Remove ${command.CurrentSource}. It is same to ${file}
- Support operators in hit count condition breakpoint ``<``, ``<=``, ``==``, ``>``, ``>=``, ``%``


### 0.1.3
Bug fixed

### 0.1.0
Initial release.
