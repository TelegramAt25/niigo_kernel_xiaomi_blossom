/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2017 MediaTek Inc.
 */

#ifndef __MUSB_LINUX_DEBUG_H__
#define __MUSB_LINUX_DEBUG_H__

#define yprintk(facility, format, args...) \
		pr_debug("[MUSB]%s %d: " format, \
		__func__, __LINE__, ## args)

/* workaroud for redefine warning in usb_dump.c */
#ifdef WARNING
#undef WARNING
#endif

#ifdef INFO
#undef INFO
#endif

#ifdef ERR
#undef ERR
#endif

#define WARNING(fmt, args...) yprintk(KERN_WARNING, fmt, ## args)
#define INFO(fmt, args...) yprintk(KERN_INFO, fmt, ## args)
#define ERR(fmt, args...) yprintk(KERN_ERR, fmt, ## args)

#define xprintk(level,  format, args...) do { \
		pr_debug("[MUSB]%s %d: " format, \
			__func__, __LINE__, ## args); \
	} while (0)

extern unsigned int musb_debug;
extern unsigned int musb_debug_limit;
extern unsigned int musb_uart_debug;

static inline int _dbg_level(unsigned int level)
{
	return level <= musb_debug;
}

#ifdef DBG
#undef DBG
#endif
#define DBG(level, fmt, args...) xprintk(level, fmt, ## args)

#define DBG_LIMIT(FREQ, fmt, args...) do {\
		DBG(0, fmt "<unlimit>\n", ## args);\
} while (0)

/* extern const char *otg_state_string(struct musb *); */
extern int musb_init_debugfs(struct musb *musb)  __attribute__((weak));
extern void musb_exit_debugfs(struct musb *musb) __attribute__((weak));

#endif				/*  __MUSB_LINUX_DEBUG_H__ */
