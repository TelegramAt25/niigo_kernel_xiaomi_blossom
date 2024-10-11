/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _H_DDP_LOG_
#define _H_DDP_LOG_
#ifdef CONFIG_MTK_AEE_FEATURE
#include "mt-plat/aee.h"
#endif
#include "display_recorder.h"
#include "ddp_debug.h"
#include "disp_drv_log.h"

#ifndef LOG_TAG
#define LOG_TAG
#endif

#define DDPSVPMSG(fmt, args...) DISPMSG(fmt, ##args)

#define DISP_LOG_I(fmt, args...) ((void)0)
#define DISP_LOG_V(fmt, args...) ((void)0)
#define DISP_LOG_D(fmt, args...) ((void)0)

#define DISP_LOG_W(fmt, args...)					\
	do {								\
		dprec_logger_pr(DPREC_LOGGER_DEBUG, fmt, ##args);	\
		pr_debug("[DDP/"LOG_TAG"]warn:"fmt, ##args);		\
	} while (0)

#define DISP_LOG_E(fmt, args...)					\
	do {								\
		dprec_logger_pr(DPREC_LOGGER_ERROR, fmt, ##args);	\
		pr_debug("[DDP/"LOG_TAG"]error:"fmt, ##args);		\
	} while (0)

#define DDPIRQ(fmt, args...) ((void)0)

#define DDPDBG(fmt, args...) DISP_LOG_D(fmt, ##args)

#define DDPMSG(fmt, args...) DISP_LOG_I(fmt, ##args)

#define DDPWRN(fmt, args...) DISP_LOG_W(fmt, ##args)

#define DDPERR(fmt, args...) DISP_LOG_E(fmt, ##args)

#define DDPDUMP(fmt, ...) ((void)0)

#ifndef ASSERT
#define ASSERT(expr)					\
	do {						\
		if (expr)				\
			break;				\
		pr_debug("DDP ASSERT FAILED %s, %d\n", __FILE__, __LINE__); \
		WARN_ON(1);\
	} while (0)
#endif

#ifdef CONFIG_MTK_AEE_FEATURE
#define DDPAEE(string, args...)						\
	do {								\
		char str[200];						\
		int n;							\
		n = snprintf(str, 199, "DDP:"string, ##args);		\
		if (n < 0 || n >= 199)					\
			pr_debug("DDP copy str error\n");		\
		aee_kernel_warning_api(__FILE__, __LINE__,		\
			DB_OPT_DEFAULT | DB_OPT_MMPROFILE_BUFFER, str, \
			string, ##args);	\
		pr_debug("[DDP Error]"string, ##args);			\
	} while (0)
#else
#define DDPAEE(string, args...) ((void)0)
#endif

#endif
