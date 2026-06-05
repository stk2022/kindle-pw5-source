#ifndef _LIBFBOOT_H
#define _LIBFBOOT_H

#define PAGE_SIZE	0x1000
#define PAGE_MASK	(~(PAGE_SIZE-1))
#define PAGE_ALIGN(x)	(((x) + PAGE_SIZE - 1) & PAGE_MASK)

#ifndef FBOOT_WB_SIZE
#define FBOOT_WB_SIZE	(PAGE_SIZE * 1)
#endif

struct bank_info {
	unsigned int start;
	unsigned int length;
};

struct bootarg_t {
	int bank;
	int learn;
	int learn_mode;
	struct bank_info *bankinfo;
};

/**
 * fb_is_exist_bios
 *
 * Check BIOS is exists at addr.
 *
 * @param[in]	bios_addr	BIOS address
 * @param[in]	sbios_addr	Storage BIOS address
 * @retval		true 	exist
 * @retval		false	not exist
 */
int fb_is_exist_bios(unsigned long bios_addr, unsigned long sbios_addr);

/**
 * fb_bios_init
 *
 * Set vector to F-BIOS. And, call bios_init API.
 *
 * @param[in]	bios_addr	address of F-BIOS
 * @param[in]	sbios_addr	addresss of S-BIOS
 * @param[in]	arg			argument of bios_init API
 * @return		fail or success
 * @retval		0		success
 * @retval		!=0		fail
 */
int fb_bios_init(unsigned long bios_addr, unsigned long sbios_addr, void *arg);

/**
 * fb_fastboot
 *
 * Enter to fast boot sequence.
 * \note This function never returns to caller.
 */
void fb_fastboot(void);

/**
 * fb_is_valid_image
 *
 * Check a image is valid or not.
 *
 * @retval		true 	valid
 * @retval		false	invalid
 *
 * @param[in]	buff	snapshot image header
 */
int fb_is_valid_image(void);

/**
 * fb_is_valid_image
 *
 * Check a image is valid or not.
 *
 * @retval		true 	valid
 * @retval		false	invalid
 *
 * @param[in]	buff	snapshot image header
 */
int fb_is_valid_image_buff(void *buff);


/**
 * fb_get_vector_base
 *
 * Get a vector base address.
 *
 * @return		vector base address
 */
unsigned long fb_get_vector_base(void);

#endif /* _LIBFBOOT_H */
