/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cache.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/compat.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/platform_data/spi-mt65xx.h>
#include <mach/mtk_hifi4dsp_api.h>


/*
 * SPI command description.
 */
#define CMD_PWOFF			0x02 /* Power Off */
#define CMD_PWON			0x04 /* Power On */
#define CMD_RS				0x06 /* Read Status */
#define CMD_WS				0x08 /* Write Status */
#define CMD_CR				0x0a /* Config Read */
#define CMD_CW				0x0c /* Config Write */
#define CMD_RD				0x81 /* Read Data */
#define CMD_WD				0x0e /* Write Data */
#define CMD_CT				0x10 /* Config Type */
/*
 * SPI slave status register (to master).
 */
#define SLV_ON				BIT(0)
#define SR_CFG_SUCCESS		BIT(1)
#define SR_TXRX_FIFO_RDY	BIT(2)
#define SR_RD_ERR			BIT(3)
#define SR_WR_ERR			BIT(4)
#define SR_RDWR_FINISH		BIT(5)
#define SR_TIMOUT_ERR		BIT(6)
#define SR_CMD_ERR			BIT(7)
#define CONFIG_READY  ((SR_CFG_SUCCESS | SR_TXRX_FIFO_RDY))

/*
 * hardware limit for once transfter.
 */
#define MAX_SPI_XFER_SIZE_ONCE		(64 * 1024 - 1)
#define MAX_SPI_TRY_CNT			(10)

/*
 * default never pass more than 32 bytes
 */
#define MTK_SPI_BUFSIZ	max(32, SMP_CACHE_BYTES)

#define DEFAULT_SPI_MODE_QUAD	(2)
#define SPI_READ		     true
#define SPI_WRITE		     false
#define SPI_READ_STA_ERR_RET	(1)
#define DSP_SPIS1_CLKSEL_ADDR	(0x1d00e0cc)
#define SPI_FREQ_52M		(52*1000*1000)
#define SPI_FREQ_26M		(26*1000*1000)
#define SPI_FREQ_13M		(13*1000*1000)


static DEFINE_MUTEX(hifi4dsp_bus_lock);
static struct mtk_hifi4dsp_private_data hifi4dsp_data;


static int mtk_spi_write(struct spi_device *spi,
			const void *buf, size_t len, u32 speed)
{
	struct spi_transfer t = {
			.tx_buf		= buf,
			.len		= len,
			.tx_nbits	= SPI_NBITS_SINGLE,
			.rx_nbits	= SPI_NBITS_SINGLE,
			.speed_hz   = speed,
	};

	return spi_sync_transfer(spi, &t, 1);
}

static int mtk_spi_write_then_read(struct spi_device *spi,
			const void *txbuf, unsigned int n_tx,
			void *rxbuf, unsigned int n_rx, u32 speed)
{
	int status;
	struct spi_message message;
	struct spi_transfer x[2];
	u8 mtk_spi_buffer[MTK_SPI_BUFSIZ];
	u8 *local_buf;

	if ((n_tx + n_rx) > MTK_SPI_BUFSIZ) {
		local_buf = kmalloc((n_tx + n_rx), GFP_KERNEL | GFP_DMA);
		if (!local_buf)
			return -ENOMEM;
	} else {
		local_buf = mtk_spi_buffer;
		memset(local_buf, 0, MTK_SPI_BUFSIZ);
	}

	spi_message_init(&message);
	memset(x, 0, sizeof(x));
	if (n_tx) {
		x[0].len = n_tx;
		x[0].tx_nbits = SPI_NBITS_SINGLE;
		x[0].rx_nbits = SPI_NBITS_SINGLE;
		x[0].speed_hz = speed;
		spi_message_add_tail(&x[0], &message);
	}
	if (n_rx) {
		x[1].len = n_rx;
		x[1].tx_nbits = SPI_NBITS_SINGLE;
		x[1].rx_nbits = SPI_NBITS_SINGLE;
		x[1].speed_hz = speed;
		spi_message_add_tail(&x[1], &message);
	}

	memcpy(local_buf, txbuf, n_tx);
	x[0].tx_buf = local_buf;
	x[1].rx_buf = local_buf + n_tx;

	status = spi_sync(spi, &message);
	if (status == 0)
		memcpy(rxbuf, x[1].rx_buf, n_rx);

	if (local_buf != mtk_spi_buffer)
		kfree(local_buf);

	return status;
}

