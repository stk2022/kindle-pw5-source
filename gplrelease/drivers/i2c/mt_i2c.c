// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Housong Zhang <housong.zhang@mediatek.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <common.h>
#include <clk.h>
#include <dm.h>
#include <i2c.h>
#include <asm/io.h>

#define I2C_OK			0
#define EINVAL_I2C		22  /* invalid argument */
#define ETIMEDOUT_I2C		110 /* connection timed out */
#define EACK_I2C		6   /* remote I/O error */

#define I2C_RS_TRANSFER		BIT(4)
#define I2C_HS_NACKERR		BIT(2)
#define I2C_ACKERR		BIT(1)
#define I2C_TRANSAC_COMP	BIT(0)
#define I2C_TRANSAC_START	BIT(0)
#define I2C_RS_MUL_CNFG		BIT(15)
#define I2C_RS_MUL_TRIG		BIT(14)
#define I2C_DCM_DISABLE			0x0000
#define I2C_IO_CONFIG_OPEN_DRAIN	0x0003
#define I2C_IO_CONFIG_PUSH_PULL		0x0000
#define I2C_SOFT_RST			0x0001
#define I2C_FIFO_ADDR_CLR		0x0001
#define I2C_DELAY_LEN			0x0002
#define I2C_ST_START_CON		0x8001
#define I2C_FS_START_CON		0x1800
#define I2C_TIME_CLR_VALUE		0x0000
#define I2C_TIME_DEFAULT_VALUE		0x0003
#define I2C_WRRD_TRANAC_VALUE		0x0002
#define I2C_RD_TRANAC_VALUE		0x0001

#define I2C_DMA_CON_TX			0x0000
#define I2C_DMA_CON_RX			0x0001
#define I2C_DMA_START_EN		0x0001
#define I2C_DMA_INT_FLAG_NONE		0x0000
#define I2C_DMA_CLR_FLAG		0x0000
#define I2C_DMA_HARD_RST		0x0002

#define MAX_ST_MODE_SPEED		100000
#define MAX_FS_MODE_SPEED		400000
#define MAX_HS_MODE_SPEED		3400000
#define MAX_SAMPLE_CNT_DIV		8
#define MAX_STEP_CNT_DIV		64
#define MAX_HS_STEP_CNT_DIV		8
#define I2C_DEFAULT_CLK_DIV		4

#define MAX_I2C_ADDR		0x7F
#define MAX_I2C_LEN		0xFF
#define TRANS_ADDR_ONLY		BIT(8)
#define TRANSFER_TIMEOUT		50000  /* us */
#define I2C_FIFO_STAT1_MASK		0x001F
#define TIMING_SAMPLE_OFFSET	8
#define HS_SAMPLE_OFFSET	12
#define HS_STEP_OFFSET		8

#define I2C_CONTROL_WRAPPER		BIT(0)
#define I2C_CONTROL_RS			BIT(1)
#define I2C_CONTROL_DMA_EN		BIT(2)
#define I2C_CONTROL_CLK_EXT_EN		BIT(3)
#define I2C_CONTROL_DIR_CHANGE		BIT(4)
#define I2C_CONTROL_ACKERR_DET_EN	BIT(5)
#define I2C_CONTROL_TRANSFER_LEN_CHANGE BIT(6)
#define I2C_CONTROL_DMAACK		BIT(8)
#define I2C_CONTROL_ASYNC		BIT(9)

