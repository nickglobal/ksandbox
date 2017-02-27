#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <unistd.h>

#include "gyro.h"
#include "hwconfig.h"

#define RAD2DEG(r) ((r) * 57.29577951307855)

const char *gyro_sysfs_files[] =
{
    HW_GYRO_X_PATH,
    HW_GYRO_Y_PATH,
    HW_GYRO_Z_PATH
};

const char *acc_sysfs_files[] =
{
    HW_ACC_X_PATH,
    HW_ACC_Y_PATH,
    HW_ACC_Z_PATH
};

const char *temp_sysfs_files[] =
{
    HW_TEMP_PATH
};

typedef enum IndicatorIdx
{
    temp_idx  = 0,
    axe_x_idx = temp_idx,
    axe_y_idx,
    axe_z_idx,

    max_indicator_idx
} IndicatorIdx;

/**
 * Monitor context
 */
struct GyroMonitor
{
    int                 active;
    GyroMonitorMode     mode;
    pthread_t           thread;
    GyroCallback        *callback;
    void                *userData;
};

typedef struct CorrectionsData{
    int   tempOffset;
    float tempScale;
    float accScale;
    float angvScale;
} CorrectionsData;
/**
 * Gyroscope context
 */
struct Gyro {
    /* General */
    GyroMode        mode;
    CorrectionsData corrections;
    int             indicatorsCount;
    int             indicatorValues[max_indicator_idx];
    struct pollfd   pfds[max_indicator_idx];
    char           *indicatorsFiles[max_indicator_idx];

    /* Monitoding */
    struct GyroMonitor monitor;
};

static Error readCorrections(Gyro *gyro)
{
    Error error      = NoError;
    int   fd         = -1;
    char  buffer[12] = {0};

    fd = open(HW_TEMP_SCALE_PATH, O_RDONLY);
    if (fd >= 0)
    {
        if(read(fd, buffer, sizeof(buffer) - 1) >= 0)
            gyro->corrections.tempScale = strtof(buffer, NULL);
        else
            error = ErrorUnknown;
        close(fd);
    }
    else
        error = ErrorNotFound;
    CHK(error);

    fd = open(HW_TEMP_OFFSET_PATH, O_RDONLY);
    if (fd >= 0)
    {
        if(read(fd, buffer, sizeof(buffer) - 1) >= 0)
            gyro->corrections.tempOffset = (int)strtol(buffer, NULL, 10);
        else
            error = ErrorUnknown;
        close(fd);
    }
    else
        error = ErrorNotFound;
    CHK(error);

    fd = open(HW_GYRO_SCALE_PATH, O_RDONLY);
    if (fd >= 0)
    {
        if(read(fd, buffer, sizeof(buffer) - 1) >= 0)
            gyro->corrections.angvScale = strtof(buffer, NULL);
        else
            error = ErrorUnknown;
        close(fd);
    }
    else
        error = ErrorNotFound;
    CHK(error);

    fd = open(HW_ACC_SCALE_PATH, O_RDONLY);
    if (fd >= 0)
    {
        if(read(fd, buffer, sizeof(buffer) - 1) >= 0)
            gyro->corrections.accScale = strtof(buffer, NULL);
        else
            error = ErrorUnknown;
        close(fd);
    }
    else
        error = ErrorNotFound;

cleanup:
    if (error != NoError)
        DBG_ERR("read corrections failed with %d\n", error);
    return error;
}

static Error openFiles(Gyro *gyro)
{
    int   i, fd;

    for(i = 0; i < gyro->indicatorsCount; i++)
    {
        fd = open(gyro->indicatorsFiles[i], O_RDONLY);
        if (fd < 0)
            return ErrorNotFound;
        gyro->pfds[i].fd = fd;
        gyro->pfds[i].events = POLLIN;
    }

    return NoError;
}

static void closeFiles(Gyro *gyro)
{
    int i;

    for (i = 0; i < gyro->indicatorsCount; i++)
    {
        if (gyro->pfds[i].fd >= 0)
            close(gyro->pfds[i].fd);
    }
}

