// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <mt-plat/mtk_ccci_common.h>
#include <linux/sched/clock.h>

#include "ccci_debug.h"
#include "ccci_debug_info.h"
#include "ccci_hif_ccif.h"

#define TAG "deb"

#define RECV_DATA_SIZE 5000



struct recv_time {
	u64 recv_time;
	u8 qno;

};

struct queue_recv_info {
	u64 first_time;
	u64 last_time;
	int count;

};

struct total_recv_info {
	int ring_Read;
	int ring_Write;
	int repeat_count;
	u64 pre_time;
	struct recv_time times[RECV_DATA_SIZE];

};

static struct total_recv_info s_total_info;
static spinlock_t s_recv_data_info_lock;


static inline void calc_irq_info_per_q(
		struct queue_recv_info *q_inofs, int s, int e)
{
	int qno;

	while (s <= e) {
		qno = s_total_info.times[s].qno;
		q_inofs[qno].count++;

		if (!q_inofs[qno].first_time) {
			q_inofs[qno].first_time =
					s_total_info.times[s].recv_time;
			q_inofs[qno].last_time =
					s_total_info.times[s].recv_time;
			s++;
			continue;
		}

		if (s_total_info.times[s].recv_time <
				q_inofs[qno].first_time)
			q_inofs[qno].first_time =
					s_total_info.times[s].recv_time;
		else if (s_total_info.times[s].recv_time >
				q_inofs[qno].last_time)
			q_inofs[qno].last_time =
					s_total_info.times[s].recv_time;

		s++;
	}
}

static inline void ccif_debug_print_irq_info(void)
{
}

void ccif_debug_save_irq(u8 qno, u64 cur_time)
{
}

void ccif_debug_info_init(void)
{
}
