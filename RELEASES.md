Release Notes
-------------

## Non-Release 39.2 ("Hokkaido Jingu")

* Internals
	* Replaced the YM core with ymfm, a BSD 3-clause library.
	* YM2151 now supports timers, IRQs, and strict enforcement of the busy status.
* Removed a variety of redundant dll files (akumanatt)
* Fixed anisotropic filtering for display ("best" scaling option).
* Fixed middle mouse button support (ZeroByteOrg)
* Fixed YM2151 reset (ZeroByteOrg)
* Many overlays and UIs replaced or improved (akumanatt, et al)
	* Added CPU visualization to show where/how the CPU is spending its time per frame.
	* Added command-line parameter hints to tooltip on Options display.
	* Added CPU stack to debugger
	* Re-wrote PSG monitor (akumanatt)
	* Added YM2151 monior (akumanatt)
	* Improved Layer debugger (akumanatt)
	* Improved Tile visualizer (akumanatt)
	* Usability tweak to color picker
	* Improved Sprite settings (akumanatt)
* Added basic MIDI support, it is now possible to play with the X16's audio devices via MIDI control.
* Fixed a variety of relatived and absolute pathing issues.
* Added support for "break" VICE label command in VICE label files.
* Moved YM2151 to r39 memory address
* Performance improvements
	* Faster `rel` instruction.
	* Faster check for lo-RAM breakpoints.
* Added WAV recorder

## Non-Release 39.1 ("Kiyomizu-dera Temple")

* Supporting hardware Rev2.
* Removed NES controller support.
* Added -nosound to explicity disable sound system init.
* Added ini file, comparable to r39. Will regenerate the file based on 
	current command line options if no ini file is present at load time. 
	Entries are identical to the command line parameters. Command line 
	parameters will overrule ini file values.
* Internals
	* Faster joystick implementation, now supporting hot-swapping.
	* Even faster PS2.
	* Rebuilt build script. Windows is now a VS2019 solution. Still no Mac, but Linux builds A-OK. Pls halp with iThings, 	PRs absolutely welcome.
* Fixes VERA scaling for scaling values greater than $80.
* Added a bunch of imgui windows for debugging and options. Full list, including new ones:
	* Options
	* Memory Dumps
	* ASM Monitor
	* CPU Visualizer
	* VRAM Visualizer
	* VERA Monitor
	* Palette
	* Layer Settings
	* Sprite Settings
	* PSG Monitor
* Fixes ported from official emulator:
	* PR 216, fixing occasional flicker and hang issues on Linux. (nigels-com)
	* PR 317, fixing debugger break timing after hypercalls.
	* PR 321, adjusting mouse state send rate. (Elektron72)
	* PR 322, fixing vsync timing. (Elektron72)
	* PR 323, fixing front/back porch terms, quantities, and use. (Elektron72)
	* PR 349, adding Delete, Insert, End, Page Up, and Page Down keys. (stefan-b-jakobsson)

## Non-Release 38.1 ("Kinkakuji Temple")

* Internals
	* Converted code to C++17, cleaned up all build warnings, save one.
	* Converted build scripts to CMake, but lost Mac along the way. Halp fix pls?
	* Converted rendering to OpenGL.
	* Converted overlay to ImGUI.
	* Unified keyboard-like input and data queues.
	* Restructured memory access to provide real read-only access for the debugger. Also small speed boost.
	* Rewrote the PS2 ports to be lean mean speed machines.
	* Pulled non-VERA code out of VERA code.
* Debugger is now always enabled, the command-line option now requires an address and is used to set breakpoints.

Official X16 Emulator Release Notes
-------------

## Release 38 ("Kyoto")

* CPU
	* added WAI, BBS, BBR, SMB, and RMB instructions [Stephen Horn]
