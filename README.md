# WinPowerDMS

A utility for switching the resolution of your laptop's display based on the current power state.

Currently a work in progress. Not very useful yet since it doesn't save your preferences. It doesn't even have an icon.

This is a simple C program written using Visual Studio 2022 that has no external dependencies. It should support Windows Vista or higher, but this hasn't been tested yet.

## TODO
- [x] Save preferences to the Windows registry.
- [ ] Add a checkbox to preferences for starting at login.
- [ ] Add an icon.
- [ ] Add behavior for when multiple displays are connected.
- [ ] Block program from running when the system has no battery.
- [x] Make the default display mode the current one instead of the highest resolution if there are no preferences set.
- [ ] Make an actual about dialog.