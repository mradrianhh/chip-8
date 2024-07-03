#ifndef CORE_DISPLAY_H
#define CORE_DISPLAY_H

#include <stdint.h>
#include <pthread.h>

// 64 pixels width, 32 pixels height. 4 bytes per pixel.
#define CH8_DISPLAY_WIDTH (64)
#define CH8_DISPLAY_HEIGHT (32)
#define CH8_INTERNAL_DISPLAY_CHANNELS (4)
#define CH8_INTERNAL_DISPLAY_BUFFER_SIZE (CH8_DISPLAY_WIDTH * CH8_DISPLAY_HEIGHT * CH8_INTERNAL_DISPLAY_CHANNELS)
#define CH8_DISPLAY_BUFFER_SIZE (CH8_DISPLAY_WIDTH * CH8_DISPLAY_HEIGHT)

typedef struct Display
{
    pthread_mutex_t display_buffer_lock;
    uint8_t display_buffer[CH8_INTERNAL_DISPLAY_BUFFER_SIZE];
    size_t display_buffer_size;
    size_t display_buffer_width;
    size_t display_buffer_height;
    size_t display_buffer_channels;
} Display;

#endif
