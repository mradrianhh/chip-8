#ifndef AUDIOSYS_AUDIOSYS_H
#define AUDIOSYS_AUDIOSYS_H

#include <stdbool.h>
#include <stdint.h>

#include <AL/al.h>
#include <AL/alc.h>

#include <logger/logger.h>

#ifdef CH8_SOUNDS_DIR
#define SOUNDS_BASE_PATH CH8_SOUNDS_DIR
#else
#define SOUNDS_BASE_PATH "../../assets/sounds/"
#endif

typedef struct AudioContext
{
    const ALchar *devicename;
    ALCdevice *device;
    ALCcontext *context;
    uint8_t numSlots;
    bool *slots;
    ALuint *source_buffers;
    ALuint *sources;
    // Internal
    Logger *logger;
} AudioContext;

AudioContext *aud_CreateAudioContext(uint8_t numSlots);

void aud_DestroyAudioContext(AudioContext *ctx);

bool aud_CreateSound(AudioContext *ctx, const char *path, uint8_t slot, bool looping);

bool aud_PlaySound(AudioContext *ctx, uint8_t slot);

bool aud_StopSound(AudioContext *ctx, uint8_t slot);

#endif
