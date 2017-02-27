#ifndef GYRO_H
#define GYRO_H

#include "common.h"

typedef struct Gyro Gyro;

typedef struct AxisData
{
    float x;
    float y;
    float z;
} AxisData;

/**
 * General mode
 */
typedef enum GyroMode
{
    GyroMode_Accelerometer,
    GyroMode_AngVel,
    GyroMode_Temperature
} GyroMode;

/**
 * Monitoring mode
 */
typedef enum GyroMonitorMode {
    GyroMonitorMode_None = 0,
    GyroMonitorMode_byFileChange,
    GyroMonitorMode_byInterrupt
} GyroMonitorMode;

/**
 * Gyro indicators information
 * depending on the operating mode
 */
typedef struct GyroIndicatorsInfo
{
    GyroMode mode;
    union
    {
	float temp;             /* GyroMode_Temperature */
	AxisData angvData;      /* GyroMode_AngVel */
	AxisData accData;       /* GyroMode_Accelerometer */
    };
} GyroIndicatorsInfo;


/* Monitoring callback function */
typedef void (GyroCallback)(GyroIndicatorsInfo *info, void *userData);

/**
 * Initialization funtion
 */
Error Gyro_initialize(Gyro **gyro, GyroMode mode);

/**
 * Destroy gyro object
 */
void Gyro_destroy(Gyro *gyro);

/**
 * Get current Gyro indicators
 *
 * @param [in]		gyro	Gyro object
 * @param [out]		data	Pointer to store data
 * @param [in]		feth    Fetch from files (return saved data otherwise)
 *
 * @return Error
 */
Error Gyro_getCurrentData(Gyro *gyro, GyroIndicatorsInfo *info, int fetch);

/**
 * Set Gyro indicators monitoring
 *
 * @param[in]	gyro		Gyro object
 * @param[in]	type		Monitoring type
 * @param[in]	callback	Callback
 * @param[in]	userData	User data
 *
 * @return Error
 */
Error Gyro_setMonitoring(Gyro *gyro, GyroMonitorMode type, GyroCallback *callback, void *userData);

#endif /* GYRO_H */

