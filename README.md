# PicoPOST

> [!NOTE]
> You can get a quick summary from the blog post! [Introducing the PicoPOST - The Retro Web Blog](https://blog.theretroweb.com/2023/01/03/introducing-the-picopost/)

> [!NOTE]
> More in-depth information can be found on our Wiki! [PicoPOST - The Retro Web Wiki](https://wiki.theretroweb.com/index.php?title=PicoPOST)

## Introduction

![PicoPOST Logo](/readme_files/logo.png)

**PicoPOST** is a PC diagnostic card for systems with, at least, an 8-bit ISA slot. It's based around the Raspberry Pi
Pico, powered by an RP2040.

PicoPOST is designed to provide many additional benefits over the basic POST cards that are commonly available from
Chinese vendors on eBay and other sites.

This project is getting ready for public usage, with a few prototypes already in use for development purposes. More
information soon as it gets finalized.

## Implemented features

- Simple user interface, with a graphical monochrome 128x32 OLED display and a few buttons living on their own remote
  control
- Port 80h readout, with 4-place output history
- Port 90h readout, for IBM PS/2s
- Port 84h reaodut, for Compaq machines*
- Port 300h readout, for some EISA systems*
- Port 378h readout, for some Olivetti machines*
- More complete bus activity dumping facility
- Reset pulse detection
- +5V, +12V and -12V voltage monitor
- Display is dimmed after 15s of inactivity to mitigate burn-in
- Flying Toasters! screensaver after 30s of inactivity on the main menu

> *: We don't have a specimen handy, so we need confirmation from someone else out there.

### Videos

We are slowly compiling a [YouTube playlist](https://www.youtube.com/playlist?list=PLejCJef6DKnVJob0ycztrpl9FoBufaOPA) with a whole bunch of demonstration pieces, showing what the PicoPOST is capable of doing.\
Stay tuned for more content!

### Wishlist

New features have been implemented with the new rev 6 PCB and matching firmware.
- Full bus activity trace (HW OK, FW provisioned, but we need to make sure we are not missing data)
- Bus clock reader (HW OK, FW TBD)
- ... anything else, as long as the hardware allows it and someone implements it in firmware

## Building the firmware

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

### Customize the firmware

There are a couple of details you might want to tune at compile time. More specifically, you may want to better calibrate
the voltage monitor to show more accurate results.

In the primary `firmware\CMakeLists.txt` file, there are three variables you can set to skew the final ADC conversion result. These were added to account for resistor tolerances in the voltage divider circuits.
- `CALIBADJ_5V`, correction factor for the +5V rail
- `CALIBADJ_12V`, correction factor for the +12V rail
- `CALIBADJ_N12V`, correction factor for the -12V rail

Calculating these factors is pretty straight forward:
- Install the default firmware
- Connect PicoPOST to your computer of choice with a known good power supply
- Enter `Voltage Monitor` mode
- Turn on the computer
- With a multimeter, measure the rail voltage as close as possible to PicoPOST
- Compute the required calibration factor by dividing the measured value with the one shown by PicoPOST
- Edit the `CALIBADJ` variable and build the firmware again

## Flashing the firmware

1. In order to load new firmware, the Pico must be booted into UF2 mode:
   - If the board is powered off, keep the BOOTSEL button pressed while plugging the USB cable into the Pico itself.
   - If the board is already powered, you can short the "Pico Reset" pins on the back of the card instead of removing the USB cable.
   - Alternatively, if the board is powered via USB from a PC with a data cable, you can move to the `Update FW` menu entry. By pressing Enter, the Pico will then be rebooted into UF2 mode automatically.
2. A mass storage device should be now mounted on your computer.
3. Drag and drop (or copy and paste) the `.uf2` firmware file into the device.
4. Once loaded, it will automatically restart and disconnect.
5. If all went well, you should now be greeted by the main menu on the OLED display.\
   You can verify the current firmware version by entering the `Info` page.

## Interested in helping?
- Submit issues and pull requests!
- Join us in the #picopost channel in [The Retro Web discord server](https://discord.gg/TdD4tqQ7fv)
- Join us and other developers of retro Pico hardware (such as PicoGUS and PicoMEM) in the [Retro Pico Hardware discord server](https://discord.gg/QBUkpPqFa5)

## Directories

```
datasheets/  Reference documentation for components used in the PicoPOST
firmware/    Source code for the PicoPOST firmware (written in C/C++ with the official Pico SDK)
pcb/         KiCad schematics and PCB design files
```

## License

The firmware portion of PicoPOST is licensed under the MIT license.\
The hardware portion of PicoPOST is licensed under the CERN OHL v2 Permissive license.