static int spi_config_type(struct spi_device *spi, int type, u32 speed)
{
	int status;
	u8 tx_cmd_type_single[] = {CMD_CT, 0x04};
	u8 tx_cmd_type_dual[]   = {CMD_CT, 0x05};
	u8 tx_cmd_type_quad[]   = {CMD_CT, 0x06};
	size_t len;
	void *buffer;

	if (type == 2) {
		buffer = tx_cmd_type_quad;
		len = ARRAY_SIZE(tx_cmd_type_quad);
	} else if (type == 1) {
		buffer = tx_cmd_type_dual;
		len = ARRAY_SIZE(tx_cmd_type_dual);
	} else if (type == 0) {
		buffer = tx_cmd_type_single;
		len = ARRAY_SIZE(tx_cmd_type_single);
	} else {
		status = -EINVAL;
		pr_warn("Input wrong type!\n");
		goto tail;
	}

	status = mtk_spi_write(spi, buffer, len, speed);
tail:
	if (status) {
		pr_err("config type err, line(%d), type(%d), ret(%d)\n",
				__LINE__, type, status);
	}

	return status;
}

static int spi_config_wr(struct spi_device *spi, u32 addr, int len, bool wr,
						u32 speed)
{
	int status;
	int i;
	u8 cmd_config[] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,};
	size_t size;
	void *buffer;
	u8 tx_cmd_read_sta[2] = {CMD_RS, 0x00};
	u8 rx_cmd_read_sta[2] = {0, 0};
	u8 read_status;
	int try = 0;

	/* Must setting for config CMD */
	spi->mode &= ~(SPI_TX_DUAL | SPI_TX_QUAD | SPI_RX_DUAL | SPI_RX_QUAD);

	if (wr)
		cmd_config[0] = CMD_CR;
	else
		cmd_config[0] = CMD_CW;

	for (i = 0; i < 4; i++) {
		cmd_config[1 + i] = (addr & (0xff << (i * 8))) >> (i * 8);
		cmd_config[5 + i] = ((len - 1) & (0xff << (i * 8))) >> (i * 8);
	}

loop:
	buffer = cmd_config;
	size = ARRAY_SIZE(cmd_config);
	status = mtk_spi_write(spi, buffer, size, speed);
	if (status)
		goto tail;

	/*
	 * Check SPI-Slave Read Status,
	 * SR_CFG_SUCCESS = 1 & SR_TXRX_FIFO_RDY = 1 ???
	 * maybe loop.
	 */
	memset(rx_cmd_read_sta, 0, ARRAY_SIZE(rx_cmd_read_sta));
	status = mtk_spi_write_then_read(spi,
				tx_cmd_read_sta, 2,
				rx_cmd_read_sta, 2, speed);
	if (status)
		goto tail;

	read_status = rx_cmd_read_sta[1];
	if ((read_status & CONFIG_READY) != CONFIG_READY) {
		pr_warn("SPI slave status error: 0x%x, line:%d\n",
				read_status, __LINE__);
		if (try++ <= MAX_SPI_TRY_CNT)
			goto loop;
	}

tail:
	if (status) {
		pr_err("config address & size fail, line(%d), len(%d), ret(%d)\n",
				__LINE__, (int)size, status);
	}
	return status;
}

