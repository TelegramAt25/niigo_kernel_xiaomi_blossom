/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef __KD_SENSORLIST_H__
#define __KD_SENSORLIST_H__

#include "kd_camera_typedef.h"
#include "imgsensor_sensor.h"

struct IMGSENSOR_INIT_FUNC_LIST {
	MUINT32   id;
	MUINT8    name[32];
	MUINT32 (*init)(struct SENSOR_FUNCTION_STRUCT **pfFunc);
};

/*M505 imagesensor*/
UINT32 HYNIX_HI1337_I_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc);
UINT32 HYNIX_HI1337_II_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc);
UINT32 HYNIX_HI1337_III_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc);
UINT32 HYNIX_HI1337_IIII_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc);
UINT32 GC_GC5035_I_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc);
UINT32 GC_GC5035_II_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc);
UINT32 GC_GC5035_III_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc);
UINT32 GC_GC5035_IIII_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc);

extern struct IMGSENSOR_INIT_FUNC_LIST kdSensorList[];

#endif

