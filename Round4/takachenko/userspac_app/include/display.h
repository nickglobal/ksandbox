#ifndef DISPLAY_H
#define DISPLAY_H

#include "common.h"

typedef struct Display Display;

typedef struct DisplayBitmap
{
    int     width;
    int     height;
    int     bitsPerPixel;
    int     dataSize;
    char   *data;
} DisplayBitmap;

/**
 * Initialize display
 */
Error Display_initialize(Display **display);

/**
 * Destroy display
 */
void Display_destroy(Display *display);

/**
 * Get display bitmap.
 */
DisplayBitmap *Display_getBitmap(Display *display);

/**
 * Refresh display.
 *
 * @param [in] display Display context
 *
 * @return NoError on success, error otherwise
 */
Error Display_refresh(Display *display);

#endif /* DISPLAY_H */
