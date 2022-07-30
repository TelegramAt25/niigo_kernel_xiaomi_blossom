// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#include "kd_imgsensor.h"
#include "imgsensor_sensor_list.h"

/* Add Sensor Init function here
 * Note:
 * 1. Add by the resolution from ""large to small"", due to large sensor
 *    will be possible to be main sensor.
 *    This can avoid I2C error during searching sensor.
 * 2. This should be the same as
 *     mediatek\custom\common\hal\imgsensor\src\sensorlist.cpp
 */
struct IMGSENSOR_INIT_FUNC_LIST kdSensorList[MAX_NUM_OF_SUPPORT_SENSOR] = {
/*M510 imagesensor*/
#if defined(HYNIX_HI1337_I)
	{HYNIX_HI1337_I_SENSOR_ID,
	  SENSOR_DRVNAME_HYNIX_HI1337_I,
	  HYNIX_HI1337_I_SensorInit},
#endif
#if defined(HYNIX_HI1337_II)
	{HYNIX_HI1337_II_SENSOR_ID,
	  SENSOR_DRVNAME_HYNIX_HI1337_II,
	  HYNIX_HI1337_II_SensorInit},
#endif
#if defined(HYNIX_HI1337_III)
	{HYNIX_HI1337_III_SENSOR_ID,
	  SENSOR_DRVNAME_HYNIX_HI1337_III,
	  HYNIX_HI1337_III_SensorInit},
#endif
#if defined(HYNIX_HI1337_IIII)
	{HYNIX_HI1337_IIII_SENSOR_ID,
	  SENSOR_DRVNAME_HYNIX_HI1337_IIII,
	  HYNIX_HI1337_IIII_SensorInit},
#endif
#if defined(GC_GC5035_I)
	{GC_GC5035_I_SENSOR_ID,
	  SENSOR_DRVNAME_GC_GC5035_I,
	  GC_GC5035_I_SensorInit},
#endif
#if defined(GC_GC5035_II)
	{GC_GC5035_II_SENSOR_ID,
	  SENSOR_DRVNAME_GC_GC5035_II,
	  GC_GC5035_II_SensorInit},
#endif
#if defined(GC_GC5035_III)
	{GC_GC5035_III_SENSOR_ID,
	  SENSOR_DRVNAME_GC_GC5035_III,
	  GC_GC5035_III_SensorInit},
#endif
#if defined(GC_GC5035_IIII)
	{GC_GC5035_IIII_SENSOR_ID,
	  SENSOR_DRVNAME_GC_GC5035_IIII,
	  GC_GC5035_IIII_SensorInit},
#endif

	/*  ADD sensor driver before this line */
	{0, {0}, NULL}, /* end of list */
};
/* e_add new sensor driver here */