static int spi_trigger_wr_data(struct spi_device *spi,
			int type, int len, bool wr, void *buf_store, u32 speed)
{
	int status;
	struct spi_message msg;
	struct spi_transfer xfer_buf;
	size_t size;
	void *local_buf = NULL;
	u8 mtk_spi_buffer[MTK_SPI_BUFSIZ];
	u8 tx_cmd_read_sta[2] = {CMD_RS, 0x00};
	u8 rx_cmd_read_sta[2] = {0, 0};
	u8 tx_cmd_write_sta[2] = {CMD_WS, 0x01};
	u8 rx_cmd_write_sta[2] = {0, 0};
	u8 read_status;

	if (!buf_store) {
		status = -EINVAL;
		goto tail;
	}

	size = len + 1;
	if (size > MTK_SPI_BUFSIZ) {
		local_buf = kzalloc(size, GFP_KERNEL);
		if (!local_buf) {
			status = -ENOMEM;
			pr_err("tx/rx malloc fail!, line:%d\n", __LINE__);
			goto tail;
		}
	} else {
		local_buf = mtk_spi_buffer;
		memset(local_buf, 0, MTK_SPI_BUFSIZ);
	}

	/* spi->mode only for data transfters */
	memset(&xfer_buf, 0, sizeof(xfer_buf));
	xfer_buf.tx_nbits = SPI_NBITS_SINGLE;
	xfer_buf.rx_nbits = SPI_NBITS_SINGLE;
	if (type == 1) {
		spi->mode |= SPI_TX_DUAL | SPI_RX_DUAL;
		xfer_buf.tx_nbits = SPI_NBITS_DUAL;
		xfer_buf.rx_nbits = SPI_NBITS_DUAL;
	} else if (type == 2) {
		spi->mode |= SPI_TX_QUAD | SPI_RX_QUAD;
		xfer_buf.tx_nbits = SPI_NBITS_QUAD;
		xfer_buf.rx_nbits = SPI_NBITS_QUAD;
	}
	xfer_buf.len = size;
	xfer_buf.speed_hz = speed;
	if (wr) {
		/* for dual/quad read */
		*((u8 *)local_buf) = CMD_RD;
		xfer_buf.tx_buf = local_buf;
		xfer_buf.rx_buf = local_buf;
	} else {
		/* for dual/quad write */
		*((u8 *)local_buf) = CMD_WD;
		memcpy((u8 *)local_buf + 1, buf_store, len);
		xfer_buf.tx_buf = local_buf;
	}

	spi_message_init(&msg);
	spi_message_add_tail(&xfer_buf, &msg);
	status = spi_sync(spi, &msg);
	if (status)
		goto tail;

	/*
	 * Check SPI-Slave Read Status,
	 * SR_RDWR_FINISH = 1 & RD_ERR/WR_ERR = 0 ???
	 * maybe loop (< 10).
	 */
	memset(rx_cmd_read_sta, 0, ARRAY_SIZE(rx_cmd_read_sta));
	/* Must CLEAR mode type for other transfters */
	spi->mode &= ~(SPI_TX_DUAL | SPI_TX_QUAD | SPI_RX_DUAL | SPI_RX_QUAD);
	status = mtk_spi_write_then_read(spi,
				tx_cmd_read_sta, 2,
				rx_cmd_read_sta, 2, speed);
	if (status)
		goto tail;

	read_status = rx_cmd_read_sta[1];
	if (((read_status & SR_RDWR_FINISH) != SR_RDWR_FINISH)
		|| ((read_status & SR_RD_ERR) == SR_RD_ERR)
		|| ((read_status & SR_WR_ERR) == SR_WR_ERR)) {
		pr_warn("SPI slave status error: 0x%x, line:%d\n",
				read_status, __LINE__);

		/* Write Status CMD if any error,
		 * then clear RD_ERR/WR_ERR.
		 */
		status = mtk_spi_write_then_read(spi,
					tx_cmd_write_sta, 2,
					rx_cmd_write_sta, 2, speed);
		if (status)
			goto tail;

		status = SPI_READ_STA_ERR_RET;
	}

tail:
	/* Only for successful read */
	if (wr && !status)
		memcpy(buf_store, ((u8 *)xfer_buf.rx_buf + 1), len);

	if (local_buf != mtk_spi_buffer)
		kfree(local_buf);

	if (status)
		pr_err("write/read to slave err, line(%d), len(%d), ret(%d)\n",
				__LINE__, len, status);

	return status;
}

