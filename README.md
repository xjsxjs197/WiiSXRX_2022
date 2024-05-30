![WiiStation logo](https://github.com/xjsxjs197/WiiSXRX_2022/raw/main/logo.png)

# WiiStation

WiiStation (formerly WiiSXRX_2022), is a Sony PlayStation 1 (PS1/PSX/PSone) emulator, forked from the original WiiSX-RX (https://github.com/niuus/WiiSXRX) emulator by NiuuS, originally a port of PCSX-Reloaded, but with many changes from PCSX-ReARMed, for the Nintendo Wii/Wii U.

## The following changes have been made to the code based on WiiSXRX.

* HID controllers support!

  Supported controllers referred to [Nintendont](https://github.com/FIX94/Nintendont).

  If you want to add controllers beyond what Nintendont supports, you need to add a new INI configuration file. The process should be as follows:
  
  Connect your HID controller to the Wii, and then start **HID_Test.dol** (please modify it to **boot.dol**). If the controller is supported, you will see normal connection information and information when each button is pressed.
  Send me the information when each button is pressed, and I will create an INI file. Or, refer to 0810_0003.ini to create your INI file.
  You can refer to the information in this link:
  https://gbatemp.net/threads/nintendont-custom-controller-configuration-file.633068/#post-10164777

* Incorporating the latest CDROM and CDRISO codes from PCSX-ReARMed, the compatibility of the system has been greatly improved.
  Many games that could not be run or had problems before can be run now.

* CDDA (Compact Disc Digital Audio) tracks & multi-tracks support.

* Incorporating the timer (system timings emulation) codes from PCSX-ReARMed.

* Combined the latest DFSound module from PCSX-ReARMed and used the SDL library.
  The sound quality of the system has been greatly improved.

* Adding the new, updated PSX dynamic recompiler [Lightrec](https://github.com/pcercuei/lightrec) by pcercuei, the speed/performance of the emulation is greatly improved. The 'new' PPC Dynarec is kept as an option in case compatibility or speed changes much.

* Posibility of selecting between the old video plugin P.E.Op.S. Soft GPU and the new video plugin based on DFXVideo.

* 240p support!

* Interlace mode support - renders the games to a resolution close to the real PSX hardware (480i mode), which gives full speed to 480i games!

* Posibility of enabling/disabling bilinear, trap, and deflicker filters!

* PS1 Lightguns support! Both Namco GunCon and Konami Justifier lightguns are supported and emulated with the Wiimotes! (Needs to be enabled in emulator settings and calibrate them by using the in-game calibration screen)

* Experimental PS1 Mouse support via the Wiimote IR.

* PS1 Multitap support! With options for enabling them on both Port 1 and Port 2, supports up to 8 players. (Needs to be enabled in emulator settings and controllers need to be set for use the Multitap adequately)

* Support for BIN+CUE, ISO, IMG, and eboot PBP (a compressed format for PS1 games on PSP) formats.

* CHD v1-v5 compressed format support with the [libCHDr](https://github.com/rtissera/libchdr) library from [MAME](https://github.com/mamedev/mame).

* Support for multiple languages.
  At first, I wanted to refer to Snes9x GX and support TTF font library.
  However, it encountered a memory leak problem, resulting in automatic exit.
  So it can only be made into a specific font.
  Font char information: first two byte: BigEndianUnicode char code, followed by a character picture in IA8 format with a size of 24 * 24.

* For some customized Chinese culture games, specific BIOS is automatically loaded.
  For example:  sd:\wiisxrx\isos\武藏传.ISO => sd:\wiisxrx\bios\武藏传.bin

* Other minor corrections, such as disc changing (swap) and automatic fixes (autoFix functions) for some games.

  ※※※ Note: It reads a font file in a fixed location, so make sure that [sd:/wiisxrx/fonts/chs.dat] exists ※※※

## Changes on 'new' PPC Dynarec:

* Modification of some dynamic compilation instructions on the PPC Dynarec, such as SLLV, SRLV, SRAV, Final Fantasy 9 and Biohazard 3 (Resident Evil 3) can be run.
(Part of the division instruction uses a static compilation instruction)

* Emulation of more instructions, such BREAK and SYSCALL, makes more games to be playable with the PPC Dynarec (ex., EA Sports F1 2000).

* Many other emulation, speed and events improvements, some of them courtesy of PCSX-ReARMed.

## Goals

* Improve GTE (Geometry Transformation Engine) code to provide 3D game speed.
  By using the paired single instruction, most of the GTE logic has been rewritten, and FPS has indeed improved by about 2 frames.
  However, due to accuracy issues, there may be minor image/graphical issues.

* Further improvement of HID controller support via USB.
  By reading and checking the code of [Nintendont](https://github.com/FIX94/Nintendont), we have understood the working principle of the HID controller,
  but it is still a little bit short to port Nintendont's HID control logic to WiiStation.

* Use the graphics display mode of GL to provide image quality and performance.
  (At least transplant the texture caching logic in OpenGL, it may improve running efficiency/performance.)

Any help is appreciated.

## Compilation information

* devkitPPC r41-2 + libOGC2 (until git [7456c4ab](https://github.com/extremscorner/libogc2/commit/7456c4abf3e8e8ccd7eac7bb7cbe808128befa55)) + SDL + GNU Lightning + Lightrec

  You can download devkitPPC r41-2 (and older versions) here: https://wii.leseratte10.de/devkitPro/

  The modified devkitPPC base rules and the compiled libOCG2, SDL, GNU Lightning and Lightrec libraries are here: https://github.com/xjsxjs197/WiiSXRX_2022/raw/main/lightrec+Libogc2.zip

  The compiled SDL is here: https://github.com/xjsxjs197/WiiSXRX_2022/raw/main/libSDL.a

## WiiStation Credits

WiiStation (formerly WiiSXRX_2022) - developed by xjsxjs197 - https://github.com/xjsxjs197/WiiSXRX_2022

240p/Lightgun/Mouse/Multitap support, some fixes and additional improvements by Jokippo - http://github.com/Jokippo

WiiStation icon - made by Dakangel (high quality logo made by saulfabreg)

WiiSX-RX fork - developed by NiuuS - https://github.com/NiuuS/WiiSXRX

WiiSX-R fork - developed by Mystro256 - https://github.com/Mystro256/WiiSXR

PCSX-Revolution - developed by Firnis - https://code.google.com/archive/p/pcsx-revolution/downloads ; https://github.com/Firnis/pcsx-revolution

WiiSX - developed by emu_kidid, tehpola, sepp256 - https://github.com/emukidid/PCSXGC ; https://code.google.com/archive/p/pcsxgc/downloads

PCSX-ReARMed - developed by notaz - https://github.com/notaz/pcsx_rearmed ; some other changes by Libretro - https://github.com/libretro/pcsx_rearmed

libOGC2 - developed by Extrems - https://github.com/extremscorner/libogc2

Lightrec - developed by pcercuei - https://github.com/pcercuei/lightrec

libCHDr - developed by MAME Team and rtissera - https://github.com/rtissera/libchdr

Thanks for everyone's attention and enthusiasm, which gives me the motivation to continue this project.
