# Change Log


## [Unreleased]
### Fixed
- Did not stop breakpoints on remote debugging with different system. (windows and **nix)
- Array is 1 origin in Lua...

## [0.2.0] - 2017-02-04
### Changed
- Improve variables viewing
- Change LuaVM native code to javascript by Emscripten.
## [0.1.9] - 2017-02-03
### Changed
- Added support sourceRequest. It mean can step execute on string chunk.

## [0.1.8] - 2017-02-01
### Changed
- Update Lua 5.3.3 to 5.3.4
### Fixed
- Fix stepOver and pause did not work.

## [0.1.7] - 2017-01-27
### Fixed
- `rogram arg is not available at Lua
- Improve launch.json snipped

## [0.1.6] - 2017-01-27
### Changed
- ``null`` to ``nil`` at watch and variable view

## [0.1.4] - 2017-01-26
### Changed
- Remove ${command.CurrentSource}. It is same to ${file}
- Support operators in hit count condition breakpoint ``<``, ``<=``, ``==``, ``>``, ``>=``, ``%``

## [0.1.3] - 2017-01-24
### Fixed
- Hit conditional breakpoint not working
### Changed
- Change pointer output format. e.g. ``function(0x41b1f0)`` to ``function: 0x41b1f0``

## [0.1.1] - 2017-01-24
### Fixed
- Can not set break last line
- No output stdout at debug console

## [0.1.0] - 2017-01-24
- Initial release