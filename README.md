# FAKETABLETD

### **THIS IS AN EXPERIMENTAL PROJECT, AND IT COULD POTENTIALY CAUSE SYSTEMWIDE CRASHES. USE IT AT YOUR OWN DISCRETION**
A userspace driver for drawing tablets based on the [digimend-userspace-drivers](https://github.com/DIGImend/digimend-userspace-drivers) project. **faketabletd** is meant to support those tablets not currently supported by the **Digimend** project, as such, it should only be used as a last resort should there not be any kernel drivers available for your device.

## Installation
In order to use **faketabletd** you will need to compile this repository, as there are currently no binaries provided with the project.

### Getting the required dependecies
As of the writing of this guide, only `cmake` and `libusb` are required to build **faketabletd** (this is, of course, assuming you already have `gcc` installed as well as `git` to pull the code from this repo). You will also need the [input-wacom](https://github.com/linuxwacom/input-wacom) and [xf86-input-wacom](https://github.com/linuxwacom/xf86-input-wacom) drivers respectively, as this tool serves as translation layer between your device and the linux wacom tablet drivers.

On Ubuntu based distros, you can install the required dependecies with the following command
```bash
sudo apt install build-essential cmake libusb-1.0-0-dev git xserver-xorg-input-wacom
```

### Fetching the code
Just open your terminal and `cd` into a folder where you'd like to download the code into and use the following command.
```bash
git clone https://github.com/TheLastBilly/faketabletd
```

### Building the code
Use the following commands to build **faketabletd**
```bash
cd faketabletd
mkdir build
cmake ..
make
```

This will create a file called `build/bin/faketabletd` on the folder where you clonned this repository. To make this executable more accessible in the future, you can copy it into your `/usr/bin/` folder.
```bash
sudo cp build/bin/faketabletd /usr/bin
```

### Using faketabletd

As of the writing of this guide, **faketabletd** requires **root** to work (yeah...), so in order to run it you will need to use `sudo`.

```bash
sudo faketabletd
```

This will run **faketabletd** in your terminal where it will start waiting for compatible tablets. Just plug your tablet in, and you should see it being detected on your screen. i.e:
```
[faketabletd.c:466] [info]: looking for compatible devices...
[faketabletd.c:472] [info]: found supported device: HS610 (256c:006e)
[faketabletd.c:475] [info]: connecting to device
[faketabletd.c:493] [info]: connected!
[faketabletd.c:495] [info]: configuring device...
[faketabletd.c:527] [info]: done configuring device!
[faketabletd.c:529] [info]: ready!
```

You should now be able to use your tablet just like a regular wacom tablet.
#### Options
```
Usage: faketabletd [OPTION]
User space driver for drawing tablets

Options
  -m                    Enables virtual mouse emulation
  -r                    Resets the program back to the scanning phase on disconnect (experimental)

Examples:
  faketabletd -m        Runs driver with virtual mouse emulation
  faketabletd -mr       Runs driver with virtual mouse emulation. Will not exit on disconnect
```