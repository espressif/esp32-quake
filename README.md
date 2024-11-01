This is a version of the Quake 1 engine that runs on the ESP32-P4-Function-EV-Board 
when coupled with the ESP32-P4-HMI-SubBoard that contains a 1024x600 MIPI LCD. 

Features
========

 * Can run both the shareware as well as the registered version of Quake

 * Runs at 512x300 pixels upscaled to 1024x600 at a framerate of 20-25fps.

 * Audio support. Both digital sound, as well as music if a .cue/.bin image of the
   original CDROM is provided.

 * Supports USB keyboard for input

 * Network multiplayer support over the LAN connector


Hardware required
=================

As stated, you'll need an ESP32-P4-Function-EV-Board with the ESP32-P4-HMI-SubBoard that 
contains a 1024x600 MIPI LCD. Versions tested are V1.4 for the EV-Board and v1.2 for the MIPI
subboard. Note that aside from the MIPI FPC cable, you'll need to run some jumpers between
the two boards (J6 on the sub-board to J1 on the EV-Board)

| Sub-board | EV board      |
| --------- | ------------- |
| 5V        | 5V            |
| PWM       | GPIO26 ('26') |
| RST_LCD   | GPIO27 ('27') |
| GND       | GND           |

If you want sound, you'll have to connect a speaker to the SPK header. To control Quake,
plug a USB keyboard into the USB-A receptacle J18 on the EVB-board. In theory, you can
also connect a USB hub with a keyboard and mouse into this connector, but this has not
been tested.

You will also need a micro-SD card to put the Quake files on. The size depends on the 
version of Quake you're going to put on there, but generally anything 2GiB or larger 
will fit nearly any version of Quake out there.

Quake game data
===============

This repository does not contain any Quake game data; you should bring your own. Here
are a few options. If you have an original Quake CDROM or other source of game
data, you can also install it somewhere and copy the files from there.

Quake (The Offering) from GOG.com
---------------------------------

Buy the game at [Good Old Games](https://www.gog.com/en/game/quake_the_offering). Download 
and run the installer (under Windows or Wine) to install to a directory. Alternatively, use 
[innoextract](https://constexpr.org/innoextract/) to extract the Quake files from the 
installation executable; in this case the data you need is under the 'app' subdirectory.

Shareware Quake
---------------

Download quake106.zip from e.g. [dosarchives](https://www.dosgamesarchive.com/file/quake/quake106/)
or simply chuck 'quake106.zip' into your favourite search engine and download the resulting
file. Extracting the files gives you the DOS Quake shareware installer. You can either use 
e.g. Dosbox to run install.bat, or alternatively use 7zip to extract the 'resource.1' file
in the archive.

LibreQuake
----------

LibreQuake has been tested, but seems to be written for more modern engines. Regardless, 
it still seems to load and kind-of work. To install, download 'full.zip' from the
[releases](https://github.com/lavenderdotpet/LibreQuake/releases) page and extract.
Note you need to take the 'id1' folder (inside the 'full' folder) and copy that to your
micro-SD card.

Copying the data
================

Preferably, your micro-SD card is formatted as FAT: ExFAT may work but is untested. The
data needs to be copied directly to the root of the micro-SD-card: most notably, the 'ID1'
folder should be located in the root, not in a subfolder.

If you want music (and the version of Quake you're using supports it - the shareware version
and LibreQuake does not) you'll need to place a .bin/.cue image of the CDROM in the root
of the Micro-SD card as well. If you're using the GOG.com version, no need to do anything - it 
already comes with the required image.

Compiling, flashing and running
===============================

This project needs ESP-IDF v5.4 or later, or until v5.4 is released the master branch of ESP-IDF,
to properly compile. (It will compile properly with v5.3, but USB host won't work making the
game uncontrollable.) With a proper ESP-IDF installed, run:

- ``idf.py set-target esp32p4`` (only the first time)

- ``idf.py flash``

Now insert the micro-SD-card with the Quake data files into the devkit micro-SD slot,
connect a keyboard to the USB-A connector, reset the board, and have fun!


Network play
============

Network play is possible by connecting the RJ-45 Ethernet port of the devkit to a network.
On startup, the ESP32P4 will request an IP via DHCP. Note that because of a bug in the
network layer, it's not currently possible to connect to an external server, but Quake 
on the ESP32-P4 is able to act like a server itself.


Porting
=======

This version of Quake heavily relies on the esp-bsp to abstract away any details in hardware
the game is running on. It should be fairly easy to port to any other board that has an esp-bsp
implementation. However, no such attempt has been made and as such, there may be issues lurking
in dark corners.

Credits
=======

This is based on [quake-generic](https://github.com/erysdren/quakegeneric) by erysdren, which 
in turn is based on the [Quake sources](https://github.com/id-Software/Quake) released by 
Id Software.