enum I2C_REGS_OFFSET {
	OFFSET_PORT = 0x0,
	OFFSET_SLAVE_ADDR = 0x04,
	OFFSET_INTR_MASK = 0x08,
	OFFSET_INTR_STAT = 0x0C,
	OFFSET_CONTROL = 0x10,
	OFFSET_TRANSFER_LEN = 0x14,
	OFFSET_TRANSAC_LEN  = 0x18,
	OFFSET_DELAY_LEN = 0x1C,
	OFFSET_TIMING = 0x20,
	OFFSET_START = 0x24,
	OFFSET_EXT_CONF = 0x28,
	OFFSET_FIFO_STAT1 = 0x2C,
	OFFSET_FIFO_STAT = 0x30,
	OFFSET_FIFO_THRESH = 0x34,
	OFFSET_FIFO_ADDR_CLR = 0x38,
	OFFSET_IO_CONFIG = 0x40,
	OFFSET_RSV_DEBUG = 0x44,
	OFFSET_HS = 0x48,
	OFFSET_SOFTRESET = 0x50,
	OFFSET_DCM_EN = 0x54,
	OFFSET_DEBUGSTAT = 0x64,
	OFFSET_DEBUGCTRL = 0x68,
	OFFSET_TRANSFER_LEN_AUX = 0x6C,
	OFFSET_CLOCK_DIV = 0x70,
	OFFSET_SCL_HL_RATIO = 0x74,
	OFFSET_SCL_HS_HL_RATIO = 0x78,
	OFFSET_SCL_MIS_COMP_POINT = 0x7C,
	OFFSET_STA_STOP_AC_TIME = 0x80,
	OFFSET_HS_STA_STOP_AC_TIME = 0x84,
	OFFSET_DATA_TIME = 0x88,
};

enum DMA_REGS_OFFSET {
	OFFSET_INT_FLAG = 0x0,
	OFFSET_INT_EN = 0x04,
	OFFSET_EN = 0x08,
	OFFSET_RST = 0x0C,
	OFFSET_CON = 0x18,
	OFFSET_TX_MEM_ADDR = 0x1C,
	OFFSET_RX_MEM_ADDR = 0x20,
	OFFSET_TX_LEN = 0x24,
	OFFSET_RX_LEN = 0x28,
};

enum mtk_trans_op {
	I2C_MASTER_WR = 1,
	I2C_MASTER_RD,
	I2C_MASTER_WRRD,
};

struct mtk_i2c_soc_data {
	u8 dma_sync: 1;
};

struct mtk_i2c_priv {
	/* set in i2c probe */
	void __iomem *base;			/* i2c base addr */
	void __iomem *pdmabase;		/* dma base address*/
	struct clk clk_main;		/* main clock for i2c bus */
	struct clk clk_dma;		    /* DMA clock for i2c via DMA */
	const struct mtk_i2c_soc_data *soc_data;
	enum mtk_trans_op op;
	bool   zero_len;
	bool   pushpull;            /* open drain */
	bool   filter_msg;          /* filter msg error log */
	bool   auto_restart;
	bool ignore_restart_irq;
	u32 speed;                 /* hz */
	u8 *tx_buff;
	u8 *rx_buff;
};

static inline void i2c_writel(struct mtk_i2c_priv *priv, u8 offset, u32 value)
{
	writel(value, (priv->base + offset));
}

static inline u32 i2c_readl(struct mtk_i2c_priv *priv, u8 offset)
{
	return readl(priv->base + offset);
}

static void mtk_i2c_clk_enable(struct mtk_i2c_priv *priv)
{
	clk_enable(&priv->clk_main);
	clk_enable(&priv->clk_dma);
}

static void mtk_i2c_clk_disable(struct mtk_i2c_priv *priv)
{
	clk_disable(&priv->clk_dma);
	clk_disable(&priv->clk_main);
}

static void mtk_i2c_init_hw(struct mtk_i2c_priv *priv)
{
	u16 control_reg;

	writel(I2C_DMA_HARD_RST, priv->pdmabase + OFFSET_RST);
	writel(I2C_DMA_CLR_FLAG, priv->pdmabase + OFFSET_RST);
	i2c_writel(priv, OFFSET_SOFTRESET, I2C_SOFT_RST);
	/* set ioconfig */
	if (priv->pushpull)
		i2c_writel(priv, OFFSET_IO_CONFIG, I2C_IO_CONFIG_PUSH_PULL);
	else
		i2c_writel(priv, OFFSET_IO_CONFIG, I2C_IO_CONFIG_OPEN_DRAIN);

	i2c_writel(priv, OFFSET_DCM_EN, I2C_DCM_DISABLE);
	control_reg = I2C_CONTROL_ACKERR_DET_EN | I2C_CONTROL_CLK_EXT_EN;
	if (priv->soc_data->dma_sync)
		control_reg |= I2C_CONTROL_DMAACK | I2C_CONTROL_ASYNC;
	i2c_writel(priv, OFFSET_CONTROL, control_reg);
	i2c_writel(priv, OFFSET_DELAY_LEN, I2C_DELAY_LEN);
}

