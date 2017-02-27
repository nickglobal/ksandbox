#include <stdlib.h>
#include <string.h>

#include "drawing.h"
#include "font.h"

#define HYSTORY_SIZE 128

#define COLOR_BLACK 0x00 /* Black clr, no pixel */
#define COLOR_WHITE 0x01 /* Pixel is set. */

#define INVERT_COLOR(c) ((c == COLOR_WHITE) ? COLOR_BLACK : COLOR_WHITE)

typedef struct Point
{
    int x;
    int y;
} Point;

typedef struct DrawingState
{
    Point currentPoint;
    int   inverted;
} DrawingState;

struct Drawing {
    Display       *display;
    DisplayBitmap *bitmap;
    DrawingState   state;
};

static Error drawPixel(Drawing *drawing, Point point, int color)
{
    int width = drawing->bitmap->width;

    if (point.x >= width || point.y >= width)
        return ErrorInvalidParameters;

    /* Check if pixels are inverted */
    if (drawing->state.inverted)
        color = INVERT_COLOR(color);

    /* Set color */
    if (color == COLOR_WHITE)
        drawing->bitmap->data[point.x + (point.y / 8) * width] |= 1 << (point.y % 8);
    else
        drawing->bitmap->data[point.x + (point.y / 8) * width] &= ~(1 << (point.y % 8));

    return NoError;
}

static Error moveToPoint(Drawing *drawing, Point point)
{
    drawing->state.currentPoint = point;
    return NoError;
}

static char putChar(Drawing *drawing, char ch, TM_FontDef_t *font, int color)
{
    int i, b, j, pixel_color;
    int width  = drawing->bitmap->width;
    int height = drawing->bitmap->height;
    Point pixelPoint;

    /* Check available space */
    if (width <= (drawing->state.currentPoint.x + font->FontWidth) ||
        height <= (drawing->state.currentPoint.y + font->FontHeight))
    {
        DBG_ERR("putChar failed - no place left\n");
        return 0;
    }

    /* Go through font */
    for (i = 0; i < font->FontHeight; i++)
    {
        b = font->data[(ch - 32) * font->FontHeight + i];
        for (j = 0; j < font->FontWidth; j++)
        {
            pixel_color = ((b << j) & 0x8000) ? color : INVERT_COLOR(color);
            pixelPoint.x = drawing->state.currentPoint.x + j;
            pixelPoint.y = drawing->state.currentPoint.y + i;
            drawPixel(drawing, pixelPoint, pixel_color);
        }
    }

    /* Increase pointer */
    drawing->state.currentPoint.x += font->FontWidth;
    /* Return character written */
    return ch;
}

static char putString(Drawing *drawing, char *string, TM_FontDef_t *font, int color)
{
    while (*string)
    {
        if (putChar(drawing, *string, font, color) != *string)
        {
            DBG_ERR("putString failed\n");
            return *string;
        }
        string++;
    }

    return *string; /* Everything OK, zero should be returned */
}

static void resetBitmap(Drawing *drawing)
{
    memset(drawing->bitmap->data, COLOR_BLACK, drawing->bitmap->dataSize);
    drawing->state.currentPoint.x = 0;
    drawing->state.currentPoint.y = 0;
    drawing->state.inverted = 0;
}

Error Drawing_initialize(Drawing **drawing, Display *display)
{
    Error   error     = NoError;
    Drawing *_drawing = NULL;

    if (drawing == NULL || display == NULL)
        CHK(ErrorInvalidParameters);

    _drawing = calloc(1, sizeof(Drawing));
    if (_drawing == NULL)
        CHK(ErrorNoMem);

    _drawing->display = display;
    _drawing->bitmap = Display_getBitmap(display);
    if (_drawing->bitmap == NULL)
        CHK(ErrorInvalidParameters);

    *drawing = _drawing;
cleanup:
    if (error != NoError)
    {
        DBG_ERR("Drawing initialization failed with %d\n", error);
        Drawing_destroy(_drawing);
    }
    return error;
}

