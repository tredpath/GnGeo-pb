GnGeo-pb
========

A NeoGeo emulator for the Blackberry Playbook.

The startup UI works just like it would on a PC. Drag the mouse to the game that you want to play and press once to select it and double click it to open it. If you don't see an image to the left of the game list then you don't have a game selected.

Besides the dpad and the coin, start, A (red), B (yellow), C (green), and D (blue) buttons there are also 4 buttons in the top right that are not visible during gameplay. Clockwise from the top left they are:
- switch between 1st and 2nd player controllers,
- open the ROM select scree,
- load a saved state,
- and save the state.

ROMs need to be zip files and should be places into shared/misc/gngeo/ROMS. You also need an rc file for each ROM. They should have the extension rc and be placed in shared/misc/gngeo/ROMS/romrc.d.


GnGeo
=====

GnGeo is a portable NeoGeo emulator for Linux. It is licensed under a modified GPLV2 license. For more information see:

http://code.google.com/p/gngeo/


Requirements For Building
=========================

GnGeo-pb requires the SDL12 and TouchControlOverlay libraries that can be found at:

https://github.com/blackberry/TouchControlOverlay
https://github.com/blackberry/SDL