static int mtk_i2c_calculate_speed(struct mtk_i2c_priv *priv,
				   u32 clk_src,
				   u32 target_speed,
				   u32 *timing_step_cnt,
				   u32 *timing_sample_cnt)
{
	u32 step_cnt;
	u32 sample_cnt;
	u32 max_step_cnt;
	u32 base_sample_cnt = MAX_SAMPLE_CNT_DIV;
	u32 base_step_cnt;
	u32 opt_div;
	u32 best_mul;
	u32 cnt_mul;

	if (target_speed > MAX_HS_MODE_SPEED)
		target_speed = MAX_HS_MODE_SPEED;

	if (target_speed > MAX_FS_MODE_SPEED)
		max_step_cnt = MAX_HS_STEP_CNT_DIV;
	else
		max_step_cnt = MAX_STEP_CNT_DIV;

	base_step_cnt = max_step_cnt;
	/* Find the best combination */
	opt_div = DIV_ROUND_UP(clk_src >> 1, target_speed);
	best_mul = MAX_SAMPLE_CNT_DIV * max_step_cnt;

	/* Search for the best pair (sample_cnt, step_cnt) with
	 * 0 < sample_cnt < MAX_SAMPLE_CNT_DIV
	 * 0 < step_cnt < max_step_cnt
	 * sample_cnt * step_cnt >= opt_div
	 * optimizing for sample_cnt * step_cnt being minimal
	 */
	for (sample_cnt = 1; sample_cnt <= MAX_SAMPLE_CNT_DIV; sample_cnt++) {
		step_cnt = DIV_ROUND_UP(opt_div, sample_cnt);
		cnt_mul = step_cnt * sample_cnt;
		if (step_cnt > max_step_cnt)
			continue;

		if (cnt_mul < best_mul) {
			best_mul = cnt_mul;
			base_sample_cnt = sample_cnt;
			base_step_cnt = step_cnt;
			if (best_mul == opt_div)
				break;
		}
	}

	sample_cnt = base_sample_cnt;
	step_cnt = base_step_cnt;

	if ((clk_src / (2 * sample_cnt * step_cnt)) > target_speed) {
		/* In this case, hardware can't support such
		 * low i2c_bus_freq
		 */
		debug("Unsupported speed(%uhz)\n", target_speed);
		return -EINVAL;
	}

	*timing_step_cnt = step_cnt - 1;
	*timing_sample_cnt = sample_cnt - 1;
	return 0;
}

static int mtk_i2c_set_speed(struct udevice *dev, uint speed)
{
	u32 clk_src;
	u32 step_cnt;
	u32 sample_cnt;
	u16 timing_reg;
	u16 high_speed_reg;
	int ret = 0;
	struct mtk_i2c_priv *priv = dev_get_priv(dev);

	priv->speed = speed;
	mtk_i2c_clk_enable(priv);
	clk_src = clk_get_rate(&priv->clk_main) /
			I2C_DEFAULT_CLK_DIV;
	i2c_writel(priv, OFFSET_CLOCK_DIV, (I2C_DEFAULT_CLK_DIV - 1));
	if (priv->speed > MAX_FS_MODE_SPEED) {
		/* Set master code speed register */
		ret = mtk_i2c_calculate_speed(priv, clk_src, MAX_FS_MODE_SPEED,
					      &step_cnt, &sample_cnt);
		if (ret < 0)
			goto exit;

		timing_reg = (sample_cnt << TIMING_SAMPLE_OFFSET) | step_cnt;
		writel(timing_reg, priv->base + OFFSET_TIMING);
		/* Set the high speed mode register */
		ret = mtk_i2c_calculate_speed(priv, clk_src, priv->speed,
					      &step_cnt, &sample_cnt);
		if (ret < 0)
			goto exit;

		high_speed_reg = I2C_TIME_DEFAULT_VALUE |
				(sample_cnt << HS_SAMPLE_OFFSET) |
				(step_cnt << HS_STEP_OFFSET);
		writel(high_speed_reg, priv->base + OFFSET_HS);
	} else {
		ret = mtk_i2c_calculate_speed(priv, clk_src, priv->speed,
					      &step_cnt, &sample_cnt);
		if (ret < 0)
			goto exit;

		timing_reg = (sample_cnt << TIMING_SAMPLE_OFFSET) | step_cnt;
		/* Disable the high speed transaction */
		high_speed_reg = I2C_TIME_CLR_VALUE;
		writel(timing_reg, priv->base + OFFSET_TIMING);
		writel(high_speed_reg, priv->base + OFFSET_HS);
	}
exit:
	mtk_i2c_clk_disable(priv);
	return ret;
}