static Error readIndicatorVal(Gyro *gyro, IndicatorIdx idx, int *value)
{
    Error error = NoError;
    char buffer[12] = {0};

    if (gyro->pfds[idx].fd < 0)
        CHK(ErrorInvalidParameters);

    lseek(gyro->pfds[idx].fd, 0, SEEK_SET);
    if(read(gyro->pfds[idx].fd, buffer, sizeof(buffer) - 1) <= 0)
        CHK(ErrorUnknown);

    *value = (int)strtol(buffer, NULL, 10);

cleanup:
    if (error != NoError)
        DBG_ERR("Reading indicator failed with %d\n", error);
    return error;
}

static Error fillGyroInfo(Gyro *gyro, GyroIndicatorsInfo *info)
{
    Error error = NoError;

    info->mode = gyro->mode;

    switch (gyro->mode)
    {
        case GyroMode_Accelerometer:
        {
            float raw = (float) gyro->indicatorValues[axe_x_idx];
            info->accData.x = gyro->corrections.accScale * raw;
            raw = (float) gyro->indicatorValues[axe_y_idx];
            info->accData.y = gyro->corrections.accScale * raw;
            raw = (float) gyro->indicatorValues[axe_z_idx];
            info->accData.z = gyro->corrections.accScale * raw;
            break;
        }
        case GyroMode_AngVel:
        {
            float raw = (float) gyro->indicatorValues[axe_x_idx];
            info->angvData.x = RAD2DEG(gyro->corrections.angvScale * raw);
            raw = (float) gyro->indicatorValues[axe_y_idx];
            info->angvData.y = RAD2DEG(gyro->corrections.angvScale * raw);
            raw = (float) gyro->indicatorValues[axe_z_idx];
            info->angvData.z = RAD2DEG(gyro->corrections.angvScale * raw);
            break;
        }
        case GyroMode_Temperature:
        {
            float raw = (float) gyro->indicatorValues[temp_idx];
            float offset = (float) gyro->corrections.tempOffset;
            float scale = gyro->corrections.tempScale;
            info->temp = (raw + offset) * scale;
            break;
        }
        default:
            CHK(ErrorInvalidParameters);
    }
cleanup:
    if (error != NoError)
        DBG_ERR("Fill gyro info failed with %d\n", error);
    return error;
}

static Error readSysFsData(Gyro *gyro)
{
    Error error = NoError;

    switch (gyro->mode)
    {
        case GyroMode_Accelerometer:
        case GyroMode_AngVel:
            CHK(readIndicatorVal(gyro, axe_x_idx, &gyro->indicatorValues[axe_x_idx]));
            CHK(readIndicatorVal(gyro, axe_y_idx, &gyro->indicatorValues[axe_y_idx]));
            CHK(readIndicatorVal(gyro, axe_z_idx, &gyro->indicatorValues[axe_z_idx]));
            break;
        case GyroMode_Temperature:
            CHK(readIndicatorVal(gyro, temp_idx, &gyro->indicatorValues[temp_idx]));
            break;
        default:
            DBG_GYRO("Unexpected mode");
            CHK(ErrorUnknown);
    }

cleanup:
    if (error != NoError)
        DBG_ERR("Reading data from sysfs failed with %d\n", error);
    return error;
}

static void sendCallback(Gyro *gyro)
{
    Error error = NoError;
    GyroIndicatorsInfo info;

    CHK(fillGyroInfo(gyro, &info));
    gyro->monitor.callback(&info, gyro->monitor.userData);

cleanup:
    if (error != NoError)
        DBG_ERR("Sending callback failed with %d\n", error);
}

void *file_monitor(void *arg)
{
    Error error = NoError;
    Gyro *gyro = (Gyro *)arg;
    int n, j, changed = 0;

    while(1)
    {
        n = poll(gyro->pfds, gyro->indicatorsCount, 1000);
        if(n != 0)
        {
            if (n == -1)
            {
                DBG_ERR("Gyro file monitor: poll error!\n");
                return NULL;
            }

            for(j = 0; j < gyro->indicatorsCount; j++)
            {
                if(gyro->pfds[j].revents & POLLIN)
                {
                    error = readIndicatorVal(gyro, j, &gyro->indicatorValues[j]);
                    if (error == NoError)
                        changed = 1;
                }
             }

             if (changed)
                 sendCallback(gyro);
        }
    }
    return NULL;
}

