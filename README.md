# WiiStation

WiiStation (formely WiiSXRX_2022), is a Sony PlayStation 1 (PS1/PSX/PSone) emulator, forked from the original WiiSX-RX (http://github.com/niuus/WiiSXRX) emulator by NiuuS, originally a port of PCSX-Reloaded, but with many changes from PCSX-ReARMed, for the Nintendo Wii/Wii U.

## The following changes have been made to the code based on WiiSXRX.

* Incorporating the CDROM and cdiso codes of PCSX-ReARMed, the compatibility of the system has been greatly improved.
  Many games that could not be run or had problems before can be run.

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

## The following is the basic code information

![WiiSXRX logo](./logo.jpg)

Fork of WiiSXR (a port of PCSX-R), a PSX emulator for the Gamecube / Wii / Wii U.

The starting point for this code base will be Mystro256's WiiSXR, a continuation of
daxtsu's libwupc mod of wiisx, which is in turn based off of Matguitarist's "USB mod5".

* Please see the following link for details:
http://www.gc-forever.com/forums/viewtopic.php?t=2524

* WiiSX is GNU GPL and the source can be found here:
https://code.google.com/archive/p/pcsxgc/downloads

* WiiSXR is GNU GPL and the source can be found here:
https://github.com/Mystro256/wiisxr

* libwupc and libwiidrc are also GPL, which can be found here:
https://github.com/FIX94/libwupc
https://github.com/FIX94/libwiidrc


## Downloads

All downloads can be found here:

https://github.com/niuus/wiisxrx/releases

## Reporting Bugs

Feel free to report bugs, but if you can, please test pcsxr first, to eliminate redundant bugs. If it's not a bug in pcsxr, but a bug here, report it here. I would hope this can be as aligned with pcsxr as possible, so any bugs in pcsxr will be inherited unfortunately. If it is a bug in pcsxr, feel free to report bugs with pcsxr... Please note that i am not affiliated with them, so mentioning me or WiiSXRX is unnecessary and unadvised to avoid confusion.

As well, i can't guarantee this project will be a success, unless i can get some help! So if you have any programming skill, feel free to collaborate and check the Goals section!

## Goals
(some taken from Mystro256's original readme)

- Fix gcc build warnings (see build.log for details). Not sure how much the punned pointers will affect optimization, but no warnings is always better than any at all IMHO.
- Update with any code from pcsxr (take as much as possible from pcsxr development (http://pcsxr.codeplex.com).
- Improve plugins (perhaps replace them?)... e.g. cdrmooby28 has some optimization and possible memory issues. As well, maybe an opengl plugin can be ported to gx (with the help of something like gl2gx, WIP see gxrender branch), and a sound plugin with the help of a SDL layer (or ported?).
------------------------------------------
- Xbox 360 and USB HID controller support.
- DualShock 3, DualShock 4 and DualShock 5 controller support.
- Ability to take screenshots like Snes9x RX.
- Possibility to select other BIOS with some basic buttons.
- 240p support.
- CD-DA support.
- CHD, ECM, PBP compressed file support.
- PS1 multitap support.

Any help is appreciated.

# DISCLAIMER: Please do not report issues with specific games, as they may or not be fixed with updates to the code later in the future.

## Credits:
Original WiiSX team:
tehpola, sepp256, emu_kidid

Original WiiSXR coder:
Mystro256

Mods & updates:
Matguitarist, daxtsu, Mystro256, FIX94, NiuuS, xjsxjs197
