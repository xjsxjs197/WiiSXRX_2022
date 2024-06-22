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

#include <ogc/machine/processor.h>
#include <ogc/lwp.h>
#include "KernelHID.h"
#include "usb.h"
#include "../Gamecube/DEBUG.h"

#include <stdlib.h>

#ifndef DEBUG_HID
#define dbgprintf(...)
#else
extern int dbgprintf( const char *fmt, ...);
#endif

static u8 *kb_input = (u8*)0x93026C60;

#define GetDeviceChange 1
#define GetDeviceParameters 3
#define AttachFinish 6
#define ResumeDevice 16
#define ControlMessage 18
#define InterruptMessage 19

static const u8 ss_led_pattern[8] = {0x0, 0x02, 0x04, 0x08, 0x10, 0x12, 0x14, 0x18};

static s32 HIDHandle = -1;
static u32 PS3LedSet = 0;
static u32 ControllerID  = 0;
static u32 KeyboardID  = 0;
static u32 bEndpointAddressController = 0;
static u32 bEndpointAddressKeyboard = 0;
static u32 wMaxPacketSize = 0;
static u32 MemPacketSize = 0;
static u8 *Packet = (u8*)NULL;

static u32 RumbleType = 0;
static u32 RumbleEnabled = 0;
static u32 bEndpointAddressOut = 0;
static u8 *RawRumbleDataOn = NULL;
static u8 *RawRumbleDataOff = NULL;
static u32 RawRumbleDataLen = 0;
static u32 RumbleTransferLen = 0;
static u32 RumbleTransfers = 0;

static const unsigned char rawData[] =
{
    0x01, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xFF, 0x27, 0x10, 0x00, 0x32,
    0xFF, 0x27, 0x10, 0x00, 0x32, 0xFF, 0x27, 0x10, 0x00, 0x32, 0xFF, 0x27, 0x10, 0x00, 0x32, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00,
//    0x01,                         /* ??? */
//    0x00,                         /* Padding */
//    0xFF, 0x00, 0xFF, 0x00,       /* Rumble (r, r, l, l) */
//    0x00, 0x00, 0x00, 0x00,       /* Padding */
//    0x02,                         /* LED_1 = 0x02, LED_2 = 0x04, ... */
//    0xFF, 0x27, 0x10, 0x00, 0x32, /* LED_4 */
//    0xFF, 0x27, 0x10, 0x00, 0x32, /* LED_3 */
//    0xFF, 0x27, 0x10, 0x00, 0x32, /* LED_2 */
//    0xFF, 0x27, 0x10, 0x00, 0x32, /* LED_1 */
//    0x00, 0x00, 0x00, 0x00, 0x00  /* LED_5 (not soldered) */
};

static u8 *ps3buf = (u8*)NULL;
static u8 *gcbuf = (u8*)NULL;
static u8 *kbbuf = (u8*)NULL;

typedef void (*HIDReadFunc)();
static HIDReadFunc HIDRead = NULL;

typedef void (*RumbleFunc)(u32 Enable);
static RumbleFunc HIDRumble = NULL;

static usb_device_entry *AttachedDevices = NULL;
#define ATTACHED_DEVICES_SIZE     (sizeof(usb_device_entry) * 32)

static volatile int hidRun = 0;
static lwp_t HID_Thread = LWP_THREAD_NULL;
static u64 HID_Timer = 0;
static vu32 hidread = 0, keyboardread = 0, hidchange = 0, hidattach = 0, hidattached = 0, hidwaittimer = 0;
static u32 *HIDRun();
static void HIDUpdateRegisters(u32 LoaderRequest);
static void HIDGCInit( void );
static void HIDPS3Init( void );
static void HIDPS3Read( void );
static void HIDPS4Init( void );
static void HIDIRQRead( void );
static void HIDPS3SetLED( u8 led );
static void HIDGCRumble( u32 Enable );
static void HIDPS3Rumble( u32 rumblePower );
static void HIDIRQRumble( u32 Enable );
static void HIDCTRLRumble( u32 Enable );
static u32 ConfigGetValue( char *Data, const char *EntryName, u32 Entry );
static u32 ConfigGetDecValue( char *Data, const char *EntryName, u32 Entry );
static void HIDPS3SetRumble( u8 duration_right, u8 power_right, u8 duration_left, u8 power_left);
static s32 HIDInterruptMessage(u32 isKBreq, u8 *Data, u32 Length, u32 Endpoint, s32 msgData);
static s32 HIDControlMessage(u32 isKBreq, u8 *Data, u32 Length, u32 RequestType, u32 Request, u32 Value, s32 msgData);

static s32 ipcCallBack(s32 result, void *usrdata);

static controller *HID_CTRL = (controller*)HID_CTRL_ADDR;
static void *HID_Packet = (void*)HID_Packet_ADDR;

#define HID_STATUS 0x93003440
#define HID_CHANGE HID_STATUS + 4
#define HID_CFG_SIZE HID_STATUS + 8
#define HID_CFG_FILE 0x93003460


#define READ_CONTROLLER_MSG   1
#define READ_KEYBOARD_MSG     2
#define HID_CHANGE_MSG        3
#define HID_ATTACH_MSG        4

#define HID_SET_RUMBLE        5
#define HID_SET_LEDS          6

#define USBV4_IOCTL_GETVERSION                   6 // returns 0x40001
#define USBV5_IOCTL_GETVERSION                   0 // should return 0x50001

#define USB_HEAPSIZE          32768
static s32 hId = -1;

static struct _usb_msg *comMsg = NULL;
static struct _usb_msg *read_ctrl_req = NULL;
static struct _usb_msg *write_ctrl_req = NULL;
static struct _usb_msg *read_irq_req = NULL;
static struct _usb_msg *write_irq_req = NULL;
static struct _usb_msg *read_kb_ctrl_req = NULL;
static struct _usb_msg *write_kb_ctrl_req = NULL;
static struct _usb_msg *read_kb_irq_req = NULL;
static struct _usb_msg *write_kb_irq_req = NULL;

s32 hidControllerConnected = 0;
s32 loadingControllerIni = 0;

static s32 ipcCallBack(s32 result, void *usrdata)
{
    if (!hidRun)
    {
        return 0;
    }

    struct _usb_msg *msgParam = (struct _usb_msg*)usrdata;
    if (msgParam->msgData == READ_CONTROLLER_MSG)
    {
        hidread = 1;
    }
    else if (msgParam->msgData == READ_KEYBOARD_MSG)
    {
        keyboardread = 1;
    }
    else if (msgParam->msgData == HID_CHANGE_MSG)
    {
        if (result >= 0)
        {
            hidchange = 1;
            // wipe unused device entries
            memset(&AttachedDevices[result], 0, sizeof(usb_device_entry) * (32 - result));
        }

        if (result <= 0)
        {
            hidControllerConnected = 0;
        }
    }
    else if (msgParam->msgData == HID_ATTACH_MSG)
    {
        if (result >= 0)
        {
            hidattach = 1;
        }
    }

    return 0;
}

