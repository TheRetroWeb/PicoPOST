# PicoPOST

## Index
<!-- vscode-markdown-toc -->
* 1. [Introduction](#Introduction)
* 2. [Implemented features](#Implementedfeatures)
	* 2.1. [Wishlist](#Wishlist)
* 3. [Building the firmware](#Buildingthefirmware)
* 4. [Flashing the firmware](#Flashingthefirmware)
* 5. [Directories](#Directories)
* 6. [License](#License)

<!-- vscode-markdown-toc-config
	numbering=true
	autoSave=true
	/vscode-markdown-toc-config -->
<!-- /vscode-markdown-toc -->

-----------------------------

> You can get a quick summary from the blog post! [Introducing the PicoPOST - The Retro Web Blog](https://blog.theretroweb.com/2023/01/03/introducing-the-picopost/)

##  1. <a name='Introduction'></a>Introduction

![PicoPOST Logo](/readme_files/logo.png)

**PicoPOST** is a PC diagnostic card for systems with, at least, an 8-bit ISA slot. It's based around the Raspberry Pi
Pico, powered by an RP2040.

PicoPOST is designed to provide many additional benefits over the basic POST cards that are commonly available from
Chinese vendors on eBay and other sites.

This project is getting ready for public usage, with a few prototypes already in use for development purposes. More
information soon as it gets finalized.

##  2. <a name='Implementedfeatures'></a>Implemented features

- Simple user interface, with a graphical monochrome 128x32 OLED display and a few buttons living on their own remote
  control
- Port 80h readout, with 4-place output history
- Port 90h readout, for IBM PS/2s
- Port 84h reaodut, for Compaq machines*
- Port 300h readout, for some EISA systems*
- Port 378h readout, for some Olivetti machines*
- Reset pulse detection**
- +5V, +12V and -12V*** voltage monitor
- Display is dimmed after 15s of inactivity to mitigate burn-in
- Flying Toasters! screensaver after 30s of inactivity on the main menu

*: We don't have a specimen handy, so we need confirmation from someone else out there.\
**: It used to work at some point, now it's not doing anything. Maybe I'm not smart enough.\
***: It's there as far as hardware goes, but I still need to figure out the right coefficients. Again, not smart enough.

###  2.2. Videos

**Flying Toasters!**\
[![Flying Toasters!](https://img.youtube.com/vi/YaeOhzFURtc/hqdefault.jpg)](https://www.youtube.com/watch?v=YaeOhzFURtc)

**Demo on IBM PS/2 Model 30/286**\
[![Demo on IBM PS/2 Model 30/286](https://img.youtube.com/vi/QVEzR7sBcNQ/hqdefault.jpg)](https://www.youtube.com/watch?v=QVEzR7sBcNQ)

###  2.1. <a name='Wishlist'></a>Wishlist

New features have been implemented with the new rev 6 PCB and matching firmware. 
- -12V rail monitoring (HW OK, FW almost there)
- Full bus activity trace (HW OK, FW provisioned, but we need to make sure we are not missing data)
- Bus clock reader (HW OK, FW TBD)
- ... anything else, as long as the hardware allows it and someone implements it in firmware

##  3. <a name='Buildingthefirmware'></a>Building the firmware

The firmware is built using the official Raspberry Pi RP2040 SDK. Follow the
[official documentation](https://github.com/raspberrypi/pico-sdk#quick-start-your-own-project) for setting it up.

The procedure is pretty straight-forward:
1. Clone the repo
   ```
   mkdir ~/repo
   cd ~/repo
   git clone https://github.com/TheRetroWeb/PicoPOST
   ```
2. Enter the repo and initialize it
   ```
   cd PicoPOST
   git submodule init
   git submodule update
   ```
3. Prepare the build system with CMake
   ```
   export PICO_SDK_PATH /path/to/pico/sdk
   cmake -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE \
       -DCMAKE_BUILD_TYPE:STRING=Release \
       -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/arm-none-eabi-gcc \
       -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/arm-none-eabi-g++ \
       -S./firmware \
       -B./build \
       -G "Unix Makefiles"
   ```
4. Build the firmware
   ```
   cmake --build ./build --config Release --target all -j $(nproc)
   ```

If you're using an IDE like Visual Studio Code, you can follow the relevant build instructions in the
[Pico SDK Getting Started guide](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf), or you can
manually install the necessary extensions for C++ development and CMake build systems, then follow the configuration 
wizard for setting up the bare-metal ARM GCC compiler.

##  4. <a name='Flashingthefirmware'></a>Flashing the firmware

In order to load new firmware, the Pico must be booted into UF2 mode.\
If the board is powered off, keep the BOOTSEL button pressed while plugging the USB cable into the Pico itself.\
If the board is already powered, you can short the "Pico Reset" pins on the back of the card instead of removing the USB
cable.\
Alternatively, if the board is powered via USB from a PC with a data cable, you can move to the Update FW menu entry.
By pressing Enter, the Pico will then be rebooted into UF2 mode automatically.

A mass storage device should be now mounted on your computer.

Drag and drop (or copy and paste) the `.uf2` firmware file into the device.
Once loaded, it will automatically restart and disconnect.

If all went well, you should now be greeted by the main menu on the OLED display.

##  5. <a name='Directories'></a>Directories

```
datasheets/  Reference documentation for components used in the PicoPOST
firmware/    Source code for the PicoPOST firmware (written in C/C++ with the official Pico SDK)
pcb/         KiCad schematics and PCB design files
```

##  6. <a name='License'></a>License

The firmware portion of PicoPOST is licensed under the MIT license.\
The hardware portion of PicoPOST is licensed under the CERN OHL v2 Permissive license.
