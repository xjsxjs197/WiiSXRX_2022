![WiiStation logo](https://github.com/xjsxjs197/WiiSXRX_2022/raw/main/logo.png)

# WiiStation

WiiStation (formely WiiSXRX_2022), is a Sony PlayStation 1 (PS1/PSX/PSone) emulator, forked from the original WiiSX-RX (http://github.com/niuus/WiiSXRX) emulator by NiuuS, originally a port of PCSX-Reloaded, but with many changes from PCSX-ReARMed, for the Nintendo Wii/Wii U.

## The following changes have been made to the code based on WiiSXRX.

* Incorporating the latest CDROM and CDRISO codes from PCSX-ReARMed, the compatibility of the system has been greatly improved.
  Many games that could not be run or had problems before can be run now.

* CDDA (Compact Disc Digital Audio) tracks & multi-tracks support.

* Incorporating the timer (system timings emulation) codes from PCSX-ReARMed.

* Combined the latest DFSound module from PCSX-ReARMed and used the SDL library.
  The sound quality of the system has been greatly improved.

* Adding the new, updated PSX dynamic recompiler [Lightrec](https://github.com/pcercuei/lightrec) by pcercuei, the speed/performance of the emulation is greatly improved. The original old (and fixed/improved) PPC dynarec is kept as an option in case compatibility or speed changes much.

* 240p support!

* Interlace mode support - renders the games to a resolution close to the real PSX hardware (480i mode), which gives full speed to 480i games!

* Posibility of enabling/disabling bilinear, trap, and deflicker filters!

* PS1 Lightguns support! Both Namco GunCon and Konami Justifier lightguns are supported and emulated with the Wiimotes! (Needs to be enabled in emulator settings and calibrate them by using the in-game calibration screen)

* Experimental PS1 Mouse support via the Wiimote IR.

* PS1 Multitap support! With options for enabling them on both Port 1 and Port 2, supports up to 8 players. (Needs to be enabled in emulator settings and controllers need to be set for use the Multitap adequately)

* Support for BIN+CUE, ISO, IMG, and eboot PBP (a compressed format for PS1 games on PSP) formats.

* CHD v1-v5 compressed format support with the [libchdr](https://github.com/rtissera/libchdr) library from [MAME](https://github.com/mamedev/mame).

* Support for multiple languages.
  At first, I wanted to refer to Snes9x GX and support TTF font library.
  However, it encountered a memory leak problem, resulting in automatic exit.
  So it can only be made into a specific font.
  Font char information: first two byte: BigEndianUnicode char code, followed by a character picture in IA8 format with a size of 24 * 24.

* For some customized Chinese culture games, specific BIOS is automatically loaded.
  For example:  sd:\wiisxrx\isos\武藏传.ISO => sd:\wiisxrx\bios\武藏传.bin

* Other minor corrections, such as disc changing (swap) and automatic fixes (autoFix functions) for some games.

  ※※※ Note: It reads a font file in a fixed location, so make sure that [sd:/wiisxrx/fonts/chs.dat] exists ※※※

## Changes on old PPC dynarec:

* Modification of some dynamic compilation instructions on the old PPC dynarec, such as SLLV, SRLV, SRAV, Final Fantasy 9 and Biohazard 3 (Resident Evil 3) can be run.
(Part of the division instruction uses a static compilation instruction)

## Goals

(some taken from NiuuS' WiiSXRX readme)

* Improve GTE (Geometry Transformation Engine) code to provide 3D game speed.
  Although I used paired single instruction, the speed is basically not improved.

* Use the graphics display mode of GL to provide image quality and performance.
  I don't know anything about OpenGL, and I don't know if I can use grrlib.

* DualShock 3, DualShock 4 and DualShock 5 controller support.

* Possibility to select other BIOS with some basic buttons.

Any help is appreciated.

## Compilation information

* devkitPPC r41-2 + libOGC2 (until git [7456c4ab](https://github.com/extremscorner/libogc2/commit/7456c4abf3e8e8ccd7eac7bb7cbe808128befa55)) + SDL + GNU Lightning + Lightrec

  You can download devkitPPC r41-2 (and older versions) here: https://wii.leseratte10.de/devkitPro/

  The modified devkitPPC base rules and the compiled libOCG2, SDL, GNU Lightning and Lightrec libraries are here: https://github.com/xjsxjs197/WiiSXRX_2022/raw/main/lightrec+Libogc2.zip

  The compiled SDL is here: https://github.com/xjsxjs197/WiiSXRX_2022/raw/main/libSDL.a

## WiiStation Credits

WiiStation (formely WiiSXRX_2022) - developed by xjsxjs197 - https://github.com/xjsxjs197/WiiSXRX_2022

240p/Lightgun/Mouse/Multitap support, some fixes and additional improvements by Jokippo - http://github.com/Jokippo

WiiStation icon - made by Dakangel (high quality logo made by saulfabreg)

WiiSX-RX fork - developed by NiuuS - https://github.com/niuus/WiiSXRX

WiiSX-R fork - developed by Mystro256 - https://github.com/Mystro256/WiiSXR

PCSX-Revolution - developed by Firnis - https://code.google.com/archive/p/pcsx-revolution/downloads ; https://github.com/Firnis/pcsx-revolution

WiiSX - developed by emu_kidid, tehpola, sepp256 - https://github.com/emukidid/pcsxgc ; https://code.google.com/archive/p/pcsxgc/downloads

PCSX-ReARMed - developed by notaz - https://github.com/notaz/pcsx_rearmed ; some other changes by Libretro - https://github.com/libretro/pcsx_rearmed

libOGC2 - developed by Extrems - https://github.com/extremscorner/libogc2

Lightrec - developed by pcercuei - https://github.com/pcercuei/lightrec

libchdr - developed by MAME Team and rtissera - https://github.com/rtissera/libchdr

Thanks for everyone's attention and enthusiasm, which gives me the motivation to continue this project.