void HIDInit( u32 ios )
{
    if (ios != 58)
    {
        #ifdef DISP_DEBUG
        sprintf(txtbuffer, "IOS Ver is not 58\r\n");
        writeLogFile(txtbuffer);
        #endif // DISP_DEBUG
        return;
    }

    HIDHandle = IOS_Open("/dev/usb/hid", 0 );
    if(HIDHandle < 0) return; //should never happen

    if (hId == -1 ) hId = iosCreateHeap(USB_HEAPSIZE);
    u32 *hid_ver = (u32*)iosAlloc(hId, 0x20);
    s32 checkRet = IOS_Ioctl(HIDHandle, USBV4_IOCTL_GETVERSION, NULL, 0, NULL, 0);
    if (checkRet == 0x40001)
    {
        #ifdef DISP_DEBUG
        sprintf(txtbuffer, "HID chk V4 error\r\n");
        writeLogFile(txtbuffer);
        #endif // DISP_DEBUG
        IOS_Close(HIDHandle);
        iosFree(hId, hid_ver);
        return;
    }

    checkRet = IOS_Ioctl(HIDHandle, USBV5_IOCTL_GETVERSION, NULL, 0, hid_ver, 0x20);
    if (checkRet  || hid_ver[0] != 0x50001)
    {
        #ifdef DISP_DEBUG
        sprintf(txtbuffer, "HID chk V5 error %08x %05x\r\n", checkRet, hid_ver[0]);
        writeLogFile(txtbuffer);
        #endif // DISP_DEBUG
        IOS_Close(HIDHandle);
        iosFree(hId, hid_ver);
        return;
    }
    iosFree(hId, hid_ver);

    ps3buf = (u8*)iosAlloc(hId, 64);
    gcbuf = (u8*)iosAlloc(hId, 32);
    kbbuf = (u8*)iosAlloc(hId, 32);

    comMsg = (struct _usb_msg*)iosAlloc(hId, sizeof(struct _usb_msg));
    read_ctrl_req = (struct _usb_msg*)iosAlloc(hId, sizeof(struct _usb_msg));
    write_ctrl_req = (struct _usb_msg*)iosAlloc(hId, sizeof(struct _usb_msg));
    read_irq_req = (struct _usb_msg*)iosAlloc(hId, sizeof(struct _usb_msg));
    write_irq_req = (struct _usb_msg*)iosAlloc(hId, sizeof(struct _usb_msg));
    read_kb_ctrl_req = (struct _usb_msg*)iosAlloc(hId, sizeof(struct _usb_msg));
    write_kb_ctrl_req = (struct _usb_msg*)iosAlloc(hId, sizeof(struct _usb_msg));
    read_kb_irq_req = (struct _usb_msg*)iosAlloc(hId, sizeof(struct _usb_msg));
    write_kb_irq_req = (struct _usb_msg*)iosAlloc(hId, sizeof(struct _usb_msg));
    memset(comMsg, 0, sizeof(struct _usb_msg));
    memset(read_ctrl_req, 0, sizeof(struct _usb_msg));
    memset(write_ctrl_req, 0, sizeof(struct _usb_msg));
    memset(read_irq_req, 0, sizeof(struct _usb_msg));
    memset(write_irq_req, 0, sizeof(struct _usb_msg));
    memset(read_kb_ctrl_req, 0, sizeof(struct _usb_msg));
    memset(write_kb_ctrl_req, 0, sizeof(struct _usb_msg));
    memset(read_kb_irq_req, 0, sizeof(struct _usb_msg));
    memset(write_kb_irq_req, 0, sizeof(struct _usb_msg));

    hidattached = 0;
    hidwaittimer = 0;
    AttachedDevices = (struct usb_device_entry*)iosAlloc(hId, ATTACHED_DEVICES_SIZE);
    memset(AttachedDevices, 0, ATTACHED_DEVICES_SIZE);
    comMsg->msgData = HID_CHANGE_MSG;
    checkRet = IOS_IoctlAsync(HIDHandle, GetDeviceChange, NULL, 0, AttachedDevices, ATTACHED_DEVICES_SIZE, ipcCallBack, comMsg);
    //hidchange = 1;
    #ifdef DISP_DEBUG
    sprintf(txtbuffer, "GetDeviceChange %08x\r\n", checkRet);
    writeLogFile(txtbuffer);
    #endif // DISP_DEBUG

    hidRun = 1;
    HID_Timer = gettime();
    LWP_CreateThread(&HID_Thread, HIDRun, NULL, NULL, 0, 70);

    memset((void*)HID_STATUS, 0, 0x20);
    memset(HID_Packet, 0, 128);
    memset(HID_CTRL, 0, sizeof(controller));

    usleep(100);
    #ifdef DISP_DEBUG
    sprintf(txtbuffer, "HIDInit end\r\n");
    writeLogFile(txtbuffer);
    #endif // DISP_DEBUG
}

