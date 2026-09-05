# Tiny386

## Introduction
Tiny386 is an x86 PC emulator written in C99. The highlight of the project is its portability. It now boots Windows 9x/NT on MCU such as ESP32-S3.

The core of the project is a built-from-scratch, simple and stupid i386 cpu emulator. Some features are missing, e.g. debugging, hardware tasking and some permission checks, but it should be able to run most 16/32 bit software. To boot modern linux kernel and windows, some 486 and 586 instrutions are added. The cpu emulator is kept in ~6K LOC. There is also an optional x87 fpu emulator.

To assemble a complete PC system, we have ported many peripherals from TinyEMU and QEMU, it now includes:
 - 8259 PIC
 - 8254 PIT
 - 8042 Keyboard Controller
 - CMOS RTC
 - ISA VGA with Bochs VBE
 - IDE Disk Controller
 - NE2000 ISA Network Card
 - 8257 ISA DMA
 - PC Speaker
 - Adlib OPL2 (optional)
 - SoundBlaster 16

For firmware, the BIOS/VGABIOS comes from seabios. Tiny386 also supports booting linux kernel directly, without traditional BIOS. The idea comes from JSLinux, and it uses a small stub code called linuxstart.

Tiny386 now supports i686, MMX, SSE (up to SSSE3) and amd64 (64bit long mode only). As these features are optional, they may be safely disregarded if not required.

