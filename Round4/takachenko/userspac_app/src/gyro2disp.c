#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include  <signal.h>
#include <sys/time.h>

#include "gyro.h"
#include "display.h"
#include "drawing.h"

#define DEF_INTERVAL 50 /* ms */

static const char *m_values[] = {"acc", "angvel", "temp", 0};
//static const char *p_values[] = {"file", "int", 0};
static const char *p_values[] = {"file", 0};
static const char *u_values[] = {"onchange", 0};

struct Gyro2Disp {
    volatile int        running;
    GyroMode            gyroMode;
    GyroMonitorMode     gyroMonitorMode;
    int                 displayUpdateOnChange;
    int                 displayUpdateMs;
    GyroIndicatorsInfo  lastGyroInfo;
    Gyro               *gyro;
    Display            *display;
    Drawing            *drawing;
};

static struct Gyro2Disp gyro2disp;

void sighandler(int sig);

static int getValIdx(const char *values[], char *val)
{
    int i = 0;
    while(values[i]) {
        if(strcmp(values[i], val) == 0) {
            return i;
        }
        i++;
    }
    return -1;
}

static Error parseArgs(int argc, char *argv[])
{
    Error error = NoError;
    int i, idx;

    if (argc < 2) /* Nothing to parse */
        return error;

    for (i = 1; i < (argc - 1); i++)
    {
        if (argv[i][0] == '-') /* key */
        {
            switch (argv[i][1])
            {
                case 'm':
                    idx = getValIdx(m_values, argv[++i]);
                    gyro2disp.gyroMode = (idx < 0) ? 0 : idx;
                    break;
                case 'p':
                    idx = getValIdx(p_values, argv[++i]);
                    gyro2disp.gyroMonitorMode = (idx + 1);
                    break;
                case 'u':
                {
                    char *val = argv[++i];
                    idx = getValIdx(u_values, val);
                    if (idx < 0)
                        gyro2disp.displayUpdateMs = (int)strtol(val, NULL, 10);
                    else
                        gyro2disp.displayUpdateOnChange = 1;
                    break;
                }
                default:
                    DBG("Parse args: Unknown parameter: %s\n", argv[i]);
                break;
            }
        }
    }
    return error;
}

static Error updateGyroInfo(GyroIndicatorsInfo *info)
{
    if (info == NULL || gyro2disp.lastGyroInfo.mode != info->mode)
        return ErrorInvalidParameters;
    memcpy(&gyro2disp.lastGyroInfo, info, sizeof(GyroIndicatorsInfo));
    return NoError;
}

static int cmpGyroInfo(GyroIndicatorsInfo *i1, GyroIndicatorsInfo *i2)
{
    int changed = 0;

    if (i1->mode != i2->mode)
        return 1;

    switch (i1->mode)
    {
        case GyroMode_Accelerometer:
            if (i1->accData.x != i2->accData.x ||
                i1->accData.y != i2->accData.y ||
                i1->accData.z != i2->accData.z)
                changed = 1;
            break;
        case GyroMode_AngVel:
            if (i1->angvData.x != i2->angvData.x ||
                i1->angvData.y != i2->angvData.y ||
                i1->angvData.z != i2->angvData.z)
                changed = 1;
            break;
        case GyroMode_Temperature:
            if (i1->temp != i2->temp)
            changed = 1;
            break;
        default:
            break;
    }

    return changed;
}

