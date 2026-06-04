#ifndef _DDC_I2C_H_
#define _DDC_I2C_H_

void vHdmiDDCGetGpio(void);
void vI2CDelay2us(int ui4Value);
void vInitDDC2Line(void);
void vSetDDC2Clock(char bClck);

#define FG_SEQREAD 1
#define FG_RANDREAD 0
#define DDC_CLK_DEFAULT 66
//54khz

#define EDID_ID     0x50
//0xA0
#define EDID_ID1    0x51
//0xA2

#define FALSE 0
#define TRUE 1

#endif
