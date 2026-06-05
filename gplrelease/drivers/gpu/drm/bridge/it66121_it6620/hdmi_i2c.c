
#include "hdmi_i2c.h"
#include <linux/delay.h>
#include <linux/of_gpio.h>

#include <linux/string.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>

#define DDC_RETRY_COUNT 1
#define ACK_DELAY  200     // time out for acknowledge check
#define BUS_DELAY  255     // time out bus arbitration

static int _u4DDC2_Clk = 3;//for about 54khz DDC

int hdmi_ddc2c_pin, hdmi_ddc2d_pin;

#define SET_DDC2D_OUT (gpio_direction_output(hdmi_ddc2d_pin, 1))
#define SET_DDC2D_IN (gpio_direction_input(hdmi_ddc2d_pin))

#define SET_DDC2C_OUT (gpio_direction_output(hdmi_ddc2c_pin, 1))
#define SET_DDC2C_IN (gpio_direction_input(hdmi_ddc2c_pin))

#define SET_DDC2D (gpio_direction_input(hdmi_ddc2d_pin))
#define SET_DDC2C (gpio_direction_input(hdmi_ddc2c_pin))

#define CLR_DDC2D (gpio_direction_output(hdmi_ddc2d_pin, 0))
#define CLR_DDC2C (gpio_direction_output(hdmi_ddc2c_pin, 0))

#define DDC2D (gpio_get_value(hdmi_ddc2d_pin))
#define DDC2C (gpio_get_value(hdmi_ddc2c_pin))

static unsigned int HDMI_DDC_LOG_on;