void refreshScreen()
{
    switch (gyro2disp.gyroMode)
    {
        case GyroMode_Accelerometer:
            DBG("ACC: x = %.4f, y = %.4f, z = %.4f\n", gyro2disp.lastGyroInfo.accData.x,
                                                       gyro2disp.lastGyroInfo.accData.y,
                                                       gyro2disp.lastGyroInfo.accData.z);
            Drawing_showAccelerometerData(gyro2disp.drawing, &gyro2disp.lastGyroInfo.accData);
            break;
        case GyroMode_AngVel:
            DBG("ANGV: x = %.4f, y = %.4f, z = %.4f\n", gyro2disp.lastGyroInfo.angvData.x,
                                                        gyro2disp.lastGyroInfo.angvData.y,
                                                        gyro2disp.lastGyroInfo.angvData.z);
            Drawing_showAngVelData(gyro2disp.drawing, &gyro2disp.lastGyroInfo.angvData);
            break;
        case GyroMode_Temperature:
            DBG("TEMP: %.2f\n", gyro2disp.lastGyroInfo.temp);
            Drawing_showTemperatureData(gyro2disp.drawing, gyro2disp.lastGyroInfo.temp);
            break;
        default:
            DBG_ERR("Refresh screeen: Unexpected mode!\n");
            break;
    }
}

void gyroCallback(GyroIndicatorsInfo *info, void *userData)
{
    struct Gyro2Disp *g2d = (struct Gyro2Disp *)userData;
    if (g2d == NULL || updateGyroInfo(info) != NoError)
    {
        DBG("Gyro callback: invalid parameters");
        return;
    }

    if (gyro2disp.displayUpdateOnChange)
        refreshScreen();
}

static void run()
{
    GyroIndicatorsInfo gyroInfo;
    unsigned long us_period, us_interval;
    int change;
    struct timeval stop, start;

    us_period = (gyro2disp.displayUpdateMs > 0 ? gyro2disp.displayUpdateMs : DEF_INTERVAL) * 1000;
    gyro2disp.running = 1;

    while (gyro2disp.running)
    {
        change = 0;
        gettimeofday(&start, NULL);
        if (gyro2disp.gyroMonitorMode == GyroMonitorMode_None &&
            Gyro_getCurrentData(gyro2disp.gyro, &gyroInfo, 1) == NoError)
        {
            if (gyro2disp.displayUpdateOnChange &&
                cmpGyroInfo(&gyro2disp.lastGyroInfo, &gyroInfo) != 0)
            {
                change = 1;
            }
            updateGyroInfo(&gyroInfo);
        }

        if (!gyro2disp.displayUpdateOnChange || change)
            refreshScreen();

        gettimeofday(&stop, NULL);
        us_interval = stop.tv_usec - start.tv_usec;
        usleep(us_interval < us_period ? us_period - us_interval : 0);
    }
}

int main(int argc, char *argv[])
{
    Error error;

    DBG("Gyro2Disp\n");

    signal(SIGINT, sighandler);
    gyro2disp.running = 0;
    gyro2disp.gyroMode = GyroMode_Accelerometer;
    gyro2disp.gyroMonitorMode = GyroMonitorMode_None;
    gyro2disp.displayUpdateOnChange = 0;
    gyro2disp.gyro = NULL;
    gyro2disp.display = NULL;

    CHK(parseArgs(argc, argv));
    CHK(Gyro_initialize(&gyro2disp.gyro, gyro2disp.gyroMode));
    CHK(Display_initialize(&gyro2disp.display));
    CHK(Drawing_initialize(&gyro2disp.drawing, gyro2disp.display));

    CHK(Gyro_getCurrentData(gyro2disp.gyro, &gyro2disp.lastGyroInfo, 1));

    if (gyro2disp.gyroMonitorMode != GyroMonitorMode_None)
    {
        CHK(Gyro_setMonitoring(gyro2disp.gyro, gyro2disp.gyroMonitorMode, &gyroCallback, &gyro2disp));
    }

    run();

cleanup:
    Gyro_destroy(gyro2disp.gyro);
    Drawing_destroy(gyro2disp.drawing);
    Display_destroy(gyro2disp.display);
    return error == NoError ? 0 : -1;
}

void sighandler(int sig)
{
    char  c;
    signal(sig, SIG_IGN);
    printf("\n\nDo you really want to quit? [y/n]: ");
    c = getchar();
    if (c == 'y' || c == 'Y')
        gyro2disp.running = 0;
    else
        signal(SIGINT, sighandler);
}
