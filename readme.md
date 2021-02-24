## What's this?
This is a small flash tool written by c++ for unisoc modems.
Anybody can use it for free to update there unisoc Phones but with no guarantee.

## How to use?
1. First build via `make`, the you will get a executable file named with 'dloader'
2. Run the file
```shell
➜  dloader git:(main) ✗ ./dloader -h
./dloader version MAJOR_VER.MINOR_VER.REVISION_VER build at: Feb 24 2021
./dloader [config] [options]
    -f pac_file           firmware file, with suffix of '.pac'
    -d device             tty device, example(/dev/ttyUSB0)
    -p usb port           usb port, it's port string, see '-l' for details
    -x pac_file [dir]     exract pac_file only
    -c chip_set           udx710(5g) or uix8910(4g)
    -l                    list devices
    -h                    help message
```