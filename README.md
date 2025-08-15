# WinPowerDMS

A utility for switching the resolution of your laptop's display based on the current power state.

Currently a work in progress. It works reasonably well for a standard single display configuration, but other scenarios have not been extensively tested, and there are some edge cases that may need to be worked out.

This is a simple C program written using Visual Studio 2022 that has no external dependencies. It supports Windows Vista or higher.

## TODO
- [x] Save preferences to the Windows registry.
- [x] Add a checkbox to preferences for starting at login.
- [x] Add an icon.
- [ ] Add behavior for when multiple displays are connected.
- [x] Make the default display mode the current one instead of the highest resolution if there are no preferences set.
- [ ] Make an actual about dialog.
- [ ] Support dark mode.