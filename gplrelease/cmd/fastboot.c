// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2008 - 2009 Windriver, <www.windriver.com>
 * Author: Tom Rix <Tom.Rix@windriver.com>
 *
 * (C) Copyright 2014 Linaro, Ltd.
 * Rob Herring <robh@kernel.org>
 */
#include <common.h>
#include <command.h>
#include <console.h>
#include <g_dnl.h>
#include <fastboot.h>
#include <malloc.h>
#include <net.h>
#include <usb.h>
#include <watchdog.h>
#if defined(CONFIG_CHARGER_DET_MAX20342)
#include <power/max20342.h>
#endif


static int do_fastboot_udp(int argc, char *const argv[],
			   uintptr_t buf_addr, size_t buf_size)
{
#if CONFIG_IS_ENABLED(UDP_FUNCTION_FASTBOOT)
	int err = net_loop(FASTBOOT);

	if (err < 0) {
		printf("fastboot udp error: %d\n", err);
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
#else
	pr_err("Fastboot UDP not enabled\n");
	return CMD_RET_FAILURE;
#endif
}

static int do_fastboot_usb(int argc, char *const argv[],
			   uintptr_t buf_addr, size_t buf_size)
{
#if CONFIG_IS_ENABLED(USB_FUNCTION_FASTBOOT)
	int controller_index;
	char *usb_controller;
	char *endp;
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;

	usb_controller = argv[1];
	controller_index = simple_strtoul(usb_controller, &endp, 0);
	if (*endp != '\0') {
		pr_err("Error: Wrong USB controller index format\n");
		return CMD_RET_FAILURE;
	}

	ret = usb_gadget_initialize(controller_index);
	if (ret) {
		pr_err("USB init failed: %d\n", ret);
		return CMD_RET_FAILURE;
	}

	g_dnl_clear_detach();
	ret = g_dnl_register("usb_dnl_fastboot");
	if (ret)
		return ret;

	if (!g_dnl_board_usb_cable_connected()) {
		puts("\rUSB cable not detected.\n" \
		     "Command exit.\n");
		ret = CMD_RET_FAILURE;
		goto exit;
	}

#if defined(CONFIG_CHARGER_DET_MAX20342)
	max20342_fastboot_initialize();
#endif

	while (1) {
		if (g_dnl_detach())
			break;
		if (ctrlc())
			break;
		WATCHDOG_RESET();

#if defined(CONFIG_CHARGER_DET_MAX20342)
		max20342_handle_interrupts();
#endif
		usb_gadget_handle_interrupts(controller_index);
	}

#if defined(CONFIG_CHARGER_DET_MAX20342)
	// Set the USB/UART switch back to UART if ctrl-c
	max20342_swc_auto();
#endif

	ret = CMD_RET_SUCCESS;

exit:
	g_dnl_unregister();
	g_dnl_clear_detach();
	usb_gadget_release(controller_index);

	return ret;
#else
	pr_err("Fastboot USB not enabled\n");
	return CMD_RET_FAILURE;
#endif
}

const char *default_args[] = {"fastboot", "usb", "0"};
static int do_fastboot(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	uintptr_t buf_addr = (uintptr_t)NULL;
	size_t buf_size = 0;
	int ret;

	if (argc < 2) {
		argc = 3;
		argv = default_args;
	}

	while (argc > 1 && **(argv + 1) == '-') {
		char *arg = *++argv;

		--argc;
		while (*++arg) {
			switch (*arg) {
			case 'l':
				if (--argc <= 0)
					return CMD_RET_USAGE;
				buf_addr = simple_strtoul(*++argv, NULL, 16);
				goto NXTARG;

			case 's':
				if (--argc <= 0)
					return CMD_RET_USAGE;
				buf_size = simple_strtoul(*++argv, NULL, 16);
				goto NXTARG;

			default:
				return CMD_RET_USAGE;
			}
		}
NXTARG:
		;
	}

	/* Handle case when USB controller param is just '-' */
	if (argc == 1) {
		pr_err("Error: Incorrect USB controller index\n");
		return CMD_RET_USAGE;
	}

	/* Try to dynamically allocate the buffer */
	buf_addr = (uintptr_t)malloc(buf_size ?
		 buf_size : CONFIG_FASTBOOT_BUF_SIZE);

	if (!buf_addr) {
		debug("Failed to allocate fastboot buffer, use default.\n");
	}

	fastboot_init((void *)buf_addr, buf_size);

	if (!strcmp(argv[1], "udp")) {
		ret = do_fastboot_udp(argc, argv, buf_addr, buf_size);
		goto FASTBOOT_EXIT;
	}

	if (!strcmp(argv[1], "usb")) {
		argv++;
		argc--;
	}

	ret = do_fastboot_usb(argc, argv, buf_addr, buf_size);

FASTBOOT_EXIT:
	if (buf_addr)
		free(buf_addr);

	return ret;
}

#ifdef CONFIG_SYS_LONGHELP
static char fastboot_help_text[] =
	"[-l addr] [-s size] usb <controller> | udp\n"
	"\taddr - address of buffer used during data transfers ("
	__stringify(CONFIG_FASTBOOT_BUF_ADDR) ")\n"
	"\tsize - size of buffer used during data transfers ("
	__stringify(CONFIG_FASTBOOT_BUF_SIZE) ")"
	;
#endif

U_BOOT_CMD(
	fastboot, CONFIG_SYS_MAXARGS, 1, do_fastboot,
	"run as a fastboot usb or udp device", fastboot_help_text
);
