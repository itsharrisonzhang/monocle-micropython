#include "tensorflow/lite/micro/examples/micro_speech/audio_provider.h"
// #include "c_microphone.h"

namespace {
  int16_t g_audio_output_buffer[kMaxAudioSampleSize];
  uint8_t g_audio_capture_buffer[kAudioCaptureBufferSize];
  volatile int32_t g_latest_audio_timestamp = 0;
  constexpr int DEFAULT_BUFFER_SIZE = 512;
}

void CaptureSamples() {
  // Find correct index in `g_audio_capture_buffer' to read to for continuous audio
//   const int number_of_samples = DEFAULT_BUFFER_SIZE / 2;
//   const int32_t time_in_ms = g_latest_audio_timestamp + (number_of_samples / (kAudioSampleFrequency / 1000));
//   const int32_t start_sample_offset = g_latest_audio_timestamp * (kAudioSampleFrequency / 1000);
//   const int capture_index = start_sample_offset % kAudioCaptureBufferSize;

//   const float record_duration = (number_of_samples / (kAudioSampleFrequency / 1000.)) / 1000.;
//   int record_status = microphone_record(record_duration, 16, kAudioSampleFrequency);
//   if (record_status != kTfLiteOk) {
//     return;
//   }
//   int read_status = microphone_read(127, g_audio_capture_buffer + capture_index, DEFAULT_BUFFER_SIZE);
//   if (read_status != kTfLiteOk) {
//     return;
//   }
//   g_latest_audio_timestamp = time_in_ms;

  // Simulate sample capture
  const int number_of_samples = DEFAULT_BUFFER_SIZE / 2;
  const int32_t time_in_ms = g_latest_audio_timestamp + (number_of_samples / (kAudioSampleFrequency / 1000));
  const int32_t start_sample_offset = g_latest_audio_timestamp * (kAudioSampleFrequency / 1000);
  const int capture_index = start_sample_offset % kAudioCaptureBufferSize;
  for (int i = capture_index; i < DEFAULT_BUFFER_SIZE; ++i) {
    g_audio_capture_buffer[i] = 75;
  }
  g_latest_audio_timestamp = time_in_ms;
}

TfLiteStatus GetAudioSamples(int start_ms, int duration_ms,
                             int* audio_samples_size, int16_t** audio_samples) {
  // Read audio from microphone
  CaptureSamples();

  // Circular buffer for data collection (using overwrites)
  const int start_offset = start_ms * (kAudioSampleFrequency / 1000);
  const int duration_sample_count = duration_ms * (kAudioSampleFrequency / 1000);
  for (int i = 0; i < duration_sample_count; ++i) {
    const int capture_index = (start_offset + i) % kAudioCaptureBufferSize;
    g_audio_output_buffer[i] = (int16_t)g_audio_capture_buffer[capture_index];
  }

  *audio_samples_size = kMaxAudioSampleSize;
  *audio_samples = g_audio_output_buffer;
  return kTfLiteOk;
}

int32_t LatestAudioTimestamp() {
  return g_latest_audio_timestamp;
}