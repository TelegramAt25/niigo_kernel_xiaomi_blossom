// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/sched.h>
#include "mtk_mfgsys.h"
#ifndef MTK_BRINGUP
#include <mtk_gpufreq.h>
#endif

#include <mtk_boot.h>
#include "rgxdevice.h"
#include "osfunc.h"
#include "pvrsrv.h"
#include "rgxhwperf.h"
#include "device.h"
#include "ospvr_gputrace.h"
#include "rgxinit.h"
#include "pvr_dvfs.h"
#include "pvr_notifier.h"

#include "ged_dvfs.h"
#include "ged_log.h"
#include "ged_base.h"

#include <linux/platform_device.h>
#include <linux/clk.h>

#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>


#define mt_gpufreq_get_frequency_by_level mt_gpufreq_get_freq_by_idx

/* MTK: disable 6795 DVFS temporarily, fix and remove me */

#ifdef CONFIG_MTK_HIBERNATION
#include "sysconfig.h"
#include <mach/mtk_hibernate_dpm.h>
#include <mach/mt_irq.h>
#include <mach/irqs.h>
#endif

// #include <trace/events/mtk_events.h>
#include <mtk_gpu_utility.h>

#define mfg_readl(addr) readl(addr)
#define mfg_writel(val, addr) \
	do { writel(val, addr); wmb(); } while (0) /* sync_write */

void __iomem *topck_base;
#define TOPCK_CLK2 (topck_base + 0x0120)

#ifndef MTK_BRINGUP
#define MTK_GPU_DVFS					1
#endif
#define MTK_DEFER_DVFS_WORK_MS		  10000
#define MTK_DVFS_SWITCH_INTERVAL_MS	 50
#define MTK_SYS_BOOST_DURATION_MS	   50
#define MTK_WAIT_FW_RESPONSE_TIMEOUT_US 5000
#define MTK_GPIO_REG_OFFSET			 0x30
#define MTK_GPU_FREQ_ID_INVALID		 0xFFFFFFFF
#define MTK_RGX_DEVICE_INDEX_INVALID	-1

static IMG_HANDLE g_RGXutilUser;
static POS_LOCK ghDVFSLock;

static IMG_CPU_PHYADDR gsRegsPBase;


static void *g_pvRegsKM;
#ifdef MTK_CAL_POWER_INDEX
static void *g_pvRegsBaseKM;
#endif

static IMG_BOOL g_bExit = IMG_TRUE;
static IMG_INT32 g_iSkipCount;
static IMG_UINT32 g_sys_dvfs_time_ms;

static IMG_UINT32 g_bottom_freq_id;
static IMG_UINT32 gpu_bottom_freq;
static IMG_UINT32 g_cust_boost_freq_id;
static IMG_UINT32 gpu_cust_boost_freq;
static IMG_UINT32 g_cust_upbound_freq_id;
static IMG_UINT32 gpu_cust_upbound_freq;

static IMG_UINT32 gpu_power;
static IMG_UINT32 gpu_dvfs_enable;
static IMG_UINT32 boost_gpu_enable;
static IMG_UINT32 gpu_dvfs_force_idle;
static IMG_UINT32 gpu_dvfs_cb_force_idle;

static IMG_UINT32 gpu_pre_loading;
static IMG_UINT32 gpu_loading;
static IMG_UINT32 gpu_block;
static IMG_UINT32 gpu_idle;
static IMG_UINT32 gpu_freq;

static IMG_BOOL g_bDeviceInit = IMG_FALSE;

static IMG_BOOL g_bUnsync = IMG_FALSE;
static IMG_UINT32 g_ui32_unsync_freq_id;


struct clk *mfg_clk_top;
struct clk *mfg_clk_off;
struct clk *mfg_clk_on;

struct clk *mtcmos_mfg0;
struct clk *mtcmos_mfg1;

unsigned int _mtk_ged_log;

#ifdef CONFIG_MTK_SEGMENT_TEST
static IMG_UINT32 efuse_mfg_enable;
#endif

#ifdef CONFIG_MTK_HIBERNATION
int gpu_pm_restore_noirq(struct device *device)
{
#if defined(MTK_CONFIG_OF) && defined(CONFIG_OF)
	int irq = MTKSysGetIRQ();
#else
	int irq = SYS_MTK_RGX_IRQ;
#endif
	mt_irq_set_sens(irq, MT_LEVEL_SENSITIVE);
	mt_irq_set_polarity(irq, MT_POLARITY_LOW);
	return 0;
}
#endif