void Drawing_destroy(Drawing *drawing)
{
    if (drawing == NULL)
        return;

    free(drawing);
}

Error Drawing_showAccelerometerData(Drawing *drawing, AxisData *axisData)
{
    Error error = NoError;
    char head[] = "Acceleration:";
    char xstr[20] = {0};
    char ystr[20] = {0};
    char zstr[20] = {0};
    Point start = {0, 8};
    sprintf(xstr, "x = %4.1f g", axisData->x);
    sprintf(ystr, "y = %4.1f g", axisData->y);
    sprintf(zstr, "z = %4.1f g", axisData->z);
    resetBitmap(drawing);
    moveToPoint(drawing, start);
    if (putString(drawing, head, &TM_Font_7x10, COLOR_WHITE))
        CHK(ErrorUnknown);
    start.y += 12;
    moveToPoint(drawing, start);
    if (putString(drawing, xstr, &TM_Font_7x10, COLOR_WHITE))
        CHK(ErrorUnknown);
    start.y += 12;
    moveToPoint(drawing, start);
    if (putString(drawing, ystr, &TM_Font_7x10, COLOR_WHITE))
        CHK(ErrorUnknown);
    start.y += 12;
    moveToPoint(drawing, start);
    if (putString(drawing, zstr, &TM_Font_7x10, COLOR_WHITE))
        CHK(ErrorUnknown);

    CHK(Display_refresh(drawing->display));
cleanup:
    if (error != NoError)
        DBG_ERR("Showing accelerometer data failed with %d\n", error);
    return error;
}

Error Drawing_showAngVelData(Drawing *drawing, AxisData *axisData)
{
    Error error = NoError;
    char head[] = "Angular Velocity:";
    char xstr[20] = {0};
    char ystr[20] = {0};
    char zstr[20] = {0};
    Point start = {0, 8};
    sprintf(xstr, "x = %5.1f deg/s", axisData->x);
    sprintf(ystr, "y = %5.1f deg/s", axisData->y);
    sprintf(zstr, "z = %5.1f deg/s", axisData->z);
    resetBitmap(drawing);
    moveToPoint(drawing, start);
    if (putString(drawing, head, &TM_Font_7x10, COLOR_WHITE))
        CHK(ErrorUnknown);
    start.y += 12;
    moveToPoint(drawing, start);
    if (putString(drawing, xstr, &TM_Font_7x10, COLOR_WHITE))
        CHK(ErrorUnknown);
    start.y += 12;
    moveToPoint(drawing, start);
    if (putString(drawing, ystr, &TM_Font_7x10, COLOR_WHITE))
        CHK(ErrorUnknown);
    start.y += 12;
    moveToPoint(drawing, start);
    if (putString(drawing, zstr, &TM_Font_7x10, COLOR_WHITE))
        CHK(ErrorUnknown);

    CHK(Display_refresh(drawing->display));
cleanup:
    if (error != NoError)
        DBG_ERR("Showing angular velocity data failed with %d\n", error);
    return error;
}

Error Drawing_showTemperatureData(Drawing *drawing, float temp)
{
    Error error = NoError;
    char head[] = "Temperature:";
    char tstr[20] = {0};
    Point start = {0, 8};
    sprintf(tstr, "%.1f C", temp);
    resetBitmap(drawing);
    moveToPoint(drawing, start);
    if (putString(drawing, head, &TM_Font_7x10, COLOR_WHITE))
        CHK(ErrorUnknown);
    start.y += 12;
    moveToPoint(drawing, start);
    if (putString(drawing, tstr, &TM_Font_7x10, COLOR_WHITE))
        CHK(ErrorUnknown);

    CHK(Display_refresh(drawing->display));
cleanup:
    if (error != NoError)
        DBG_ERR("Showing temperature data failed with %d\n", error);
    return error;
}

