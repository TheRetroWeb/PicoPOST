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

**PicoPOST** is a PC diagnostic card for systems with, at least, an 8-bit ISA
slot. It's based around the Raspberry Pi Pico, powered by an RP2040.

PicoPOST is designed to provide many additional benefits over the basic POST
cards that are commonly available from Chinese vendors on eBay and other sites.

This project is getting ready for public usage, with a few prototypes already in
use for development purposes. More information soon as it gets finalized.

##  2. <a name='Implementedfeatures'></a>Implemented features

- Simple user interface, with a graphical monochrome 128x32 OLED display and a
  few buttons living on their own remote control
- Port 80h readout, with 4-place output history
- +5V and +12V voltage monitor

###  2.1. <a name='Wishlist'></a>Wishlist

More features are coming with the new rev 6 PCB and matching firmware. This
readme will be updated accordingly. Expected features include:
- -12V rail monitoring
- Bus clock reader
- Bus output readout for specific ports
- Full bus activity trace
- Improved remote control setup
- ...

##  3. <a name='Buildingthefirmware'></a>Building the firmware

The firmware is built using the official Raspberry Pi RP2040 SDK. Follow the
[official documentation](https://github.com/raspberrypi/pico-sdk#quick-start-your-own-project)
for setting it up.

The default CMake environment shipped with the repo expects the RP2040 SDK to be
present at `~/repo/pico-sdk`. Feel free to edit the main CMakeLists.txt file so
it can automatically fetch the SDK from Raspberry Pi's official repository.

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

If you're using an IDE like Visual Studio Code, you can follow the relevant build instructions
in the [Pico SDK Getting Started guide](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf), or you can manually install the necessary
extensions for C++ development and CMake build systems, then follow the
configuration wizard for setting up the bare-metal ARM GCC compiler.

##  4. <a name='Flashingthefirmware'></a>Flashing the firmware

Like all RPi Pico software, keep the BOOTSEL button pressed while plugging the
USB cable into the Pico itself. *(If the board is already powered, you can short
the "Pico Reset" pins on the back of the card instead of removing the USB cable.)*

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