static PVRSRV_DEVICE_NODE *MTKGetRGXDevNode(void)
{
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	IMG_UINT32 i;

	if (psPVRSRVData) {
		for (i = 0; i < psPVRSRVData->ui32RegisteredDevices; i++) {
			PVRSRV_DEVICE_NODE *psDeviceNode =
			&psPVRSRVData->psDeviceNodeList[i];

			if (psDeviceNode && psDeviceNode->psDevConfig)
				return psDeviceNode;
		}
	}
	return NULL;
}

/* extern void ged_log_trace_counter(char *name, int count); */
static void MTKWriteBackFreqToRGX(PVRSRV_DEVICE_NODE *psDevNode,
		IMG_UINT32 ui32NewFreq)
{
	RGX_DATA *psRGXData = (RGX_DATA *)psDevNode->psDevConfig->hDevData;

	/* kHz to Hz write to RGX as the same unit */
	psRGXData->psRGXTimingInfo->ui32CoreClockSpeed = ui32NewFreq * 1000;
}


#ifndef MTK_BRINGUP
#define MTKCLK_prepare_enable(clk) \
	do { \
		if (clk) \
			clk_prepare_enable(clk); \
	} while (0)

#define MTKCLK_disable_unprepare(clk) \
	do { \
		if (clk) \
			clk_disable_unprepare(clk); \
	} while (0)