static s32 HIDOpen( u32 LoaderRequest )
{
    s32 ret = -1;
    dbgprintf("HIDOpen()\r\n");

    memset((void*)HID_STATUS, 0, 0x20);
    u32 HIDControllerConnected = 0, HIDKeyboardConnected = 0;
    hidControllerConnected = 0;

    s32 *io_buffer = (s32*)iosAlloc(hId, 32);
    u8 *HIDHeap = (u8*)iosAlloc(hId, 0x60);
    u32 i;
    s32 chkRet;
    u32 DeviceVID = 0, DevicePID = 0;
    for (i = 0; i < 32; ++i)
    {
        if(AttachedDevices[i].vid != 0)
        {
            #ifdef DISP_DEBUG
            sprintf(txtbuffer, "Device VID %05x PID %05x\r\n", AttachedDevices[i].vid, AttachedDevices[i].pid);
            writeLogFile(txtbuffer);
            DEBUG_print(txtbuffer, DBG_CORE3);
            #endif // DISP_DEBUG
            u32 DeviceID = AttachedDevices[i].device_id;
            if(DeviceID == ControllerID)
            {
                HIDControllerConnected = 1;
                hidControllerConnected = 1;
                continue;
            }
            if(DeviceID == KeyboardID)
            {
                HIDKeyboardConnected = 1;
                continue;
            }
            DeviceVID = AttachedDevices[i].vid;
            DevicePID = AttachedDevices[i].pid;

            dbgprintf("HID:DeviceID:%u\r\n", DeviceID );
            dbgprintf("HID:VID:%04X PID:%04X\r\n", DeviceVID, DevicePID );

            memset(io_buffer, 0, 0x20);
            io_buffer[0] = DeviceID;
            io_buffer[2] = 1; //resume device
            chkRet = IOS_Ioctl(HIDHandle, ResumeDevice, io_buffer, 0x20, NULL, 0);
            #ifdef DISP_DEBUG
            sprintf(txtbuffer, "ResumeDevice %05x\r\n", chkRet);
            writeLogFile(txtbuffer);
            #endif // DISP_DEBUG

            memset(HIDHeap, 0, 0x60);

            memset(io_buffer, 0, 0x20);
            io_buffer[0] = DeviceID;
            io_buffer[2] = 0;
            chkRet = IOS_Ioctl(HIDHandle, GetDeviceParameters, io_buffer, 0x20, HIDHeap, 0x60);
            #ifdef DISP_DEBUG
            sprintf(txtbuffer, "GetDeviceParameters %05x\r\n", chkRet);
            writeLogFile(txtbuffer);
            #endif // DISP_DEBUG

            u32 Offset = 36;

            u32 DeviceDescLength    = *(vu8*)(HIDHeap+Offset);
            Offset += (DeviceDescLength+3)&(~3);

            u32 ConfigurationLength = *(vu8*)(HIDHeap+Offset);
            Offset += (ConfigurationLength+3)&(~3);

            u32 InterfaceDescLength = *(vu8*)(HIDHeap+Offset);

            u32 bInterfaceClass = *(vu8*)(HIDHeap+Offset+5);
            u32 bInterfaceSubClass = *(vu8*)(HIDHeap+Offset+6);
            u32 bInterfaceProtocol = *(vu8*)(HIDHeap+Offset+7);
            dbgprintf("HID:bInterfaceClass:%02X\r\n", bInterfaceClass );
            dbgprintf("HID:bInterfaceSubClass:%02X\r\n", bInterfaceSubClass );
            dbgprintf("HID:bInterfaceProtocol:%02X\r\n", bInterfaceProtocol );

            Offset += (InterfaceDescLength+3)&(~3);

            u32 EndpointDescLengthO = *(vu8*)(HIDHeap+Offset);

            u32 bEndpointAddress = *(vu8*)(HIDHeap+Offset+2);

            if( (bEndpointAddress & 0xF0) != 0x80 )
            {
                bEndpointAddressOut = bEndpointAddress;
                Offset += (EndpointDescLengthO+3)&(~3);
            }
            bEndpointAddress = *(vu8*)(HIDHeap+Offset+2);
            wMaxPacketSize   = *(vu16*)(HIDHeap+Offset+4);

            dbgprintf("HID:bEndpointAddress:%02X\r\n", bEndpointAddress );
            dbgprintf("HID:wMaxPacketSize  :%u\r\n", wMaxPacketSize );

            #ifdef DISP_DEBUG
            sprintf(txtbuffer, "Chk %04x %04x %04x\r\n", bInterfaceClass, bInterfaceSubClass, bInterfaceProtocol);
            writeLogFile(txtbuffer);
            #endif // DISP_DEBUG

            if(!HIDKeyboardConnected &&
                (bInterfaceClass == USB_CLASS_HID) &&
                (bInterfaceSubClass == USB_SUBCLASS_BOOT) &&
                (bInterfaceProtocol == USB_PROTOCOL_KEYBOARD))
            {
                dbgprintf("HID:Keyboard detected\r\n");
                KeyboardID = DeviceID;
                bEndpointAddressKeyboard = bEndpointAddress;
                #ifdef DISP_DEBUG
                sprintf(txtbuffer, "HIDKeyboardConnected %08x\r\n", bEndpointAddress);
                writeLogFile(txtbuffer);
                #endif // DISP_DEBUG
                HIDKeyboardConnected = 1;
                //set to boot protocol (0)
                chkRet = HIDControlMessage(1, NULL, 0, USB_REQTYPE_INTERFACE_SET, USB_REQ_SETPROTOCOL, 0, 0);
                #ifdef DISP_DEBUG
                sprintf(txtbuffer, "HIDKeyboard CTRL MSG %08x\r\n", chkRet);
                writeLogFile(txtbuffer);
                #endif // DISP_DEBUG
                //start reading data
                HIDInterruptMessage(1, kbbuf, 8, bEndpointAddressKeyboard, READ_KEYBOARD_MSG);
                //keyboardread = 1;
                if(HIDControllerConnected)
                    break;
            }
            else if(!HIDControllerConnected &&
                (bInterfaceProtocol != USB_PROTOCOL_KEYBOARD) &&
                (bInterfaceProtocol != USB_PROTOCOL_MOUSE))
            {
                #ifdef DISP_DEBUG
                sprintf(txtbuffer, "HIDControllerConnected \r\n");
                writeLogFile(txtbuffer);
                #endif // DISP_DEBUG

                memset(ps3buf, 0, 64);
                memcpy(ps3buf, rawData, sizeof(rawData));
                DCFlushRange((void*)ps3buf, 64);

                memset(gcbuf, 0, 32);
                gcbuf[0] = 0x13;

                HIDRead = NULL;
                HIDRumble = NULL;

                RumbleEnabled = 0;

                ControllerID = DeviceID;
                bEndpointAddressController = bEndpointAddress;

                if ( DeviceVID == 0x054c && DevicePID == 0x0268 )
                {
                    dbgprintf("HID:PS3 Dualshock Controller detected\r\n");
                    MemPacketSize = SS_DATA_LEN;
                    HIDPS3Init();
                    RumbleEnabled = 1;
                    HIDPS3SetRumble( 0, 0, 0, 0 );
                }
                else if ( DeviceVID == 0x057e && DevicePID == 0x0337 )
                {
                    HIDGCInit();
                }
                else if( DeviceVID == 0x054C && (DevicePID == 0x09CC || DevicePID == 0x05C4) )
                {
                    HIDPS4Init();
                }

                //if (needLoadControllerIni)
                {
                    //Load controller config
                    char *Data = NULL;
                    if (LoaderRequest)
                    {
                        dbgprintf("Sending controller.ini request\r\n");
                        memset((void*)HID_STATUS, 0, 0x20);
                        *((u32*)(HID_CHANGE)) = DeviceVID;
                        *((u32*)(HID_CFG_SIZE)) = DevicePID;
                        HID_CTRL->VID = DeviceVID;

                        #ifdef DISP_DEBUG
                        sprintf(txtbuffer, "Waiting LoadHIDControllerIni\r\n");
                        writeLogFile(txtbuffer);
                        #endif // DISP_DEBUG
                        loadingControllerIni = 1;
                        while (hidRun)
                        {
                            if(*((u32*)(HID_CHANGE)) == 0) break;
                            usleep(100);
                        }
                        loadingControllerIni = 0;
                        u32 cfgsize = *((u32*)(HID_CFG_SIZE));
                        if (cfgsize == 0)
                        {
                            dbgprintf("HID:No controller config found!\r\n");
                            HID_CTRL->VID = 0;
                        }
                        else
                        {
                            Data = (char*)iosAlloc(hId, cfgsize + 1);
                            if (Data)
                            {
                                memcpy(Data, (void*)HID_CFG_FILE, cfgsize);
                                Data[cfgsize] = 0x00;    //null terminate the file
                            }
                        }
                    }

                    if(Data != NULL) //initial check
                    {
                        HID_CTRL->VID = ConfigGetValue( Data, INI_KYY_VID, 0 );
                        HID_CTRL->PID = ConfigGetValue( Data, INI_KYY_PID, 0 );

                        if( DeviceVID != HID_CTRL->VID || DevicePID != HID_CTRL->PID )
                        {
                            dbgprintf("HID:Config does not match device VID/PID\r\n");
                            dbgprintf("HID:Config VID:%04X PID:%04X\r\n", HID_CTRL->VID, HID_CTRL->PID );
                            iosFree(hId, Data);
                            Data = NULL;
                        }
                    }
                    #ifdef DISP_DEBUG
                    sprintf(txtbuffer, "HIDController ID %05x\r\n", HID_CTRL->VID);
                    writeLogFile(txtbuffer);
                    #endif // DISP_DEBUG
                    if(Data == NULL)
                    {
                        #ifdef DISP_DEBUG
                        sprintf(txtbuffer, "HIDController Data NULL\r\n");
                        writeLogFile(txtbuffer);
                        #endif // DISP_DEBUG
                        continue;
                    }
                    else
                    {
                        HID_CTRL->DPAD        = ConfigGetValue( Data, INI_KYY_DPAD, 0 );
                        HID_CTRL->DigitalLR    = ConfigGetValue( Data, INI_KYY_DIGITAl_LR, 0 );
                        HID_CTRL->Polltype    = ConfigGetValue( Data, INI_KYY_POLL_TYPE, 0 );
                        HID_CTRL->MultiIn    = ConfigGetValue( Data, INI_KYY_MULTI_IN, 0 );

                        if( HID_CTRL->MultiIn )
                        {
                            HID_CTRL->MultiInValue= ConfigGetValue( Data, INI_KYY_MULTI_IN_VAL, 0 );

                            dbgprintf("HID:MultIn:%u\r\n", HID_CTRL->MultiIn );
                            dbgprintf("HID:MultiInValue:%u\r\n", HID_CTRL->MultiInValue );
                        }

                        if( HID_CTRL->DPAD > 1 )
                        {
                            dbgprintf("HID: %u is an invalid DPAD value\r\n", HID_CTRL->DPAD );
                            #ifdef DISP_DEBUG
                            sprintf(txtbuffer, "HIDController invalid DPAD %d\r\n", HID_CTRL->DPAD);
                            writeLogFile(txtbuffer);
                            #endif // DISP_DEBUG
                            iosFree(hId, Data);
                            continue;
                        }

                        HID_CTRL->A.Offset    = ConfigGetValue( Data, INI_KYY_A, 0 );
                        HID_CTRL->A.Mask    = ConfigGetValue( Data, INI_KYY_A, 1 );

                        HID_CTRL->B.Offset    = ConfigGetValue( Data, INI_KYY_B, 0 );
                        HID_CTRL->B.Mask    = ConfigGetValue( Data, INI_KYY_B, 1 );

                        HID_CTRL->X.Offset    = ConfigGetValue( Data, INI_KYY_X, 0 );
                        HID_CTRL->X.Mask    = ConfigGetValue( Data, INI_KYY_X, 1 );

                        HID_CTRL->Y.Offset    = ConfigGetValue( Data, INI_KYY_Y, 0 );
                        HID_CTRL->Y.Mask    = ConfigGetValue( Data, INI_KYY_Y, 1 );

                        HID_CTRL->ZL.Offset    = ConfigGetValue( Data, INI_KYY_ZL, 0 );
                        HID_CTRL->ZL.Mask    = ConfigGetValue( Data, INI_KYY_ZL, 1 );

                        HID_CTRL->L.Offset    = ConfigGetValue( Data, INI_KYY_L2, 0 );
                        HID_CTRL->L.Mask    = ConfigGetValue( Data, INI_KYY_L2, 1 );

                        HID_CTRL->R.Offset    = ConfigGetValue( Data, INI_KYY_R2, 0 );
                        HID_CTRL->R.Mask    = ConfigGetValue( Data, INI_KYY_R2, 1 );

                        HID_CTRL->L1.Offset    = ConfigGetValue( Data, INI_KYY_L1, 0 );
                        HID_CTRL->L1.Mask    = ConfigGetValue( Data, INI_KYY_L1, 1 );

                        HID_CTRL->R1.Offset    = ConfigGetValue( Data, INI_KYY_R1, 0 );
                        HID_CTRL->R1.Mask    = ConfigGetValue( Data, INI_KYY_R1, 1 );

                        HID_CTRL->S.Offset    = ConfigGetValue( Data, INI_KYY_START, 0 );
                        HID_CTRL->S.Mask    = ConfigGetValue( Data, INI_KYY_START, 1 );

                        HID_CTRL->Select.Offset    = ConfigGetValue( Data, INI_KYY_SELECT, 0 );
                        HID_CTRL->Select.Mask    = ConfigGetValue( Data, INI_KYY_SELECT, 1 );

                        HID_CTRL->Left.Offset    = ConfigGetValue( Data, INI_KYY_LEFT, 0 );
                        HID_CTRL->Left.Mask        = ConfigGetValue( Data, INI_KYY_LEFT, 1 );

                        HID_CTRL->Down.Offset    = ConfigGetValue( Data, INI_KYY_DOWN, 0 );
                        HID_CTRL->Down.Mask        = ConfigGetValue( Data, INI_KYY_DOWN, 1 );

                        HID_CTRL->Right.Offset    = ConfigGetValue( Data, INI_KYY_RIGHT, 0 );
                        HID_CTRL->Right.Mask    = ConfigGetValue( Data, INI_KYY_RIGHT, 1 );

                        HID_CTRL->Up.Offset        = ConfigGetValue( Data, INI_KYY_UP, 0 );
                        HID_CTRL->Up.Mask        = ConfigGetValue( Data, INI_KYY_UP, 1 );

                        if( HID_CTRL->DPAD )
                        {
                            HID_CTRL->RightUp.Offset    = ConfigGetValue( Data, INI_KYY_RIGHT_UP, 0 );
                            HID_CTRL->RightUp.Mask        = ConfigGetValue( Data, INI_KYY_RIGHT_UP, 1 );

                            HID_CTRL->DownRight.Offset    = ConfigGetValue( Data, INI_KYY_DOWN_RIGHT, 0 );
                            HID_CTRL->DownRight.Mask    = ConfigGetValue( Data, INI_KYY_DOWN_RIGHT, 1 );

                            HID_CTRL->DownLeft.Offset    = ConfigGetValue( Data, INI_KYY_DOWN_LEFT, 0 );
                            HID_CTRL->DownLeft.Mask        = ConfigGetValue( Data, INI_KYY_DOWN_LEFT, 1 );

                            HID_CTRL->UpLeft.Offset        = ConfigGetValue( Data, INI_KYY_UP_LEFT, 0 );
                            HID_CTRL->UpLeft.Mask        = ConfigGetValue( Data, INI_KYY_UP_LEFT, 1 );
                        }

                        if( HID_CTRL->DPAD  &&    //DPAD == 1 and all offsets the same
                            HID_CTRL->Left.Offset == HID_CTRL->Down.Offset &&
                            HID_CTRL->Left.Offset == HID_CTRL->Right.Offset &&
                            HID_CTRL->Left.Offset == HID_CTRL->Up.Offset &&
                            HID_CTRL->Left.Offset == HID_CTRL->RightUp.Offset &&
                            HID_CTRL->Left.Offset == HID_CTRL->DownRight.Offset &&
                            HID_CTRL->Left.Offset == HID_CTRL->DownLeft.Offset &&
                            HID_CTRL->Left.Offset == HID_CTRL->UpLeft.Offset )
                        {
                            HID_CTRL->DPADMask = HID_CTRL->Left.Mask | HID_CTRL->Down.Mask | HID_CTRL->Right.Mask | HID_CTRL->Up.Mask
                                | HID_CTRL->RightUp.Mask | HID_CTRL->DownRight.Mask | HID_CTRL->DownLeft.Mask | HID_CTRL->UpLeft.Mask;    //mask is all the used bits ored togather
                            if ((HID_CTRL->DPADMask & 0xF0) == 0)    //if hi nibble isnt used
                                HID_CTRL->DPADMask = 0x0F;            //use all bits in low nibble
                            if ((HID_CTRL->DPADMask & 0x0F) == 0)    //if low nibble isnt used
                                HID_CTRL->DPADMask = 0xF0;            //use all bits in hi nibble
                        }
                        else
                            HID_CTRL->DPADMask = 0xFFFF;    //check all the bits

                        HID_CTRL->StickX.Offset        = ConfigGetValue( Data, INI_KYY_STICKX, 0 );
                        HID_CTRL->StickX.DeadZone    = ConfigGetValue( Data, INI_KYY_STICKX, 1 );
                        HID_CTRL->StickX.Radius        = ConfigGetDecValue( Data, INI_KYY_STICKX, 2 );
                        if (HID_CTRL->StickX.Radius == 0)
                            HID_CTRL->StickX.Radius = 80;
                        HID_CTRL->StickX.Radius = (u64)HID_CTRL->StickX.Radius * 1280 / (128 - HID_CTRL->StickX.DeadZone);    //adjust for DeadZone
                    //        dbgprintf("HID:StickX:  Offset=%3X Deadzone=%3X Radius=%d\r\n", HID_CTRL->StickX.Offset, HID_CTRL->StickX.DeadZone, HID_CTRL->StickX.Radius);

                        HID_CTRL->StickY.Offset        = ConfigGetValue( Data, INI_KYY_STICKY, 0 );
                        HID_CTRL->StickY.DeadZone    = ConfigGetValue( Data, INI_KYY_STICKY, 1 );
                        HID_CTRL->StickY.Radius        = ConfigGetDecValue( Data, INI_KYY_STICKY, 2 );
                        if (HID_CTRL->StickY.Radius == 0)
                            HID_CTRL->StickY.Radius = 80;
                        HID_CTRL->StickY.Radius = (u64)HID_CTRL->StickY.Radius * 1280 / (128 - HID_CTRL->StickY.DeadZone);    //adjust for DeadZone
                    //        dbgprintf("HID:StickY:  Offset=%3X Deadzone=%3X Radius=%d\r\n", HID_CTRL->StickY.Offset, HID_CTRL->StickY.DeadZone, HID_CTRL->StickY.Radius);

                        HID_CTRL->CStickX.Offset    = ConfigGetValue( Data, INI_KYY_CSTICKX, 0 );
                        HID_CTRL->CStickX.DeadZone    = ConfigGetValue( Data, INI_KYY_CSTICKX, 1 );
                        HID_CTRL->CStickX.Radius    = ConfigGetDecValue( Data, INI_KYY_CSTICKX, 2 );
                        if (HID_CTRL->CStickX.Radius == 0)
                            HID_CTRL->CStickX.Radius = 80;
                        HID_CTRL->CStickX.Radius = (u64)HID_CTRL->CStickX.Radius * 1280 / (128 - HID_CTRL->CStickX.DeadZone);    //adjust for DeadZone
                    //        dbgprintf("HID:CStickX: Offset=%3X Deadzone=%3X Radius=%d\r\n", HID_CTRL->CStickX.Offset, HID_CTRL->CStickX.DeadZone, HID_CTRL->CStickX.Radius);

                        HID_CTRL->CStickY.Offset    = ConfigGetValue( Data, INI_KYY_CSTICKY, 0 );
                        HID_CTRL->CStickY.DeadZone    = ConfigGetValue( Data, INI_KYY_CSTICKY, 1 );
                        HID_CTRL->CStickY.Radius    = ConfigGetDecValue( Data, INI_KYY_CSTICKY, 2 );
                        if (HID_CTRL->CStickY.Radius == 0)
                            HID_CTRL->CStickY.Radius = 80;
                        HID_CTRL->CStickY.Radius = (u64)HID_CTRL->CStickY.Radius * 1280 / (128 - HID_CTRL->CStickY.DeadZone);    //adjust for DeadZone
                    //        dbgprintf("HID:CStickY: Offset=%3X Deadzone=%3X Radius=%d\r\n", HID_CTRL->CStickY.Offset, HID_CTRL->CStickY.DeadZone, HID_CTRL->CStickY.Radius);

                        HID_CTRL->LAnalog    = ConfigGetValue( Data, INI_KYY_LANALOG, 0 );
                        HID_CTRL->RAnalog    = ConfigGetValue( Data, INI_KYY_RANALOG, 0 );

                        if(ConfigGetValue( Data, INI_KYY_RUMBLE, 0 ))
                        {
                            RawRumbleDataLen = ConfigGetValue( Data, INI_KYY_RUMBLE_DATA_LEN, 0 );
                            if(RawRumbleDataLen > 0)
                            {
                                RumbleEnabled = 1;
                                u32 DataAligned = (RawRumbleDataLen+31) & (~31);

                                if (RawRumbleDataOn == NULL) RawRumbleDataOn = (u8*)iosAlloc(hId, DataAligned);
                                memset(RawRumbleDataOn, 0, DataAligned);
                                ConfigGetValue( Data, INI_KYY_RUMBLE_DATA_ON, 3 );

                                if (RawRumbleDataOff == NULL) RawRumbleDataOff = (u8*)iosAlloc(hId, DataAligned);
                                memset(RawRumbleDataOff, 0, DataAligned);
                                ConfigGetValue( Data, INI_KYY_RUMBLE_DATA_OFF, 4 );

                                RumbleType = ConfigGetValue( Data, INI_KYY_RUMBLE_TYPE, 0 );
                                RumbleTransferLen = ConfigGetValue( Data, INI_KYY_RUMBLE_TRANS_LEN, 0 );
                                RumbleTransfers = ConfigGetValue( Data, INI_KYY_RUMBLE_TRANS, 0 );
                            }
                        }
                        iosFree(hId, Data);

                        dbgprintf("HID:Config file for VID:%04X PID:%04X loaded\r\n", HID_CTRL->VID, HID_CTRL->PID );
                    }

                }

                if( HID_CTRL->Polltype == 0 )
                    MemPacketSize = 128;
                else
                    MemPacketSize = wMaxPacketSize;

                if (Packet == NULL) Packet = (u8*)iosAlloc(hId, MemPacketSize);
                memset(Packet, 0, MemPacketSize);

                memset(HID_Packet, 0, MemPacketSize);

                bool Polltype = HID_CTRL->Polltype;
                if(HID_CTRL->Polltype)
                    HIDRead = HIDIRQRead;
                else
                    HIDRead = HIDPS3Read;

                if((HID_CTRL->VID == 0x057E) && (HID_CTRL->PID == 0x0337))
                {
                    HIDRumble = HIDGCRumble;
                    RumbleEnabled = true;
                }
                else if(RumbleEnabled)
                {
                    if(Polltype)
                    {
                        if(RumbleType)
                            HIDRumble = HIDIRQRumble;
                        else
                            HIDRumble = HIDCTRLRumble;
                    }
                    else
                        HIDRumble = HIDPS3Rumble;
                }

                HIDControllerConnected = 1;
                hidControllerConnected = 1;
                if (HIDKeyboardConnected)
                    break;
            }
        }
    }

    iosFree(hId, io_buffer);
    iosFree(hId, HIDHeap);

    #ifdef DISP_DEBUG
    sprintf(txtbuffer, "HIDOpen ConnInfo %d %d \r\n", HIDControllerConnected, HIDKeyboardConnected);
    writeLogFile(txtbuffer);
    #endif // DISP_DEBUG
    hidread = 0;
    if( !HIDControllerConnected )
    {
        dbgprintf("HID:No controller connected!\r\n");
        ControllerID = 0;
    }
    else //(re)start reading
    {
        memset((void*)HID_STATUS, 0, 0x20);
        *((u32*)(HID_STATUS)) = 1;
        //sync_after_write((void*)HID_STATUS, 0x20);
        if(HID_CTRL->Polltype)
        {
            HIDInterruptMessage(0, Packet, wMaxPacketSize, bEndpointAddressController, READ_CONTROLLER_MSG);
            //hidread = 1;
        }
        else
        {
            HIDControlMessage(0, Packet, SS_DATA_LEN, USB_REQTYPE_INTERFACE_GET,
                USB_REQ_GETREPORT, (USB_REPTYPE_INPUT<<8) | 0x1, READ_CONTROLLER_MSG);
            //hidread = 1;
        }
    }

    keyboardread = 0;
    if ( !HIDKeyboardConnected )
    {
        dbgprintf("HID:No keyboard connected!\r\n");
        KeyboardID = 0;
        memset(kb_input, 0, 8);
    }
    else {
        //(re)start reading
        HIDInterruptMessage(1, kbbuf, 8, bEndpointAddressKeyboard, READ_KEYBOARD_MSG);
        //keyboardread = 1;
    }

    return 0;
}

