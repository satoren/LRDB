# Lua Remote DeBugger for Visual Studio Code

## Introduction

This extension is debug Lua programs with Visual Studio Code.

![Lua Debug](https://raw.githubusercontent.com/satoren/LRDB/master/vscode_extension/images/lrdb.gif)

## Features

* Supports Windows,macOS,Linux
* Add/remove break points
* Conditional break points
* Continue,Pause,Step over, Step in, Step out
* Local,Global,_ENV,Upvalue variables and arguments
* Watch window
* Evaluate Expressions
* Debug with embedded Lua interpreter(Lua 5.3.3 on Javascript by Emscripten)
* Debug with Your host program([require embed debug server](https://github.com/satoren/LRDB))
* Remote debugging over TCP



## Extension Settings

launch.json example:
```
{
    "version": "0.2.0",
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

## Release Notes

### 0.2.1
- Bug fixes.
### 0.2.0
- Change LuaVM native code to javascript by Emscripten
### 0.1.9
- Bug fixes.
- Added support sourceRequest. It mean can step execute on string chunk.
### 0.1.8
- Bug fixes.
- Update Lua 5.3.3 to 5.3.4
