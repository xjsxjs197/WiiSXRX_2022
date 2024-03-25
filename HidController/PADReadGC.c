/***************************************************************************
 *   Copyright (C) 2024 WiiStation                                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA.           *
 ***************************************************************************/

#include <stdio.h>
#include "global.h"
#include "KernelHID.h"
#include "wiidrc.h"
#include "../Gamecube/DEBUG.h"

#define PAD_CHAN0_BIT                0x80000000


static u32 stubsize = 0x1800;
static vu32 *stubdest = (vu32*)0x80004000;
static vu32 *stubsrc = (vu32*)0x93011810;
static vu16* const _memReg = (vu16*)0xCC004000;
static vu16* const _dspReg = (vu16*)0xCC005000;
static vu32* const _siReg = (vu32*)0xCD006400;
static vu32* const MotorCommand = (vu32*)0x93003010;
static vu32* HID_STATUS = (vu32*)0x93003440;
static vu32* HID_CHANGE = (vu32*)0x93003444;
static vu32* HID_CFG_SIZE = (vu32*)0x93003448;
static vu32* HID_CFG_FILE = (vu32*)0x93003460;

static vu32* HIDMotor = (vu32*)0x93003020;
static vu32* PadUsed = (vu32*)0x93003024;

static vu32* PADIsBarrel = (vu32*)0x93003130;
static vu32* PADBarrelEnabled = (vu32*)0x93003140;
static vu32* PADBarrelPress = (vu32*)0x93003150;

static volatile struct BTPadCont *BTPad = (volatile struct BTPadCont*)0x932F0000;
static vu32* BTMotor = (vu32*)0x93003040;
static vu32* BTPadFree = (vu32*)0x93003050;
static vu32* SIInited = (vu32*)0x93003060;
static vu32* PADSwitchRequired = (vu32*)0x93003064;
static vu32* PADForceConnected = (vu32*)0x93003068;
static vu32* drcAddress = (vu32*)0x9300306C;
static vu32* drcAddressAligned = (vu32*)0x93003070;

static u32 PrevAdapterChannel1 = 0;
static u32 PrevAdapterChannel2 = 0;
static u32 PrevAdapterChannel3 = 0;
static u32 PrevAdapterChannel4 = 0;
static u32 PrevDRCButton = 0;

static volatile controller *HID_CTRL = (volatile controller*)0x93005000;

#define DRC_SWAP (1<<16)

const s8 DEADZONE = 0x1A;

#define HID_PAD_NONE    4
#define HID_PAD_NOT_SET    0xFF

#define C_NOT_SET    (0<<0)
#define C_CCP        (1<<0)
#define C_CC        (1<<1)
#define C_SWAP        (1<<2)
#define C_RUMBLE_WM    (1<<3)
#define C_NUN        (1<<4)
#define C_NSWAP1    (1<<5)
#define C_NSWAP2    (1<<6)
#define C_NSWAP3    (1<<7)
#define C_ISWAP        (1<<8)

#define ALIGN32(x)     (((u32)x) & (~31))

#define DRC_DEADZONE 10
#define _DRC_BUILD_TMPSTICK(inval) \
    tmp_stick16 = (((s8)(inval-0x80))*14)>>3; \
    if(tmp_stick16 > DRC_DEADZONE) tmp_stick16 = (tmp_stick16-DRC_DEADZONE)*1.08f; \
    else if(tmp_stick16 < -DRC_DEADZONE) tmp_stick16 = (tmp_stick16+DRC_DEADZONE)*1.08f; \
    else tmp_stick16 = 0; \
    if(tmp_stick16 > 0x7F) tmp_stick8 = 0x7F; \
    else if(tmp_stick16 < -0x80) tmp_stick8 = -0x80; \
    else tmp_stick8 = (s8)tmp_stick16;

void HIDUpdateControllerIni()
{
    if (*(vu32*)HID_CHANGE == 0)
    {
        return;
    }

    unsigned int DeviceVID = *(vu32*)HID_CHANGE;
    unsigned int DevicePID = *(vu32*)HID_CFG_SIZE;
    //gprintf("Trying to get VID%04x PID%04x\n", DeviceVID, DevicePID);

    /* I hope this covers all possible ini files */
    char file_sd[40];
    char file_usb[40];
    snprintf(file_sd, sizeof(file_sd), "sd:/wiisxrx/controllers/%04X_%04X.ini", DeviceVID, DevicePID);
    snprintf(file_usb, sizeof(file_usb), "usb:/wiisxrx/controllers/%04X_%04X.ini", DeviceVID, DevicePID);

    const char *const filenames[2] =
    {
        file_sd, file_usb
    };

    int i;
    FILE* f = NULL;
    for (i = 0; i < 2; i++)
    {
        f = fopen(filenames[i], "r");
        if (f != NULL)
            break;
    }

    if (f != NULL)
    {
        fseek(f, 0, SEEK_END);
        size_t fsize = ftell(f);
        fseek(f, 0, SEEK_SET);
        fread((void*)HID_CFG_FILE, 1, fsize, f);
        DCFlushRange((void*)HID_CFG_FILE, fsize);
        fclose(f);
        *(vu32*)HID_CFG_SIZE = fsize;
    }
    else
    {
        // No controller configuration file.
        *(vu32*)HID_CFG_SIZE = 0;
    }

    *(vu32*)HID_CHANGE = 0;
}