void HIDClose(int closeType)
{
    if (closeType == 0)
    {
        hidRun = 0;
        hidchange = 0;
        hidattach = 0;
        hidattached = 0;
    }
    else
    {
        usleep(300);

        if (HID_Thread != LWP_THREAD_NULL)
        {
            LWP_JoinThread(HID_Thread, NULL);
            HID_Thread = LWP_THREAD_NULL;
        }

        if (HIDHandle)
        {
            IOS_Close(HIDHandle);
            HIDHandle = -1;
        }

        if (ps3buf != NULL) iosFree(hId, ps3buf);
        if (gcbuf != NULL) iosFree(hId, gcbuf);
        if (kbbuf != NULL) iosFree(hId, kbbuf);

        if (read_ctrl_req != NULL) iosFree(hId, read_ctrl_req);
        if (write_ctrl_req != NULL) iosFree(hId, write_ctrl_req);
        if (read_irq_req != NULL) iosFree(hId, read_irq_req);
        if (write_irq_req != NULL) iosFree(hId, write_irq_req);
        if (read_kb_ctrl_req != NULL) iosFree(hId, read_kb_ctrl_req);
        if (write_kb_ctrl_req != NULL) iosFree(hId, write_kb_ctrl_req);
        if (read_kb_irq_req != NULL) iosFree(hId, read_kb_irq_req);
        if (write_kb_irq_req != NULL) iosFree(hId, write_kb_irq_req);
        if (comMsg != NULL) iosFree(hId, comMsg);

        if (AttachedDevices != NULL) iosFree(hId, AttachedDevices);
        if (Packet != NULL) iosFree(hId, Packet);
        if (RawRumbleDataOn != NULL) iosFree(hId, RawRumbleDataOn);
        if (RawRumbleDataOff != NULL) iosFree(hId, RawRumbleDataOff);
    }
}