#define MTKCLK_set_parent(clkC, clkP) \
	do { \
		if (clkC && clkP) { \
			clk_set_parent(clkC, clkP) \
	} while (0)
#else
#define MTKCLK_prepare_enable(clk)

#define MTKCLK_disable_unprepare(clk)

#define MTKCLK_set_parent(clkC, clkP)
#endif

#ifdef MTK_MFGMTCMOS_AO
static bool isPowerOn;
#endif

static void MTKEnableMfgClock(IMG_BOOL bForce)
{
#ifdef MTK_GPU_DVFS
	ged_log_buf_print2(_mtk_ged_log, GED_LOG_ATTR_TIME, "BUCK_ON");
	mt_gpufreq_voltage_enable_set(BUCK_ON);
#endif

#ifdef MTCMOS_CONTROL
#ifndef MTK_USE_HW_APM
	mt_gpufreq_enable_MTCMOS(false);
#else
	mt_gpufreq_enable_MTCMOS(true);
#endif
#endif

	ged_dvfs_gpu_clock_switch_notify(1);
	mtk_notify_gpu_power_change(1);
}

#define MFG_BUS_IDLE_BIT (1 << 16)

static void MTKDisableMfgClock(IMG_BOOL bForce)
{
	int buck_state;

	mtk_notify_gpu_power_change(0);
	ged_dvfs_gpu_clock_switch_notify(0);

#ifdef MTCMOS_CONTROL
#ifndef MTK_USE_HW_APM
	mt_gpufreq_disable_MTCMOS(false);
#else
	mt_gpufreq_disable_MTCMOS(true);
#endif
#endif

#ifdef MTK_GPU_DVFS
	buck_state = mt_gpufreq_voltage_enable_set(BUCK_OFF);
#endif
	ged_log_buf_print2(_mtk_ged_log, GED_LOG_ATTR_TIME, "BUCK_OFF");

}

#if defined(MTK_USE_HW_APM)
static int MTKInitHWAPM(void)
{
	if (!g_pvRegsKM)
		g_pvRegsKM = OSMapPhysToLin(gsRegsPBase, 0x1000, 0);

	mfg_writel(0x01a80000, (g_pvRegsKM + 0x504));
	mfg_writel(0x00080010, (g_pvRegsKM + 0x508));
	mfg_writel(0x00100008, (g_pvRegsKM + 0x50c));
	mfg_writel(0x00b800c8, (g_pvRegsKM + 0x510));
	mfg_writel(0x00b000c0, (g_pvRegsKM + 0x514));
	mfg_writel(0x00c000c8, (g_pvRegsKM + 0x518));
	mfg_writel(0x00b000b8, (g_pvRegsKM + 0x51c));
	mfg_writel(0x00d000d0, (g_pvRegsKM + 0x520));
	mfg_writel(0x00d000d0, (g_pvRegsKM + 0x524));
	mfg_writel(0x00d00000, (g_pvRegsKM + 0x528));

	mfg_writel(0x9004001b, (g_pvRegsKM + 0x24));
	mfg_writel(0x8004001b, (g_pvRegsKM + 0x24));

	return PVRSRV_OK;
}

static int MTKDeInitHWAPM(void)
{
	return PVRSRV_OK;
}
#endif

#ifdef MTK_GPU_DVFS
static IMG_BOOL MTKDoGpuDVFS(IMG_UINT32 ui32NewFreqID, IMG_BOOL bIdleDevice)
{
	PVRSRV_ERROR eResult;
	PVRSRV_DEVICE_NODE *psDevNode;
	unsigned int ui32GPUFreq;
	unsigned int ui32CurFreqID;
	PVRSRV_DEV_POWER_STATE ePowerState;

	/* bottom bound */
	if (ui32NewFreqID > g_bottom_freq_id)
		ui32NewFreqID = g_bottom_freq_id;
	if (ui32NewFreqID > g_cust_boost_freq_id)
		ui32NewFreqID = g_cust_boost_freq_id;

	/* up bound */
	if (ui32NewFreqID < g_cust_upbound_freq_id)
		ui32NewFreqID = g_cust_upbound_freq_id;

	/* thermal power limit */
	if (ui32NewFreqID < mt_gpufreq_get_thermal_limit_index())
		ui32NewFreqID = mt_gpufreq_get_thermal_limit_index();

	/* no change */
	if (ui32NewFreqID == mt_gpufreq_get_cur_freq_index())
		return IMG_FALSE;

	psDevNode = MTKGetRGXDevNode();
	if (psDevNode == NULL)
		return IMG_FALSE;

	eResult = PVRSRVDevicePreClockSpeedChange(psDevNode,
			bIdleDevice, (void *)NULL);
	if ((eResult == PVRSRV_OK) || (eResult == PVRSRV_ERROR_RETRY)) {
		PVRSRVGetDevicePowerState(psDevNode, &ePowerState);
		if (ePowerState != PVRSRV_DEV_POWER_STATE_ON)
			MTKEnableMfgClock(IMG_FALSE);

		mt_gpufreq_target(ui32NewFreqID);
		ui32CurFreqID = mt_gpufreq_get_cur_freq_index();
		ui32GPUFreq = mt_gpufreq_get_frequency_by_level(ui32CurFreqID);
		gpu_freq = ui32GPUFreq;

		MTKWriteBackFreqToRGX(psDevNode, ui32GPUFreq);

		if (ePowerState != PVRSRV_DEV_POWER_STATE_ON)
			MTKDisableMfgClock(IMG_TRUE);

		if (eResult == PVRSRV_OK)
			PVRSRVDevicePostClockSpeedChange(psDevNode,
			bIdleDevice, (void *)NULL);

		return IMG_TRUE;
	}

	return IMG_FALSE;
}

/* For ged_dvfs idx commit */
static void MTKCommitFreqIdx(unsigned long ui32NewFreqID,
	GED_DVFS_COMMIT_TYPE eCommitType, int *pbCommited)
{
	unsigned int ui32GPUFreq;
	unsigned int ui32CurFreqID;
	PVRSRV_DEV_POWER_STATE ePowerState;
	PVRSRV_DEVICE_NODE *psDevNode = MTKGetRGXDevNode();
	PVRSRV_ERROR eResult;

	if (psDevNode) {
		eResult = PVRSRVDevicePreClockSpeedChange(psDevNode,
			IMG_FALSE, (void *)NULL);
		if ((eResult == PVRSRV_OK) || (eResult == PVRSRV_ERROR_RETRY)) {
			PVRSRVGetDevicePowerState(psDevNode, &ePowerState);

			if (ePowerState == PVRSRV_DEV_POWER_STATE_ON) {
				mt_gpufreq_target(ui32NewFreqID);
				g_bUnsync = IMG_FALSE;
			} else {
				g_ui32_unsync_freq_id = ui32NewFreqID;
				g_bUnsync = IMG_TRUE;
			}

			ui32CurFreqID = mt_gpufreq_get_cur_freq_index();

			ui32GPUFreq =
			mt_gpufreq_get_frequency_by_level(ui32CurFreqID);

			gpu_freq = ui32GPUFreq;

			MTKWriteBackFreqToRGX(psDevNode, ui32GPUFreq);

			if (eResult == PVRSRV_OK)
				PVRSRVDevicePostClockSpeedChange(psDevNode,
				IMG_FALSE, (void *)NULL);

			/* Always return true because the APM would almost
			 * letting GPU power down with high possibility
			 * while DVFS committing
			 */
			if (pbCommited)
				*pbCommited = IMG_TRUE;
			return;
		}
	}

	if (pbCommited)
		*pbCommited = IMG_FALSE;
}

static void MTKFreqInputBoostCB(unsigned int ui32BoostFreqID)
{
	if (g_iSkipCount > 0)
		return;


	if (boost_gpu_enable == 0)
		return;

	OSLockAcquire(ghDVFSLock);

	if (ui32BoostFreqID < mt_gpufreq_get_cur_freq_index()) {
		if (MTKDoGpuDVFS(ui32BoostFreqID,
			gpu_dvfs_cb_force_idle == 0 ? IMG_FALSE : IMG_TRUE))
			g_sys_dvfs_time_ms = OSClockms();
	}

	OSLockRelease(ghDVFSLock);

}

#endif /* ifdef MTK_GPU_DVFS */

#ifdef MTK_CAL_POWER_INDEX
static void MTKStartPowerIndex(void)
{
	PVRSRV_RGXDEV_INFO *psDevInfo;
	PVRSRV_DEVICE_NODE *psDevNode;

	if (!g_pvRegsBaseKM) {
		psDevNode = MTKGetRGXDevNode();

		if (psDevNode) {
			psDevInfo = psDevNode->pvDevice;

			if (psDevInfo)
				g_pvRegsBaseKM = psDevInfo->pvRegsBaseKM;
		}
	}

	if (g_pvRegsBaseKM)
		DRV_WriteReg32(g_pvRegsBaseKM + (uintptr_t)0x6320, 0x1);
}

static void MTKReStartPowerIndex(void)
{
	if (g_pvRegsBaseKM)
		DRV_WriteReg32(g_pvRegsBaseKM + (uintptr_t)0x6320, 0x1);
}

static void MTKStopPowerIndex(void)
{
	if (g_pvRegsBaseKM)
		DRV_WriteReg32(g_pvRegsBaseKM + (uintptr_t)0x6320, 0x0);
}

static IMG_UINT32 MTKCalPowerIndex(void)
{
	IMG_UINT32 ui32State, ui32Result;
	PVRSRV_DEV_POWER_STATE  ePowerState;
	IMG_BOOL bTimeout;
	IMG_UINT32 u32Deadline;
	void *pvGPIO_REG = g_pvRegsKM + (uintptr_t)MTK_GPIO_REG_OFFSET;
	void *pvPOWER_ESTIMATE_RESULT = g_pvRegsBaseKM + (uintptr_t)6328;

	if ((!g_pvRegsKM) || (!g_pvRegsBaseKM))
		return 0;

	if (PVRSRVPowerLock() != PVRSRV_OK)
		return 0;

	PVRSRVGetDevicePowerState(MTKGetRGXDevIdx(), &ePowerState);
	if (ePowerState != PVRSRV_DEV_POWER_STATE_ON) {
		PVRSRVPowerUnlock();
		return 0;
	}

	/* writes 1 to GPIO_INPUT_REQ, bit[0] */
	DRV_WriteReg32(pvGPIO_REG, DRV_Reg32(pvGPIO_REG) | 0x1);

	/* wait for 1 in GPIO_OUTPUT_REQ, bit[16] */
	bTimeout = IMG_TRUE;
	u32Deadline = OSClockus() + MTK_WAIT_FW_RESPONSE_TIMEOUT_US;

	while (OSClockus() < u32Deadline) {
		if (0x10000 & DRV_Reg32(pvGPIO_REG)) {
			bTimeout = IMG_FALSE;
			break;
		}
	}

	/* writes 0 to GPIO_INPUT_REQ, bit[0] */
	DRV_WriteReg32(pvGPIO_REG, DRV_Reg32(pvGPIO_REG) & (~0x1));
	if (bTimeout) {
		PVRSRVPowerUnlock();
		return 0;
	}

	/* read GPIO_OUTPUT_DATA, bit[24] */
	ui32State = DRV_Reg32(pvGPIO_REG) >> 24;

	/* read POWER_ESTIMATE_RESULT */
	ui32Result = DRV_Reg32(pvPOWER_ESTIMATE_RESULT);

	/* writes 1 to GPIO_OUTPUT_ACK, bit[17] */
	DRV_WriteReg32(pvGPIO_REG, DRV_Reg32(pvGPIO_REG)|0x20000);

	/* wait for 0 in GPIO_OUTPUT_REG, bit[16] */
	bTimeout = IMG_TRUE;
	u32Deadline = OSClockus() + MTK_WAIT_FW_RESPONSE_TIMEOUT_US;

	while (OSClockus() < u32Deadline) {
		if (!(0x10000 & DRV_Reg32(pvGPIO_REG))) {
			bTimeout = IMG_FALSE;
			break;
		}
	}

	/* writes 0 to GPIO_OUTPUT_ACK, bit[17] */
	DRV_WriteReg32(pvGPIO_REG, DRV_Reg32(pvGPIO_REG) & (~0x20000));
	if (bTimeout) {
		PVRSRVPowerUnlock();
		return 0;
	}

	MTKReStartPowerIndex();

	PVRSRVPowerUnlock();

	return (ui32State == 1) ? ui32Result : 0;
}
#endif

#ifdef MTK_GPU_DVFS
static void MTKCalGpuLoading(unsigned int *pui32Loading,
	unsigned int *pui32Block, unsigned int *pui32Idle)
{
	PVRSRV_DEVICE_NODE *psDevNode;
	PVRSRV_RGXDEV_INFO *psDevInfo;

	psDevNode = MTKGetRGXDevNode();
	if (!psDevNode) {
		PVR_DPF((PVR_DBG_ERROR, "psDevNode not found"));
		return;
	}

	psDevInfo = psDevNode->pvDevice;

	if (psDevInfo && psDevInfo->pfnGetGpuUtilStats) {
		RGXFWIF_GPU_UTIL_STATS sGpuUtilStats = {0};

		if (g_RGXutilUser == NULL)
			return;

		psDevInfo->pfnGetGpuUtilStats(psDevInfo->psDeviceNode,
		g_RGXutilUser, &sGpuUtilStats);

		if (sGpuUtilStats.bValid) {
/*
 *			PVR_DPF((PVR_DBG_ERROR, "Loading: A(%d), I(%d), B(%d)",
 *				sGpuUtilStats.ui64GpuStatActiveHigh,
 *				sGpuUtilStats.ui64GpuStatIdle,
 *				sGpuUtilStats.ui64GpuStatBlocked));
 */
#if defined(__arm64__) || defined(__aarch64__)
			*pui32Loading =
				(100*(sGpuUtilStats.ui64GpuStatActive)) /
				sGpuUtilStats.ui64GpuStatCumulative;
			*pui32Block =
				(100*(sGpuUtilStats.ui64GpuStatBlocked)) /
				sGpuUtilStats.ui64GpuStatCumulative;
			*pui32Idle =
				(100*(sGpuUtilStats.ui64GpuStatIdle)) /
				sGpuUtilStats.ui64GpuStatCumulative;
#else
			*pui32Loading =
			(unsigned long)(100*
			(sGpuUtilStats.ui64GpuStatActive)) /
			(unsigned long)sGpuUtilStats.ui64GpuStatCumulative;

			*pui32Block =
			(unsigned long)(100*
			(sGpuUtilStats.ui64GpuStatBlocked)) /
			(unsigned long)sGpuUtilStats.ui64GpuStatCumulative;

			*pui32Idle =
			(unsigned long)(100*
			(sGpuUtilStats.ui64GpuStatIdle)) /
			(unsigned long)sGpuUtilStats.ui64GpuStatCumulative;
#endif
		}
	}
}

static IMG_BOOL MTKGpuDVFSPolicy(IMG_UINT32 ui32GPULoading,
				unsigned int *pui32NewFreqID)
{
	int i32MaxLevel = (int)(mt_gpufreq_get_dvfs_table_num() - 1);
	int i32CurFreqID = (int)mt_gpufreq_get_cur_freq_index();
	int i32NewFreqID = i32CurFreqID;

	if (ui32GPULoading >= 99)
		i32NewFreqID = 0;
	else if (ui32GPULoading <= 1)
		i32NewFreqID = i32MaxLevel;
	else if (ui32GPULoading >= 85)
		i32NewFreqID -= 2;
	else if (ui32GPULoading <= 30)
		i32NewFreqID += 2;
	else if (ui32GPULoading >= 70)
		i32NewFreqID -= 1;
	else if (ui32GPULoading <= 50)
		i32NewFreqID += 1;

	if (i32NewFreqID < i32CurFreqID) {
		if (gpu_pre_loading * 17 / 10 < ui32GPULoading)
			i32NewFreqID -= 1;
	} else if (i32NewFreqID > i32CurFreqID) {
		if (ui32GPULoading * 17 / 10 < gpu_pre_loading)
			i32NewFreqID += 1;
	}

	if (i32NewFreqID > i32MaxLevel)
		i32NewFreqID = i32MaxLevel;
	else if (i32NewFreqID < 0)
		i32NewFreqID = 0;

	if (i32NewFreqID != i32CurFreqID) {

		*pui32NewFreqID = (unsigned int)i32NewFreqID;
		return IMG_TRUE;
	}

	return IMG_FALSE;
}

#endif

static bool MTKCheckDeviceInit(void)
{
	PVRSRV_DEVICE_NODE *psDevNode = MTKGetRGXDevNode();
	bool ret = false;

	if (psDevNode) {
		if (psDevNode->eDevState == PVRSRV_DEVICE_STATE_ACTIVE)
			ret = true;
	}
	return ret;
}

PVRSRV_ERROR MTKDevPrePowerState(IMG_HANDLE hSysData,
	PVRSRV_DEV_POWER_STATE eNewPowerState,
	PVRSRV_DEV_POWER_STATE eCurrentPowerState,
	IMG_BOOL bForced)
{
	if (eNewPowerState == PVRSRV_DEV_POWER_STATE_OFF &&
		eCurrentPowerState == PVRSRV_DEV_POWER_STATE_ON) {
		if (g_bDeviceInit) {
			g_bExit = IMG_TRUE;

#ifdef MTK_CAL_POWER_INDEX
			MTKStopPowerIndex();
#endif
		} else
			g_bDeviceInit = MTKCheckDeviceInit();

#if defined(MTK_USE_HW_APM)
		MTKDeInitHWAPM();
#endif
		MTKDisableMfgClock(bForced);
	}
#ifdef MTK_MFGMTCMOS_AO
	else if ((eNewPowerState == PVRSRV_DEV_POWER_STATE_OFF &&
			eCurrentPowerState == PVRSRV_DEV_POWER_STATE_OFF) &&
			bForced == true) {
		MTKDisableMfgMtcmos0();
	}
#endif
	return PVRSRV_OK;
}

PVRSRV_ERROR MTKDevPostPowerState(IMG_HANDLE hSysData,
	PVRSRV_DEV_POWER_STATE eNewPowerState,
	PVRSRV_DEV_POWER_STATE eCurrentPowerState,
	IMG_BOOL bForced)
{
	if (eCurrentPowerState == PVRSRV_DEV_POWER_STATE_OFF &&
		eNewPowerState == PVRSRV_DEV_POWER_STATE_ON) {
		MTKEnableMfgClock(bForced);
#if defined(MTK_USE_HW_APM)
		MTKInitHWAPM();
#endif
			if (g_bDeviceInit) {
#ifdef MTK_CAL_POWER_INDEX
				MTKStartPowerIndex();
#endif
				g_bExit = IMG_FALSE;
			} else
				g_bDeviceInit = MTKCheckDeviceInit();


			if (g_bUnsync == IMG_TRUE) {
#ifdef MTK_GPU_DVFS
				mt_gpufreq_target(g_ui32_unsync_freq_id);
#endif
				g_bUnsync = IMG_FALSE;
			}

		}

	return PVRSRV_OK;
}

PVRSRV_ERROR MTKSystemPrePowerState(PVRSRV_SYS_POWER_STATE eNewPowerState)
{
	if (eNewPowerState == PVRSRV_SYS_POWER_STATE_OFF)
		;

	return PVRSRV_OK;
}

PVRSRV_ERROR MTKSystemPostPowerState(PVRSRV_SYS_POWER_STATE eNewPowerState)
{
	if (eNewPowerState == PVRSRV_SYS_POWER_STATE_ON)
		;

	return PVRSRV_OK;
}

/* extern int (*ged_dvfs_vsync_trigger_fp)(int idx); */

#ifdef SUPPORT_PDVFS
#include "rgxpdvfs.h"
#define  mt_gpufreq_get_freq_by_idx mt_gpufreq_get_frequency_by_level
static IMG_OPP *gpasOPPTable;

static int MTKMFGOppUpdate(int ui32ThrottlePoint)
{
	PVRSRV_DEVICE_NODE *psDevNode;
	PVRSRV_RGXDEV_INFO *psDevInfo;
	PVRSRV_DEVICE_CONFIG *psDevConfig;
	int i;
	int ui32OPPTableSize;

	static RGXFWIF_PDVFS_OPP sPDFVSOppInfo;
	static int bNotReady;

	bNotReady = 1;
	psDevNode = MTKGetRGXDevNode();

	if (bNotReady) {
		ui32OPPTableSize = mt_gpufreq_get_dvfs_table_num();
		gpasOPPTable
		= (IMG_OPP *)OSAllocZMem(sizeof(IMG_OPP) * ui32OPPTableSize);

		for (i = 0; i < ui32OPPTableSize; i++) {
			gpasOPPTable[i].ui32Volt
			= mt_gpufreq_get_volt_by_idx(i);

			gpasOPPTable[i].ui32Freq
			= mt_gpufreq_get_freq_by_idx(i) * 1000;
		}

		if (psDevNode) {
			psDevInfo = psDevNode->pvDevice;
			psDevConfig = psDevNode->psDevConfig;

			psDevConfig->sDVFS.sDVFSDeviceCfg.pasOPPTable
			= gpasOPPTable;

			psDevConfig->sDVFS.sDVFSDeviceCfg.ui32OPPTableSize
			= ui32OPPTableSize;

			bNotReady = 0;

			PVR_DPF((PVR_DBG_ERROR,
				"PDVFS opptab=%p size=%d init completed",
				psDevConfig->sDVFS.sDVFSDeviceCfg.pasOPPTable,
				ui32OPPTableSize));
		} else {
			if (gpasOPPTable)
				OSFreeMem(gpasOPPTable);
		}
	}

}
#endif

static void MTKFakeGpuLoading(unsigned int *pui32Loading,
			      unsigned int *pui32Block,
			      unsigned int *pui32Idle)
{
	*pui32Loading = 0;
	*pui32Block = 0;
	*pui32Idle = 0;
}

void mtk_fdvfs_update_cur_freq(int ui32GPUFreq)
{
	PVRSRV_DEVICE_NODE *psDevNode = MTKGetRGXDevNode();
	PVRSRV_ERROR eResult;
	static int s_ui32GPUFreq;

	if (s_ui32GPUFreq != ui32GPUFreq) {
		if (psDevNode) {
			eResult = PVRSRVDevicePreClockSpeedChange(psDevNode,
			IMG_FALSE, (void *)NULL);
			if ((eResult == PVRSRV_OK) ||
				(eResult == PVRSRV_ERROR_RETRY)) {
				MTKWriteBackFreqToRGX(psDevNode, ui32GPUFreq);
				if (eResult == PVRSRV_OK)
					PVRSRVDevicePostClockSpeedChange(psDevNode,
					IMG_FALSE, (void *)NULL);
			}
		}
		s_ui32GPUFreq = ui32GPUFreq;
	}
}
EXPORT_SYMBOL(mtk_fdvfs_update_cur_freq);

PVRSRV_ERROR MTKMFGSystemInit(void)
{
#ifndef MTK_GPU_DVFS
	gpu_dvfs_enable = 0;
#else
	gpu_dvfs_enable = 1;

#ifdef SUPPORT_PDVFS
	/* ged_dvfs_vsync_trigger_fp = MTKMFGOppUpdate; */
	/* turn-off GED loading based DVFS */
	ged_dvfs_cal_gpu_utilization_fp = MTKFakeGpuLoading;
	/* mt_gpufreq_power_limit_notify_registerCB( ); */
#else
	ged_dvfs_cal_gpu_utilization_fp = MTKCalGpuLoading;
	ged_dvfs_gpu_freq_commit_fp = MTKCommitFreqIdx;
#endif


#endif /* ifdef MTK_GPU_DVFS */


#ifdef CONFIG_MTK_HIBERNATION
	register_swsusp_restore_noirq_func(ID_M_GPU,
	gpu_pm_restore_noirq, NULL);
#endif

#if defined(MTK_USE_HW_APM)
	if (!g_pvRegsKM) {
		PVRSRV_DEVICE_NODE *psDevNode = MTKGetRGXDevNode();

		if (psDevNode) {
			PVRSRV_DEVICE_CONFIG *psDevConfig
				= psDevNode->psDevConfig;

			if (psDevConfig && (!g_pvRegsKM)) {
				gsRegsPBase = psDevConfig->sRegsCpuPBase;
				gsRegsPBase.uiAddr += 0xffe000;

				g_pvRegsKM =
				OSMapPhysToLin(gsRegsPBase, 0x1000, 0);

				PVR_DPF((PVR_DBG_ERROR,
				"g_pvRegsKM = 0x%p", g_pvRegsKM));
			}
		}
	}
#endif

	return PVRSRV_OK;
}

void MTKMFGSystemDeInit(void)
{
#ifdef SUPPORT_PDVFS
	if (gpasOPPTable)
		OSFreeMem(gpasOPPTable);
#endif

#ifdef CONFIG_MTK_HIBERNATION
	unregister_swsusp_restore_noirq_func(ID_M_GPU);
#endif

	g_bExit = IMG_TRUE;

#ifdef MTK_GPU_DVFS
#endif /* MTK_GPU_DVFS */

#ifdef MTK_CAL_POWER_INDEX
	g_pvRegsBaseKM = NULL;
#endif

#if defined(MTK_USE_HW_APM)
	if (g_pvRegsKM) {
		OSUnMapPhysToLin(g_pvRegsKM, 0x1000, 0);
		g_pvRegsKM = NULL;
	}
#endif
}


int MTKRGXDeviceInit(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
#if !defined(CONFIG_MTK_ENABLE_GMO)
	_mtk_ged_log = ged_log_buf_alloc(64, 64 * 32,
			GED_LOG_BUF_TYPE_RINGBUFFER, "PowerLog", "ppL");
#else
	_mtk_ged_log = 0;
#endif /* CONFIG_MTK_ENABLE_GMO */

#ifdef MTK_GPU_DVFS
	/* Only Enable buck to get cg & mtcmos */
	mt_gpufreq_voltage_enable_set(BUCK_ON);
#endif

#ifndef MTK_BRINGUP
	/* Enable CG & mtcmos */
	MTKEnableMfgClock(IMG_TRUE);
#endif
	if (psDevConfig && (!g_pvRegsKM)) {
		gsRegsPBase = psDevConfig->sRegsCpuPBase;
		gsRegsPBase.uiAddr += 0xffe000;
#if defined(MTK_USE_HW_APM)
		MTKInitHWAPM();
#endif
	} else
		PVR_DPF((PVR_DBG_ERROR,
		"psDevConfig = 0x%p, g_pvRegsKM = 0x%p",
		psDevConfig, g_pvRegsKM));

	/* 1.5+DDK support multiple user to query GPU utilization */
	/* Need Init here */
	if (g_RGXutilUser == NULL)
		SORgxGpuUtilStatsRegister(&g_RGXutilUser);

#if MTK_PM_SUPPORT
	MTKDisableMfgClock(IMG_TRUE);
#endif

	return 0;
}

int MTKRGXDeviceDeInit(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	return 0;
}

void MTKFWDump(void)
{
#if 0
	PVRSRV_DEVICE_NODE *psDevNode = MTKGetRGXDevNode();

	if (psDevNode) {
		MTK_PVRSRVDebugRequestSetSilence(IMG_TRUE);
		PVRSRVDebugRequest(psDevNode, DEBUG_REQUEST_VERBOSITY_MAX, NULL, NULL);
		MTK_PVRSRVDebugRequestSetSilence(IMG_FALSE);
	}
#endif
}
EXPORT_SYMBOL(MTKFWDump);

#if defined(MODULE)
int mtk_mfg_async_init(void)
#else
static int __init mtk_mfg_async_init(void)
#endif
{
	return 0;
}


#if defined(MODULE)
int mtk_mfg_2d_init(void)
#else
static int __init mtk_mfg_2d_init(void)
#endif
{
	return 0;
}


module_param(gpu_power, uint, 0644);


#ifdef CONFIG_MTK_SEGMENT_TEST
module_param(efuse_mfg_enable, uint, 0644);
#endif
