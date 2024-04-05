// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifdef CONFIG_MTK_AEE_IPANIC
/* for turn on/off debug.c's log */
#define DEBUG 0

#include <linux/sched.h>
#include "mboot_params.h" /* aee's lib */
#include "sched.h"

/* sched: aee for sched/debug */
#define TRYLOCK_NUM 10
#include <linux/delay.h>
#include <linux/sched/signal.h>

/*
 * from kernel/sched/debug.c
 * Ease the printing of nsec fields:
 */
static long long nsec_high(unsigned long long nsec)
{
	if ((long long)nsec < 0) {
		nsec = -nsec;
		do_div(nsec, 1000000);
		return -nsec;
	}
	do_div(nsec, 1000000);

	return nsec;
}

static unsigned long nsec_low(unsigned long long nsec)
{
	if ((long long)nsec < 0)
		nsec = -nsec;

	return do_div(nsec, 1000000);
}

#define SPLIT_NS(x) (nsec_high(x), nsec_low(x))

#ifdef CONFIG_CGROUP_SCHED
static char group_path[PATH_MAX];

static char *task_group_path(struct task_group *tg)
{
	if (autogroup_path(tg, group_path, PATH_MAX))
		return group_path;

	cgroup_path(tg->css.cgroup, group_path, PATH_MAX);

	return group_path;
}
#endif

static DEFINE_SPINLOCK(sched_debug_lock);

static const char * const sched_tunable_scaling_names[] = {
	"none",
	"logaritmic",
	"linear"
}; /* kernel/sched/debug.c */

char print_at_AEE_buffer[160];

#define SEQ_printf_at_AEE(m, x...)		\
do { } while (0)

static void
print_task_at_AEE(struct seq_file *m, struct rq *rq, struct task_struct *p)
{
}

/* sched: add aee log */
#define read_trylock_irqsave(lock, flags) \
	({ \
	 typecheck(unsigned long, flags); \
	 local_irq_save(flags); \
	 read_trylock(lock) ? \
	 1 : ({ local_irq_restore(flags); 0; }); \
	 })

int read_trylock_n_irqsave(rwlock_t *lock,
		unsigned long *flags, struct seq_file *m, char *msg)
{
	int locked, trylock_cnt = 0;

	do {
		locked = read_trylock_irqsave(lock, *flags);
		trylock_cnt++;
		mdelay(10);
	} while ((!locked) && (trylock_cnt < TRYLOCK_NUM));

	return locked;
}

int raw_spin_trylock_n_irqsave(raw_spinlock_t *lock,
		unsigned long *flags, struct seq_file *m, char *msg)
{
	int locked, trylock_cnt = 0;

	do {
		locked = raw_spin_trylock_irqsave(lock, *flags);
		trylock_cnt++;
		mdelay(10);
	} while ((!locked) && (trylock_cnt < TRYLOCK_NUM));

	return locked;
}

int spin_trylock_n_irqsave(spinlock_t *lock,
		unsigned long *flags, struct seq_file *m, char *msg)
{
	int locked, trylock_cnt = 0;

	do {
		locked = spin_trylock_irqsave(lock, *flags);
		trylock_cnt++;
		mdelay(10);

	} while ((!locked) && (trylock_cnt < TRYLOCK_NUM));

	return locked;
}
static void print_rq_at_AEE(struct seq_file *m, struct rq *rq, int rq_cpu)
{
}

#ifdef CONFIG_FAIR_GROUP_SCHED
static void print_cfs_group_stats_at_AEE(struct seq_file *m,
		int cpu, struct task_group *tg)
{
}
#endif

void print_cfs_rq_at_AEE(struct seq_file *m, int cpu, struct cfs_rq *cfs_rq)
{
}

#define for_each_leaf_cfs_rq(rq, cfs_rq) \
	list_for_each_entry_rcu(cfs_rq, &rq->leaf_cfs_rq_list, leaf_cfs_rq_list)


void print_cfs_stats_at_AEE(struct seq_file *m, int cpu)
{
}

void print_rt_rq_at_AEE(struct seq_file *m, int cpu, struct rt_rq *rt_rq)
{
}


#ifdef CONFIG_RT_GROUP_SCHED

static inline struct task_group *next_task_group(struct task_group *tg)
{
	do {
		tg = list_entry_rcu(tg->list.next,
			typeof(struct task_group), list);
	} while (&tg->list != &task_groups && task_group_is_autogroup(tg));

	if (&tg->list == &task_groups)
		tg = NULL;

	return tg;
}

#define for_each_rt_rq(rt_rq, iter, rq)					\
	for (iter = container_of(&task_groups, typeof(*iter), list);	\
		(iter = next_task_group(iter)) &&			\
		(rt_rq = iter->rt_rq[cpu_of(rq)]);)

#else /* !CONFIG_RT_GROUP_SCHED */

#define for_each_rt_rq(rt_rq, iter, rq) \
	for ((void) iter, rt_rq = &rq->rt; rt_rq; rt_rq = NULL)

#endif

void print_rt_stats_at_AEE(struct seq_file *m, int cpu)
{
}

void print_dl_rq_at_AEE(struct seq_file *m, int cpu, struct dl_rq *dl_rq)
{
}

void print_dl_stats_at_AEE(struct seq_file *m, int cpu)
{
}

static void print_cpu_at_AEE(struct seq_file *m, int cpu)
{
}

static void sched_debug_header_at_AEE(struct seq_file *m)
{
}

void sysrq_sched_debug_show_at_AEE(void)
{
}

#endif /* end CONFIG_MTK_AEE_IPANIC */
