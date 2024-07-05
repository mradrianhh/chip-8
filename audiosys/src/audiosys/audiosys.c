#include <stdlib.h>
#include <signal.h>

#include "audiosys/audiosys.h"

#include <audio/wave.h>

static bool CheckOpenalError();
static uint8_t *LoadWaveFile(const char *path, WaveInfo **wave);

static inline ALenum to_al_format(short channels, short samples)
{
    bool stereo = (channels > 1);

    switch (samples)
    {
    case 16:
        if (stereo)
            return AL_FORMAT_STEREO16;
        else
            return AL_FORMAT_MONO16;
    case 8:
        if (stereo)
            return AL_FORMAT_STEREO8;
        else
            return AL_FORMAT_MONO8;
    default:
        return -1;
    }
}

AudioContext *aud_CreateAudioContext(uint8_t numSlots)
{
    AudioContext *ctx = calloc(1, sizeof(AudioContext));
    ctx->numSlots = numSlots;
    ctx->slots = calloc(ctx->numSlots, sizeof(bool));
    ctx->logger = logger_Initialize(LOGS_BASE_PATH "audio.log", LOG_LEVEL_FULL);

    // Get default/preferred device name.
    ctx->devicename = alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);

    // Open the default/preferred device.
    ctx->device = alcOpenDevice(ctx->devicename);

    // Then we create a context.
    ctx->context = alcCreateContext(ctx->device, NULL);

    // Then we set the newly created context as the current context.
    alcMakeContextCurrent(ctx->context);

    // After creating the context, we now have to clear/initialize its error state.
    alGetError();

    // We create a buffer for each sound slot.
    ctx->source_buffers = calloc(ctx->numSlots, sizeof(ALuint));
    alGenBuffers(ctx->numSlots, ctx->source_buffers);
    if (CheckOpenalError())
        raise(SIGABRT);

    // We create a source for each sound slot.
    ctx->sources = calloc(ctx->numSlots, sizeof(ALuint));
    alGenSources(ctx->numSlots, ctx->sources);
    if (CheckOpenalError())
        raise(SIGABRT);

    return ctx;
}

void aud_DestroyAudioContext(AudioContext *ctx)
{
    // First we delete the sources.
    alDeleteSources(ctx->numSlots, ctx->sources);
    if (CheckOpenalError())
        raise(SIGABRT);

    // ... then the buffers.
    alDeleteBuffers(ctx->numSlots, ctx->source_buffers);
    if (CheckOpenalError())
        raise(SIGABRT);

    // Then we set the current context to NULL...
    alcMakeContextCurrent(NULL);

    // ... and delete the context.
    alcDestroyContext(ctx->context);

    // Finally we close the device.
    alcCloseDevice(ctx->device);

    // Destroy the logger.
    logger_Destroy(ctx->logger);

    // Free the calloc'ed memory for source buffers.
    free(ctx->source_buffers);

    // Free the calloc'ed memory for the sources.
    free(ctx->sources);

    // Free the calloc'ed memory for slots.
    free(ctx->slots);

    // Finally, free the context.
    free(ctx);
}

bool aud_CreateSound(AudioContext *ctx, const char *path, uint8_t slot, bool looping)
{
    if (ctx->slots[slot])
    {
        logger_LogInfo(ctx->logger, "Overwriting slot %d.", slot);
        // If slot is already taken, we need to detach buffer from source first.
        alSourcei(ctx->sources[slot], AL_BUFFER, NULL);
    }

    // Then we load our .wav file.
    WaveInfo *wave;
    uint8_t *buffer_data = LoadWaveFile(path, &wave);

    if (!buffer_data)
        return false;

    // Then we fill the buffer with data loaded from our .wav file...
    alBufferData(ctx->source_buffers[slot], to_al_format(wave->channels, wave->bitsPerSample),
                 buffer_data, wave->dataSize, wave->sampleRate);
    if (CheckOpenalError())
        raise(SIGABRT);

    // ... and connect it to its source.
    alSourcei(ctx->sources[slot], AL_BUFFER, ctx->source_buffers[slot]);
    if (CheckOpenalError())
        raise(SIGABRT);

    // If the sound is supposed to loop, we need to configure that aswell.
    if (looping)
    {
        alSourcei(ctx->sources[slot], AL_LOOPING, looping ? AL_TRUE : AL_FALSE);
    }

    ctx->slots[slot] = true;

    return true;
}

