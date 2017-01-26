# Change Log


## [Unreleased]


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