#define HDMI_DDC_LOG(fmt, arg...) \
	do { \
		if (HDMI_DDC_LOG_on) { \
			pr_debug("[hdmi_ddc]%s,%d ", __func__, __LINE__); \
			pr_debug(fmt, ##arg); \
		} \
	} while (0)

void vHdmiDDCGetGpio(void)
{
	int ret;
	struct device_node *np = NULL;

	np = of_find_compatible_node(NULL, NULL, "mediatek,mt8518-hdmitx");
	if (np == NULL)
		HDMI_DDC_LOG("np == NULL,cannot find node\n");

	hdmi_ddc2c_pin = of_get_named_gpio(np, "hdmi_ddc2c_pin", 0);
	ret = gpio_request(hdmi_ddc2c_pin, "hdmi DDC SCL pin");
	if (ret)
		HDMI_DDC_LOG("hdmi DDC SCL pin, failure of setting\n");

	hdmi_ddc2d_pin = of_get_named_gpio(np, "hdmi_ddc2d_pin", 0);
	ret = gpio_request(hdmi_ddc2d_pin, "hdmi DDC SDA pin");
	if (ret)
		HDMI_DDC_LOG("hdmi DDC SDA pin, failure of setting\n");

	vInitDDC2Line();
}
EXPORT_SYMBOL(vHdmiDDCGetGpio);

void vInitDDC2Line(void)
{
//when init : set input mode, whether need to enhance gpio current?
	SET_DDC2D_IN;
	SET_DDC2C_IN;

	vSetDDC2Clock(DDC_CLK_DEFAULT);
}
/************************************************************************
 *Function : void vSetDDCClock(char bClck)
 *Description : Set DDC Clock
 *Parameter : None
 *Return    : None
 ***********************************************************************
 */
void vSetDDC2Clock(char bClck)
{
	int u4Temp;

	u4Temp = (int)((1000/(int)bClck)/5);

	if (u4Temp < 3)
		_u4DDC2_Clk = 3;//max 66k
	else
		_u4DDC2_Clk = u4Temp;
}

/************************************************************************
 *Function : void vI2CDelay2us(char bValue)
 *Description : Delay Routine (bValue = 1 --> 2us, 2->4us)
 *Parameter : bDelayValue
 *Return    : None
 ***********************************************************************
 */
void vI2CDelay2us(int ui4Value) // I2C speed control
{
	int ui4Cnt;

	for (ui4Cnt = 0; ui4Cnt < ui4Value; ui4Cnt++)
		udelay(2);
}


/************************************************************************
 *Function : int fgDDCSend(char bValue)
 *Description : Send Routine
 *              timing : SCL ___|^|___|^|__~__|^|___|^|__
 *                      SDA __/D7 \_/D6 \_~_/D0 \_/ACK\_
 * Parameter : bValue(8-bit output data)
 *  Return    : TRUE  : successful with ACK from slave
 *             FALSE  : bus (DDCC = 0) or ACK failure
 **********************************************************************
 */
int fgDDC2Send(char bValue)
{
	char bBitMask = 0x80;
	int dwTime;

// step 1 : 8-bit data transmission
	while (bBitMask) {
		if (bBitMask & bValue)
			SET_DDC2D;
		else
			CLR_DDC2D;

		vI2CDelay2us(_u4DDC2_Clk);
		SET_DDC2C;
		// data clock in, input
		vI2CDelay2us(_u4DDC2_Clk);
		CLR_DDC2C;
		// ready for next clock in, output
		vI2CDelay2us(_u4DDC2_Clk);
		bBitMask = bBitMask >> 1;
		// MSB first & timing delay
	}
	// step 2 : slave acknowledge check
	SET_DDC2D;
	// release SDA for ACK polling, input
	vI2CDelay2us(_u4DDC2_Clk);
	SET_DDC2C;
	// start ACK polling, input
	dwTime = ACK_DELAY;
	// time out protection
	vI2CDelay2us(_u4DDC2_Clk);
	//SET_DDCD_IN;
	while (DDC2D && --dwTime)
		;  // wait for ACK, SDA=0 or bitMask=0->jump to this loop

	CLR_DDC2C;
	// end ACK polling, out
	vI2CDelay2us(_u4DDC2_Clk);

	if (dwTime) {
		return TRUE;
		// return TRUE if ACK detected
	} else {
		return FALSE;
		// return FALSE if time out
	}
}

/************************************************************************
 *Function : void vDDCRead(char *prValue, int fgSeq_Read)
 *Description : Read Routine
 *timing : SCL ___|^|___|^|__~__|^|___|^|__
 *                       SDA __/D7 \_/D6 \_~_/D0 \_/ACK\_
 *   Parameter : *prValue(8-bit input pointer value)
 *   Return    : NONE
 **********************************************************************
 */
void vDDC2Read(char *prValue, int fgSeq_Read)
{
	char bBitMask = 0x80;

	*prValue = 0;
	// reset data buffer
	SET_DDC2D;
	// make sure SDA released
	vI2CDelay2us(_u4DDC2_Clk);

// step 1 : 8-bit data reception
	while (bBitMask) {
		SET_DDC2C;
		// data clock out
		vI2CDelay2us(_u4DDC2_Clk);
		if (DDC2D) {
			*prValue = *prValue | bBitMask;
			// Get all data
		}
		// non-zero bits to buffer
		CLR_DDC2C;
		// ready for next clock out
		vI2CDelay2us(_u4DDC2_Clk);
		bBitMask = bBitMask >> 1;
		// shift bit mask & clock delay
	}

// step 2 : acknowledgment to slave
	if (fgSeq_Read) {
		CLR_DDC2D;
		// ACK here for Sequential Read
	} else {
		SET_DDC2D;
		// NACK here (for single byte read)
	}

	vI2CDelay2us(_u4DDC2_Clk);
	SET_DDC2C;
	// NACK clock out
	vI2CDelay2us(_u4DDC2_Clk);
	CLR_DDC2C;
	// ready for next clock out
	vI2CDelay2us(_u4DDC2_Clk);
	SET_DDC2D;
	// release SDA
	vI2CDelay2us(_u4DDC2_Clk);

}

/************************************************************************
 *Function : BOOL fgDDCStart(char bValue, int fgRead)
 *Description : Start Routine
 *             timing : SCL ^^^^|___|^|___|^|__~__|^|___|^|___|^|__~
 *                      SDA ^^|____/A6 \_/A5 \_~_/A0 \_/R/W\_/ACK\_~
 *                           (S)
 *                            |<--- start condition
 *  Parameter : bDevice(7-bit slave address) + fgRead(R/W bit)
 *  Return    : TRUE  : successful with ACK from slave
 *             FALSE  : bus (DDCC = 0) or ACK failure
 ***********************************************************************
 */
int fgDDC2Start(char bDevice)
{
	int dwBusDelayTemp = BUS_DELAY;


	SET_DDC2D;
	//input,  make sure SDA released
	vI2CDelay2us(_u4DDC2_Clk);
	SET_DDC2C;
	// make sure SCL released

	vI2CDelay2us(_u4DDC2_Clk);

	while ((!DDC2C) && (--dwBusDelayTemp))
		;  // simple bus abritration

	if (!dwBusDelayTemp) {
		return FALSE;
		// time out protection & timing delay
	}

	CLR_DDC2D;
	// start condition here, output
	vI2CDelay2us(_u4DDC2_Clk);
	CLR_DDC2C;
	// ready for clocking, output
	vI2CDelay2us(_u4DDC2_Clk);

	return fgDDC2Send(bDevice);
	// slave address & R/W transmission
}

/************************************************************************
 *Function : void vDDCStop(void)
 *Description : Stop Routine
 *             timing : SCL ___|^^^^^
 *                     SDA xx___|^^^
 *                              (P)
 *                                |<--- stop condition
 *  Parameter : NONE
 *  Return    : NONE
 ***********************************************************************
 */
void vDDC2Stop(void)
{

	CLR_DDC2D;          // ready for stop condition

	vI2CDelay2us(_u4DDC2_Clk);
	SET_DDC2C;          // ready for stop condition

	vI2CDelay2us(_u4DDC2_Clk);
	SET_DDC2D;          // stop condition here
	vI2CDelay2us(_u4DDC2_Clk);

}



/***************************************************
 *Function : int fgDDCDataWrite(char bDevice, char bData_Addr,
 *                                 char bDataCount, char *prData)
 *Description : ByteWrite Routine
 * Parameter : bDevice -> Device Address
 *             bData_Addr -> Data Address
 *             bDataCount -> Data Content Cont
 *              *prData -> Data Content Pointer
 * Return    : TRUE  : successful with ACK from slave
 *              FALSE  : bus (DDCC = 0) or ACK failure
 ******************************************************
 */
int fgDDC2DataWrite(char bDevice, char bData_Addr,
	char bDataCount, char *prData)
{
	char bRetry, bOri_Device, bError = 0, bOri_count;

	if (bDevice >= 128) {
		vDDC2Stop();
		return FALSE;             // Device Address exceeds the range
	}

	bOri_Device = bDevice;
	bOri_count = bDataCount;
	for (bRetry = 0; bRetry < DDC_RETRY_COUNT; bRetry++) {
		bDevice = bOri_Device;
		bDevice = bDevice << 1;
		// Shift the 7-bit address to 7+1 format

		//sFlagI2C.fgBusBusy = 1;

		if (!fgDDC2Start(bDevice)) {
			//sFlagI2C.fgBusBusy = 0;
			vDDC2Stop();
			continue; // Device Address exceeds the range
		}

		if (!fgDDC2Send(bData_Addr)) {
			// Word Address
			//sFlagI2C.fgBusBusy = 0;
			vDDC2Stop();
			continue;// Device Address exceeds the range
		}

		bDataCount = bOri_count;
		bError = 0;
		while (bDataCount) {
			if (!fgDDC2Send(*(prData++))) {
				// Data Content Write
				//sFlagI2C.fgBusBusy = 0;
				vDDC2Stop();
				bError = 1;
				break;// Device Address exceeds the range
			}
			bDataCount--;
		}

		if (bError)
			continue;
		else {
			vDDC2Stop();
			//sFlagI2C.fgBusBusy = 0;
			return TRUE;
		}
	}

	vDDC2Stop();
	//sFlagI2C.fgBusBusy = 0;
	return FALSE;
}
EXPORT_SYMBOL(fgDDC2DataWrite);

/************************************************************************
 *   Function : int fgI2CDataRead(char bDevice, char bData_Addr,
 *                                  char bDataCount, char *prData)
 *Description : DataRead Routine
 *  Parameter : bDevice -> Device Address
 *              bData_Addr -> Data Address
 *             bDataCount -> Data Content Cont
 *              *prData -> Data Content Pointer
 *  Return    : TRUE  : successful with ACK from slave
 *             FALSE  : bus (SCL = 0) or ACK failure
 **********************************************************************
 */
int fgDDC2DataRead(char bDevice, char bData_Addr, char bDataCount,
	char *prData)
{
	char bDeviceOri;
	//WORD wPos;
	char bRetry;
	//char bEDIDReadIndex;
	char bTemp;

	bDeviceOri = bDevice;


	// Step 1 : Dummy Write
	if (bDevice >= 128) {
		vDDC2Stop();
		return FALSE;
	}

	for (bRetry = 0; bRetry < DDC_RETRY_COUNT; bRetry++) {
		bDevice = bDeviceOri;
		if (bDevice > EDID_ID) {
			//Max'0619'04, 4-block EEDID reading
			if (!fgDDC2Start(0x60)) {
				// Write Command
				//sFlagI2C.fgBusBusy = 0;
				vDDC2Stop();
				continue;// Start fail
			}
			if (!fgDDC2Send(bDevice-EDID_ID)) {
				// Word Address
				//sFlagI2C.fgBusBusy = 0;
				vDDC2Stop();
				continue;// Data Address Fail
			}
			bDevice = EDID_ID;
		}

		bDevice = bDevice << 1;
		// Shift the 7-bit address to 7+1 format

		if (!fgDDC2Start(bDevice)) {
			// sFlagI2C.fgBusBusy = 0;
			vDDC2Stop();
			continue;
		}
		if (!fgDDC2Send(bData_Addr)) {
			//sFlagI2C.fgBusBusy = 0;
			vDDC2Stop();
			continue;
		}

		// Step 2 : Real Read
		bDevice = bDevice + 1;
		// Shift the 7-bit address to 7+1 format
		if (!fgDDC2Start(bDevice)) {
			//sFlagI2C.fgBusBusy = 0;
			vDDC2Stop();
			continue;
		}

		while (bDataCount) {
			if (bDataCount == 1)
				vDDC2Read(&bTemp, FG_RANDREAD);
			else
				vDDC2Read(&bTemp, FG_SEQREAD);
			*prData = bTemp;
			prData++;
			bDataCount--;
		}
		// Step 3 : Stop
		vDDC2Stop();
		//sFlagI2C.fgBusBusy = 0;
		return TRUE;
	}

	//retry fail
	// sFlagI2C.fgBusBusy = 0;
	vDDC2Stop();
	return FALSE;
}
EXPORT_SYMBOL(fgDDC2DataRead);
/******************************************************************************/
char buff[512];

void mt_ddc_debug_write(const char *pbuf)
{
	char *buf;
	unsigned int SDA, SCL;

	buf = (char *)pbuf;

	if (strncmp(buf, "ReadEdid", 8) == 0) {
		if (fgDDC2DataRead(0x50, 0x00, 128, buff) != TRUE)
			HDMI_DDC_LOG("read edid error\n");
	}

	if (strncmp(buf, "out_enable", 6) == 0) {
		HDMI_DDC_LOG("ddc-enable gpio\n");
		SET_DDC2D_OUT;
		SET_DDC2C_OUT;
	}

	if (strncmp(buf, "out_disable", 7) == 0) {
		HDMI_DDC_LOG("ddc-disable gpio\n");
		CLR_DDC2D;
		CLR_DDC2C;
	}

	if (strncmp(buf, "in", 7) == 0) {
		HDMI_DDC_LOG("ddc-set gpio input direction\n");
		SET_DDC2D_IN;
		SET_DDC2C_IN;
		SDA = DDC2D;
		SCL = DDC2C;
		HDMI_DDC_LOG("SDA = %ud\n", SDA);
		HDMI_DDC_LOG("SCL = %ud\n", SCL);
	}

}



static void process_dbg_opt(const char *opt)
{
		mt_ddc_debug_write(opt);
}
static void process_dbg_cmd(char *cmd)
{
	char *tok;

	pr_debug("[extd] %s\n", cmd);

	while ((tok = strsep(&cmd, " ")) != NULL)
		process_dbg_opt(tok);
}



struct dentry *ddc_dbgfs;

static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static char debug_buffer[128];

static ssize_t debug_read(struct file *file,
	char __user *ubuf, size_t count, loff_t *ppos)
{
	const int debug_bufmax = sizeof(buff) - 1;

	buff[debug_bufmax] = 0;

	return simple_read_from_buffer(ubuf, count, ppos, buff, debug_bufmax);
}

static ssize_t debug_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	const int debug_bufmax = sizeof(debug_buffer) - 1;
	size_t ret;

	ret = count;

	if (count > debug_bufmax)
		count = debug_bufmax;

	if (copy_from_user(&debug_buffer, ubuf, count))
		return -EFAULT;

	debug_buffer[count] = 0;

	process_dbg_cmd(debug_buffer);

	return ret;
}


static const struct file_operations debug_fops = {
	.read = debug_read,
	.write = debug_write,
	.open = debug_open,
};


void ddc_DBG_Init(void)
{
	ddc_dbgfs = debugfs_create_file("hdmi_i2c",
		S_IFREG | 444, NULL, (void *)0, &debug_fops);
}
EXPORT_SYMBOL(ddc_DBG_Init);

void ddc_DBG_Deinit(void)
{
	debugfs_remove(ddc_dbgfs);
}
EXPORT_SYMBOL(ddc_DBG_Deinit);