#ifdef CONFIG_MTK_HIFI4DSP_CHECK_DSP_DVFS
static int dsp_check_spis1_clk(u32 *value, u32 speed)
{
	int ret, try = 0, len = 4;
	int type = DEFAULT_SPI_MODE_QUAD;
	struct spi_device *spi = hifi4dsp_data.spi_bus_data[0];
	u32 addr = DSP_SPIS1_CLKSEL_ADDR;

	pr_debug("%s addr = 0x%08x, len = %d\n", __func__, addr, len);

	mutex_lock(&hifi4dsp_bus_lock);
	ret = spi_config_type(spi, type, speed);
	if (ret < 0) {
		pr_debug("SPI config type fail!, line:%d\n", __LINE__);
		goto tail;
	}
spis_check_config_read:
	ret = spi_config_wr(spi, addr, len, SPI_READ, speed);
	if (ret < 0) {
		pr_debug("SPI config read fail!, line:%d\n", __LINE__);
		goto tail;
	}
	ret = spi_trigger_wr_data(spi, type, len, SPI_READ, (u8 *)value, speed);
	if (ret < 0) {
		pr_debug("SPI read data error!, line:%d\n", __LINE__);
		goto tail;
	}
	if (ret > 0) {
		if (try++ < MAX_SPI_TRY_CNT)
			goto spis_check_config_read;
		else
			pr_debug("SPI read failed, retry count > %d, line:%d\n",
				MAX_SPI_TRY_CNT, __LINE__);
	}

tail:
	mutex_unlock(&hifi4dsp_bus_lock);
	return ret;
}

static int spi_select_speed(u32 spis_clk_sel, int target_speed,
			    int *selected_speed) {
	if (spis_clk_sel & (0x7 << 17)) {
		if (target_speed > SPI_FREQ_52M) {
			pr_debug("Availabel max spi speed is 52MHz!\n");
			*selected_speed = SPI_FREQ_52M;
		} else
			*selected_speed = target_speed;
		return 0;
	} else if (spis_clk_sel & (0x1 << 20)) {
		if (target_speed > SPI_FREQ_26M) {
			pr_debug("Availabel max spi speed is 26MHz!\n");
			*selected_speed = SPI_FREQ_26M;
		} else
			*selected_speed = target_speed;
		return 0;
	} else if (spis_clk_sel & (0xe1 << 16)) {
		if (target_speed > SPI_FREQ_13M) {
			pr_debug("Availabel max spi speed is 13MHz!\n");
			*selected_speed = SPI_FREQ_13M;
		} else
			*selected_speed = target_speed;
		return 0;
	}

	return -1;
}
#endif

int dsp_spi_write(u32 addr, void *value, int len, u32 speed)
{
	int ret, try = 0, xfer_speed;
	int type = DEFAULT_SPI_MODE_QUAD;
	struct spi_device *spi = hifi4dsp_data.spi_bus_data[0];
	void *tx_store;

#ifdef CONFIG_MTK_HIFI4DSP_CHECK_DSP_DVFS
	u32 dsp_spis1_clksel_reg;
#endif

	pr_debug("%s addr = 0x%08x, len = %d\n", __func__, addr, len);
	xfer_speed = speed;

#ifdef CONFIG_MTK_HIFI4DSP_CHECK_DSP_DVFS
	if (speed > SPI_FREQ_13M) {
		ret = dsp_check_spis1_clk(&dsp_spis1_clksel_reg, SPI_SPEED_LOW);
		if (ret < 0) {
			pr_debug("SPI check dsp spis1 failed! line:%d\n",
				 __LINE__);
			return ret;
		}

		ret = spi_select_speed(dsp_spis1_clksel_reg, speed,
				       &xfer_speed);
		if (ret < 0) {
			pr_debug("DSP SPIs clk err! line:%d\n", __LINE__);
			return ret;
		}
	}
#endif

	mutex_lock(&hifi4dsp_bus_lock);
	ret = spi_config_type(spi, type, xfer_speed);
	if (ret < 0) {
		pr_debug("SPI config type fail! line:%d\n", __LINE__);
		goto tail;
	}
spi_config_write:
	ret = spi_config_wr(spi, addr, len, SPI_WRITE, xfer_speed);
	if (ret < 0) {
		pr_debug("SPI config write fail! line:%d\n", __LINE__);
		goto tail;
	}
	tx_store = value;
	ret = spi_trigger_wr_data(spi, type, len, SPI_WRITE, tx_store,
				  xfer_speed);
	if (ret < 0) {
		pr_debug("SPI write data error! line:%d\n", __LINE__);
		goto tail;
	}
	if (ret > 0) {
		if (try++ < MAX_SPI_TRY_CNT)
			goto spi_config_write;
		else
			pr_debug("SPI write fail, retry count > %d, line:%d\n",
				 MAX_SPI_TRY_CNT, __LINE__);
	}

tail:
	mutex_unlock(&hifi4dsp_bus_lock);
	return ret;
}

