#include "recording.h"
#include <portaudio.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define SAMPLE_RATE 44100
#define NUM_CHANNELS 1
#define SAMPLE_FORMAT paInt16
#define FRAMES_PER_BUFFER 512

static int g_selected_device = -1;

// Only return input devices (microphones)
char **get_input_device_names(void) {
    Pa_Initialize();
    int numDevices = Pa_GetDeviceCount();
    if (numDevices < 0) return NULL;

    // Count input devices
    int count = 0;
    for (int i = 0; i < numDevices; ++i) {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
        if (info->maxInputChannels > 0)
            count++;
    }

    char **names = g_new0(char *, count + 1);
    int j = 0;
    for (int i = 0; i < numDevices; ++i) {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
        if (info->maxInputChannels > 0) {
            names[j++] = g_strdup(info->name);
        }
    }
    names[count] = NULL;
    Pa_Terminate();
    return names;
}

void set_recording_device(int device_index) {
    g_selected_device = device_index;
}

typedef struct {
    FILE *file;
    int total_frames;
} AudioData;

static void write_wav_header(FILE *file, int sample_rate, int num_channels, int bits_per_sample, int data_size) {
    uint8_t header[44];
    int byte_rate = sample_rate * num_channels * bits_per_sample / 8;
    int block_align = num_channels * bits_per_sample / 8;
    memcpy(header, "RIFF", 4);
    *(uint32_t*)(header + 4) = 36 + data_size;
    memcpy(header + 8, "WAVEfmt ", 8);
    *(uint32_t*)(header + 16) = 16;
    *(uint16_t*)(header + 20) = 1;
    *(uint16_t*)(header + 22) = num_channels;
    *(uint32_t*)(header + 24) = sample_rate;
    *(uint32_t*)(header + 28) = byte_rate;
    *(uint16_t*)(header + 32) = block_align;
    *(uint16_t*)(header + 34) = bits_per_sample;
    memcpy(header + 36, "data", 4);
    *(uint32_t*)(header + 40) = data_size;
    fwrite(header, 1, 44, file);
}

static int record_callback(const void *input, void *output,
                          unsigned long frameCount,
                          const PaStreamCallbackTimeInfo* timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void *userData)
{
    AudioData *data = (AudioData*)userData;
    if (input && data->file) {
        fwrite(input, sizeof(int16_t), frameCount * NUM_CHANNELS, data->file);
        data->total_frames += frameCount;
    }
    return paContinue;
}

bool record_audio(const char *filename, int seconds) {
    PaStream *stream;
    PaError err;
    AudioData data = {0};

    FILE *file = fopen(filename, "wb+");
    if (!file) return false;
    // Reserve space for WAV header
    fseek(file, 44, SEEK_SET);

    PaStreamParameters inputParams;
    const PaDeviceInfo *info;

    Pa_Initialize();
    int numDevices = Pa_GetDeviceCount();

    // Find the Nth input device (since we only show input devices in the dropdown)
    int input_device_index = -1;
    int input_count = 0;
    for (int i = 0; i < numDevices; ++i) {
        const PaDeviceInfo *dev = Pa_GetDeviceInfo(i);
        if (dev->maxInputChannels > 0) {
            if (input_count == g_selected_device) {
                input_device_index = i;
                break;
            }
            input_count++;
        }
    }
    if (input_device_index == -1)
        inputParams.device = Pa_GetDefaultInputDevice();
    else
        inputParams.device = input_device_index;

    info = Pa_GetDeviceInfo(inputParams.device);
    inputParams.channelCount = 1;
    inputParams.sampleFormat = paInt16;
    inputParams.suggestedLatency = info->defaultLowInputLatency;
    inputParams.hostApiSpecificStreamInfo = NULL;

    data.file = file;
    data.total_frames = 0;

    err = Pa_OpenStream(&stream, &inputParams, NULL, SAMPLE_RATE, FRAMES_PER_BUFFER, paClipOff, record_callback, &data);

    if (err != paNoError) { fclose(file); Pa_Terminate(); return false; }

    Pa_StartStream(stream);
    Pa_Sleep(seconds * 1000);
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();

    int data_size = data.total_frames * NUM_CHANNELS * sizeof(int16_t);
    fseek(file, 0, SEEK_SET);
    write_wav_header(file, SAMPLE_RATE, NUM_CHANNELS, 16, data_size);
    fclose(file);

    return true;
}

static int play_callback(const void *input, void *output,
                         unsigned long frameCount,
                         const PaStreamCallbackTimeInfo* timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void *userData)
{
    AudioData *data = (AudioData*)userData;
    size_t read = fread(output, sizeof(int16_t), frameCount * NUM_CHANNELS, data->file);
    if (read < frameCount * NUM_CHANNELS) {
        memset((int16_t*)output + read, 0, (frameCount * NUM_CHANNELS - read) * sizeof(int16_t));
        return paComplete;
    }
    return paContinue;
}

bool play_audio(const char *filename) {
    PaStream *stream;
    PaError err;
    AudioData data = {0};

    FILE *file = fopen(filename, "rb");
    if (!file) return false;
    fseek(file, 44, SEEK_SET); // skip WAV header

    err = Pa_Initialize();
    if (err != paNoError) { fclose(file); return false; }

    data.file = file;

    err = Pa_OpenDefaultStream(&stream, 0, NUM_CHANNELS, SAMPLE_FORMAT, SAMPLE_RATE, FRAMES_PER_BUFFER, play_callback, &data);
    if (err != paNoError) { fclose(file); Pa_Terminate(); return false; }

    Pa_StartStream(stream);
    while (Pa_IsStreamActive(stream)) {
        Pa_Sleep(100);
    }
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();

    fclose(file);
    return true;
}