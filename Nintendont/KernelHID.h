#ifndef __HID_H__
#define __HID_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "global.h"
#include "PS3Controller.h"

#define C_NOT_SET	(0<<0)
#define HID_CTRL_ADDR                    0x93005000
#define HID_Packet_ADDR                  0x930050F0

#define USB_REPTYPE_FEATURE                             0x03
#define USB_REPTYPE_OUTPUT                              0x02
#define USB_REPTYPE_INPUT                               0x01


#define USB_REQ_GETREPORT                               0x01
#define USB_REQ_GETDESCRIPTOR                           0x06
#define USB_REQ_SETREPORT                               0x09


/* control message request type bitmask */
#define USB_CTRLTYPE_DIR_HOST2DEVICE    (0<<7)
#define USB_CTRLTYPE_DIR_DEVICE2HOST    (1<<7)
#define USB_CTRLTYPE_TYPE_STANDARD              (0<<5)
#define USB_CTRLTYPE_TYPE_CLASS                 (1<<5)
#define USB_CTRLTYPE_TYPE_VENDOR                (2<<5)
#define USB_CTRLTYPE_TYPE_RESERVED              (3<<5)
#define USB_CTRLTYPE_REC_DEVICE                 0
#define USB_CTRLTYPE_REC_INTERFACE              1
#define USB_CTRLTYPE_REC_ENDPOINT               2
#define USB_CTRLTYPE_REC_OTHER                  3


#define USB_REQTYPE_INTERFACE_GET       (USB_CTRLTYPE_DIR_DEVICE2HOST|USB_CTRLTYPE_TYPE_CLASS|USB_CTRLTYPE_REC_INTERFACE)
#define USB_REQTYPE_INTERFACE_SET       (USB_CTRLTYPE_DIR_HOST2DEVICE|USB_CTRLTYPE_TYPE_CLASS|USB_CTRLTYPE_REC_INTERFACE)

typedef struct Layout
{
	u32 Offset;
	u32 Mask;
} layout;

typedef struct StickLayout
{
	u32 	Offset;
	s8		DeadZone;
	u32		Radius;
} stickLayout;

typedef struct Controller
{
	u32 VID;
	u32 PID;
	u32 Polltype;
	u32 DPAD;
	u32 DPADMask;
	u32 DigitalLR;
	u32 MultiIn;
	u32 MultiInValue;

	layout Power;

	layout A;
	layout B;
	layout X;
	layout Y;
	layout ZL;
	layout Z;

	layout L;
	layout R;
	layout L2; // L2 for WiiStation
	layout R2; // R2 for WiiStation
	layout S;
	layout Select; // Select for WiiStation

	layout Left;
	layout Down;
	layout Right;
	layout Up;

	layout RightUp;
	layout DownRight;
	layout DownLeft;
	layout UpLeft;

	stickLayout StickX;
	stickLayout StickY;
	stickLayout CStickX;
	stickLayout CStickY;
	u32 LAnalog;
	u32 RAnalog;

} controller;

typedef struct Rumble {
	u32 VID;
	u32 PID;

	u32 RumbleType;
	u32 RumbleDataLen;
	u32 RumbleTransfers;
	u32 RumbleTransferLen;
	u8 *RumbleDataOn;
	u8 *RumbleDataOff;
} kernelRumble;

typedef struct {
	u8 padding[16]; // anything you want can go here
	s32 device_no;
	union {
		struct {
			u8 bmRequestType;
			u8 bmRequest;
			u16 wValue;
			u16 wIndex;
			u16 wLength;
		} control;
		struct {
			u32 endpoint;
			u32 dLength;
		} interrupt;
		struct {
			u8 bIndex;
		} string;
	};
	void *data; // virtual pointer, not physical!
} req_args; // 32 bytes

void HIDInit(u32 ios);
s32 HIDOpen();
void HIDClose();
void HIDUpdateRegisters(u32 LoaderRequest);
void HIDGCInit( void );
void HIDPS3Init( void );
void HIDPS3Read( void );
void HIDIRQRead( void );
void HIDPS3SetLED( u8 led );
void HIDGCRumble( u32 Enable );
void HIDPS3Rumble( u32 Enable );
void HIDIRQRumble( u32 Enable );
void HIDCTRLRumble( u32 Enable );
u32 ConfigGetValue( char *Data, const char *EntryName, u32 Entry );
u32 ConfigGetDecValue( char *Data, const char *EntryName, u32 Entry );
void HIDPS3SetRumble( u8 duration_right, u8 power_right, u8 duration_left, u8 power_left);
u32 HID_Run(void *arg);

typedef void (*RumbleFunc)(u32 Enable);
extern RumbleFunc HIDRumble;

u32 ReadHidData(u32 calledByGame);
void HIDUpdateControllerIni();

#ifdef __cplusplus
}
#endif
#endif
