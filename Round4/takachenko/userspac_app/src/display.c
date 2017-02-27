#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "display.h"
#include "hwconfig.h"

struct Display {
    int             mmapFd;
    DisplayBitmap   bitmap;
};

static void destroyDisplayBitmap(Display* display)
{
    close(display->mmapFd);
}

static Error initDisplayBitmap(Display *display)
{
    size_t data_size;
    Error  error   = NoError;
    char  *address = NULL;

    data_size = (DISPLAY_WIDTH * DISPLAY_HEIGHT / 8) * DISPLAY_BITS_PER_PIXEL;
    if (data_size > MMAP_BUFFER_SIZE)
        CHK(ErrorNoMem);

    display->mmapFd = open("/sys/kernel/debug/"MMAP_FILE_NAME, O_RDWR);
    if(display->mmapFd < 0)
        CHK(ErrorNotFound);

    address = mmap(NULL, MMAP_BUFFER_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, display->mmapFd, 0);
    if (address == MAP_FAILED)
        CHK(ErrorUnknown);

    /* Clear screen */
    memset(address, 0x00, data_size);
    CHK(Display_refresh(display));

    display->bitmap.dataSize = data_size;
    display->bitmap.width = DISPLAY_WIDTH;
    display->bitmap.height = DISPLAY_HEIGHT;
    display->bitmap.bitsPerPixel = DISPLAY_BITS_PER_PIXEL;
    display->bitmap.data = address;


cleanup:
    if (error != NoError)
        DBG_ERR("bitmap initialization failed with %d\n", error);
    return error;
}

Error Display_initialize(Display **display)
{
    Error error;
    Display *_display;

    DBG_DISP("Display_initialize\n");
    if (display == NULL)
        CHK(ErrorInvalidParameters);

    _display = calloc(1, sizeof(Display));
    if (_display == NULL)
        CHK(ErrorNoMem);

    CHK(initDisplayBitmap(_display));

    *display = _display;

    DBG_ERR("Display_initialize: success\n");
        return error;

cleanup:
    DBG_DISP("Display_initialize: failed with %d error\n", error);
    Display_destroy(_display);
    return error;
}

void Display_destroy(Display *display)
{
    DBG_DISP("Display_destroy\n");

    if (display == NULL)
        return;
    destroyDisplayBitmap(display);
    free(display);
}

DisplayBitmap *Display_getBitmap(Display *display)
{
    if (display != NULL)
        return &display->bitmap;
    return NULL;
}

Error Display_refresh(Display *display)
{
    Error error = NoError;

    if (display == NULL)
        CHK(ErrorInvalidParameters);

    if(ioctl(display->mmapFd, IOCTL_UPDATE_SCREEN, NULL))
        CHK(ErrorUnknown);

cleanup:
    if (error != NoError)
        DBG_ERR("Didplsy refresh failed with %d\n", error);
    return error;
}