#ifndef TENSORFLOW_LITE_MICRO_EXAMPLES_MICRO_SPEECH_AUDIO_PROVIDER_H_
#define TENSORFLOW_LITE_MICRO_EXAMPLES_MICRO_SPEECH_AUDIO_PROVIDER_H_

#include "tensorflow/lite/micro/examples/micro_speech/micro_features/micro_model_settings.h"
#include "tensorflow/lite/c/common.h"

#define kAudioCaptureBufferSize 8192

// This is an abstraction around an audio source like a microphone, and is
// expected to return 16-bit PCM sample data for a given point in time. The
// sample data itself should be used as quickly as possible by the caller, since
// to allow memory optimizations there are no guarantees that the samples won't
// be overwritten by new data in the future. In practice, implementations should
// ensure that there's a reasonable time allowed for clients to access the data
// before any reuse.
// The reference implementation can have no platform-specific dependencies, so
// it just returns an array filled with zeros. For real applications, you should
// ensure there's a specialized implementation that accesses hardware APIs.
TfLiteStatus GetAudioSamples(int start_ms, int duration_ms,
                             int* audio_samples_size, int16_t** audio_samples);

int32_t LatestAudioTimestamp();

#endif