int dsp_spi_write_ex(u32 addr, void *value, int len, u32 speed)
{
	int ret = 0;
	int res_len;
	int once_len;
	int loop;
	int cycle;
	u32 new_addr;
	u8 *new_buf;

	once_len = MAX_SPI_XFER_SIZE_ONCE;
	cycle = len / once_len;
	res_len = len % once_len;

	for (loop = 0; loop < cycle; loop++) {
		new_addr = addr + once_len * loop;
		new_buf = (u8 *)value + once_len * loop;
		ret = dsp_spi_write(new_addr, new_buf, once_len, speed);
		if (ret)
			pr_debug("dsp_spi_write() fail! line:%d\n", __LINE__);
	}

	if (res_len) {
		new_addr = addr + once_len * loop;
		new_buf = (u8 *)value + once_len * loop;
		ret = dsp_spi_write(new_addr, new_buf, res_len, speed);
		if (ret)
			pr_debug("dsp_spi_write() fail! line:%d\n", __LINE__);
	}

	return ret;
}

int dsp_spi_read(u32 addr, void *value, int len, u32 speed)
{
	int ret, try = 0, xfer_speed;
	int type = DEFAULT_SPI_MODE_QUAD;
	struct spi_device *spi = hifi4dsp_data.spi_bus_data[0];

#ifdef CONFIG_MTK_HIFI4DSP_CHECK_DSP_DVFS
	u32 dsp_spis1_clksel_reg;
#endif

	pr_debug("%s addr = 0x%08x, len = %d\n", __func__, addr, len);
	xfer_speed = speed;

#ifdef CONFIG_MTK_HIFI4DSP_CHECK_DSP_DVFS
	if (speed > SPI_FREQ_13M) {
		ret = dsp_check_spis1_clk(&dsp_spis1_clksel_reg, SPI_SPEED_LOW);
		if (ret < 0) {
			pr_debug("SPI check dsp spis1 failed! line:%d\n",
				 __LINE__);
			return ret;
		}

		ret = spi_select_speed(dsp_spis1_clksel_reg, speed,
				       &xfer_speed);
		if (ret < 0) {
			pr_debug("DSP SPIs clk err! line:%d\n", __LINE__);
			return ret;
		}
	}
#endif

	mutex_lock(&hifi4dsp_bus_lock);
	ret = spi_config_type(spi, type, xfer_speed);
	if (ret < 0) {
		pr_debug("SPI config type fail! line:%d\n", __LINE__);
		goto tail;
	}
spi_config_read:
	ret = spi_config_wr(spi, addr, len, SPI_READ, xfer_speed);
	if (ret < 0) {
		pr_debug("SPI config read fail! line:%d\n", __LINE__);
		goto tail;
	}
	ret = spi_trigger_wr_data(spi, type, len, SPI_READ, value, xfer_speed);
	if (ret < 0) {
		pr_debug("SPI read data error! line:%d\n", __LINE__);
		goto tail;
	}
	if (ret > 0) {
		if (try++ < MAX_SPI_TRY_CNT)
			goto spi_config_read;
		else
			pr_debug("SPI read fail, retry count > %d, line:%d\n",
				 MAX_SPI_TRY_CNT, __LINE__);
	}

tail:
	mutex_unlock(&hifi4dsp_bus_lock);
	return ret;
}

