/*
 * mt8518-snd-utils.h  --  Mediatek 8518 sound utility
 *
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Hidalgo Huang <hidalgo.huang@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "mt8518-snd-utils.h"
#include <sound/core.h>
#include <sound/control.h>


static struct snd_kcontrol *snd_ctl_find_name(struct snd_card *card,
	unsigned char *name)
{
	struct snd_kcontrol *kctl;

	if (!card || !name) {
		pr_info("%s null handle\n", __func__);
		return NULL;
	}

	list_for_each_entry(kctl, &card->controls, list) {
		if (!strncmp(kctl->id.name, name, sizeof(kctl->id.name)))
			return kctl;
	}

	return NULL;
}

int mt8518_snd_ctl_notify(struct snd_card *card,
	unsigned char *ctl_name, unsigned int mask)
{

	struct snd_kcontrol *kctl;

	kctl = snd_ctl_find_name(card, ctl_name);
	if (!kctl) {
		pr_info("%s can not find ctl %s\n", __func__, ctl_name);
		return -1;
	}

	snd_ctl_notify(card, mask, &kctl->id);

	return 0;
}

