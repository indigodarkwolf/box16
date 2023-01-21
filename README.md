# Box16

[![Build status](https://github.com/indigodarkwolf/box16/actions/workflows/build.yml/badge.svg)](https://github.com/indigodarkwolf/box16/actions/workflows/build.yml)<br/>
[![Release](https://img.shields.io/github/v/release/indigodarkwolf/box16)](https://github.com/indigodarkwolf/box16/releases)

This is an emulator for the Commander X16 computer system. Unlike [the official emulator](https://github.com/commanderx16/x16-emulator), this has a few more dependencies, see the build instructions below. It compiles on Windows, Debian Linux, and Raspbian, and probably 
other Linux-based platforms.

Don't expect official "releases" until the physical X16 is out. Until then, there will be "non-releases" of Box16.
Since this is originally forked from the Official X16 emulator, the goal is to remain fully compatible with the Official X16 emulator, but with feature-adds such as ImGui displays, and performance improvements from OpenGL acceleration. 

Some of the feature-adds will be prototype or preview in nature, as this is also my platform for fixing bugs and adding features to the official emulator.

Features
--------

* CPU: Full 65C02 instruction set
* VERA
	* Mostly cycle exact emulation
	* Supports almost all features:
		* composer
		* two layers
		* sprites
		* VSYNC, raster, sprite IRQ
* Sound
    * PCM
    * PSG
    * YM2151
* SD card: reading and writing (image file)
* VIA
	* ROM/RAM banking
	* keyboard
	* mouse
	* gamepad
	* Counters/timers/IRQs

Missing Features
----------------

* VERA
	* Does not support the "CURRENT_FIELD" bit
	* Interlaced modes (NTSC/RGB) don't render at the full horizontal fidelity

Binaries & Compiling
--------------------

The emulator itself is dependent on SDL2 and OpenGL. However, to run the emulated system you will also need a compatible `rom.bin` ROM image. This will be
loaded from the directory containing the emulator binary, or you can use the `-rom .../path/to/rom.bin` option.

> __WARNING:__ Older versions of the ROM might not work in newer versions of the emulator, and vice versa.

You can build a ROM image yourself using the [build instructions](https://github.com/commanderx16/x16-rom#releases-and-building) in the [x16-rom](https://github.com/commanderx16/x16-rom) repo. The `rom.bin` included in the [_latest_ release](https://github.com/commanderx16/x16-emulator/releases) of the emulator may also work with the HEAD of this repo, but this is not guaranteed.

### Linux Build

Read `resources/r41/README.box16` and build or acquire the necessary files.

The needed development packages are available as a distribution package with most major versions of Linux:
- Debian: `sudo apt-get install libgtk-3-dev libsdl2-dev`
- Fedora 37: `sudo dnf install gtk3-devel SDL2-devel alsa-lib-devel`
- Raspbian: `sudo apt-get install libgtk-3-dev libsdl2-dev`

Type `make` to build the source. The output will be `box16` in the output directory. Remember you will also need a `rom.bin` as described above.

### Windows Build

Read `resources/r41/README.box16` and build or acquire the necessary files.

Install Microsoft Visual Studio Community 2022, make sure to include the following modules:
- `Desktop development with C++`

Open build\vs2019\box16.sln with Visual Studio Community 2022.
Select `Build All` from the `Build` menu.

The built .exe and associated files should be located in `build\vs2022\out`, plus a few more subdirectories based on the exact build generated.


Releases
--------

As of 3 May 2022, the only release process is to package up the Windows builds. Creating these packages are meant as a convenience for Windows users
who may not be willing to build Box16 for themselves.

### Building release zips

Follow the instructions for making a Windows build, above.

Install the Windows Subsystem for Linux (WSL2). See the instructions at https://docs.microsoft.com/en-us/windows/wsl/install.
As of 27 April 2022, most of this should be as simple as running "wsl --install" from a PowerShell or Windows Command Prompt with *administrator* permissions.

The needed development packages are available as a distribution package with most major versions of Linux, even for WSL2.
- Debian: `sudo apt-get install p7zip-full`
- Raspbian: `sudo apt-get install p7zip-full`

In the Linux shell, go to the `build` directory from your Box16 repository directory.

Type `make release` to create all the zip files.

Starting
--------

You can start `box16`/`box16.exe` either by double-clicking it, or from the command line. The latter allows you to specify additional arguments.

* When starting `box16` without arguments, it will pick up the system ROM (`rom.bin`) from the executable's directory.
* `-abufs <number>` Is provided for backward-compatibility with x16emu toolchains, but is non-functional in Box16.
* `-bas` lets you specify a BASIC program in ASCII format that automatically typed in (and tokenized).
* `-create_patch <patch_target.bin>` creates a ROM patch file, which can then patch the current ROM to match the specified patch target.
* `-debug <address>` adds a breakpoint to the debugger.
* `-dump {C|R|B|V}` configure system dump (e.g. `-dump CB`):
	* `C`: CPU registers (7 B: A,X,Y,SP,STATUS,PC)
	* `R`: RAM (40 KiB)
	* `B`: Banked RAM (2 MiB)
	* `V`: Video RAM and registers (128 KiB VRAM, 32 B composer registers, 512 B pallete, 16 B layer0 registers, 16 B layer1 registers, 16 B sprite registers, 2 KiB sprite attributes)
* `-echo [{iso|raw}]` (e.g. `-echo iso`) causes all KERNAL/BASIC output to be printed to the host's terminal. Enable this and use the BASIC command "LIST" to convert a BASIC program to ASCII (detokenize):
	* By default, everything but printable ASCII will be escaped.
	* `iso` will escape everything but non-printable ISO-8859-1 characters and convert the output to UTF-8.
	* `raw` will not do any substitutions.
* `-geos` launches GEOS at startup.
* `-gif <file.gif>[,wait]` records frames generated by the VERA to the specified gif file (e.g. `-gif capture.gif` or `-gif capture.gif,wait`)
	* Recording normally begins immediately.
	* `,wait` will begin the gif file, but immediately pause recording.
	* POKE $9FB5,0 will pause GIF recording
	* POKE $9FB5,1 will snapshot a single frame
	* POKE $9FB5,2 will unpause GIF recording
* `-help` lists all command line options and then exits.
* `-hypercall_path <path>` sets the default path for all LOAD and SAVE calls to BASIC and the kernal.
* `-ignore_ini` will ignore the contents of any ini file that Box16 might be aware of. This option is not saved to the ini file.
* `-ignore_patch` will ignore the contents of any patch file that Box16 might be aware of.
* `-ini <custom.ini>` will allow manually specifying an ini file for Box16 to use.
* `-keymap` tells the KERNAL to switch to a specific keyboard layout. Use it without an argument to view the supported layouts.
* `-log` enables one or more types of logging (e.g. `-log KS`):
	* `K`: keyboard (key-up and key-down events)
	* `S`: speed (CPU load, frame misses)
	* `V`: video I/O reads and writes
* `-nobinds` will disable most emulator keyboard bindings, allowing the X16 to see most keys and key chords.
* `-nohostieee` will disable IEEE-488 hypercalls. These are normally enabled unless an SD card is attached or -serial is specified.
* `-nopanels` will disable loading panel settings from the ini file. This option is not saved to the ini file.
* `-nopatch` is an alias for `-ignore_patch`.
* `-nosound` can be used to specify that the audio subsystem should not be enabled in the first place. This is incompatible with `-sound`.
* `-nvram` lets you specify a 64 byte file for the system's non-volatile RAM. If it does not exist, it will be created once the NVRAM is modified.
* `-patch <patch.bpf>` specify a patch file to apply to the current ROM.
* `-prg` lets you specify a `.prg` file that gets injected into RAM after start.
* `-quality {nearest|linear|best}` lets you specify video scaling quality.
* `-ram <ramsize>` will adjust the amount of banked RAM emulated, in KB. (8, 16, 31, 64, ... 2048)
* `-rom <rom.bin>` will allow you to override the KERNAL/BASIC/ROM file used by the emulator.
* `-rtc` will set the real-time clock to the current system time and date.
* `-run` executes the application specified through `-prg` or `-bas` using `RUN` or `SYS`, depending on the load address.
* `-save_ini` will save Box16's settings to an ini file (at a default location, unless otherwise specified with `-ini`)
* `-scale {1|2|3|4}` sizes the Box16 window to scale video output to an integer multiple of 640x480. (e.g. `-scale 2`)
* `-sdcard <sdcard.img>` lets you specify an SD card image (partition table + FAT32).
* `-serial` Enables serial bus emulation (experimental).
* `-sound <device>` lets you specify a specific sound device to use. If given an improper device or no device, will list all audio devices and exit. Incompatible with `-nosound`.
* `-stds` will automatically load all kernal and BASIC labels, if available.
* `-sym <filename>` will load a VICE label file. Note that not all VICE debug commands are available. (e.g. `-sym myprg.lbl`)
* `-test {0, 1, 2, 3}` will automatically invoke the TEST command with the provided test number.
* `-verbose` enables additional output messages from Box16.
* `-version` will print the version of Box16 and then exit.
* `-warp` causes the emulator to run as fast as possible, possibly faster than a real X16.
* `-wav <file.wav>[{,wait|,auto}]` records audio to the specified wav file (e.g. `-wav audio.wav` or `-wav audio.wav,wait`)
	* Recording normally begins immediately.
	* `,wait` will start with recording paused.
	* `,auto` will start with recording paused, but recording will automatically start at the first non-zero audio signal.
	* POKE $9FB6,0 will pause wav recording
	* POKE $9FB6,1 will unpause wav recording
	* POKE $9FB6,2 will unpause wav recording at the fist non-zero audio signal
* `-vsync {none|get|wait}` uses specified vsync rendering strategy to avoid visual tearing. Some drivers may not support all types of vsync.
	* `none`: Use if the content area remains white after start. Disables vsync.
	* `get`: Default, should work with OpenGL ES >= 3.0
	* `wait`: Alternative, should work with OpenGL >= 3.2
* `-ymirq` will enable interrupts from the YM2151 audio chip (this is disabled by default to match the behavior of the official emulator r38)
* `-ymstrict` will enable strict enforcement of the YM2151's busy status, dropping writes to the chip if it's busy at the time of write (this is disabled by default to match the behavior of the official emulator r38)

Run `box16 -help` to see all command line options.

Keyboard Layout
---------------

The X16 uses a PS/2 keyboard, and the ROM currently supports several different layouts. The following table shows their names, and what keys produce different characters than expected:

|Name  |Description 	       |Differences|
|------|------------------------|-------|
|en-us |US		       |[`] ⇒ [←], [~] ⇒ [π], [&#92;] ⇒ [£]|
|en-gb |United Kingdom	       |[`] ⇒ [←], [~] ⇒ [π]|
|de    |German		       |[§] ⇒ [£], [´] ⇒ [^], [^] ⇒ [←], [°] ⇒ [π]|
|nordic|Nordic                 |key left of [1] ⇒ [←],[π]|
|it    |Italian		       |[&#92;] ⇒ [←], [&vert;] ⇒ [π]|
|pl    |Polish (Programmers)   |[`] ⇒ [←], [~] ⇒ [π], [&#92;] ⇒ [£]|
|hu    |Hungarian	       |[&#92;] ⇒ [←], [&vert;] ⇒ [π], [§] ⇒ [£]|
|es    |Spanish		       |[&vert;] ⇒ π, &#92; ⇒ [←], Alt + [<] ⇒ [£]|
|fr    |French		       |[²] ⇒ [←], [§] ⇒ [£]|
|de-ch |Swiss German	       |[^] ⇒ [←], [°] ⇒ [π]|
|fr-be |Belgian French	       |[²] ⇒ [←], [³] ⇒ [π]|
|fi    |Finnish		       |[§] ⇒ [←], [½] ⇒ [π]|
|pt-br |Portuguese (Brazil ABNT)|[&#92;] ⇒ [←], [&vert;] ⇒ [π]|

Keys that produce international characters (like [ä] or [ç]) will not produce any character.

Since the emulator tells the computer the *position* of keys that are pressed, you need to configure the layout for the computer independently of the keyboard layout you have configured on the host.

**Use the F9 key to cycle through the layouts, or set the keyboard layout at startup using the `-keymap` command line argument.**

The following keys can be used for controlling games:

|Keyboard Key  | NES Equivalent |
|--------------|----------------|
|Ctrl          | A              |
|Alt           | B              |
|Space         | SELECT         |
|Enter         | START          |
|Cursor Up     | UP	            |
|Cursor Down   | DOWN           |
|Cursor Left   | LEFT           |
|Cursor Right  | RIGHT          |


Functions while running
-----------------------

* `Ctrl` + `R` will reset the computer.
* `Ctrl` + `V` will paste the clipboard by injecting key presses.
* `Ctrl` + `S` will save a system dump (configurable with `-dump`) to disk.
* `Ctrl` + `F` and `Ctrl` + `Return` will toggle full screen mode.
* `Ctrl` + `=` and `Ctrl` + `+` will toggle warp mode.
* `Ctrl` + `A` will attach the SD Card image, if available.
* `Ctrl` + `D` will detach the SD Card image.

On the Mac, use the `Cmd` key instead.


GIF Recording
-------------

With the argument `-gif`, followed by a filename, a screen recording will be saved into the given GIF file. Please exit the emulator before reading the GIF file.

If the option `,wait` is specified after the filename, it will start recording on `POKE $9FB5,2`. It will capture a single frame on `POKE $9FB5,1` and pause recording on `POKE $9FB5,0`. 

`PEEK($9FB5)` returns a 0 if recording is disabled, 1 if recording is enabled but not active, 2 if snapshotting a single frame, and 3 if recording.


WAV Recording
-------------

With the argument `-wav`, followed by a filename, a audio recording will be saved into the given WAV file. Please exit the emulator before reading the WAV file.

If the option `,wait` is specified after the filename, it will start recording on `POKE $9FB6,1`. If the option `,auto` is specified after the filename, it will start recording on the first non-zero audio signal, or on `POKE $9FB6,1`. `POKE $9FB6,0` will pause recording, and `POKE $9FB6,2` will pause recording until the next non-zero audio signal.

 `PEEK($9FB6)` returns 0 if recording is disabled, 1 if recording is enabled but not active, 2 if recording is paused waiting on a non-zero audio signal, and 3 if recording.


BASIC and the Screen Editor
---------------------------

On startup, the X16 presents direct mode of BASIC V2. You can enter BASIC statements, or line numbers with BASIC statements and `RUN` the program, just like on Commodore computers.

* To stop execution of a BASIC program, hit the `RUN/STOP` key (`Esc` in the emulator), or `Ctrl + C`.
* To insert characters, first insert spaces by pressing `Shift + Backspaces`, then type over those spaces.
* To clear the screen, press `Shift + Home`.
* The X16 does not have a `STOP + RESTORE` function.


SD Card Images
--------------

The command line argument `-sdcard` lets you attach an image file for the emulated SD card. Using an emulated SD card makes filesystem operations go through the X16's DOS implementation, so it supports all filesystem operations (including directory listing though `DOS"$` command channel commands using the `DOS` statement) and guarantees full compatibility with the real device.

Images must be greater than 32 MB in size and contain an MBR partition table and a FAT32 filesystem. The file `sdcard.img.zip` in this repository is an empty 100 MB image in this format.

On macOS, you can just double-click an image to mount it, or use the command line:

	# hdiutil attach sdcard.img
	/dev/disk2              FDisk_partition_scheme
	/dev/disk2s1            Windows_FAT_32                  /Volumes/X16 DISK
	# [do something with the filesystem]
	# hdiutil detach /dev/disk[n] # [n] = number of device as printed above

On Linux, you can use the command line:

	# sudo losetup -P /dev/loop21 disk.img
	# sudo mount /dev/loop21p1 /mnt # pick a location to mount it to, like /mnt
	# [do something with the filesystem]
	# sudo umount /mnt
	# sudo losetup -d /dev/loop21

On Windows, you can use the [OSFMount](https://www.osforensics.com/tools/mount-disk-images.html) tool.


Host Filesystem Interface
-------------------------

If the system ROM contains any version of the KERNAL, and there is no SD card image attached, the LOAD (`$FFD5`) and SAVE (`$FFD8`) KERNAL calls (and BASIC statements) are intercepted by the emulator for device 8 (the default). So the BASIC statements will target the host computer's local filesystem:

      LOAD"$
      LOAD"FOO.PRG
      LOAD"IMAGE.PRG",8,1
      SAVE"BAR.PRG

Note that this feature is very limited! Manually reading and writing files (e.g. `OPEN` in BASIC) is not supported by the host filesystem interface. Use SD card images for this.

The emulator will interpret filenames relative to the directory it was started in. On macOS, when double-clicking the executable, this is the home directory.

To avoid incompatibility problems between the PETSCII and ASCII encodings, you can

* use lower case filenames on the host side, and unshifted filenames on the X16 side.
* use `Ctrl+O` to switch to the X16 to ISO mode for ASCII compatibility.
* use `Ctrl+N` to switch to the upper/lower character set for a workaround.


Dealing with BASIC Programs
---------------------------

BASIC programs are encoded in a tokenized form, they are not simply ASCII files. If you want to edit BASIC programs on the host's text editor, you need to convert it between tokenized BASIC form and ASCII.

* To convert ASCII to BASIC, reboot the machine and paste the ASCII text using `Ctrl + V` (Mac: `Cmd + V`). You can now run the program, or use the `SAVE` BASIC command to write the tokenized version to disk.
* To convert BASIC to ASCII, start box16 with the `-echo` argument, `LOAD` the BASIC file, and type `LIST`. Now copy the ASCII version from the terminal.


Using the KERNAL/BASIC environment
----------------------------------

Please see the KERNAL/BASIC documentation.


Debugger
--------

There are a number of debugging windows that can be displayed:
* Two independent memory dump windows.
* A disassembly and CPU status window.
* A VERA memory dump and status window.

The debugger keys are similar to the Microsoft Debugger shortcut keys, and work as follows

|Key|Description 																			|
|---|---------------------------------------------------------------------------------------|
|F1 |resets the shown code position to the current PC										|
|F2 |resets the 65C02 CPU but not any of the hardware.										|
|F5 |is used to return to Run mode, the emulator should run as normal.						|
|F9 |sets the breakpoint to the currently code position.									|
|F10|steps 'over' routines - if the next instruction is JSR it will break on return.		|
|F11|steps 'into' routines.																	|
|F12|is used to break back into the debugger.                                               |

The STP instruction (opcode $DB) will break into the debugger automatically.

Effectively keyboard routines only work when the debugger is running normally. Single stepping through keyboard code will not work at present.


Forum
-----

[https://www.commanderx16.com/forum/](https://www.commanderx16.com/forum/)


Wiki
----

[https://github.com/commanderx16/x16-emulator/wiki](https://github.com/commanderx16/x16-emulator/wiki)


License
-------

Copyright (c) 2019-2023 Michael Steil &lt;mist64@mac.com&gt;, [www.pagetable.com](https://www.pagetable.com/), et al.<br>
Portions copyright (c) 2021-2023 Stephen Horn, et al.<br>
All rights reserved. License: 2-clause BSD


Release Notes
-------------

## Non-Release 41.1 ("Koutoubia Mosque")
* Improved hypercall performance
* -ignore_patch is now saved to ini file.
* Saving which panels are open.
* Keymappings now match r41 kernal.
* Optimized VIA timer implementation.
* Moved imgui.ini location to match box16.ini
* Added -nohypercalls to disable all hypercalls.
* Added VERA feature: AUDIO_CTRL bit 6 FIFO_EMPTY (ZeroByteOrg)
* Improved parsing of addresses when loading symbols from a VICE Label File. (claudiobrotto)
* Improved framerate consistency for 60Hz displays, possibly others too.
* Added -noemucmdkeys from x16emu (aliased to -nobinds)
* Added -wuninit from x16emu to print a warning when the X16 accesses uninitialized RAM.
* Added -randram from x16emu to randomize the contents of RAM, more closely matching hardware behavior.
* Added -widescreen from x16emu to display the X16's graphics in a widescreen format instead of the default 4:3.
* Disassembler improvements:
	* Disassembler now follows into current RAM and ROM bank.
	* Added inline buttons to toggle breakpoints in disassembler.
	* Added 'F9' to toggle breakpoint on current instruction.
* Ported fixes/updates from x16emu:
	* Usage text had wrong ISO type. https://github.com/commanderx16/x16-emulator/pull/404
	* Fixed 65C02 WAI instruction behavior. https://github.com/commanderx16/x16-emulator/pull/434
	* Fixed 65C02 D flag on IRQ/NMI. https://github.com/commanderx16/x16-emulator/pull/436
	* Updated VERA IEN and IRQLINE_L/SCANLINE_L to match latest behavior in the VERA repo.
* Bugfixes:
	* Fixed warp mode toggling.
	* Fixed YM busy flag when emulator audio is disabled.
	* Fixed crash on Linux when listing directories with subdirectories.
	* Disabled patch loading when patch path exists but is empty. (jburks)
	* Fixed RTC to increment on CPU simulation.
	* Fixed various false-positives in a check that disabled v-sync.
	* Pulled fix from Imgui to fix docked windows when the window is minimized.
	* Fixed display of 16x16x1 w/ t256c tiles.
	* Fixed empty default set to dump when the X16 detects a crash condition
	* Fixed VIA writes to IFR registers

<!-------------------------------------------------------------------->
[x16rom]: https://github.com/commanderx16/x16-rom