int dsp_spi_read_ex(u32 addr, void *value, int len, u32 speed)
{
	int ret = 0;
	int res_len;
	int once_len;
	int loop;
	int cycle;
	u32 new_addr;
	u8 *new_buf;

	once_len = MAX_SPI_XFER_SIZE_ONCE;
	cycle = len / once_len;
	res_len = len % once_len;

	for (loop = 0; loop < cycle; loop++) {
		new_addr = addr + once_len * loop;
		new_buf = (u8 *)value + once_len * loop;
		ret = dsp_spi_read(new_addr, new_buf, once_len, speed);
		if (ret)
			pr_debug("dsp_spi_read() fail! line:%d\n", __LINE__);
	}

	if (res_len) {
		new_addr = addr + once_len * loop;
		new_buf = (u8 *)value + once_len * loop;
		ret = dsp_spi_read(new_addr, new_buf, res_len, speed);
		if (ret)
			pr_debug("dsp_spi_read() fail! line:%d\n", __LINE__);
	}

	return ret;
}

int spi_read_register(u32 addr, u32 *val, u32 speed)
{
	return dsp_spi_read(addr, (u8 *)val, 4, speed);
}

int spi_write_register(u32 addr, u32 val, u32 speed)
{
	return dsp_spi_write(addr, (u8 *)&val, 4, speed);
}

int spi_set_register32(u32 addr, u32 val, u32 speed)
{
	u32 read_val;

	spi_read_register(addr, &read_val, speed);
	spi_write_register(addr, read_val | val, speed);
	return 0;
}

int spi_clr_register32(u32 addr, u32 val, u32 speed)
{
	u32 read_val;

	spi_read_register(addr, &read_val, speed);
	spi_write_register(addr, read_val & (~val), speed);
	return 0;
}

int spi_write_register_mask(u32 addr, u32 val, u32 msk, u32 speed)
{
	u32 read_val;

	spi_read_register(addr, &read_val, speed);
	spi_write_register(addr, ((read_val & (~(msk))) | ((val) & (msk))),
						speed);
	return 0;
}

void *get_hifi4dsp_private_data(void)
{
	return (void *)&hifi4dsp_data;
}

#define DSP_ADDR 0x1fc00000
int spi_multipin_loopback_transfer(int len, int xfer_speed)
{
	int ret = 0;
	void *tx_buf;
	void *rx_buf;
	int i, err = 0;

	tx_buf = kzalloc(len, GFP_KERNEL);
	rx_buf = kzalloc(len, GFP_KERNEL);

	for (i = 0; i < len; i++)
		*((char *)tx_buf + i) = i%255;
	memset(rx_buf, 0, len);

	if (xfer_speed == 13) {
		ret = dsp_spi_write_ex(DSP_ADDR, tx_buf, len, SPI_SPEED_LOW);
		if (ret < 0) {
			pr_debug("Write transfer err,line(%d):%d\n", __LINE__,
				 ret);
			goto tail;
		}
		ret = dsp_spi_read_ex(DSP_ADDR, rx_buf, len, SPI_SPEED_LOW);
		if (ret < 0) {
			pr_debug("Read transfer err,line(%d):%d\n", __LINE__,
				 ret);
			goto tail;
		}
	} else if (xfer_speed == 52) {
		dsp_spi_write_ex(DSP_ADDR, tx_buf, len, SPI_SPEED_HIGH);
		if (ret < 0) {
			pr_debug("Write transfer err,line(%d):%d\n", __LINE__,
				 ret);
			goto tail;
		}
		dsp_spi_read_ex(DSP_ADDR, rx_buf, len, SPI_SPEED_HIGH);
		if (ret < 0) {
			pr_debug("Read transfer err,line(%d):%d\n", __LINE__,
				 ret);
			goto tail;
		}
	} else {
		pr_debug("Unavailabel speed!\n");
		goto tail;
	}

	for (i = 0; i < len; i++) {
		if (*((char *)tx_buf+i) != *((char *)rx_buf+i)) {
			pr_debug("tx[%d]:0x%x, rx[%d]:0x%x\r\n",
				i, *((char *)tx_buf+i), i,
				*((char *)rx_buf + i));
			err++;
		}
	}

	pr_debug("total length %d bytes, err %d bytes.\n", len, err);

tail:
	kfree(tx_buf);
	kfree(rx_buf);

	if (ret < 0)
		return ret;

	return err;
}