static int mtk_i2c_do_transfer(struct mtk_i2c_priv *priv,
			       struct i2c_msg *msgs,
			       int num, int left_num)
{
	u16 addr_reg;
	u16 start_reg;
	u16 control_reg;
	u16 restart_flag = 0;
	u16	irq_stat = 0;
	u8 trans_error = 0;
	bool tmo = false;
	u8 *ptr = msgs->buf;
	u16 data_size = msgs->len;
	u32 tmo_poll = 0;
	int ret;

	if (priv->auto_restart)
		restart_flag = I2C_RS_TRANSFER;

	control_reg = i2c_readl(priv, OFFSET_CONTROL) &
		~(I2C_CONTROL_DIR_CHANGE | I2C_CONTROL_RS);

	if (priv->speed > MAX_FS_MODE_SPEED || num > 1)
		control_reg |= I2C_CONTROL_RS;

	if (priv->op == I2C_MASTER_WRRD)
		control_reg |= I2C_CONTROL_DIR_CHANGE | I2C_CONTROL_RS;
	control_reg |= I2C_CONTROL_DMA_EN;
	i2c_writel(priv, OFFSET_CONTROL, control_reg);

	/* set start condition */
	if (priv->speed <= MAX_ST_MODE_SPEED)
		i2c_writel(priv, OFFSET_EXT_CONF, I2C_ST_START_CON);
	else
		i2c_writel(priv, OFFSET_EXT_CONF, I2C_FS_START_CON);

	addr_reg = msgs->addr << 1;
	if (priv->op == I2C_MASTER_RD)
		addr_reg |= I2C_M_RD;
	if (priv->zero_len)
		i2c_writel(priv, OFFSET_SLAVE_ADDR, addr_reg | TRANS_ADDR_ONLY);
	else
		i2c_writel(priv, OFFSET_SLAVE_ADDR, addr_reg);

	/* clear interrupt status */
	i2c_writel(priv, OFFSET_INTR_STAT, restart_flag | I2C_HS_NACKERR |
						I2C_ACKERR | I2C_TRANSAC_COMP);
		i2c_writel(priv, OFFSET_FIFO_ADDR_CLR, I2C_FIFO_ADDR_CLR);

	/* enable interrupt */
	i2c_writel(priv, OFFSET_INTR_MASK, restart_flag | I2C_HS_NACKERR |
						I2C_ACKERR | I2C_TRANSAC_COMP);

	/* set transfer and transaction len */
	if (priv->op == I2C_MASTER_WRRD) {
		i2c_writel(priv, OFFSET_TRANSFER_LEN, msgs->len);
		i2c_writel(priv, OFFSET_TRANSFER_LEN_AUX,
			   (msgs + 1)->len);
		i2c_writel(priv, OFFSET_TRANSAC_LEN, I2C_WRRD_TRANAC_VALUE);
	} else {
		i2c_writel(priv, OFFSET_TRANSFER_LEN, msgs->len);
		i2c_writel(priv, OFFSET_TRANSAC_LEN, num);
	}

	/* prepare buffer data to start transfer */
	if (priv->op == I2C_MASTER_RD) {
		writel(I2C_DMA_INT_FLAG_NONE, priv->pdmabase +
		OFFSET_INT_FLAG);
		writel(I2C_DMA_CON_RX, priv->pdmabase + OFFSET_CON);
		priv->rx_buff = (u8 *)noncached_alloc(msgs->len,
				ARCH_DMA_MINALIGN);
		writel((u32)priv->rx_buff, priv->pdmabase +
		OFFSET_RX_MEM_ADDR);
		writel(msgs->len, priv->pdmabase + OFFSET_RX_LEN);
	} else if (priv->op == I2C_MASTER_WR) {
		writel(I2C_DMA_INT_FLAG_NONE, priv->pdmabase +
			  OFFSET_INT_FLAG);
		writel(I2C_DMA_CON_TX, priv->pdmabase + OFFSET_CON);
		priv->tx_buff = (u8 *)noncached_alloc(msgs->len,
				ARCH_DMA_MINALIGN);
		memcpy(priv->tx_buff, msgs->buf, msgs->len);
		writel((u32)priv->tx_buff, priv->pdmabase +
			  OFFSET_TX_MEM_ADDR);
		writel(msgs->len, priv->pdmabase + OFFSET_TX_LEN);
	} else {
		writel(I2C_DMA_CLR_FLAG, priv->pdmabase +
			OFFSET_INT_FLAG);
		writel(I2C_DMA_CLR_FLAG, priv->pdmabase + OFFSET_CON);
		priv->tx_buff = (u8 *)noncached_alloc(msgs->len,
				ARCH_DMA_MINALIGN);
		priv->rx_buff = (u8 *)noncached_alloc((msgs + 1)->len,
				ARCH_DMA_MINALIGN);
		memcpy(priv->tx_buff, msgs->buf, msgs->len);
		writel((u32)priv->tx_buff, priv->pdmabase +
			  OFFSET_TX_MEM_ADDR);
		writel((u32)priv->rx_buff, priv->pdmabase +
			  OFFSET_RX_MEM_ADDR);
		writel(msgs->len, priv->pdmabase + OFFSET_TX_LEN);
		writel((msgs + 1)->len, priv->pdmabase +
			  OFFSET_RX_LEN);
	}
	writel(I2C_DMA_START_EN, priv->pdmabase + OFFSET_EN);

	if (!priv->auto_restart) {
		start_reg = I2C_TRANSAC_START;
	} else {
		start_reg = I2C_TRANSAC_START | I2C_RS_MUL_TRIG;
		if (left_num >= 1)
			start_reg |= I2C_RS_MUL_CNFG;
	}
	i2c_writel(priv, OFFSET_START, start_reg);

	for (;;) {
		irq_stat = i2c_readl(priv, OFFSET_INTR_STAT);

		/* ignore the first restart irq after the master code */
		if (priv->ignore_restart_irq && (irq_stat | restart_flag)) {
			priv->ignore_restart_irq = false;
			irq_stat = 0;
			writel(I2C_RS_MUL_CNFG | I2C_RS_MUL_TRIG |
			       I2C_TRANSAC_START, priv->base + OFFSET_START);
		}

		if (irq_stat & (I2C_TRANSAC_COMP | restart_flag)) {
			tmo = false;
			if (irq_stat & (I2C_HS_NACKERR | I2C_ACKERR))
				trans_error = 1;

			break;
		}
		udelay(1);
		if (tmo_poll++ >= TRANSFER_TIMEOUT) {
			tmo = true;
			break;
		}
	}
	/* clear interrupt mask */
	i2c_writel(priv, OFFSET_INTR_MASK, ~(restart_flag | I2C_HS_NACKERR |
		  I2C_ACKERR | I2C_TRANSAC_COMP));

	if (!tmo && trans_error == 0) {
		/* transfer success ,we need to get data from fifo */
		if (priv->op == I2C_MASTER_RD || priv->op == I2C_MASTER_WRRD) {
			ptr = priv->op == I2C_MASTER_RD ?
					msgs->buf : (msgs + 1)->buf;
			data_size = priv->op == I2C_MASTER_RD ?
					msgs->len : (msgs + 1)->len;
			memcpy(ptr, priv->rx_buff, data_size);
		}
	} else {
		/* timeout or ACKERR */
		if (tmo) {
			ret = -ETIMEDOUT_I2C;
			if (!priv->filter_msg)
				debug("transfer timeout! addr: 0x%x,\n",
				      msgs->addr);
		} else {
			ret = -EACK_I2C;
			if (!priv->filter_msg)
				debug("ACKERR! addr: 0x%x,IRQ:0x%x\n",
				      msgs->addr, irq_stat);
		}
		mtk_i2c_init_hw(priv);
		return ret;
	}
	return 0;
}

