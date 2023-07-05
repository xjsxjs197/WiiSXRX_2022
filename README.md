![WiiStation logo](https://github.com/xjsxjs197/WiiSXRX_2022/raw/main/logo.png)

# WiiStation

WiiStation (formely WiiSXRX_2022), is a Sony PlayStation 1 (PS1/PSX/PSone) emulator, forked from the original WiiSX-RX (http://github.com/niuus/WiiSXRX) emulator by NiuuS, originally a port of PCSX-Reloaded, but with many changes from PCSX-ReARMed, for the Nintendo Wii/Wii U.

## The following changes have been made to the code based on WiiSXRX.

* Incorporating the CDROM and CDRISO codes from PCSX-ReARMed, the compatibility of the system has been greatly improved.
  Many games that could not be run or had problems before can be run now.

* CDDA (Compact Disc Digital Audio) tracks & multi-tracks support.

* Incorporating the timer codes from PCSX-ReARMed.

* Combined the DFSound module from PCSX-ReARMed and used the SDL library.
  The sound quality of the system has been greatly improved.

* Replacing the old PowerPC (PPC) dynarec with the new dynamic recompiler [Lightrec](https://github.com/pcercuei/lightrec) by pcercuei, the speed/performance of the emulation is greatly improved.

* Support for multiple languages.
  At first, I wanted to refer to Snes9x GX and support TTF font library.
  However, it encountered a memory leak problem, resulting in automatic exit.
  So it can only be made into a specific font.
  Font char information: first two byte: BigEndianUnicode char code, followed by a character picture in IA8 format with a size of 24 * 24.

* For some customized Chinese culture games, specific BIOS is automatically loaded.
  For example:  sd:\wiisxrx\isos\武藏传.ISO => sd:\wiisxrx\bios\武藏传.bin

* Other minor corrections, such as disc changing (swap) and automatic fixes (autoFix functions) for some games.

  ※※※ Note: It reads a font file in a fixed location, so make sure that [sd:/wiisxrx/fonts/chs.dat] exists ※※※

## Old changes (before Lightrec)

* ̶M̶o̶d̶i̶f̶i̶c̶a̶t̶i̶o̶n̶ ̶o̶f̶ ̶s̶o̶m̶e̶ ̶d̶y̶n̶a̶m̶i̶c̶ ̶c̶o̶m̶p̶i̶l̶a̶t̶i̶o̶n̶ ̶i̶n̶s̶t̶r̶u̶c̶t̶i̶o̶n̶s̶,̶ ̶s̶u̶c̶h̶ ̶a̶s̶ ̶S̶L̶L̶V̶,̶ ̶S̶R̶L̶V̶,̶ ̶S̶R̶A̶V̶,̶ ̶F̶i̶n̶a̶l̶ ̶F̶a̶n̶t̶a̶s̶y̶ ̶9̶ ̶a̶n̶d̶ ̶B̶i̶o̶h̶a̶z̶a̶r̶d̶ ̶3̶ ̶(̶R̶e̶s̶i̶d̶e̶n̶t̶ ̶E̶v̶i̶l̶ ̶3̶)̶ ̶c̶a̶n̶ ̶b̶e̶ ̶r̶u̶n̶.̶
̶ ̶ ̶(̶P̶a̶r̶t̶ ̶o̶f̶ ̶t̶h̶e̶ ̶d̶i̶v̶i̶s̶i̶o̶n̶ ̶i̶n̶s̶t̶r̶u̶c̶t̶i̶o̶n̶ ̶u̶s̶e̶s̶ ̶a̶ ̶s̶t̶a̶t̶i̶c̶ ̶c̶o̶m̶p̶i̶l̶a̶t̶i̶o̶n̶ ̶i̶n̶s̶t̶r̶u̶c̶t̶i̶o̶n̶)̶

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

240p support by Jokkipo - http://github.com/Jokippo

WiiStation icon - made by Dakangel (high quality logo made by saulfabreg)

WiiSX-RX fork - developed by NiuuS - https://github.com/niuus/WiiSXRX

WiiSX-R fork - developed by Mystro256 - https://github.com/Mystro256/WiiSXR

PCSX-Revolution - developed by Firnis - https://code.google.com/archive/p/pcsx-revolution/downloads ; https://github.com/Firnis/pcsx-revolution

WiiSX - developed by emu_kidid, tehpola, sepp256 - https://github.com/emukidid/pcsxgc ; https://code.google.com/archive/p/pcsxgc/downloads

PCSX-ReARMed - developed by notaz - https://github.com/notaz/pcsx_rearmed

libOGC2 - developed by Extrems - https://github.com/extremscorner/libogc2

Lightrec - developed by pcercuei - https://github.com/pcercuei/lightrec

Thanks for everyone's attention and enthusiasm, which gives me the motivation to continue this project.
