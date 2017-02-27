#ifndef DRAWING_H
#define DRAWING_H
#include "common.h"
#include "gyro.h"
#include "display.h"

typedef struct Drawing Drawing;


Error Drawing_initialize(Drawing **drawing, Display *display);
void Drawing_destroy(Drawing *drawing);

Error Drawing_showAccelerometerData(Drawing *drawing, AxisData *axisData);
Error Drawing_showAngVelData(Drawing *drawing, AxisData *axisData);
Error Drawing_showTemperatureData(Drawing *drawing, float temp);

#endif /* DRAWING_H */
