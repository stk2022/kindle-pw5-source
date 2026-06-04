#ifndef _SPM_MTCMOS_
#define _SPM_MTCMOS_

#define STA_POWER_DOWN  0
#define STA_POWER_ON    1

/*
 * for non-CPU MTCMOS:
 */
int spm_mtcmos_ctrl_conn(int state);
int spm_mtcmos_ctrl_mm(int state);
int spm_mtcmos_ctrl_img(int state);
int spm_mtcmos_ctrl_ip0(int state);
int spm_mtcmos_ctrl_ip1(int state);
int spm_mtcmos_ctrl_ip2(int state);
int spm_mtcmos_ctrl_usb_mac_p1(int state);
int spm_mtcmos_ctrl_dsp(int state);
int spm_mtcmos_ctrl_audio(int state);
int spm_mtcmos_ctrl_asrc(int state);

int dump_pwr_status(void);
#endif