* VERA
	* VERA speed optimizations [Stephen Horn]
	* fixed raster line interrupt [Stephen Horn]
	* added sprite collision interrupt [Stephen Horn]
	* fixed sprite wrapping [Stephen Horn]
	* added VERA dump, fill commands to debugger [Stephen Horn]
	* fixed VRAM memory dump [Stephen Horn]
* SD card
	* SD card write support
	* Ctrl+D/Cmd+D detaches/attaches SD card (for debugging)
	* improved/cleaned up SD card emulation [Frank van den Hoef]
	* SD card activity/error LED support
	* VERA-SPI: support Auto-TX mode
* misc
	* added warp mode (Ctrl+'+'/Cmd+'+' to toggle, or `-warp`)
	* added '-version' shell option [Alice Trillian Osako]
	* new app icon [Stephen Horn]
	* expose 32 bit cycle counter (up to 500 sec) in emulator I/O area
	* zero page register display in debugger [Mike Allison]
	* Various WebAssembly improvements and fixes [Sebastian Voges]


### Release 37 ("Geneva")

* VERA 0.9 register layout [Frank van den Hoef]
* audio [Frank van den Hoef]
    * VERA PCM and PSG audio support
    * YM2151 support is now enabled by default
    * added `-abufs` to specify number of audio buffers
* removed UART [Frank van den Hoef]
* added window icon [Nigel Stewart]
* fixed access to paths with non-ASCII characters on Windows [Serentty]
* SDL HiDPI hint to fix mouse scaling [Edward Kmett]

### Release 36 ("Berlin")

* added VERA UART emulation (`-uart-in`, `-uart-out`)
* correctly emulate missing SD card
* moved host filesystem interface from device 1 to device 8, only available if no SD card is attached
* require numeric argument for `-test` to auto-run test
* fixed JMP (a,x) for 65c02
* Fixed ESC as RUN/STOP [Ingo Hinterding]

### Release 35

* video optimization [Neil Forbes-Richardson]
* added `-geos` to launch GEOS on startup
* added `-test` to launch (graphics) unit test on startup
* debugger
	* switch viewed RAM/ROM bank with `numpad +` and `numpad -` [Kobrasadetin]
	* optimized character printing [Kobrasadetin]
* trace mode:
	* prepend ROM bank to address in trace
	* also prints 16 bit virtual regs (graph/GEOS)
* fixes
	* initialize memory to 0 [Kobrasadetin]
	* fixed SYS hex argument
	* disabled "buffer full, skipping" and SD card debug text, it was too noisy

### Release 34

* PS/2 mouse
* support for text mode with tiles other than 8x8 [Serentty]
* fix: programmatic echo mode control [Mikael O. Bonnier]

### Release 33

* significant performance optimizations
* VERA
	* enabled all 128 sprites
	* correct sprite zdepth
	* support for raster IRQs
* SDL controller support using `-joy1` and `-joy2` [John J Bliss]
* 65C02 BCD fixes [Norman B. Lancaster]
* feature parity with new LOAD/VLOAD features [John-Paul Gignac]
* default RAM and ROM banks are now 0, matching the hardware
* GIF recording can now be controlled from inside the machine [Randall Bohn]
* Debugging
	* Major enhancements to the debugger [kktos]
	* `-echo` will now encode non-printable characters like this: \X93 for CHR$(93), `-bas` as 	well as pasting accepts this convention again
	* `-echo raw` for the original behavior
	* `-echo iso` for correct character encoding in ISO mode
	* `-ram` to specify RAM size; now defaults to 512

### Release 32

* correct ROM banking
* VERA emulation optimizations [Stephen Horn]
* added `-dump` option to allow writing RAM, CPU state or VERA state to disk [Nils Hasenbanck]
* added `-quality` option to change scaling algorithm; now defaults to "best" [Maurizio Porrato]
* output of `-echo` can now be fed into UNIX pipes [Anonymous Maarten]
* relative speed of emulator is shown in the title if host can't keep up [Rien]
* fix: 6502 BCD arithmetic [Rien]
* fix: colors (white is now white) [Rien]
* fix: sprite flipping [jjbliss]