static int mtk_i2c_transfer(struct udevice *dev, struct i2c_msg *msg,
			    int nmsgs)
{
	int ret;
	int left_num;
	u8 num_cnt;
	struct mtk_i2c_priv *priv = dev_get_priv(dev);

	priv->auto_restart = true;
	left_num = nmsgs;
	mtk_i2c_clk_enable(priv);
	for (num_cnt = 0; num_cnt < nmsgs; num_cnt++) {
		if (((msg + num_cnt)->addr) > MAX_I2C_ADDR) {
			ret = -EINVAL_I2C;
			goto err_exit;
		}
		if ((msg + num_cnt)->len > MAX_I2C_LEN) {
			ret = -EINVAL_I2C;
			goto err_exit;
		}
	}

	/* checking if we can skip restart and optimize using WRRD mode */
	if (priv->auto_restart && nmsgs == 2) {
		if (!(msg[0].flags & I2C_M_RD) && (msg[1].flags & I2C_M_RD) &&
		    msg[0].addr == msg[1].addr) {
			priv->auto_restart = false;
		}
	}

	if (priv->auto_restart && nmsgs >= 2 && priv->speed > MAX_FS_MODE_SPEED)
		/* ignore the first restart irq after the master code,
		 * otherwise the first transfer will be discarded.
		 */
		priv->ignore_restart_irq = true;
	else
		priv->ignore_restart_irq = false;

	while (left_num--) {
		/*priv->zero_len = true
		 *transfer slave address only to support devices detect
		 */
		if (!msg->buf)
			priv->zero_len = true;
		else
			priv->zero_len = false;

		if (msg->flags & I2C_M_RD)
			priv->op = I2C_MASTER_RD;
		else
			priv->op = I2C_MASTER_WR;

		if (!priv->auto_restart) {
			if (nmsgs > 1) {
				/* combined two messages into one transaction */
				priv->op = I2C_MASTER_WRRD;
				left_num--;
			}
		}
		ret = mtk_i2c_do_transfer(priv, msg, nmsgs, left_num);
		if (ret < 0)
			goto err_exit;
		msg++;
	}
	ret = I2C_OK;

err_exit:
	mtk_i2c_clk_disable(priv);
	return ret;
}

