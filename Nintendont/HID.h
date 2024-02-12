
#ifndef __HID_H__
#define __HID_H__

#ifdef __cplusplus
extern "C" {
#endif

#define HID_STATUS 0xD3003440
#define DEVICE_VID (HID_STATUS + 4)
#define DEVICE_PID (HID_STATUS + 8)

void HIDUpdateRegisters();

#ifdef __cplusplus
}
#endif

#endif