### Release 31

* VERA 0.8 register layout
* removed `-char` (character ROM is now part of `rom.bin`)
* GIF recording using `-gif` [Neil Forbes-Richardson]
* numpad support [Maurizio Porrato]
* fake support of VIA timers to work around BASIC RND(0)
* default ROM is taken from executable's directory [Michael Watters]
* emulator window has a title [Michael Watters]
* `-debug` allows specifying a breakpoint [Frank Buss]
* package contains the ROM symbols in `rom.txt`
* support for VERA SPI

### Release 30

Emulator:
* VERA can now generate VSYNC interrupts
* added `-keymap` for setting the keyboard layout
* added `-scale` for integer scaling of the window [Stephen Horn]
* added `-log` to enable various logging features (can also be enabled at runtime (POKE $9FB0+) [Randall Bohn])
* changed `-run` to be an option to `-prg` and `-bas`
* emulator detection: read $9FBE/$9FBF, must read 0x31 and 0x36
* fix: `-prg` and `-run` no longer corrupt BASIC programs.
* fix: `LOAD,1` into RAM bank [Stephen Horn]
* fix: 2bpp and 4bpp drawing [Stephen Horn]
* fix: 4bpp sprites [MonstersGoBoom]
* fix: build on Linux/ARM

### Release 29

* better keyboard support: if you pretend you have a US keyboard layout when typing, all keys should now be reachable [Paul Robson]
* `-debug` will enable the new debugger [Paul Robson]
* runs at the correct speed (was way too slow on most machines)
* keyboard shortcuts work on Windows/Linux: `Ctrl + F/R/S/V`
* `Ctrl + V` pastes the clipboard as keypresses
* `-bas file.txt` loads a BASIC program in ASCII encoding
* `-echo` prints all BASIC/KERNAL output to the terminal, use it with LIST to convert a BASIC program to ASCII
* `-run` acts like `-prg`, but also autostarts the program
* `JMP $FFFF` and `SYS 65535` exit the emulator and save memory into the host's storage
* the packages now contain the current version of the Programmer's Reference Guide (HTML)
* fix: on Windows, some file load/saves may be been truncated

### Release 28

* support for 65C02 opcodes [Paul Robson]
* keep aspect ratio when resizing window [Sebastian Voges]
* updated sprite logic to VERA 0.7 – **the layout of the sprite data registers has changed, you need to change your code!**


### Release 27

* Command line overhaul. Supports `-rom`, `-char`, `-sdcard` and `-prg`.
* ROM and char filename defaults, so box16 can be started without arguments.
* Host Filesystem Interface supports `LOAD"$"`
* macOS and Windows packaging logic in Makefile

### Release 26

* better sprite support (clipping, palette offset, flipping)
* better border support
* KERNAL can set up interlaced NTSC mode with scaling and borders (compile time option)

### Release 25

* sdcard: fixed `LOAD,x,1` to load to the correct addressg
* sdcard: all temp data will be on bank #255; current bank will remain unchanged
* DOS: support for DOS commands ("UI", "I", "V", ...) and more status messages (e.g. 26,WRITE PROTECT ON,00,00)
* BASIC: `DOS` command. Without argument: print disk status; with "$" argument: show directory; with "8" or "9" argument: switch default drive; otherwise: send DOS command; also accessible through F7/F8
* Vera: cycle exact rendering, NTSC, interlacing, border

### Release 24

* SD card support
	* pass path to SD card image as third argument
	* access SD card as drive 8
	* the local PC/Mac disk is still drive 1
	* modulo debugging, this would work on a real X16 with the SD card (plus level shifters) hooked up to VIA#2PB as described in sdcard.c in the emulator surce

### Release 23

* Updated emulator and ROM to spec 0.6 – the ROM image should work on a real X16 with VERA 0.6 now.

### Release 22

SYS65375 (SWAPPER) now also clears the screen, avoid ing side effects.