static int mtk_i2c_ofdata_to_platdata(struct udevice *dev)
{
	int ret;
	struct mtk_i2c_priv *priv = dev_get_priv(dev);

	priv->base = dev_remap_addr_index(dev, 0);
	priv->pdmabase = dev_remap_addr_index(dev, 1);
	ret = clk_get_by_index(dev, 0, &priv->clk_main);
	if (ret)
		return ret;

	ret = clk_get_by_index(dev, 1, &priv->clk_dma);

	return ret;
}

static int mtk_i2c_probe(struct udevice *dev)
{
	struct mtk_i2c_priv *priv = dev_get_priv(dev);

	priv->soc_data = (struct mtk_i2c_soc_data *)dev_get_driver_data(dev);
	mtk_i2c_clk_enable(priv);
	mtk_i2c_init_hw(priv);
	mtk_i2c_clk_disable(priv);
	return 0;
}

static int mtk_i2c_deblock(struct udevice *dev)
{
	struct mtk_i2c_priv *priv = dev_get_priv(dev);

	mtk_i2c_clk_enable(priv);
	mtk_i2c_init_hw(priv);
	mtk_i2c_clk_disable(priv);
	return 0;
}

static const struct mtk_i2c_soc_data mt8518_soc_data = {
	.dma_sync = 0,
};

static const struct mtk_i2c_soc_data mt8512_soc_data = {
	.dma_sync = 1,
};

static const struct dm_i2c_ops mtk_i2c_ops = {
	.xfer		= mtk_i2c_transfer,
	.set_bus_speed	= mtk_i2c_set_speed,
	.deblock	= mtk_i2c_deblock,
};

static const struct udevice_id mtk_i2c_ids[] = {
	{
		.compatible = "mediatek,mt8518-i2c",
		.data = (ulong)&mt8518_soc_data,
	},
	{
		.compatible = "mediatek,mt8512-i2c",
		.data = (ulong)&mt8512_soc_data,
	},
	{
	}
};

U_BOOT_DRIVER(mtk_i2c) = {
	.name		= "mtk_i2c",
	.id		= UCLASS_I2C,
	.of_match	= mtk_i2c_ids,
	.ofdata_to_platdata = mtk_i2c_ofdata_to_platdata,
	.probe		= mtk_i2c_probe,
	.priv_auto_alloc_size = sizeof(struct mtk_i2c_priv),
	.ops		= &mtk_i2c_ops,
};