int IsHidRuning(void)
{
    return hidRun;
}

static u32 *HIDRun(void *param)
{
    while (hidRun)
    {
        extern char shutdown;
        if (shutdown)
        {
            break;
        }
        HIDUpdateRegisters(1);
        usleep(800);
    }
    return 0;
}

static s32 HIDControlMessage(u32 isKBreq, u8 *Data, u32 Length, u32 RequestType, u32 Request, u32 Value, s32 msgData)
{
    u8 request_dir = !!(RequestType & USB_CTRLTYPE_DIR_DEVICE2HOST);
    struct _usb_msg *msg;

    if(isKBreq)
    {
        msg = request_dir ? read_kb_ctrl_req : write_kb_ctrl_req;
        msg->fd = KeyboardID;
    }
    else
    {
        msg = request_dir ? read_ctrl_req : write_ctrl_req;
        msg->fd = ControllerID;
    }

    msg->ctrl.bmRequestType = RequestType;
    msg->ctrl.bmRequest = Request;
    msg->ctrl.wValue = Value;
    msg->ctrl.wIndex = 0;
    msg->ctrl.wLength = Length;
    msg->ctrl.rpData = Data;

    msg->vec[0].data = msg;
    msg->vec[0].len = 64;
    msg->vec[1].data = Data;
    msg->vec[1].len = Length;

    s32 ret;
    if (msgData != 0)
    {
        msg->msgData = msgData;
        ret = IOS_IoctlvAsync(HIDHandle, ControlMessage, 2-request_dir, request_dir, msg->vec, ipcCallBack, msg);
    }
    else
    {
        ret = IOS_Ioctlv(HIDHandle, ControlMessage, 2-request_dir, request_dir, msg->vec);
    }

    return ret;
}

