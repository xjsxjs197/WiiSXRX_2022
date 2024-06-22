#ifndef __HID_H__
#define __HID_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "global.h"
#include "PS3Controller.h"

#define C_NOT_SET	(0<<0)
#define HID_CTRL_ADDR                    0x93005000
#define HID_Packet_ADDR                  0x93005100

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

#define INI_KYY_A                    "KeyA"
#define INI_KYY_B                    "KeyB"
#define INI_KYY_X                    "KeyX"
#define INI_KYY_Y                    "KeyY"
#define INI_KYY_L1                   "KeyL1"
#define INI_KYY_L2                   "KeyL2"
#define INI_KYY_R1                   "KeyR1"
#define INI_KYY_R2                   "KeyR2"
#define INI_KYY_START                "KeyStart"
#define INI_KYY_SELECT               "KeySelect"
#define INI_KYY_LEFT                 "KeyLeft"
#define INI_KYY_DOWN                 "KeyDown"
#define INI_KYY_RIGHT                "KeyRight"
#define INI_KYY_UP                   "KeyUp"
#define INI_KYY_STICKX               "StickX"
#define INI_KYY_STICKY               "StickY"
#define INI_KYY_CSTICKX              "CStickX"
#define INI_KYY_CSTICKY              "CStickY"
#define INI_KYY_LANALOG              "LAnalog"
#define INI_KYY_RANALOG              "RAnalog"

#define INI_KYY_RIGHT_UP             "RightUp"
#define INI_KYY_DOWN_RIGHT           "DownRight"
#define INI_KYY_DOWN_LEFT            "DownLeft"
#define INI_KYY_UP_LEFT              "UpLeft"

#define INI_KYY_ZL                   "KeyZL"

#define INI_KYY_VID                  "VID"
#define INI_KYY_PID                  "PID"
#define INI_KYY_POLL_TYPE            "Polltype"
#define INI_KYY_DPAD                 "DPAD"
#define INI_KYY_DIGITAl_LR           "DigitalLR"
#define INI_KYY_MULTI_IN             "MultiIn"
#define INI_KYY_MULTI_IN_VAL         "MultiInValue"

#define INI_KYY_RUMBLE               "Rumble"
#define INI_KYY_RUMBLE_DATA_LEN      "RumbleDataLen"
#define INI_KYY_RUMBLE_DATA_ON       "RumbleDataOn"
#define INI_KYY_RUMBLE_DATA_OFF      "RumbleDataOff"
#define INI_KYY_RUMBLE_TYPE          "RumbleType"
#define INI_KYY_RUMBLE_TRANS_LEN     "RumbleTransferLen"
#define INI_KYY_RUMBLE_TRANS         "RumbleTransfers"

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
	layout L1; // L1 for WiiStation
	layout R1; // R1 for WiiStation
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
void HIDClose(int closeType);


u32 HidFormatData(void);
void HIDUpdateControllerIni();

void HIDReadData(void);
int IsHidRuning(void);

extern s32 hidControllerConnected;
extern s32 loadingControllerIni;

#ifdef __cplusplus
}
#endif
#endif