u32 HidFormatData(void)
{
    // Registers r1,r13-r31 automatically restored if used.
    // Registers r0, r3-r12 should be handled by calling function
    // Register r2 not changed
    u32 Rumble = 0, memInvalidate, memFlush;
    u32 used = 0;

    PADStatusKernel *Pad = (PADStatusKernel*)(0x93003100); //PadBuff
    u32 MaxPads = 0;

    u32 HIDPad = (*HID_STATUS == 0) ? HID_PAD_NONE : HID_PAD_NOT_SET;
    u32 chan;

    memInvalidate = (u32)SIInited;
    asm volatile("dcbi 0,%0; sync" : : "b"(memInvalidate) : "memory");

    u32 HIDMemPrep = 0;
    if (HIDPad == HID_PAD_NOT_SET)
        HIDPad = MaxPads;

    vu8* HID_Packet = (vu8*)HID_Packet_ADDR;
    #ifdef DISP_DEBUG
    //sprintf(txtbuffer, "HidFormat %08x %08x %08x %08x \r\n", *(u32*)HID_Packet, *((u32*)HID_Packet + 1), *((u32*)HID_Packet + 2), *((u32*)HID_Packet + 3));
    //writeLogFile(txtbuffer);
    #endif // DISP_DEBUG
    for (chan = HIDPad; (chan < HID_PAD_NONE); (HID_CTRL->MultiIn == 3) ? (++chan) : (chan = HID_PAD_NONE)) // Run once unless MultiIn == 3
    {
        if(HIDMemPrep == 0) // first run
        {
            HID_Packet = (vu8*)HID_Packet_ADDR; // reset back to default offset
            memInvalidate = (u32)HID_Packet; // prepare memory
            asm volatile("dcbi 0,%0" : : "b"(memInvalidate) : "memory");
            //invalidate cache block for controllers using more than 0x10 bytes
            memInvalidate = (u32)HID_Packet+0x10; // prepare memory
            asm volatile("dcbi 0,%0; sync" : : "b"(memInvalidate) : "memory");
            HIDMemPrep = memInvalidate;
        }
        if (HID_CTRL->MultiIn == 2)        //multiple controllers connected to a single usb port
        {
            used |= (1<<(PrevAdapterChannel1 + chan)) | (1<<(PrevAdapterChannel2 + chan)) | (1<<(PrevAdapterChannel3 + chan))| (1<<(PrevAdapterChannel4 + chan));    //depending on adapter it may only send every 4th time
            chan = chan + HID_Packet[0] - 1;    // the controller number is in the first byte
            if (chan >= NIN_CFG_MAXPAD)        //if would be higher than the maxnumber of controllers
                continue;    //toss it and try next usb port
            PrevAdapterChannel1 = PrevAdapterChannel2;
            PrevAdapterChannel2 = PrevAdapterChannel3;
            PrevAdapterChannel3 = PrevAdapterChannel4;
            PrevAdapterChannel4 = HID_Packet[0] - 1;
        }

        if (HID_CTRL->MultiIn == 3)        //multiple controllers connected to a single usb port all in one message
        {
            HID_Packet = (vu8*)(HID_Packet_ADDR + (chan * HID_CTRL->MultiInValue));    //skip forward how ever many bytes in each controller
            u32 HID_CacheEndBlock = ALIGN32(((u32)HID_Packet) + HID_CTRL->MultiInValue); //calculate upper cache block used
            if(HID_CacheEndBlock > HIDMemPrep) //new cache block, prepare memory
            {
                memInvalidate = HID_CacheEndBlock;
                asm volatile("dcbi 0,%0; sync" : : "b"(memInvalidate) : "memory");
                HIDMemPrep = memInvalidate;
            }
            if ((HID_CTRL->VID == 0x057E) && (HID_CTRL->PID == 0x0337))    //Nintendo WiiU Gamecube Adapter
            {
                // 0x04=port powered 0x10=normal controller 0x22=wavebird communicating
                if (((HID_Packet[1] & 0x10) == 0)    //normal controller not connected
                 && ((HID_Packet[1] & 0x22) != 0x22))    //wavebird not connected
                {
                    *HIDMotor &= ~(1 << chan); //make sure to disable rumble just in case
                    continue;    //try next controller
                }
                if(((MotorCommand[chan]&3) == 1) && (HID_Packet[1] & 0x04))    //game wants rumbe and controller has power for rumble.
                    *HIDMotor |= (1 << chan);
                else
                    *HIDMotor &= ~(1 << chan);

                if ((HID_Packet[HID_CTRL->StickX.Offset] < 5)        //if connected device is a bongo
                  &&(HID_Packet[HID_CTRL->StickY.Offset] < 5)
                  &&(HID_Packet[HID_CTRL->CStickX.Offset] < 5)
                  &&(HID_Packet[HID_CTRL->CStickY.Offset] < 5)
                  &&(HID_Packet[HID_CTRL->LAnalog] < 5))
                {
                    PADBarrelEnabled[chan] = 1;
                    PADIsBarrel[chan] = 1;
                }
                else
                {
                    PADBarrelEnabled[chan] = 0;
                    PADIsBarrel[chan] = 0;
                }
            }
        }

        used |= (1<<chan);

        Rumble |= ((1<<31)>>chan);
        /* first buttons */
        u16 button = 0;
        if(HID_CTRL->DPAD == 0)
        {
            if( HID_Packet[HID_CTRL->Left.Offset] & HID_CTRL->Left.Mask )
                button |= PAD_BUTTON_LEFT;

            if( HID_Packet[HID_CTRL->Right.Offset] & HID_CTRL->Right.Mask )
                button |= PAD_BUTTON_RIGHT;

            if( HID_Packet[HID_CTRL->Down.Offset] & HID_CTRL->Down.Mask )
                button |= PAD_BUTTON_DOWN;

            if( HID_Packet[HID_CTRL->Up.Offset] & HID_CTRL->Up.Mask )
                button |= PAD_BUTTON_UP;
        }
        else
        {
            if(((HID_Packet[HID_CTRL->Up.Offset] & HID_CTRL->DPADMask) == HID_CTRL->Up.Mask)         || ((HID_Packet[HID_CTRL->UpLeft.Offset] & HID_CTRL->DPADMask) == HID_CTRL->UpLeft.Mask)            || ((HID_Packet[HID_CTRL->RightUp.Offset]    & HID_CTRL->DPADMask) == HID_CTRL->RightUp.Mask))
                button |= PAD_BUTTON_UP;

            if(((HID_Packet[HID_CTRL->Right.Offset] & HID_CTRL->DPADMask) == HID_CTRL->Right.Mask) || ((HID_Packet[HID_CTRL->DownRight.Offset] & HID_CTRL->DPADMask) == HID_CTRL->DownRight.Mask)    || ((HID_Packet[HID_CTRL->RightUp.Offset] & HID_CTRL->DPADMask) == HID_CTRL->RightUp.Mask))
                button |= PAD_BUTTON_RIGHT;

            if(((HID_Packet[HID_CTRL->Down.Offset] & HID_CTRL->DPADMask) == HID_CTRL->Down.Mask)     || ((HID_Packet[HID_CTRL->DownRight.Offset] & HID_CTRL->DPADMask) == HID_CTRL->DownRight.Mask)    || ((HID_Packet[HID_CTRL->DownLeft.Offset] & HID_CTRL->DPADMask) == HID_CTRL->DownLeft.Mask))
                button |= PAD_BUTTON_DOWN;

            if(((HID_Packet[HID_CTRL->Left.Offset] & HID_CTRL->DPADMask) == HID_CTRL->Left.Mask)     || ((HID_Packet[HID_CTRL->DownLeft.Offset] & HID_CTRL->DPADMask) == HID_CTRL->DownLeft.Mask)        || ((HID_Packet[HID_CTRL->UpLeft.Offset] & HID_CTRL->DPADMask) == HID_CTRL->UpLeft.Mask))
                button |= PAD_BUTTON_LEFT;
        }
        if(HID_Packet[HID_CTRL->A.Offset] & HID_CTRL->A.Mask)
            button |= PAD_BUTTON_A;
        if(HID_Packet[HID_CTRL->B.Offset] & HID_CTRL->B.Mask)
            button |= PAD_BUTTON_B;
        if(HID_Packet[HID_CTRL->X.Offset] & HID_CTRL->X.Mask)
            button |= PAD_BUTTON_X;
        if(HID_Packet[HID_CTRL->Y.Offset] & HID_CTRL->Y.Mask)
            button |= PAD_BUTTON_Y;

        if( HID_CTRL->DigitalLR == 1)    //digital trigger buttons only
        {
            if(!(HID_Packet[HID_CTRL->ZL.Offset] & HID_CTRL->ZL.Mask))    //ZL acts as shift for half pressed
            {
                if(HID_Packet[HID_CTRL->L.Offset] & HID_CTRL->L.Mask)
                    button |= PAD_TRIGGER_L;
                if(HID_Packet[HID_CTRL->R.Offset] & HID_CTRL->R.Mask)
                    button |= PAD_TRIGGER_R;

                if(HID_Packet[HID_CTRL->L1.Offset] & HID_CTRL->L1.Mask)
                    button |= PAD_TRIGGER_L1;
                if(HID_Packet[HID_CTRL->R1.Offset] & HID_CTRL->R1.Mask)
                    button |= PAD_TRIGGER_R1;
            }
        }
        else if( HID_CTRL->DigitalLR == 2)    //no digital trigger buttons compute from analog trigger values
        {
            if ((HID_CTRL->VID == 0x0925) && (HID_CTRL->PID == 0x03E8))    //Mayflash Classic Controller Pro Adapter
            {
                if((HID_Packet[HID_CTRL->L.Offset] & 0x7C) >= HID_CTRL->L.Mask)    //only some bits are part of this control
                    button |= PAD_TRIGGER_L;
                if((HID_Packet[HID_CTRL->R.Offset] & 0x0F) >= HID_CTRL->R.Mask)    //only some bits are part of this control
                    button |= PAD_TRIGGER_R;

                if ((HID_Packet[HID_CTRL->L1.Offset] & 0x7C) >= HID_CTRL->L1.Mask)
                    button |= PAD_TRIGGER_L1;
                if ((HID_Packet[HID_CTRL->R1.Offset] & 0x0F) >= HID_CTRL->R1.Mask)
                    button |= PAD_TRIGGER_R1;
            }
            else    //standard no digital trigger button
            {
                if(HID_Packet[HID_CTRL->L.Offset] & HID_CTRL->L.Mask)
                    button |= PAD_TRIGGER_L;
                if(HID_Packet[HID_CTRL->R.Offset] & HID_CTRL->R.Mask)
                    button |= PAD_TRIGGER_R;

                if(HID_Packet[HID_CTRL->L1.Offset] & HID_CTRL->L1.Mask)
                    button |= PAD_TRIGGER_L1;
                if(HID_Packet[HID_CTRL->R1.Offset] & HID_CTRL->R1.Mask)
                    button |= PAD_TRIGGER_R1;
            }
        }
        else    //standard digital left and right trigger buttons
        {
            if(HID_Packet[HID_CTRL->L.Offset] & HID_CTRL->L.Mask)
                button |= PAD_TRIGGER_L;
            if(HID_Packet[HID_CTRL->R.Offset] & HID_CTRL->R.Mask)
                button |= PAD_TRIGGER_R;

            if(HID_Packet[HID_CTRL->L1.Offset] & HID_CTRL->L1.Mask)
                button |= PAD_TRIGGER_L1;
            if(HID_Packet[HID_CTRL->R1.Offset] & HID_CTRL->R1.Mask)
                button |= PAD_TRIGGER_R1;
        }

        if (PADBarrelEnabled[chan] && PADIsBarrel[chan]) //if bongo controller
        {
            if(button & (PAD_BUTTON_A | PAD_BUTTON_B | PAD_BUTTON_X | PAD_BUTTON_Y | PAD_BUTTON_START))    //any bongo pressed
                PADBarrelPress[0+chan] = 6;
            else
            {
                if(PADBarrelPress[0+chan] > 0)
                    PADBarrelPress[0+chan]--;
            }
            if ((( HID_CTRL->DigitalLR != 1) && (HID_Packet[HID_CTRL->RAnalog] > 0x30)) //shadowfield liked 40 but didnt work for multi player
              ||(( HID_CTRL->DigitalLR == 1) && (HID_Packet[HID_CTRL->R.Offset] & HID_CTRL->R.Mask)))
                if (PADBarrelPress[0+chan] == 0)    // bongos not pressed last 6 cycles (dont pickup bongo noise as clap)
                    button |= PAD_TRIGGER_R;    //force button presss todo: bogo should only be using analog
        }

        if(HID_Packet[HID_CTRL->S.Offset] & HID_CTRL->S.Mask)
            button |= PAD_BUTTON_START;

        if(HID_Packet[HID_CTRL->Select.Offset] & HID_CTRL->Select.Mask)
            button |= PAD_BUTTON_SELECT;

        Pad[chan].button = button;

        /* then analog sticks */
        s8 stickX, stickY, substickX, substickY;
        if (PADIsBarrel[chan])
        {
            stickX = stickY = substickX = substickY = 0;    //DK Jungle Beat requires all sticks = 0 in menues
        }
        else
        if ((HID_CTRL->VID == 0x044F) && (HID_CTRL->PID == 0xB303))    //Logitech Thrustmaster Firestorm Dual Analog 2
        {
            stickX        = HID_Packet[HID_CTRL->StickX.Offset];            //raw 80 81...FF 00 ... 7E 7F (left...center...right)
            stickY        = -1 - HID_Packet[HID_CTRL->StickY.Offset];        //raw 80 81...FF 00 ... 7E 7F (up...center...down)
            substickX    = HID_Packet[HID_CTRL->CStickX.Offset];            //raw 80 81...FF 00 ... 7E 7F (left...center...right)
            substickY    = 127 - HID_Packet[HID_CTRL->CStickY.Offset];    //raw 00 01...7F 80 ... FE FF (up...center...down)
        }
        else
        if ((HID_CTRL->VID == 0x0926) && (HID_CTRL->PID == 0x2526))    //Mayflash 3 in 1 Magic Joy Box
        {
            stickX        = HID_Packet[HID_CTRL->StickX.Offset] - 128;    //raw 1A 1B...80 81 ... E4 E5 (left...center...right)
            stickY        = 127 - HID_Packet[HID_CTRL->StickY.Offset];    //raw 0E 0F...7E 7F ... E4 E5 (up...center...down)
            if (HID_Packet[HID_CTRL->CStickX.Offset] >= 0)
                substickX    = (HID_Packet[HID_CTRL->CStickX.Offset] * 2) - 128;    //raw 90 91 10 11...41 42...68 69 EA EB (left...center...right) the 90 91 EA EB are hard right and left almost to the point of breaking
            else if (HID_Packet[HID_CTRL->CStickX.Offset] < 0xD0)
                substickX    = 0xFE;
            else
                substickX    = 0;
            substickY    = 127 - ((HID_Packet[HID_CTRL->CStickY.Offset] - 128) * 4);    //raw 88 89...9E 9F A0 A1 ... BA BB (up...center...down)
        }
        else
        if ((HID_CTRL->VID == 0x045E) && (HID_CTRL->PID == 0x001B))    //Microsoft Sidewinder Force Feedback 2 Joystick
        {
            stickX        = ((HID_Packet[HID_CTRL->StickX.Offset] & 0xFC) >> 2) | ((HID_Packet[2] & 0x03) << 6);            //raw 80 81...FF 00 ... 7E 7F (left...center...right)
            stickY        = -1 - (((HID_Packet[HID_CTRL->StickY.Offset] & 0xFC) >> 2) | ((HID_Packet[4] & 0x03) << 6));    //raw 80 81...FF 00 ... 7E 7F (up...center...down)
            substickX    = HID_Packet[HID_CTRL->CStickX.Offset] * 4;            //raw E0 E1...FF 00 ... 1E 1F (left...center...right)
            substickY    = 127 - (HID_Packet[HID_CTRL->CStickY.Offset] * 2);    //raw 00 01...3F 40 ... 7E 7F (up...center...down)
        }
        else
        if ((HID_CTRL->VID == 0x044F) && (HID_CTRL->PID == 0xB315))    //Thrustmaster Dual Analog 4
        {
            stickX        = HID_Packet[HID_CTRL->StickX.Offset];            //raw 80 81...FF 00 ... 7E 7F (left...center...right)
            stickY        = -1 - HID_Packet[HID_CTRL->StickY.Offset];        //raw 80 81...FF 00 ... 7E 7F (up...center...down)
            substickX    = HID_Packet[HID_CTRL->CStickX.Offset];            //raw 80 81...FF 00 ... 7E 7F (left...center...right)
            substickY    = 127 - HID_Packet[HID_CTRL->CStickY.Offset];    //raw 00 01...7F 80 ... FE FF (up...center...down)
        }
        else
        if ((HID_CTRL->VID == 0x0925) && (HID_CTRL->PID == 0x03E8))    //Mayflash Classic Controller Pro Adapter
        {
            stickX        = ((HID_Packet[HID_CTRL->StickX.Offset] & 0x3F) << 2) - 128;    //raw 06 07 ... 1E 1F 20 ... 37 38 (left ... center ... right)
            stickY        = 127 - ((((HID_Packet[HID_CTRL->StickY.Offset] & 0x0F) << 2) | ((HID_Packet[3] & 0xC0) >> 6)) << 2);    //raw 06 07 ... 1F 20 21 ... 38 39 (up, center, down)
            substickX    = ((HID_Packet[HID_CTRL->CStickX.Offset] & 0x1F) << 3) - 128;    //raw 03 04 ... 0E 0F 10 ... 1B 1C (left ... center ... right)
            substickY    = 127 - ((((HID_Packet[HID_CTRL->CStickY.Offset] & 0x03) << 3) | ((HID_Packet[5] & 0xE0) >> 5)) << 3);    //raw 03 04 ... 1F 10 11 ... 1C 1D (up, center, down)
        }
        else
        if ((HID_CTRL->VID == 0x057E) && (HID_CTRL->PID == 0x0337))    //Nintendo wiiu Gamecube Adapter
        {
            stickX        = HID_Packet[HID_CTRL->StickX.Offset] - 128;    //raw 1D 1E 1F ... 7F 80 81 ... E7 E8 E9 (left ... center ... right)
            stickY        = HID_Packet[HID_CTRL->StickY.Offset] - 128;    //raw EE ED EC ... 82 81 80 7F 7E ... 1A 19 18 (up, center, down)
            substickX    = HID_Packet[HID_CTRL->CStickX.Offset] - 128;    //raw 22 23 24 ... 7F 80 81 ... D2 D3 D4 (left ... center ... right)
            substickY    = HID_Packet[HID_CTRL->CStickY.Offset] - 128;    //raw DB DA D9 ... 81 80 7F ... 2B 2A 29 (up, center, down)
        }
        else    //standard sticks
        {
            stickX        = HID_Packet[HID_CTRL->StickX.Offset] - 128;
            stickY        = 127 - HID_Packet[HID_CTRL->StickY.Offset];
            substickX    = HID_Packet[HID_CTRL->CStickX.Offset] - 128;
            substickY    = 127 - HID_Packet[HID_CTRL->CStickY.Offset];
        }

        s8 tmp_stick = 0;
        if(stickX > HID_CTRL->StickX.DeadZone && stickX > 0)
            tmp_stick = (double)(stickX - HID_CTRL->StickX.DeadZone) * HID_CTRL->StickX.Radius / 1000;
        else if(stickX < -HID_CTRL->StickX.DeadZone && stickX < 0)
            tmp_stick = (double)(stickX + HID_CTRL->StickX.DeadZone) * HID_CTRL->StickX.Radius / 1000;
        Pad[chan].stickX = tmp_stick;

        tmp_stick = 0;
        if(stickY > HID_CTRL->StickY.DeadZone && stickY > 0)
            tmp_stick = (double)(stickY - HID_CTRL->StickY.DeadZone) * HID_CTRL->StickY.Radius / 1000;
        else if(stickY < -HID_CTRL->StickY.DeadZone && stickY < 0)
            tmp_stick = (double)(stickY + HID_CTRL->StickY.DeadZone) * HID_CTRL->StickY.Radius / 1000;
        Pad[chan].stickY = tmp_stick;

        tmp_stick = 0;
        if(substickX > HID_CTRL->CStickX.DeadZone && substickX > 0)
            tmp_stick = (double)(substickX - HID_CTRL->CStickX.DeadZone) * HID_CTRL->CStickX.Radius / 1000;
        else if(substickX < -HID_CTRL->CStickX.DeadZone && substickX < 0)
            tmp_stick = (double)(substickX + HID_CTRL->CStickX.DeadZone) * HID_CTRL->CStickX.Radius / 1000;
        Pad[chan].substickX = tmp_stick;

        tmp_stick = 0;
        if(substickY > HID_CTRL->CStickY.DeadZone && substickY > 0)
            tmp_stick = (double)(substickY - HID_CTRL->CStickY.DeadZone) * HID_CTRL->CStickY.Radius / 1000;
        else if(substickY < -HID_CTRL->CStickY.DeadZone && substickY < 0)
            tmp_stick = (double)(substickY + HID_CTRL->CStickY.DeadZone) * HID_CTRL->CStickY.Radius / 1000;
        Pad[chan].substickY = tmp_stick;
/*
        Pad[chan].stickX = stickX;
        Pad[chan].stickY = stickY;
        Pad[chan].substickX = substickX;
        Pad[chan].substickY = substickY;
*/
        /* then triggers */
        if( HID_CTRL->DigitalLR == 1)
        {    /* digital triggers, not much to do */
            if(HID_Packet[HID_CTRL->L.Offset] & HID_CTRL->L.Mask)
                if(HID_Packet[HID_CTRL->ZL.Offset] & HID_CTRL->ZL.Mask)    //ZL acts as shift for half pressed
                    Pad[chan].triggerLeft = 0x7F;
                else
                    Pad[chan].triggerLeft = 255;
            else
                Pad[chan].triggerLeft = 0;
            if(HID_Packet[HID_CTRL->R.Offset] & HID_CTRL->R.Mask)
                if(HID_Packet[HID_CTRL->ZL.Offset] & HID_CTRL->ZL.Mask)    //ZL acts as shift for half pressed
                    Pad[chan].triggerRight = 0x7F;
                else
                    Pad[chan].triggerRight = 255;
            else
                Pad[chan].triggerRight = 0;
        }
        else
        {    /* much to do with analog */
            u8 tmp_triggerL = 0;
            u8 tmp_triggerR = 0;
            if (((HID_CTRL->VID == 0x0926) && (HID_CTRL->PID == 0x2526))    //Mayflash 3 in 1 Magic Joy Box
             || ((HID_CTRL->VID == 0x2006) && (HID_CTRL->PID == 0x0118)))    //Trio Linker Plus
            {
                tmp_triggerL =  HID_Packet[HID_CTRL->LAnalog] & 0xF0;    //high nibble raw 1x 2x ... Dx Ex
                tmp_triggerR = (HID_Packet[HID_CTRL->RAnalog] & 0x0F) * 16 ;    //low nibble raw x1 x2 ...xD xE
                if(Pad[chan].button & PAD_TRIGGER_L)
                    tmp_triggerL = 255;
                if(Pad[chan].button & PAD_TRIGGER_R)
                    tmp_triggerR = 255;
            }
            else
            if ((HID_CTRL->VID == 0x0925) && (HID_CTRL->PID == 0x03E8))    //Mayflash Classic Controller Pro Adapter
            {
                tmp_triggerL =   ((HID_Packet[HID_CTRL->LAnalog] & 0x7C) >> 2) << 3;    //raw 04 ... 1F (out ... in)
                tmp_triggerR = (((HID_Packet[HID_CTRL->RAnalog] & 0x0F) << 1) | ((HID_Packet[6] & 0x80) >> 7)) << 3;    //raw 03 ... 1F (out ... in)
            }
            else    //standard analog triggers
            {
                tmp_triggerL = HID_Packet[HID_CTRL->LAnalog];
                tmp_triggerR = HID_Packet[HID_CTRL->RAnalog];
            }
            /* Calculate left trigger with deadzone */
            if(tmp_triggerL > DEADZONE)
                Pad[chan].triggerLeft = (tmp_triggerL - DEADZONE) * 1.11f;
            else
                Pad[chan].triggerLeft = 0;
            /* Calculate right trigger with deadzone */
            if(tmp_triggerR > DEADZONE)
                Pad[chan].triggerRight = (tmp_triggerR - DEADZONE) * 1.11f;
            else
                Pad[chan].triggerRight = 0;
        }
    }

//    if(MaxPads == 0) //wiiu
//        MaxPads = 4;
//
//    for(chan = 0; chan < MaxPads; ++chan)    //bluetooth controller loop
//    {
//        if(used & (1<<chan))
//        {
//            BTPadFree[chan] = 0;
//            continue;
//        }
//        BTPadFree[chan] = 1;
//
//        memInvalidate = (u32)&BTPad[chan];
//        asm volatile("dcbi 0,%0; sync" : : "b"(memInvalidate) : "memory");
//
//        if(BTPad[chan].used == C_NOT_SET)
//            continue;
//
//        used |= (1<<chan);
//
//        Rumble |= ((1<<31)>>chan);
//        BTMotor[chan] = MotorCommand[chan]&0x3;
//
//        s8 tmp_stick = 0;
//        //Normal Stick
//        if(BTPad[chan].xAxisL > 0x7F)
//            tmp_stick = 0x7F;
//        else if(BTPad[chan].xAxisL < -0x80)
//            tmp_stick = -0x80;
//        else
//            tmp_stick = BTPad[chan].xAxisL;
//        Pad[chan].stickX = tmp_stick;
//
//        if(BTPad[chan].yAxisL > 0x7F)
//            tmp_stick = 0x7F;
//        else if(BTPad[chan].yAxisL < -0x80)
//            tmp_stick = -0x80;
//        else
//            tmp_stick = BTPad[chan].yAxisL;
//        Pad[chan].stickY = tmp_stick;
//
//        // Normal cStick
//        if((BTPad[chan].used & (C_CC | C_CCP))
//          ||((BTPad[chan].used & C_NUN) && (BTPad[chan].used & C_ISWAP)))
//        {
//            if(BTPad[chan].xAxisR > 0x7F)
//                tmp_stick = 0x7F;
//            else if(BTPad[chan].xAxisR < -0x80)
//                tmp_stick = -0x80;
//            else
//                tmp_stick = BTPad[chan].xAxisR;
//            Pad[chan].substickX = tmp_stick;
//
//            if(BTPad[chan].yAxisR > 0x7F)
//                tmp_stick = 0x7F;
//            else if(BTPad[chan].yAxisR < -0x80)
//                tmp_stick = -0x80;
//            else
//                tmp_stick = BTPad[chan].yAxisR;
//            Pad[chan].substickY = tmp_stick;
//        }
//
//        u16 button = 0;
//
//        if(BTPad[chan].used & C_CC)
//        {
//            Pad[chan].triggerLeft = BTPad[chan].triggerL;
//            if(BTPad[chan].button & BT_TRIGGER_L)
//                button |= PAD_TRIGGER_L;
//
//            Pad[chan].triggerRight = BTPad[chan].triggerR;
//            if(BTPad[chan].button & BT_TRIGGER_R)
//                button |= PAD_TRIGGER_R;
//
//            if(BTPad[chan].button & BT_TRIGGER_ZR)
//                button |= PAD_TRIGGER_Z;
//        }
//        else if(BTPad[chan].used & C_CCP)    //digital triggers
//        {
//            if(BTPad[chan].button & BT_TRIGGER_ZL)
//            {
//                if(BTPad[chan].button & BT_TRIGGER_L)
//                    Pad[chan].triggerLeft = 0x7F;
//                else
//                {
//                    button |= PAD_TRIGGER_L;
//                    Pad[chan].triggerLeft = 0xFF;
//                }
//            }
//            else
//                Pad[chan].triggerLeft = 0;
//
//            if(BTPad[chan].button & BT_TRIGGER_ZR)
//            {
//                if(BTPad[chan].button & BT_TRIGGER_L)
//                    Pad[chan].triggerRight = 0x7F;
//                else
//                {
//                    button |= PAD_TRIGGER_R;
//                    Pad[chan].triggerRight = 0xFF;
//                }
//            }
//            else
//                Pad[chan].triggerRight = 0;
//
//            if(BTPad[chan].button & BT_TRIGGER_R)
//                button |= PAD_TRIGGER_Z;
//        }
//
//// Nunchuck Buttons
//        if((BTPad[chan].used & C_NUN) && !(BTPad[chan].button & WM_BUTTON_TWO))    //nunchuck not being configured
//        {
//            switch ((BTPad[chan].used & (C_NSWAP1 | C_NSWAP2 | C_NSWAP3)) >> 5)
//            {
//                case 0:    // (2)
//                default:
//                {    //Howards general config
//                    //A=A B=B Z=Z +=X -=Y Dpad=Standard
//                    //C not pressed L R tilt tied to L R analog triggers.
//                    //C        pressed tilt control the cStick
//                    if((BTPad[chan].button & NUN_BUTTON_C)    //tilt as camera control
//                     && !(BTPad[chan].used & C_ISWAP))
//                    {
//                        //tilt as cStick
//                        /* xAccel  L=300 C=512 R=740 */
//                        if(BTPad[chan].xAccel < 350)
//                            Pad[chan].substickX = -0x78;
//                        else if(BTPad[chan].xAccel > 674)
//                            Pad[chan].substickX = 0x78;
//                        else
//                            Pad[chan].substickX = (BTPad[chan].xAccel - 512) * 0xF0 / (674 - 350);
//
//                        /* yAccel  up=280 C=512 down=720 */
//                        if(BTPad[chan].yAccel < 344)
//                            Pad[chan].substickY = -0x78;
//                        else if(BTPad[chan].yAccel > 680)
//                            Pad[chan].substickY = 0x78;
//                        else
//                            Pad[chan].substickY = (BTPad[chan].yAccel - 512) * 0xF0 / (680 - 344);
//                    }
//                    else    //    use tilt as AnalogL and AnalogR
//                    {
//                        /* xAccel  L=300 C=512 R=740 */
//                        if(BTPad[chan].xAccel < 340)
//                        {
//                            button |= PAD_TRIGGER_L;
//                            Pad[chan].triggerLeft = 0xFF;
//                        }
//                        else if(BTPad[chan].xAccel < 475)
//                            Pad[chan].triggerLeft = (475 - BTPad[chan].xAccel) * 0xF0 / (475 - 340);
//                        else
//                            Pad[chan].triggerLeft = 0;
//
//                        if(BTPad[chan].xAccel > 670)
//                        {
//                            button |= PAD_TRIGGER_R;
//                            Pad[chan].triggerRight = 0xFF;
//                        }
//                        else if(BTPad[chan].xAccel > 550)
//                            Pad[chan].triggerRight = (BTPad[chan].xAccel - 550) * 0xF0 / (670 - 550);
//                        else
//                            Pad[chan].triggerRight = 0;
//
//                        if (!(BTPad[chan].used & C_ISWAP))    //not using IR
//                        {
//                            Pad[chan].substickX = 0;
//                            Pad[chan].substickY = 0;
//                        }
//                    }
//
//                    if(BTPad[chan].button & WM_BUTTON_A)
//                        button |= PAD_BUTTON_A;
//                    if(BTPad[chan].button & WM_BUTTON_B)
//                        button |= PAD_BUTTON_B;
//                    if(BTPad[chan].button & NUN_BUTTON_Z)
//                        button |= PAD_TRIGGER_Z;
//                    if(BTPad[chan].button & WM_BUTTON_MINUS)
//                        button |= PAD_BUTTON_Y;
////                    if(BTPad[chan].button & NUN_BUTTON_C)
////                        button |= PAD_BUTTON_X;
//                    if(BTPad[chan].button & WM_BUTTON_PLUS)
//                        button |= PAD_BUTTON_X;
//
//                    if(BTPad[chan].button & WM_BUTTON_LEFT)
//                        button |= PAD_BUTTON_LEFT;
//                    if(BTPad[chan].button & WM_BUTTON_RIGHT)
//                        button |= PAD_BUTTON_RIGHT;
//                    if(BTPad[chan].button & WM_BUTTON_DOWN)
//                        button |= PAD_BUTTON_DOWN;
//                    if(BTPad[chan].button & WM_BUTTON_UP)
//                        button |= PAD_BUTTON_UP;
//                }break;
//                case 1:    // (2 & left)
//                {    //AbdallahTerro general config
//                    //A=A B=B C=X Z=Y -=Z +=R Dpad=Standard
//                    if (!(BTPad[chan].used & C_ISWAP))    //not using IR
//                    {
//                        Pad[chan].substickX = 0;
//                        Pad[chan].substickY = 0;
//                    }
//
//                    if(BTPad[chan].button & WM_BUTTON_A)
//                        button |= PAD_BUTTON_A;
//                    if(BTPad[chan].button & WM_BUTTON_B)
//                        button |= PAD_BUTTON_B;
//                    if(BTPad[chan].button & NUN_BUTTON_C)
//                        button |= PAD_BUTTON_X;
//                    if(BTPad[chan].button & NUN_BUTTON_Z)
//                        button |= PAD_BUTTON_Y;
//                    if(BTPad[chan].button & WM_BUTTON_MINUS)
//                        button |= PAD_TRIGGER_Z;
//
//                    if(BTPad[chan].button & WM_BUTTON_DOWN)
//                        button |= PAD_BUTTON_DOWN;
//                    if(BTPad[chan].button & WM_BUTTON_UP)
//                        button |= PAD_BUTTON_UP;
//                    if(BTPad[chan].button & WM_BUTTON_RIGHT)
//                        button |= PAD_BUTTON_RIGHT;
//                    if(BTPad[chan].button & WM_BUTTON_LEFT)
//                        button |= PAD_BUTTON_LEFT;
//
//                    //Pad[chan].triggerLeft = BTPad[chan].triggerL;
//                    if(BTPad[chan].button & WM_BUTTON_PLUS)
//                    {
//                        button |= PAD_TRIGGER_R;
//                        Pad[chan].triggerRight = 0xFF;
//                    }
//                    else
//                        Pad[chan].triggerRight = 0;
//                }break;
//                case 2:    // (2 & right)
//                {    //config asked for by naggers
//                    //A=A
//                    //C not pressed U=Z D=B R=X L=Y B=R Z=L
//                    //C        pressed Dpad=Standard B=R1/2 Z=L1/2 tilt controls cStick
//                    if((BTPad[chan].button & NUN_BUTTON_Z) &&
//                       (BTPad[chan].button & NUN_BUTTON_C))
//                        Pad[chan].triggerLeft = 0x7F;
//                    else
//                    if(BTPad[chan].button & NUN_BUTTON_Z)
//                    {
//                        button |= PAD_TRIGGER_L;
//                        Pad[chan].triggerLeft = 0xFF;
//                    }
////                    else if(BTPad[chan].button & WM_BUTTON_MINUS)
////                        Pad[chan].triggerLeft = 0x7F;
//                    else
//                        Pad[chan].triggerLeft = 0;
//
//                    if((BTPad[chan].button & WM_BUTTON_B) &&
//                       (BTPad[chan].button & NUN_BUTTON_C))
//                        Pad[chan].triggerRight = 0x7F;
//                    else
//                    if(BTPad[chan].button & WM_BUTTON_B)
//                    {
//                        button |= PAD_TRIGGER_R;
//                        Pad[chan].triggerRight = 0xFF;
//                    }
////                    else if(BTPad[chan].button & WM_BUTTON_PLUS)
////                        Pad[chan].triggerRight = 0x7F;
//                    else
//                        Pad[chan].triggerRight = 0;
//
//                    if(BTPad[chan].button & WM_BUTTON_A)
//                        button |= PAD_BUTTON_A;
//
//                    if(BTPad[chan].button & NUN_BUTTON_C)
//                    {
//                        if (!(BTPad[chan].used & C_ISWAP))    //not using IR
//                        {
//                            //tilt as cStick
//                            /* xAccel  L=300 C=512 R=740 */
//                            if(BTPad[chan].xAccel < 350)
//                                Pad[chan].substickX = -0x78;
//                            else if(BTPad[chan].xAccel > 674)
//                                Pad[chan].substickX = 0x78;
//                            else
//                                Pad[chan].substickX = (BTPad[chan].xAccel - 512) * 0xF0 / (674 - 350);
//
//                            /* yAccel  up=280 C=512 down=720 */
//                            if(BTPad[chan].yAccel < 344)
//                                Pad[chan].substickY = -0x78;
//                            else if(BTPad[chan].yAccel > 680)
//                                Pad[chan].substickY = 0x78;
//                            else
//                                Pad[chan].substickY = (BTPad[chan].yAccel - 512) * 0xF0 / (680 - 344);
//                        }
//
//                        if(BTPad[chan].button & WM_BUTTON_LEFT)
//                            button |= PAD_BUTTON_LEFT;
//                        if(BTPad[chan].button & WM_BUTTON_RIGHT)
//                            button |= PAD_BUTTON_RIGHT;
//                        if(BTPad[chan].button & WM_BUTTON_DOWN)
//                            button |= PAD_BUTTON_DOWN;
//                        if(BTPad[chan].button & WM_BUTTON_UP)
//                            button |= PAD_BUTTON_UP;
//                    }
//                    else
//                    {
//                        if (!(BTPad[chan].used & C_ISWAP))    //not using IR
//                        {
//                            Pad[chan].substickX = 0;
//                            Pad[chan].substickY = 0;
//                        }
//
//                        if(BTPad[chan].button & WM_BUTTON_UP)
//                            button |= PAD_TRIGGER_Z;
//                        if(BTPad[chan].button & WM_BUTTON_DOWN)
//                            button |= PAD_BUTTON_B;
//                        if(BTPad[chan].button & WM_BUTTON_RIGHT)
//                            button |= PAD_BUTTON_X;
//                        if(BTPad[chan].button & WM_BUTTON_LEFT)
//                            button |= PAD_BUTTON_Y;
//                    }
//                }break;
//                case 3:    // (2 & up)
//                {    //racing games that use AnalogR and AnalogL for gas and break
//                    //A=A B=B Z=Z +=X -=Y Dpad=Standard
//                    //C not pressed backwards forward tilt tied to L R analog triggers.
//                    //C        pressed tilt control the cStick
//                    if((BTPad[chan].button & NUN_BUTTON_C)    //tilt as camera control
//                      && !(BTPad[chan].used & C_ISWAP))    //not using IR
//                    {
//                        //tilt as cStick
//                        /* xAccel  L=300 C=512 R=740 */
//                        if(BTPad[chan].xAccel < 350)
//                            Pad[chan].substickX = -0x78;
//                        else if(BTPad[chan].xAccel > 674)
//                            Pad[chan].substickX = 0x78;
//                        else
//                            Pad[chan].substickX = (BTPad[chan].xAccel - 512) * 0xF0 / (674 - 350);
//
//                        /* yAccel  up=280 C=512 down=720 */
//                        if(BTPad[chan].yAccel < 344)
//                            Pad[chan].substickY = -0x78;
//                        else if(BTPad[chan].yAccel > 680)
//                            Pad[chan].substickY = 0x78;
//                        else
//                            Pad[chan].substickY = (BTPad[chan].yAccel - 512) * 0xF0 / (680 - 344);
//                    }
//                    else    //    gas use forward and back ward tilt as AnalogL and AnalogR
//                    {
//                        /* yAccel  up=280 C=512 down=720 */
//                        //break pedal
//                        if(BTPad[chan].yAccel < 357)
//                        {
//                            button |= PAD_TRIGGER_L;
//                            Pad[chan].triggerLeft = 0xFF;
//                        }
//                        else if(BTPad[chan].yAccel < 485)
//                            Pad[chan].triggerLeft = (485 - BTPad[chan].yAccel) * 0xF0 / (485 - 357);
//                        else
//                            Pad[chan].triggerLeft = 0;
//
//                        //gas pedal
//                        if(BTPad[chan].yAccel > 668)
//                        {
//                            button |= PAD_TRIGGER_R;
//                            Pad[chan].triggerRight = 0xFF;
//                        }
//                        else if(BTPad[chan].yAccel > 540)
//                            Pad[chan].triggerRight = (BTPad[chan].yAccel - 540) * 0xF0 / (668 - 540);
//                        else
//                            Pad[chan].triggerRight = 0;
//
//                        if (!(BTPad[chan].used & C_ISWAP))    //not using IR
//                        {
//                            Pad[chan].substickX = 0;
//                            Pad[chan].substickY = 0;
//                        }
//                    }
//
//                    if(BTPad[chan].button & WM_BUTTON_A)
//                        button |= PAD_BUTTON_A;
//                    if(BTPad[chan].button & WM_BUTTON_B)
//                        button |= PAD_BUTTON_B;
//                    if(BTPad[chan].button & NUN_BUTTON_Z)
//                        button |= PAD_TRIGGER_Z;
//                    if(BTPad[chan].button & WM_BUTTON_MINUS)
//                        button |= PAD_BUTTON_Y;
//                    if(BTPad[chan].button & WM_BUTTON_PLUS)
//                        button |= PAD_BUTTON_X;
//
//                    if(BTPad[chan].button & WM_BUTTON_LEFT)
//                        button |= PAD_BUTTON_LEFT;
//                    if(BTPad[chan].button & WM_BUTTON_RIGHT)
//                        button |= PAD_BUTTON_RIGHT;
//                    if(BTPad[chan].button & WM_BUTTON_DOWN)
//                        button |= PAD_BUTTON_DOWN;
//                    if(BTPad[chan].button & WM_BUTTON_UP)
//                        button |= PAD_BUTTON_UP;
//                }break;
//                case 4:    // (2 & down)
//                {    //racing games that require A held  for gas
//                    //A=Z B=B Z=A +=X -=Y Dpad=Standard
//                    //C not pressed L R tilt tied to L R analog triggers.
//                    //C        pressed tilt control the cStick
//                    if((BTPad[chan].button & NUN_BUTTON_C)    //tilt as camera control
//                      && !(BTPad[chan].used & C_ISWAP))    //not using IR
//                    {
//                        //tilt as cStick
//                        /* xAccel  L=300 C=512 R=740 */
//                        if(BTPad[chan].xAccel < 350)
//                            Pad[chan].substickX = -0x78;
//                        else if(BTPad[chan].xAccel > 674)
//                            Pad[chan].substickX = 0x78;
//                        else
//                            Pad[chan].substickX = (BTPad[chan].xAccel - 512) * 0xF0 / (674 - 350);
//
//                        /* yAccel  up=280 C=512 down=720 */
//                        if(BTPad[chan].yAccel < 344)
//                            Pad[chan].substickY = -0x78;
//                        else if(BTPad[chan].yAccel > 680)
//                            Pad[chan].substickY = 0x78;
//                        else
//                            Pad[chan].substickY = (BTPad[chan].yAccel - 512) * 0xF0 / (680 - 344);
//                    }
//                    else    //    use tilt as AnalogL and AnalogR
//                    {
//                        /* xAccel  L=300 C=512 R=740 */
//                        if(BTPad[chan].xAccel < 340)
//                        {
//                            button |= PAD_TRIGGER_L;
//                            Pad[chan].triggerLeft = 0xFF;
//                        }
//                        else if(BTPad[chan].xAccel < 475)
//                            Pad[chan].triggerLeft = (475 - BTPad[chan].xAccel) * 0xF0 / (475 - 340);
//                        else
//                            Pad[chan].triggerLeft = 0;
//
//                        if(BTPad[chan].xAccel > 670)
//                        {
//                            button |= PAD_TRIGGER_R;
//                            Pad[chan].triggerRight = 0xFF;
//                        }
//                        else if(BTPad[chan].xAccel > 550)
//                            Pad[chan].triggerRight = (BTPad[chan].xAccel - 550) * 0xF0 / (670 - 550);
//                        else
//                            Pad[chan].triggerRight = 0;
//
//                        if (!(BTPad[chan].used & C_ISWAP))    //not using IR
//                        {
//                            Pad[chan].substickX = 0;
//                            Pad[chan].    substickY = 0;
//                        }
//                    }
//
//                    if(BTPad[chan].button & WM_BUTTON_A)
//                        button |= PAD_TRIGGER_Z;
//                    if(BTPad[chan].button & WM_BUTTON_B)
//                        button |= PAD_BUTTON_B;
//                    if(BTPad[chan].button & NUN_BUTTON_Z)
//                        button |= PAD_BUTTON_A;
//                    if(BTPad[chan].button & WM_BUTTON_MINUS)
//                        button |= PAD_BUTTON_Y;
//                    if(BTPad[chan].button & WM_BUTTON_PLUS)
//                        button |= PAD_BUTTON_X;
//
//                    if(BTPad[chan].button & WM_BUTTON_LEFT)
//                        button |= PAD_BUTTON_LEFT;
//                    if(BTPad[chan].button & WM_BUTTON_RIGHT)
//                        button |= PAD_BUTTON_RIGHT;
//                    if(BTPad[chan].button & WM_BUTTON_DOWN)
//                        button |= PAD_BUTTON_DOWN;
//                    if(BTPad[chan].button & WM_BUTTON_UP)
//                        button |= PAD_BUTTON_UP;
//                }break;
//                case 5:    // (2 & minus)
//                {    //Troopage config
//                    //A=A
//                    //C not pressed +=X -=B Z=L    B=R    Dpad=cStick
//                    //C        pressed +=Y -=Z Z=1/2L B=1/2R Dpad=Standard
//                    if(BTPad[chan].button & NUN_BUTTON_C)
//                    {
//                        if(BTPad[chan].button & WM_BUTTON_PLUS)
//                            button |= PAD_BUTTON_Y;
//                        if(BTPad[chan].button & WM_BUTTON_MINUS)
//                            button |= PAD_TRIGGER_Z;
//                    }
//                    else
//                    {
//                        if(BTPad[chan].button & WM_BUTTON_PLUS)
//                            button |= PAD_BUTTON_X;
//                        if(BTPad[chan].button & WM_BUTTON_MINUS)
//                            button |= PAD_BUTTON_B;
//                    }
//
//                    if((BTPad[chan].button & NUN_BUTTON_C)
//                      || (BTPad[chan].used & C_ISWAP))        //using IR
//                    {
//                        if (!(BTPad[chan].used & C_ISWAP))    //not using IR
//                        {
//                            Pad[chan].substickX = 0;
//                            Pad[chan].substickY = 0;
//                        }
//
//                        if(BTPad[chan].button & WM_BUTTON_LEFT)
//                            button |= PAD_BUTTON_LEFT;
//                        if(BTPad[chan].button & WM_BUTTON_RIGHT)
//                            button |= PAD_BUTTON_RIGHT;
//                        if(BTPad[chan].button & WM_BUTTON_DOWN)
//                            button |= PAD_BUTTON_DOWN;
//                        if(BTPad[chan].button & WM_BUTTON_UP)
//                            button |= PAD_BUTTON_UP;
//                    }
//                    else
//                    // D-Pad as C-stick
//                    {
//                        if(BTPad[chan].button & WM_BUTTON_LEFT)
//                            Pad[chan].substickX = -0x78;
//                        else if(BTPad[chan].button & WM_BUTTON_RIGHT)
//                            Pad[chan].substickX = 0x78;
//                        else
//                            Pad[chan].substickX = 0;
//
//                        if(BTPad[chan].button & WM_BUTTON_DOWN)
//                            Pad[chan].substickY = -0x78;
//                        else if(BTPad[chan].button & WM_BUTTON_UP)
//                            Pad[chan].substickY = 0x78;
//                        else
//                            Pad[chan].substickY = 0;
//                    }
//
//                    if((BTPad[chan].button & NUN_BUTTON_Z) && (BTPad[chan].button & NUN_BUTTON_C))
//                        Pad[chan].triggerLeft = 0x7F;
//                    else if(BTPad[chan].button & NUN_BUTTON_Z)
//                    {
//                        button |= PAD_TRIGGER_L;
//                        Pad[chan].triggerLeft = 0xFF;
//                    }
//                    else
//                        Pad[chan].triggerLeft = 0;
//
//                    if((BTPad[chan].button & WM_BUTTON_B) && (BTPad[chan].button & NUN_BUTTON_C))
//                        Pad[chan].triggerRight = 0x7F;
//                    else if(BTPad[chan].button & WM_BUTTON_B)
//                    {
//                        button |= PAD_TRIGGER_R;
//                        Pad[chan].triggerRight = 0xFF;
//                    }
//                    else
//                        Pad[chan].triggerRight = 0;
//
//                    if(BTPad[chan].button & WM_BUTTON_A)
//                        button |= PAD_BUTTON_A;
//                }break;
//                case 6:    // (2 & 1)
//                {    //FPS using IR as cStick alt based on naggers
//                    //A=A B=R Z=L +=R1/2 -=L1/2
//                    //C not pressed U=Z D=B R=X L=Y
//                    //C        pressed Dpad=Standard, L R tilt tied to L R analog triggers.
//                    //IR controls the cStick
//
//                    if(BTPad[chan].button & NUN_BUTTON_Z)
//                    {
//                        button |= PAD_TRIGGER_L;
//                        Pad[chan].triggerLeft = 0xFF;
//                    }
//                    else if(BTPad[chan].button & WM_BUTTON_MINUS)
//                        Pad[chan].triggerLeft = 0x7F;
//                    else if(BTPad[chan].button & NUN_BUTTON_C)
//                    {
//                        //    use tilt as AnalogL
//                        /* xAccel  L=300 C=512 R=740 */
//                        if(BTPad[chan].xAccel < 340)
//                        {
//                            button |= PAD_TRIGGER_L;
//                            Pad[chan].triggerLeft = 0xFF;
//                        }
//                        else if(BTPad[chan].xAccel < 475)
//                            Pad[chan].triggerLeft = (475 - BTPad[chan].xAccel) * 0xF0 / (475 - 340);
//                        else
//                            Pad[chan].triggerLeft = 0;
//                    }
//                    else
//                        Pad[chan].triggerLeft = 0;
//
//                    if(BTPad[chan].button & WM_BUTTON_B)
//                    {
//                        button |= PAD_TRIGGER_R;
//                        Pad[chan].triggerRight = 0xFF;
//                    }
//                    else if(BTPad[chan].button & WM_BUTTON_PLUS)
//                        Pad[chan].triggerRight = 0x7F;
//                    else if(BTPad[chan].button & NUN_BUTTON_C)
//                    {
//                        //    use tilt as AnalogR
//                        /* xAccel  L=300 C=512 R=740 */
//                        if(BTPad[chan].xAccel > 670)
//                        {
//                            button |= PAD_TRIGGER_R;
//                            Pad[chan].triggerRight = 0xFF;
//                        }
//                        else if(BTPad[chan].xAccel > 550)
//                            Pad[chan].triggerRight = (BTPad[chan].xAccel - 550) * 0xF0 / (670 - 550);
//                        else
//                            Pad[chan].triggerRight = 0;
//                    }
//                    else
//                        Pad[chan].triggerRight = 0;
//
//                    if(BTPad[chan].button & WM_BUTTON_A)
//                        button |= PAD_BUTTON_A;
//
//                    if(BTPad[chan].button & NUN_BUTTON_C)
//                    {
//                        if(BTPad[chan].button & WM_BUTTON_LEFT)
//                            button |= PAD_BUTTON_LEFT;
//                        if(BTPad[chan].button & WM_BUTTON_RIGHT)
//                            button |= PAD_BUTTON_RIGHT;
//                        if(BTPad[chan].button & WM_BUTTON_DOWN)
//                            button |= PAD_BUTTON_DOWN;
//                        if(BTPad[chan].button & WM_BUTTON_UP)
//                            button |= PAD_BUTTON_UP;
//                    }
//                    else
//                    {
//                        if(BTPad[chan].button & WM_BUTTON_UP)
//                            button |= PAD_TRIGGER_Z;
//                        if(BTPad[chan].button & WM_BUTTON_DOWN)
//                            button |= PAD_BUTTON_B;
//                        if(BTPad[chan].button & WM_BUTTON_RIGHT)
//                            button |= PAD_BUTTON_X;
//                        if(BTPad[chan].button & WM_BUTTON_LEFT)
//                            button |= PAD_BUTTON_Y;
//                    }
//                }break;
////                case 7:    // (2 & plus)
////                {
////                }break;
//            }
//            if(BTPad[chan].button & WM_BUTTON_ONE)
//                button |= PAD_BUTTON_START;
//        }    //end nunchuck configs
//
//        if(BTPad[chan].used & (C_CC | C_CCP))
//        {
//            if(BTPad[chan].used & C_SWAP)
//            {    /* turn buttons quarter clockwise */
//                if(BTPad[chan].button & BT_BUTTON_B)
//                    button |= PAD_BUTTON_A;
//                if(BTPad[chan].button & BT_BUTTON_Y)
//                    button |= PAD_BUTTON_B;
//                if(BTPad[chan].button & BT_BUTTON_A)
//                    button |= PAD_BUTTON_X;
//                if(BTPad[chan].button & BT_BUTTON_X)
//                    button |= PAD_BUTTON_Y;
//            }
//            else
//            {
//                if(BTPad[chan].button & BT_BUTTON_A)
//                    button |= PAD_BUTTON_A;
//                if(BTPad[chan].button & BT_BUTTON_B)
//                    button |= PAD_BUTTON_B;
//                if(BTPad[chan].button & BT_BUTTON_X)
//                    button |= PAD_BUTTON_X;
//                if(BTPad[chan].button & BT_BUTTON_Y)
//                    button |= PAD_BUTTON_Y;
//            }
//            if(BTPad[chan].button & BT_BUTTON_START)
//                button |= PAD_BUTTON_START;
//
//            if(BTPad[chan].button & BT_DPAD_LEFT)
//                button |= PAD_BUTTON_LEFT;
//            if(BTPad[chan].button & BT_DPAD_RIGHT)
//                button |= PAD_BUTTON_RIGHT;
//            if(BTPad[chan].button & BT_DPAD_DOWN)
//                button |= PAD_BUTTON_DOWN;
//            if(BTPad[chan].button & BT_DPAD_UP)
//                button |= PAD_BUTTON_UP;
//        }
//
//        Pad[chan].button = button;
//
////#define DEBUG_cStick    1
//        #ifdef DEBUG_cStick
//            //mirrors cStick on main Stick so f-Zero GX calibration can be used
//            Pad[chan].stickX = Pad[chan].substickX;
//            Pad[chan].stickY = Pad[chan].substickY;
//        #endif
//
////#define DEBUG_Triggers    1
//        #ifdef DEBUG_Triggers
//            //mirrors triggers on main Stick so f-Zero GX calibration can be used
//            Pad[chan].stickX = Pad[chan].triggerRight;
//            Pad[chan].stickY = Pad[chan].triggerLeft;
//        #endif
//    }

    /* Some games always need the controllers "used" */
    if(*PADForceConnected)
    {
        for(chan = 0; chan < 4; ++chan)
            used |= (1<<chan);
    }

    if(*PADSwitchRequired)
    {
        *(vu32*)0xD3026438 = (*(vu32*)0xD3026438 == 0) ? 0x20202020 : 0; //switch between new data and no data
        for(chan = 0; chan < 4; ++chan)
            Pad[chan].err = ((used & (1<<chan)) && *SIInited) ? ((*(vu32*)0xD3026438 == 0) ? -3 : 0) : -1;
    }
    else
    {
        for(chan = 0; chan < 4; ++chan)
            Pad[chan].err = ((used & (1<<chan)) && *SIInited) ? 0 : -1;
    }
    *PadUsed = (*SIInited ? used : 0);

    //memFlush = (u32)HIDMotor;
    //asm volatile("dcbf 0,%0" : : "b"(memFlush) : "memory");
    //memFlush = (u32)BTMotor;
    //asm volatile("dcbf 0,%0" : : "b"(memFlush) : "memory");
    //make sure its actually sent
    //asm volatile("sync");

    return Rumble;
}