static s32 HIDInterruptMessage(u32 isKBreq, u8 *Data, u32 Length, u32 Endpoint, s32 msgData)
{
    u8 endpoint_dir = !!(Endpoint & USB_ENDPOINT_IN);
    struct _usb_msg *msg;

    if(isKBreq)
    {
        msg = endpoint_dir ? read_kb_irq_req : write_kb_irq_req;
        msg->fd = KeyboardID;
    }
    else
    {
        msg = endpoint_dir ? read_irq_req : write_irq_req;
        msg->fd = ControllerID;
    }
    msg->hid_intr_dir = !endpoint_dir;

    msg->vec[0].data = msg;
    msg->vec[0].len = 64;
    msg->vec[1].data = Data;
    msg->vec[1].len = Length;

    s32 ret;
    if (msgData != 0)
    {
        msg->msgData = msgData;
        ret = IOS_IoctlvAsync(HIDHandle, InterruptMessage, 2-endpoint_dir, endpoint_dir, msg->vec, ipcCallBack, msg);
    }
    else
    {
        ret = IOS_Ioctlv(HIDHandle, InterruptMessage, 2-endpoint_dir, endpoint_dir, msg->vec);
    }

    return ret;
}

static void HIDGCInit()
{
    s32 ret = HIDInterruptMessage(0, gcbuf, 1, bEndpointAddressOut, 0);
    if( ret < 0 )
    {
        #ifdef DISP_DEBUG
        sprintf(txtbuffer, "HIDGCInit error %08x\r\n", ret);
        writeLogFile(txtbuffer);
        #endif // DISP_DEBUG
    }
}