### Release 21

* support for $ and % number prefixes in BASIC
* support for C128 KERNAL APIs LKUPLA, LKUPSA and CLOSE_ALL

### Release 20

* Toggle fullscreen using `Cmd + F` or `Cmd + return`
* new BASIC instructions and functions:
	* `MON`: enter monitor; no more SYS65280 required
	* `VPEEK(bank, address)`
	* `VPOKE bank, address, value`
example: `VPOKE4,0,VPEEK(4,0) OR 32` [for 256 color BASIC]

### Release 19

* fixed cursor trail bug
* fixed f7 key in PS/2 driver
* f keys are assigned with shortcuts now:
F1: LIST
F2: &lt;enter monitor&gt;
F3: RUN
F4: &lt;switch 40/80&gt;
F5: LOAD
F6: SAVE"
F7: DOS"$ &lt;doesn't work yet&gt;
F8: DOS &lt;doesn't work yet&gt;

### Release 18

* Fixed scrolling in 40x30 mode when there are double lines on the screen.

### Release 17

* video RAM support in the monitor (SYS65280)
* 40x30 screen support (SYS65375 to toggle)

### Release 16

* Integrated monitor, start with SYS65280
`rom.bin` is now 3*8 KB:
	* 0: BASIC (bank 0 at $C000)
	* 1: KERNAL ($E000)
	* 2: UTIL (bank 1 at $C000)

### Release 15

* correct text mode video RAM layout both in emulator and KERNAL

### Release 14

* KERNAL: fast scrolling
* KERNAL: upper/lower switching using CHR$($0E)/CHR$($8E)
* KERNAL: banking init
* KERNAL: new PS/2 driver
* Emulator: VERA updates (more modes, second data port)
* Emulator: RAM and ROM banks start out as all 1 bits

### Release 13

* Supports mode 7 (8bpp bitmap).

### Release 12

* Supports 8bpp tile mode (mode 4)

### Release 11

* The emulator and the KERNAL now speak the bit-level PS/2 protocol over VIA#2 PA0/PA1. The system behaves the same, but keyboard input in the ROM should work on a real device.

### Release 10

updated KERNAL with proper power-on message

### Release 9

* LOAD and SAVE commands are intercepted by the emulator, can be used to access local file system, like this:

      LOAD"TETRIS.PRG
      SAVE"TETRIS.PRG

* No device number is necessary. Loading absolute works like this:

      LOAD"FILE.PRG",1,1

### Release 8

* New optional override load address for PRG files:

      ./x64emu rom.bin chargen.bin basic.prg,0401

### Release 7

* Now with banking. `POKE40801,n` to switch the RAM bank at $A000. `POKE40800,n` to switch the ROM bank at $C000. The ROM file at the command line can be up to 72 KB now (layout: 0: bank 0, 1: KERNAL, 2: bank 1, 3: bank 2 etc.), and the RAM that `Cmd + S` saves is 2088KB ($0000-$9F00: regular RAM, $9F00-$9FFF: unused, $A000+: extra banks)

### Release 6

* Vera emulation now matches the complete spec dated 2019-07-06: correct video address space layout, palette format, redefinable character set

### Release 5

* BASIC now starts at $0401 (39679 BASIC BYTES FREE)

### Release 4

* `Cmd + S` now saves all of memory (linear 64 KB for now, including ROM) to `memory.bin`, `memory-1.bin`, `memory-2.bin`, etc. You can extract parts of it with Unix "dd", like: `dd if=memory.bin of=basic.bin bs=1 skip=2049 count=38655`

### Release 3

* Supports PRG file as third argument, which is injected after "READY.", so BASIC programs work as well.

### Release 2

* STOP key support

### Release 1

* 6502 core, fake PS/2 keyboard emulation (PS/2 data bytes appear at VIA#1 PB) and text mode Vera emulation
* KERNAL/BASIC modified for memory layout, missing VIC, Vera text mode and PS/2 keyboard
