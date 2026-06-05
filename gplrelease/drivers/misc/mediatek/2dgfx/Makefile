#
# Copyright (C) 2018 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.
#

obj-y += gfx_manager.o gfx_drv.o gfx_if.o gfx_dif.o gfx_cmdque.o gfx_hw.o

ifeq ($(CONFIG_MTK_PLATFORM),y)
CFLAGS_gfx_manager.o := -DDEBUG
CFLAGS_gfx_drv.o := -DDEBUG
CFLAGS_gfx_if.o := -DDEBUG
CFLAGS_gfx_dif.o := -DDEBUG
CFLAGS_gfx_cmdque.o := -DDEBUG
CFLAGS_gfx_hw.o := -DDEBUG
endif
