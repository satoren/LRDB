# Change Log


## [Unreleased]

## [0.1.8] - 2017-02-01
### Changed
- update Lua 5.3.3 to 5.3.4
### Fixed
- fix stepOver and pause did not work.

## [0.1.7] - 2017-01-27
### Fixed
- program arg is not available at Lua
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