static void HIDPS3Init()
{
    u8 *buf = (u8*)iosAlloc(hId, 32);
    memset( buf, 0, 0x20 );
    s32 ret = HIDControlMessage(0, buf, 17, USB_REQTYPE_INTERFACE_GET,
            USB_REQ_GETREPORT, (USB_REPTYPE_FEATURE<<8) | 0xf2, 0);
    if( ret < 0 )
    {
        #ifdef DISP_DEBUG
        sprintf(txtbuffer, "HIDPS3Init error %08x\r\n", ret);
        writeLogFile(txtbuffer);
        #endif // DISP_DEBUG
    }
    iosFree(hId, buf);
}

static void HIDPS4Init()
{
    static u8 ps4Buf[] ATTRIBUTE_ALIGN(32) = {
        0x05, // Report ID
        0x03, 0x00, 0x00,
        0, // Fast motor
        0, // Slow motor
        0, 0, 32, // RGB
        0x00, // LED on duration
        0x00  // LED off duration
    };
    u8 *buf = (u8*)iosAlloc(hId, 32);
    memset( buf, 0, 0x20 );
    memcpy(buf, ps4Buf, sizeof(ps4Buf));

    HIDInterruptMessage(0, buf, sizeof(ps4Buf), bEndpointAddressController, HID_SET_LEDS);

    iosFree(hId, buf);
}

static void HIDPS3SetLED( u8 led )
{
    ps3buf[10] = ss_led_pattern[led];
    DCFlushRange((void*)ps3buf, 64);

    s32 ret = HIDInterruptMessage(0, ps3buf, sizeof(rawData), 0x02, 0);
    if( ret < 0 )
        dbgprintf("ES:IOS_Ioctl():%d\r\n", ret );
}

static void HIDPS3SetRumble( u8 duration_right, u8 power_right, u8 duration_left, u8 power_left)
{
    #ifdef DISP_DEBUG
    sprintf(txtbuffer, "PS3Rumble %d %d %d %d\r\n", duration_right, power_right, duration_left, power_left);
    DEBUG_print(txtbuffer, DBG_GPU3);
    #endif // DISP_DEBUG
    //ps3buf[2] = duration_right;
    ps3buf[3] = power_right;
    //ps3buf[4] = duration_left;
    ps3buf[5] = power_left;
    DCFlushRange((void*)ps3buf, 64);

    s32 ret = HIDInterruptMessage(0, ps3buf, sizeof(rawData), 0x02, HID_SET_RUMBLE);
}

static vu32 HIDRumbleCurrent = 0, HIDRumbleLast = 0;
static vu32 MotorCommand = 0x93003020;

static void HIDPS3Read()
{
    if( !PS3LedSet && Packet[4] )
    {
        HIDPS3SetLED(1);
        PS3LedSet = 1;
    }

    memcpy(HID_Packet, Packet, SS_DATA_LEN);
    DCFlushRange((void*)HID_Packet, SS_DATA_LEN);

    HIDControlMessage(0, Packet, SS_DATA_LEN, USB_REQTYPE_INTERFACE_GET,
            USB_REQ_GETREPORT, (USB_REPTYPE_INPUT<<8) | 0x1, READ_CONTROLLER_MSG);
    //hidread = 1;
}

static void HIDGCRumble(u32 input)
{
    #ifdef DISP_DEBUG
    sprintf(txtbuffer, "HIDGCRumble %08x\r\n", input);
    DEBUG_print(txtbuffer, DBG_GPU3);
    #endif // DISP_DEBUG
    gcbuf[0] = 0x11;
    gcbuf[1] = input & 1;
    gcbuf[2] = (input >> 1) & 1;
    gcbuf[3] = (input >> 2) & 1;
    gcbuf[4] = (input >> 3) & 1;
    DCFlushRange((void*)gcbuf, 32);

    HIDInterruptMessage(0, gcbuf, 5, bEndpointAddressOut, HID_SET_RUMBLE);
}

static void HIDIRQRumble(u32 Enable)
{
    #ifdef DISP_DEBUG
    sprintf(txtbuffer, "HIDIRQRumble %d\r\n", Enable);
    DEBUG_print(txtbuffer, DBG_GPU3);
    #endif // DISP_DEBUG
    u8 *buf = (Enable) ? RawRumbleDataOn : RawRumbleDataOff;
    u32 i = 0;
irqrumblerepeat:
    HIDInterruptMessage(0, buf, RumbleTransferLen, bEndpointAddressOut, HID_SET_RUMBLE);
    i++;
    if(i < RumbleTransfers)
    {
        buf += RumbleTransferLen;
        goto irqrumblerepeat;
    }
}

static void HIDCTRLRumble(u32 Enable)
{
    #ifdef DISP_DEBUG
    sprintf(txtbuffer, "HIDCTRLRumble %d\r\n", Enable);
    DEBUG_print(txtbuffer, DBG_GPU3);
    #endif // DISP_DEBUG
    u8 *buf = (Enable) ? RawRumbleDataOn : RawRumbleDataOff;
    u32 i = 0;
ctrlrumblerepeat:
    i++;
    HIDControlMessage(0, buf, RumbleTransferLen, USB_REQTYPE_INTERFACE_SET,
            USB_REQ_SETREPORT, (USB_REPTYPE_OUTPUT<<8) | 0x1, HID_SET_RUMBLE);
    if(i < RumbleTransfers)
    {
        buf += RumbleTransferLen;
        goto ctrlrumblerepeat;
    }
}

static void HIDIRQRead()
{
    switch( HID_CTRL->MultiIn )
    {
        default:
        case 0:    // MultiIn disabled
        case 3: // multiple controllers from a single adapter all controllers in 1 message
            break;
        case 1:    // match single controller filter on the first byte
            if (Packet[0] != HID_CTRL->MultiInValue)
                goto dohidirqread;
            break;
        case 2: // multiple controllers from a single adapter first byte contains controller number
            if ((Packet[0] < HID_CTRL->MultiInValue) || (Packet[0] > NIN_CFG_MAXPAD))
                goto dohidirqread;
            break;
    }
    memcpy(HID_Packet, Packet, wMaxPacketSize);
    DCFlushRange((void*)HID_Packet, wMaxPacketSize);
    #ifdef DISP_DEBUG
    //sprintf(txtbuffer, "HIDIRQRead %08x %08x %08x %08x \r\n", *(u32*)HID_Packet, *((u32*)HID_Packet + 1), *((u32*)HID_Packet + 2), *((u32*)HID_Packet + 3));
    //DEBUG_print(txtbuffer, DBG_GPU3);
    #endif // DISP_DEBUG
dohidirqread:
    HIDInterruptMessage(0, Packet, wMaxPacketSize, bEndpointAddressController, READ_CONTROLLER_MSG);
    //hidread = 1;
}

