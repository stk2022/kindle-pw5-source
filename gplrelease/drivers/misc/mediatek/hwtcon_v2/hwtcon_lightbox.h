#ifndef __HWTCON_LIGHTBOX_H__
#define __HWTCON_LIGHTBOX_H__

#include "hwtcon_core.h"

int hwtcon_lightbox_ioctl_set_lightbox_ctrl(void *arg);
void hwtcon_lightbox_apply_lightbox(struct hwtcon_task *task);

#endif /* __HWTCON_LIGHTBOX_H__ */