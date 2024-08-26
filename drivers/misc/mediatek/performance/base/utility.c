// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/uaccess.h>
#include "mtk_perfmgr_internal.h"
#ifdef CONFIG_TRACING
#include <linux/kallsyms.h>
#include <linux/trace_events.h>
#endif

char *perfmgr_copy_from_user_for_proc(const char __user *buffer,
		size_t count)
{
	char *buf = (char *)__get_free_page(GFP_USER);

	if (!buf)
		return NULL;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	return buf;

out:
	free_page((unsigned long)buf);

	return NULL;
}

int check_proc_write(int *data, const char *ubuf, size_t cnt)
{

	char buf[128];

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;
	buf[cnt] = 0;

	if (kstrtoint(buf, 10, data))
		return -1;
	return 0;
}

int check_group_proc_write(int *cgroup, int *data,
		const char *ubuf, size_t cnt)
{
	char buf[128];

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;
	buf[cnt] = 0;

	if (sscanf(buf, "%d %d", cgroup, data) != 2)
		return -1;
	return 0;
}