static void HIDPS3Rumble( u32 rumblePower )
{
    if (rumblePower)
    {
        HIDPS3SetRumble( 0xFF, 0, 0, 0xFF );
    }
    else
    {
        HIDPS3SetRumble( 0, 0, 0, 0 );
    }
}

static u32 ConfigGetValue( char *Data, const char *EntryName, u32 Entry )
{
    char *str = strstr( Data, EntryName );
    if( str == (char*)NULL )
    {
        dbgprintf("Entry:\"%s\" not found!\r\n", EntryName );
        return 0;
    }

    str += strlen(EntryName) + 1; // Skip '='

    char *strEnd = strchr( str, 0x0A );
    u32 ret = 0;
    u32 i;

    switch (Entry)
    {
        case 0:
            ret = strtoul(str, NULL, 16);
            break;

        case 1:
            str = strstr( str, "," );
            if( str == (char*)NULL || str > strEnd )
            {
                dbgprintf("No \",\" found in entry.\r\n");
                break;
            }

            str++; //Skip ,

            ret = strtoul(str, NULL, 16);
            break;

        case 2:
            str = strstr( str, "," );
            if( str == (char*)NULL || str > strEnd )
            {
                dbgprintf("No \",\" found in entry.\r\n");
                break;
            }

            str++; //Skip the first ,

            str = strstr( str, "," );
            if( str == (char*)NULL || str > strEnd )
            {
                dbgprintf("No \",\" found in entry.\r\n");
                break;
            }

            str++; //Skip the second ,

            ret = strtoul(str, NULL, 16);
            break;

        case 3:
            for(i = 0; i < RawRumbleDataLen; ++i)
            {
                RawRumbleDataOn[i] = strtoul(str, NULL, 16);
                str = strstr( str, "," )+1;
            }
            break;

        case 4:
            for(i = 0; i < RawRumbleDataLen; ++i)
            {
                RawRumbleDataOff[i] = strtoul(str, NULL, 16);
                str = strstr( str, "," )+1;
            }
            break;

        default:
            break;
    }

    return ret;
}

static u32 ConfigGetDecValue( char *Data, const char *EntryName, u32 Entry )
{
    char *str = strstr( Data, EntryName );
    if( str == (char*)NULL )
    {
        dbgprintf("Entry:\"%s\" not found!\r\n", EntryName );
        return 0;
    }

    str += strlen(EntryName) + 1; // Skip '='

    char *strEnd = strchr( str, 0x0A );
    u32 ret = 0;

    switch (Entry)
    {
        case 0:
            ret = strtoul(str, NULL, 10);
            break;

        case 1:
            str = strstr( str, "," );
            if( str == (char*)NULL || str > strEnd )
            {
                dbgprintf("No \",\" found in entry.\r\n");
                break;
            }

            str++; //Skip ,

            ret = strtoul(str, NULL, 10);
            break;

        case 2:
            str = strstr( str, "," );
            if( str == (char*)NULL  || str > strEnd )
            {
                dbgprintf("No \",\" found in entry.\r\n");
                break;
            }

            str++; //Skip the first ,

            str = strstr( str, "," );
            if( str == (char*)NULL  || str > strEnd )
            {
                dbgprintf("No \",\" found in entry.\r\n");
                break;
            }

            str++; //Skip the second ,

            ret = strtoul(str, NULL, 10);
            break;

        default:
            break;
    }

    return ret;
}

static void KeyboardRead()
{
    memcpy(kb_input, kbbuf, 8);
    HIDInterruptMessage(1, kbbuf, 8, bEndpointAddressKeyboard, READ_KEYBOARD_MSG);
}

static void HIDUpdateRegisters(u32 LoaderRequest)
{
    //u32 nextTimer = gettime();
    if (diff_usec(HID_Timer, gettime()) > 2500) // about 50 times a second
    {
        if (hidchange == 1)
        {
            hidattached = 0;
            //wait half a second for devices to
            //actually attach properly
            if (hidwaittimer < 60)
                hidwaittimer++;
            else
            {
                #ifdef DISP_DEBUG
                sprintf(txtbuffer, "AttachFinish\r\n");
                writeLogFile(txtbuffer);
                #endif // DISP_DEBUG
                hidchange = 0;
                hidwaittimer = 0;
                //If you dont do that it wont update anymore
                comMsg->msgData = HID_ATTACH_MSG;
                IOS_IoctlAsync(HIDHandle, AttachFinish, NULL, 0, NULL, 0, ipcCallBack, comMsg);
                //hidattach = 1;
            }
        }
        if (hidattach == 1)
        {
            hidattach = 0;
            hidattached = 1;
            HIDOpen(LoaderRequest);
            comMsg->msgData = HID_CHANGE_MSG;
            u32 checkRet = IOS_IoctlAsync(HIDHandle, GetDeviceChange, NULL, 0, AttachedDevices, ATTACHED_DEVICES_SIZE, ipcCallBack, comMsg);
            #ifdef DISP_DEBUG
            sprintf(txtbuffer, "GetDeviceChange %08x\r\n", checkRet);
            writeLogFile(txtbuffer);
            #endif // DISP_DEBUG
            //hidchange = 1;
        }
        if (hidattached)
        {
            if(RumbleEnabled)
            {
                HIDRumbleCurrent = *((u32*)(MotorCommand));
                if( HIDRumbleLast != HIDRumbleCurrent )
                {
                    if(HIDRumble) HIDRumble( HIDRumbleCurrent );
                    HIDRumbleLast = HIDRumbleCurrent;
                }
            }

            if (hidread == 1)
            {
                hidread = 0;
                if(HIDRead) HIDRead();
            }
            if(keyboardread == 1)
            {
                keyboardread = 0;
                KeyboardRead();
            }
        }
        HID_Timer = gettime();
    }
}

// Functions for synchronously reading data
// However, due to its impact on overall efficiency, it will not be used temporarily
void HIDReadData(void)
{
    if (hidattached)
    {
        if(hidread == 1)
        {
            hidread = 0;
            if(HIDRead) HIDRead();
        }
        if(keyboardread == 1)
        {
            keyboardread = 0;
            KeyboardRead();
        }
        if (RumbleEnabled)
        {
            HIDRumbleCurrent = *((u32*)(MotorCommand));
            if( HIDRumbleLast != HIDRumbleCurrent )
            {
                if(HIDRumble) HIDRumble( HIDRumbleCurrent );
                HIDRumbleLast = HIDRumbleCurrent;
            }
        }
    }
}
