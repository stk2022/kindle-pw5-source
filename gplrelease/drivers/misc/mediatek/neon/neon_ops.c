/*
 * Copyright 2020 Amazon Technologies, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/kernel.h>
#include <arm_neon.h>

#define ONE_GRAY_LEVEL 15  /* in 8-bit framebuffer unit */
void do_pixel_line(void* pixel, uint32_t *strength, uint32_t *transition, uint32_t width )
{
        uint32_t col;
        uint8_t *pixels;
        uint8x16x2_t reg;
        uint8x16_t abs_delta,mask_gray,mask_one,tran;
        uint16x8_t f2, stren;
        uint32x4_t f0;
        uint64x2_t f1;

        mask_gray = vdupq_n_u8(ONE_GRAY_LEVEL);
        mask_one = vdupq_n_u8(1);
        tran = vdupq_n_u8(0);
        stren = vdupq_n_u16(0); /* add 8-bit headroom, enough for 255*32 pixel width */

        pixels = (uint8_t *)pixel;
        for (col=0; col<width-32;col+=32,pixels+=32) {
                reg = vld2q_u8(pixels); /* load 16 pixels each even and odd column */
                abs_delta = vabdq_u8(reg.val[0], reg.val[1]); /* abs(diff) */
                stren = vpadalq_u8(stren, abs_delta);    /* strength */
                abs_delta = vcgtq_u8(abs_delta, mask_gray); /* only great than one gray level */
                abs_delta = vandq_u8(abs_delta,mask_one); 
                tran = vaddq_u8(tran, abs_delta);    /* one gray level transition */
        }

        /* de-vectorize */
        f2 = vpaddlq_u8(tran);
        f0 = vpaddlq_u16(f2);
        f1 = vpaddlq_u32(f0);
        col = (uint32_t) vgetq_lane_u64(f1,0);
        col += (uint32_t) vgetq_lane_u64(f1,1);
        *transition = col;

        f0 = vpaddlq_u16(stren);
        f1 = vpaddlq_u32(f0);
        col = (uint32_t) vgetq_lane_u64(f1,0);
        col += (uint32_t) vgetq_lane_u64(f1,1);
        *strength+= col;
}

EXPORT_SYMBOL(do_pixel_line);

