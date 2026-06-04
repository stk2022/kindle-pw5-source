/*
 * Copyright (c) 2024 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#define pr_fmt(fmt) "kernel_minerva: " fmt

#include <linux/printk.h>
#include <linux/mutex.h>
#include <linux/types.h>

#define BUFFER_SIZE 512

static int kernel_minerva_metric_boolean(char *buf, int size, va_list *args) {
	const char *key = va_arg(*args, char*);
	int value = va_arg(*args, int);

	return snprintf(buf, size, "boolean:%s=%s,", key, value ? "True" : "False");
}

static int kernel_minerva_metric_long(char *buf, int size, va_list *args) {
	const char *key = va_arg(*args, char*);
	long value = va_arg(*args, long);

	return snprintf(buf, size, "long:%s=%ld,", key, value);
}

static int kernel_minerva_metric_string(char *buf, int size, va_list *args) {
	const char *key = va_arg(*args, char*);
	const char *value = va_arg(*args, char*);

	return snprintf(buf, size, "string:%s=%s,", key, value);
}

static int kernel_minerva_metric_predefined(char *buf, int size, va_list *args) {
	int value = va_arg(*args, int);

	return snprintf(buf, size, "predefined:%d,", value);
}

void kernel_minerva_metric(const char* group_id, const char* schema_id, int c, ...) {
	char buf[BUFFER_SIZE];
	char *p = buf;
	int left = BUFFER_SIZE;

	va_list args;

	va_start(args, c);
	while (c--) {
		enum minerva_type type = va_arg(args, int);
		int n = 0;

		switch (type) {
			case MINERVA_TYPE_BOOLEAN:
				n = kernel_minerva_metric_boolean(p, left, &args);
				break;
			case MINERVA_TYPE_LONG:
				n = kernel_minerva_metric_long(p, left, &args);
				break;
			case MINERVA_TYPE_STRING:
				n = kernel_minerva_metric_string(p, left, &args);
				break;
			case MINERVA_TYPE_PREDEFINED:
				n = kernel_minerva_metric_predefined(p, left, &args);
				break;
			default:
				break;
		}

		if (n >= left) {
			pr_crit("too long\n");
			break;
		} else if (n <= 0) {
			pr_crit("wrong format\n");
			break;
		}
		left -= n;
		p += n;
	}
	va_end(args);
	pr_info("minerva_proxy,%s,%s,%s", group_id, schema_id, buf);
}
EXPORT_SYMBOL(kernel_minerva_metric);

