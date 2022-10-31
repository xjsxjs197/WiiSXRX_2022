# WiiStation

![WiiStation logo](https://github.com/xjsxjs197/WiiSXRX_2022/raw/main/logo.jpg)

WiiStation (formely WiiSXRX_2022), is a Sony PlayStation 1 (PS1/PSX/PSone) emulator, forked from the original WiiSX-RX (http://github.com/niuus/WiiSXRX) emulator by NiuuS, originally a port of PCSX-Reloaded, but with many changes from PCSX-ReARMed, for the Nintendo Wii/Wii U.

## The following changes have been made to the code based on WiiSXRX.

* Incorporating the CDROM and cdiso codes of PCSX-ReARMed, the compatibility of the system has been greatly improved.
  Many games that could not be run or had problems before can be run.

* Cdda support.

* Incorporating the timer codes of PCSX-ReARMed.

* Combined the dfsound module of PCSX-ReARMed and used the SDL library.
  The sound quality of the system has been greatly improved.

* Modification of some dynamic compilation instructions, such as sllv, SRLV, srav, Final Fantasy 9 and Biohazard 3 can be run.
  (Part of the division instruction uses a static compilation instruction)

* Support for multiple languages.
  At first, I wanted to refer to Snes9x GX and support TTF font library.
  However, it encountered a memory leak problem, resulting in automatic exit.
  So it can only be made into a specific font.
  Font char information: first two byte: BigEndianUnicode char code, followed by a character picture in IA8 format with a size of 24 * 24.

* For some customed Chinese culture games, specific BIOS is automatically loaded.
  For example:  sd:\wiisxrx\isos\武藏传.ISO => sd:\wiisxrx\bios\武藏传.bin

* Other minor corrections, such as disc changing (swap) and automatic fixed of some games.

  ※※※ Note: It reads a font file in a fixed location, so make sure that [sd:/wiisxrx/fonts/chs.dat] exists ※※※

## Goals

* Improve GTE code to provide 3D game speed.
  Although I used paired single instruction, but the speed is basically not improved

* Use the display mode of GL to provide image quality and performance.
  I don't know anything about OpenGL, and I don't know if I can use grrlib.

* DualShock 3, DualShock 4 and DualShock 5 controller support.

* Possibility to select other BIOS with some basic buttons.

* 240p support.

Any help is appreciated.

## Compilation information

* devkitPPC r29 + libOGC 1.8.16 + SDL

  You can download everything here: https://wii.leseratte10.de/devkitPro/

  The compiled SDL is here: https://github.com/xjsxjs197/WiiSXRX_2022/raw/main/libSDL.a

## WiiStation Credits

WiiStation (formely WiiSXRX_2022) - developed by xjsxjs197 - https://github.com/xjsxjs197/WiiSXRX_2022

WiiStation icon - made by Dakangel (high quality logo made by saulfabreg)

WiiSX-RX fork - developed by NiuuS - https://github.com/niuus/WiiSXRX

WiiSX-R fork - developed by Mystro256 - https://github.com/Mystro256/WiiSXR

PCSX-Revolution - developed by Firnis - https://code.google.com/archive/p/pcsx-revolution/downloads

WiiSX - developed by emu_kidid, tehpola, sepp256 - https://code.google.com/archive/p/pcsxgc/downloads ; https://github.com/emukidid/pcsxgc

PCSX-ReARMed - developed by notaz - https://github.com/notaz/pcsx_rearmed

Thanks for everyone's attention and enthusiasm, which gives me the motivation to continue this project.