bool aud_PlaySound(AudioContext *ctx, uint8_t slot)
{
    if (!ctx->slots[slot])
    {
        logger_LogError(ctx->logger, "Attempting to play sound from empty slot(%d).", slot);
        return false;
    }

    // Get current state of source.
    ALint source_state;
    alGetSourcei(ctx->sources[slot], AL_SOURCE_STATE, &source_state);
    if (CheckOpenalError())
        raise(SIGABRT);

    if (source_state == AL_PLAYING || source_state == AL_LOOPING)
    {
        logger_LogError(ctx->logger, "Attempting to play sound that is already playing(%d).", slot);
        return false;
    }

    alSourcePlay(ctx->sources[slot]);
    if (CheckOpenalError())
        raise(SIGABRT);

    return true;
}

bool aud_StopSound(AudioContext *ctx, uint8_t slot)
{
    if (!ctx->slots[slot])
    {
        logger_LogError(ctx->logger, "Attempting to stop sound from empty slot(%d).", slot);
        return false;
    }

    // Get current state of source.
    ALint source_state;
    alGetSourcei(ctx->sources[slot], AL_SOURCE_STATE, &source_state);
    if (CheckOpenalError())
        raise(SIGABRT);

    if (source_state != AL_PLAYING && source_state != AL_LOOPING)
    {
        logger_LogError(ctx->logger, "Attemp&&ting to stop sound that is not playing(%d).", slot);
        return false;
    }

    alSourceStop(ctx->sources[slot]);
    if(CheckOpenalError())
        raise(SIGABRT);

    return true;
}

bool CheckOpenalError()
{
    ALCenum error = alGetError();
    switch (error)
    {
    case ALC_NO_ERROR:
        return false;
    case ALC_INVALID_VALUE:
        printf("OpenAL ERROR: INVALID VALUE.\n");
        return true;
    case ALC_INVALID_DEVICE:
        printf("OpenAL ERROR: INVALID DEVICE.\n");
        return true;
    case ALC_INVALID_CONTEXT:
        printf("OpenAL ERROR: INVALID CONTEXT.\n");
        return true;
    case ALC_INVALID_ENUM:
        printf("OpenAL ERROR: INVALID ENUM.\n");
        return true;
    case ALC_OUT_OF_MEMORY:
        printf("OpenAL ERROR: OUT OF MEMORY.\n");
        return true;
    default:
        printf("OpenAL ERROR: UNKNOWN.\n");
        return true;
    }
}

uint8_t *LoadWaveFile(const char *path, WaveInfo **wave)
{
    *wave = WaveOpenFileForReading(path);
    if (!*wave)
    {
        printf("Failed to read wave file %s.\n", path);
        return NULL;
    }

    if (WaveSeekFile(0, *wave))
    {
        printf("Failed to seek wave file %s.\n", path);
        return NULL;
    }

    uint8_t *buffer = malloc((*wave)->dataSize);
    if (!buffer)
    {
        printf("Failed to allocate buffer for wave file %s.", path);
        return NULL;
    }

    int size;
    if ((size = WaveReadFile(buffer, (*wave)->dataSize, *wave)) != (*wave)->dataSize)
    {
        printf("Failed while reading file. Expected %d bytes. Read %d bytes.\n",
               (*wave)->dataSize, size);
        return NULL;
    }

    return buffer;
}