## Demo
See [here](https://hchunhui.github.io/tiny386)

## Build

The project is designed with simplicity in mind. Just run `gcc -O3 *.c -o tiny386_headless -lm`, and you will get a "headless" executable without any optional features.

To build with display and sound:
- Linux (with rawdraw): Install `libslirp`, `libx11`, and `libasound2` first, then run `make`.
- Linux (with SDL): Install `libslirp` `SDL1.2` (or `sdl12-compat`) first, then run `make USE_SDL=y`.
- Windows: Install `mingw-w64` first, then run `make win32`.
- macOS (with SDL): Install `libslirp` and `SDL1.2` from Homebrew first,
  then run `make USE_SDL=y SLIRP_INC="-I/opt/homebrew/include" SLIRP_LIB="$(pkg-config --libs slirp)"`.
- WebAssembly: Install `clang` first, then run `cd wasm; make`.

For details, please refer to `.github/workflows/build.yml` and `Makefile`.

Pre-built binaries: [here](https://github.com/hchunhui/tiny386/releases)

## Usage

- Prepare an ini file
```ini
[pc]
; set path to BIOS and VGA BIOS
bios = bios.bin
vga_bios = vgabios.bin

; set memory size and VGA memory size
mem_size = 32M
vga_mem_size = 2M

; fda/fdb for floppy disks (optional)
fda = floppy.img

; hda/hdb/hdc/hdd for hard disks (optional)
; cda/cdb/cdc/cdd for CD-ROM disks (optional)
hda = win95.img
cdb = win95_cd.iso

; "fill_cmos" fixes "MS-DOS compatibility mode" in win9x, but it breaks winNT...
fill_cmos = 1

; force 8-dot mode (640 pixel wide in text mode) if set to 1
vga_force_8dm = 0

[display]
width = 720
height = 480

[cpu]
; gen = 3/4/5/6, for 386/486/586/686
gen = 3
; fpu = 0/1, to disable/enable x87
fpu = 0
```
- Run
```sh
./tiny386 config.ini
./tiny386 -kvm config.ini  # run with KVM (build with `make USE_CPUABS=y`)
```

For rawdraw and SDL port:
Press "Ctrl + ]" to grab/ungrab the keyboard and mouse. Press "Ctrl + [" to show/hide OSD (On Screen Display). In OSD mode, the floppy/CD-ROM disk can be changed on the fly.

## ESP32 port
Supported boards:

With ESP-IDF 5.2.x:
- JC3248W535 (ESP32-S3, 480x320)
- [Elecrow CrowPanel Advance 7.0" HMI](https://github.com/Elecrow-RD/CrowPanel-Advance-HMI-ESP32-AI-Display) (ESP32-S3, 800x480)

With ESP-IDF 6.0.x (experimental):
- JC4880P443 (ESP32-P4 Rev1.3 360MHz, 800x480)

### Build and Flash
You can find the pre-built flash image `esp/flash_image_JC3248W535.bin` from [here](https://github.com/hchunhui/tiny386/releases).
The pre-built image can be flashed directly to offset 0.

Online flasher for esp chips: https://espressif.github.io/esptool-js

To build and flash manually:
```sh
scripts/build.sh patch_idf  # apply patches to ESP-IDF 5.2.x
#scripts/build.sh patch_idf_60  # apply patches to ESP-IDF 6.0.x
make prepare
cd esp
idf.py -DBOARD=jc3248w535 update-dependencies build  # or -DBOARD=elecrow7s3
idf.py flash
```

### Configure
All files should be put in a SD card with FAT/exFAT file system. The ini file should be `tiny386.ini` and put in the root directory.
Please refer to `esp/tiny386.ini`.

Alternative usage: `bios.bin` `vgabios.bin` `vmlinux.bin` and `linuxstart.bin` can be put in corresponding flash partition. Other files can be put in the `storage` flash partition. Please refer to `esp/partition.csv`.

### Keyboard/Mouse Input

- Forward over WIFI

`wifikbd` is used to forward keyboard/mouse events to the dev board over WIFI:
```
(ESP32-S3 board: listen on TCP port 9999) <--- WIFI ---> AP <--- WIFI/Wire ---> (PC: ./wifikbd esp_board_addr 9999)
```

- USB hid (experimental)

Add the following config:
```
[esp]
enable_usb = 1
```

Note: DO NOT enable USB and WIFI at the same time on ESP32-S3, due to insufficient memory.

More info, see [here](https://github.com/hchunhui/tiny386/pull/4).

### Windows XP on ESP32-S3 (SD card + RAM paging)

Runs a 16MB Windows XP guest on ESP32-S3 by paging guest RAM to a swap
file on the SD card. Validated on PC builds (16MB guest, 6MB resident
window, boots to idle with zero faults); the ESP target uses the same
code paths with the window sized from actual PSRAM.

#### Guest requirements

- Windows XP with the Standard PC HAL. The bundled SeaBIOS is built
  with `CONFIG_ACPI` unset, so it exposes no ACPI tables and an
  ACPI-HAL image bugchecks during init. Convert in QEMU
  (Device Manager -> Computer -> Update Driver -> "Standard PC"),
  then shut down cleanly.
- 16MB RAM (`mem_size = 16M`, checked with `qemu-system-i386 -m 16M`),
  `gen = 5`, `fpu = 1`, `fill_cmos = 0`, `vga_mem_size = 320K`.

#### Host constraints (S3)

- 240MHz LX7, 512KB internal SRAM, 8 or 16MB octal PSRAM. 16MB of
  guest RAM does not fit alongside the emulator, VGA memory (320KB),
  BIOS images (~170KB) and the frame buffer (640x480x2 = 600KB).
- External SPI SD module for the disk image and the swap file.

#### Pager design (`swap.c`, `swap.h`)

- Page size 4096. Resident window of N frames in host RAM, backing
  file holds up to `mem_size` bytes at offset `page * 4096`.
- The low 2MB is pinned (identity-mapped prefix): firmware, BIOS data
  and boot code never touch the SD card.
- Clean pages without a backing copy read back as zeros; the swap file
  needs no preallocation. Out-of-range reads return zeros, writes are
  discarded (same as flat-RAM behavior).
- Eviction is clock (second chance); use bits are fed on every
  translation, including TLB hits. Only host-dirty pages are written
  back; guest Accessed-bit updates are intentionally not persisted.
- All guest-physical access paths go through it: CPU loads/stores, TLB
  refill (page-table reads, guest dirty-bit writeback via tracked PTE
  pages), fetch-cache guards against cross-page reads, page-clamped
  string I/O with pinning, page-chunked ISA DMA (`i8257.c`).
  Eviction invalidates TLB entries, stale PTE pointers and the fetch
  cache (`cpui386_swap_invalidate`).

#### CPU changes (`i386.c`)

- Fault during exception/interrupt delivery raises double fault; fault
  during double fault delivery resets the CPU (triple fault). Delivery
  paths return failure instead of aborting, so guest handlers run.
- Fixed multi-byte stores bypassing the internal-RAM hot mirror on
  ESP32 (`hot_mirror_store`) and a self-referential `IRAM_ATTR`
  definition that broke non-ESP builds.

#### ESP integration

- `esp/main/board_s3devkit.h`: headless 640x480 target (`USE_LCD_HEADLESS`,
  `lcd_headless.c`), SPI SD pins (`SD_SPI_MOSI/MISO/SCK/CS`, adjust to
  wiring), USB HID input (`enable_usb = 1`, never together with WiFi).
- `esp/tiny386_xp.ini`: the 16MB guest config. New ini keys
  `swap_size` (resident window, 0 = auto) and `swap_file`.
- `swap_size = 0` auto-enables paging only when the guest does not fit:
  `window = psram - fb - vga_mem - 2MB`, floored at 2.5MB. Guests that
  fit run fully in PSRAM as before. Boot log prints the numbers.

#### Validation (PC builds)

- Pager unit test: 20000 random reads/writes over 64 pages in an
  8-frame window vs a flat reference model, plus pin and
  out-of-range cases. Passes.
- Forced swap (16MB guest, 6MB window): boots to idle (~90s on the
  test machine, ~34% CPU at idle), zero aborts/faults, 16MB swap file
  fully exercised.
- Baseline (128MB guest, no swap): same boot milestones, zero faults.

#### Operation

- SD card (FAT32/exFAT): `tiny386.ini` (copy of `esp/tiny386_xp.ini`),
  `bios.bin`, `vgabios.bin`, `xp.img`. `xpswap.bin` is created
  automatically. Use a spare card; paging wears flash.
- Build: `idf.py -DBOARD=s3devkit build` (ESP-IDF 5.2, after
  `scripts/build.sh patch_idf` and `make prepare`).
- Limits: expect hours to boot on 8MB PSRAM over SPI SD (each 4KB
  swap costs milliseconds; the boot working set exceeds the window).
  16MB PSRAM or SDMMC 4-bit wiring reduces this proportionally. No
  display on the devkit build; progress goes over serial logs.

## Troubleshooting

### "0 bytes of memory" during Windows 95 setup
Use "setup /im" to bypass memory check.

### "protection error" during Windows 95 startup
Use [patcher9x](https://github.com/JHRobotics/patcher9x).

### NE2000 doesn't work
Manually set the IRQ to 9(or 2).

### freeze during Windows NT4/2000/XP startup
Use `fill_cmos = 0` in the config ini file.

## License
The cpu emulator and the project as a whole are both licensed under the BSD-3-Clause license.

Adlib emulation is an optional part of the project, and it requires the library fmopl which is licensed under the LGPL.
Use `make USE_FMOPL=n` to build without adlib emulation.

SeaBIOS is distributed under the GNU LGPL-3 license.

Some parts ported from QEMU/TinyEMU are under the MIT license.