Error Gyro_initialize(Gyro **gyro, GyroMode mode)
{
    Error error = NoError;
    Gyro *_gyro = NULL;

    DBG_GYRO("Gyro_initialize\n");
    if (gyro == NULL)
        CHK(ErrorInvalidParameters);

    _gyro = calloc(1, sizeof(Gyro));
    if (_gyro == NULL)
        CHK(ErrorNoMem);

    _gyro->mode = mode;

    switch(mode)
    {
        case GyroMode_Accelerometer:
            _gyro->indicatorsFiles[axe_x_idx] = HW_ACC_X_PATH;
            _gyro->indicatorsFiles[axe_y_idx] = HW_ACC_Y_PATH;
            _gyro->indicatorsFiles[axe_z_idx] = HW_ACC_Z_PATH;
            break;
        case GyroMode_AngVel:
            _gyro->indicatorsFiles[axe_x_idx] = HW_GYRO_X_PATH;
            _gyro->indicatorsFiles[axe_y_idx] = HW_GYRO_Y_PATH;
            _gyro->indicatorsFiles[axe_z_idx] = HW_GYRO_Z_PATH;
            break;
        case GyroMode_Temperature:
            _gyro->indicatorsFiles[temp_idx] = HW_TEMP_PATH;
            break;
        default:
            CHK(ErrorInvalidParameters);
    }

    while(_gyro->indicatorsFiles[_gyro->indicatorsCount] != NULL)
        _gyro->pfds[_gyro->indicatorsCount++].fd = -1;

    CHK(readCorrections(_gyro));
    CHK(openFiles(_gyro));

    *gyro = _gyro;

    DBG_GYRO("Gyro_initialize: success\n");

cleanup:
    if (error != NoError)
    {
        DBG_ERR("Gyro_initialize: fails with %d error\n", error);
        Gyro_destroy(_gyro);
    }
    return error;
}

void Gyro_destroy(Gyro *gyro)
{
    DBG_GYRO("Gyro_destroy\n");
    if (gyro == NULL)
        return;

    if (gyro->monitor.active)
    {
        pthread_cancel(gyro->monitor.thread);
        pthread_join(gyro->monitor.thread, NULL);
    }

    closeFiles(gyro);
    free(gyro);
}

Error Gyro_getCurrentData(Gyro *gyro, GyroIndicatorsInfo *info, int fetch)
{
    Error error = NoError;

    DBG_GYRO("Gyro_getCurrentData\n");
    if (gyro == NULL || info == NULL)
        CHK(ErrorInvalidParameters);

    if (fetch)
        CHK(readSysFsData(gyro));

    fillGyroInfo(gyro, info);

    DBG_GYRO("Gyro_getCurrentData: success\n");

cleanup:
    if (error != NoError)
        DBG_ERR("Gyro_getCurrentData: fails with %d error\n", error);
    return error;
}

Error Gyro_setMonitoring(Gyro *gyro, GyroMonitorMode type, GyroCallback *callback, void *userData)
{
    Error error = NoError;

    DBG_GYRO("Gyro_setMonitoring\n");
    if (gyro == NULL || callback == NULL)
        CHK(ErrorInvalidParameters);

    gyro->monitor.callback = callback;
    gyro->monitor.userData = userData;

    switch (type)
    {
        case GyroMonitorMode_byFileChange:
            if(pthread_create(&gyro->monitor.thread, NULL, file_monitor, (void *)gyro))
            {
                DBG_GYRO("Gyro_setMonitoring: error creating thread\n");
                CHK(ErrorUnknown);
            }
            gyro->monitor.active = 1;
            break;
        default:
            DBG_GYRO("Gyro_setMonitoring: wrong monitoring type\n");
            CHK(ErrorInvalidParameters);
    }
    DBG_GYRO("Gyro_setMonitoring: success\n");

cleanup:
    if (error != NoError)
        DBG_ERR("Gyro_setMonitoring: fails with %d error\n", error);
    return error;
}
