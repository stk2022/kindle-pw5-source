/*
 * 
 */
#ifndef __LINUX_FRONTLIGHT_H
#define __LINUX_FRONTLIGHT_H 

#include <linux/mxcfb.h>
#include <linux/backlight.h>

#define FL_DEV_FILE "/dev/frontlight"

#define FL_MAGIC_NUMBER         'L'

#define FL_IOCTL_SET_INTENSITY  _IOW(FL_MAGIC_NUMBER, 0x01, int)
#define FL_IOCTL_GET_INTENSITY  _IOR(FL_MAGIC_NUMBER, 0x02, int)
#define FL_IOCTL_GET_RANGE_MAX  _IOR(FL_MAGIC_NUMBER, 0x03, int)
#define FL_IOCTL_SET_INTENSITY_FORCED  _IOW(FL_MAGIC_NUMBER, 0x04, int)
#define FL_IOCTL_SET_INTENSITY_AMBER_1  _IOW(FL_MAGIC_NUMBER, 0x05, int)
#define FL_IOCTL_GET_INTENSITY_AMBER_1  _IOR(FL_MAGIC_NUMBER, 0x06, int)
#define FL_IOCTL_GET_RANGE_MAX_AMBER_1 _IOR(FL_MAGIC_NUMBER, 0x07, int)
#define FL_IOCTL_SET_INTENSITY_FORCED_AMBER_1  _IOW(FL_MAGIC_NUMBER, 0x08, int)
#define FL_IOCTL_SET_INTENSITY_AMBER_2  _IOW(FL_MAGIC_NUMBER, 0x09, int)
#define FL_IOCTL_GET_INTENSITY_AMBER_2  _IOR(FL_MAGIC_NUMBER, 0x0a, int)
#define FL_IOCTL_GET_RANGE_MAX_AMBER_2 _IOR(FL_MAGIC_NUMBER, 0x0b, int)
#define FL_IOCTL_SET_INTENSITY_FORCED_AMBER_2  _IOW(FL_MAGIC_NUMBER, 0x0c, int)
#define FL_IOCTL_SET_INTENSITY_AMBER  _IOW(FL_MAGIC_NUMBER, 0x0d, int)
#define FL_IOCTL_GET_INTENSITY_AMBER  _IOR(FL_MAGIC_NUMBER, 0x0e, int)
#define FL_IOCTL_GET_RANGE_MAX_AMBER _IOR(FL_MAGIC_NUMBER, 0x0f, int)
#define FL_IOCTL_SET_INTENSITY_FORCED_AMBER  _IOW(FL_MAGIC_NUMBER, 0x10, int)

#define WARIO_FL_LEVEL0                     0
#define WARIO_FL_LEVEL12_MID                512
#define DUET_FL_LEVEL12_MID                 240
#define WARIO_FL_LO_TRANSITION_LEVEL        42
#define WARIO_FL_LO_GRP_HOP_LEVEL           1
#define WARIO_FL_MED_GRP_HOP_LEVEL          10
#define WARIO_FL_LO_GRP_DELAY_US            1000

enum frontlight_color {
	FRONTLIGHT_COLOR_WHITE,
	FRONTLIGHT_COLOR_AMBER,	
	FRONTLIGHT_COLOR_LAST,
};

extern int frontlight_register(struct backlight_device *device, enum frontlight_color fl_color);
extern int fl_switch(struct mxcfb_nightmode_ctrl *night_mode, const bool on_off);

#endif
