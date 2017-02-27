#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>

#define CHK(fn) ({ do { error = (fn); if (error != NoError) { goto cleanup; } } while(0); })

#define DEBUG_I2C
#define DEBUG_DRAW

#define DBG_ERR(...) printf(__VA_ARGS__)

#ifdef DEBUG
#define DBG(...) printf(__VA_ARGS__)
#else
#define DBG(...)
#endif /* DEBUG */

#ifdef DEBUG_GYRO
#define DBG_GYRO(...) printf(__VA_ARGS__)
#else
#define DBG_GYRO(...)
#endif /* DEBUG_GYRO */

#ifdef DEBUG_DISP
#define DBG_DISP(...) printf(__VA_ARGS__)
#else
#define DBG_DISP(...)
#endif /* DEBUG_DISP */

#ifdef DEBUG_DRAW
#define DBG_DRAW(...) printf(__VA_ARGS__)
#else
#define DBG_DRAW(...)
#endif /* DEBUG_DRAW */

#ifdef DEBUG_I2C
#define DBG_I2C(...) printf(__VA_ARGS__)
#else
#define DBG_I2C(...)
#endif /* DEBUG_I2C */

typedef enum Error
{
    NoError = 0,
    ErrorInvalidParameters,
    ErrorNotFound,
    ErrorNoMem,
    ErrorRead,
    ErrorWrite,
    ErrorNotImplemented,
    ErrorUnknown
} Error;

#endif /* COMMON_H */
