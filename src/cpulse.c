/*
  Finds your pulseaudio output device, gets a read-mode connection to it,
  to read all the audio data that's going out through your speakers, from
  whatever source, and does beat detection on it.

  cpulse_pulse() reads the latest data from pulseaudio and returns a pointer
  to a beat detector, with state variables:

    isBassBeat
    isTrebleBeat

  There is no threading here.  You simply call cpulse_pulse() continuously
  to get continuous beat tracking.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pulse/error.h>
#include <pulse/simple.h>
#include "beatdetector.h"

#define PULSEAUDIO_SAMPLE_TYPE float
#define PULSEAUDIO_SAMPLE_TYPE_CODE PA_SAMPLE_FLOAT32LE

const unsigned int NUM_AUDIO_FRAMES = 32;
const uint8_t NUM_CHANNELS = 2;
const uint32_t SAMPLE_RATE = 44100;
const int BEAT_DETECTOR_BUFFER_LENGTH = 4096;

pa_simple *_pulseAudioClient;
int _pulseAudioError;

int _sampleSize;
PULSEAUDIO_SAMPLE_TYPE *_pulseAudioSamples;

beatdetector_t *_beatDetector;

/*
 * Finds the index of your running pulseaudio output device.
 */
void _getRunningSink(char *sinkIndex) {

    FILE *pipe = popen("pacmd list-sinks | grep state", "r");

    char buffer[128];
    int i = 0;
    int deviceIdx = -1;
    while (!feof(pipe)) {
        if (fgets(buffer, 128, pipe) != NULL) {
            if (strstr(buffer, "RUNNING")) {
                deviceIdx = i;
                break;
            } else {
                i++;
            }
        }
    }

    pclose(pipe);

    if (deviceIdx == -1) {
        printf("cpulse could not find a running pulseaudio sink device (ran pacmd list-sinks)\n");
        exit(1);
    } else {
        sprintf(sinkIndex, "%i", deviceIdx);
    }

}

/*
 * Starts up cpulse.
 */
void cpulse_start(void) {

    // find the running pulseaudio sink
    char sinkIndex[1];
    _getRunningSink(sinkIndex);
    const char *audioDevice = &sinkIndex[0];

    // initialize the sample buffer
    _pulseAudioSamples = malloc(sizeof(PULSEAUDIO_SAMPLE_TYPE) * NUM_AUDIO_FRAMES);
    _sampleSize = sizeof(PULSEAUDIO_SAMPLE_TYPE) * NUM_AUDIO_FRAMES;

    // make a pulseaudio connection
    printf("cpulse is connecting to pulseaudio sink %s... ", audioDevice);
    const pa_sample_spec paSampleSpec = { PULSEAUDIO_SAMPLE_TYPE_CODE, SAMPLE_RATE, NUM_CHANNELS };
    if (!(_pulseAudioClient = pa_simple_new( NULL, "cpulse", PA_STREAM_RECORD, audioDevice, "cpulse",
                                             &paSampleSpec, NULL, NULL, &_pulseAudioError ))) {
        printf("cpulse couldn't connect to pulseaudio: %s\n", pa_strerror(_pulseAudioError));
        exit(1);
    } else {
        printf("connected.\n");
    }

    // initialize a beat detector
    _beatDetector = new_beatdetector(BEAT_DETECTOR_BUFFER_LENGTH);

}

/*
 * Reads the latest audio data from pulseaudio and returns a pointer
 * to a beat detector with state variables isBassBeat and isTrebleBeat.
 */
beatdetector_t * cpulse_pulse(void) {

    // read the latest data from pulseaudio into _pulseAudioSamples
    if (pa_simple_read( _pulseAudioClient, _pulseAudioSamples, _sampleSize, &_pulseAudioError ) < 0) {
        printf("cpulse error reading from pulseaudio: %s\n", pa_strerror(_pulseAudioError));
    }

    // sum up _pulseAudioSamples
    PULSEAUDIO_SAMPLE_TYPE sampleSum = 0;
    int i;
    for (i = 0; i < NUM_AUDIO_FRAMES; i++) {
        sampleSum += _pulseAudioSamples[i];
    }

    // push that sum into the beat detector
    beatdetector_push(_beatDetector, sampleSum);

    return _beatDetector;

}

/*
 * Cleans up.
 */
void cpulse_stop(void) {

    pa_simple_free(_pulseAudioClient);
    free(_pulseAudioSamples);
    del_beatdetector(_beatDetector);
    printf("cpulse successfully closed its pulseaudio connection.\n");

}