static ssize_t hifi4dsp_spi_store(struct device *dev,
			 struct device_attribute *attr,
			 const char *buf, size_t count)
{
	int len, xfer_speed, ret;

	if (!strncmp(buf, "xfer", 4)) {
		buf += 5;
		if (!strncmp(buf, "speed=", 6) &&
			(sscanf(buf + 6, "%d", &xfer_speed) == 1)) {
			buf += 9;
			if (!strncmp(buf, "len=", 4) &&
				(sscanf(buf + 4, "%d", &len) == 1))
				ret = spi_multipin_loopback_transfer(len,
								xfer_speed);
		}
	}

	return count;
}

static DEVICE_ATTR(hifi4dsp_spi, 0200, NULL, hifi4dsp_spi_store);

static struct device_attribute *spi_attribute[] = {
	&dev_attr_hifi4dsp_spi,
};

static void spi_create_attribute(struct device *dev)
{
	int size, idx;

	size = ARRAY_SIZE(spi_attribute);
	for (idx = 0; idx < size; idx++)
		device_create_file(dev, spi_attribute[idx]);
}

static int hifi4dsp_probe(struct spi_device *spi)
{
	int err = 0;
	static struct task_struct *dsp_task;
	struct mtk_chip_config *data;
	struct mtk_hifi4dsp_private_data *pri_data = &hifi4dsp_data;

	pr_info("%s() enter.\n", __func__);

	data = kzalloc(sizeof(struct mtk_chip_config), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto tail;
	}

	/*
	 * Structure filled with mtk-spi crtical values.
	 */
	spi->mode = SPI_MODE_0;
	spi->mode &= ~(SPI_TX_DUAL | SPI_TX_QUAD | SPI_RX_DUAL | SPI_RX_QUAD);
	spi->bits_per_word = 8;
	data->rx_mlsb = 0;
	data->tx_mlsb = 0;
	data->command_cnt = 1;
	data->dummy_cnt = 0;
	spi->controller_data = (void *)data;

	/* Fill  structure mtk_hifi4dsp_private_data */
	pri_data->spi_bus_data[pri_data->spi_bus_idx++] = spi;
	dsp_task = NULL;
#if SELF_TEST_HIFI4DSP
	/*
	 * Always start the kthread because there may be much seconds
	 * for loading HIFI4DSP binary
	 * and booting up to FreeRTOS kernel shell.
	 */
	dsp_task = kthread_run(breed_hifi4dsp, &(spi->dev), "breed_hifi4dsp");
	if (IS_ERR(dsp_task)) {
		pr_info("Couldn't create kthread for breed_hifi4dsp.\n");
		err = PTR_ERR(dsp_task);
		goto tail;
	} else
		pr_notice("Start to run kthread [breed_hifi4dsp].\n");
#endif
	spi_create_attribute(&spi->dev);
tail:
	return err;
}

static int hifi4dsp_remove(struct spi_device *spi)
{
	pr_info("%s().\n", __func__);

	if (spi && spi->controller_data)
		kfree(spi->controller_data);

	return 0;
}

static const struct spi_device_id hifi4dsp_id[] = {
	{ "mt8570" },
	{}
};
MODULE_DEVICE_TABLE(spi, hifi4dsp_id);

static const struct of_device_id hifi4dsp_of_table[] = {
	{ .compatible = "mediatek,hifi4dsp" },
	{}
};
MODULE_DEVICE_TABLE(of, hifi4dsp_of_table);

static struct spi_driver hifi4dsp_driver = {
	.driver = {
		.name	= "hifi4dsp",
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = hifi4dsp_of_table,
	},
	.id_table	= hifi4dsp_id,
	.probe	= hifi4dsp_probe,
	.remove	= hifi4dsp_remove,
};

module_spi_driver(hifi4dsp_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dehui.sun@mediatek.com");
MODULE_DESCRIPTION("SPI driver for hifi4dsp chip");

