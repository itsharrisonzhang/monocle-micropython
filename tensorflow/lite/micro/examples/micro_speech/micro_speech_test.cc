#include "tensorflow/lite/micro/examples/micro_speech/micro_features/no_micro_features_data.h"
#include "tensorflow/lite/micro/examples/micro_speech/micro_features/yes_micro_features_data.h"
#include "tensorflow/lite/micro/examples/micro_speech/micro_speech_model_data.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/testing/micro_test.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "tensorflow/lite/micro/examples/micro_speech/feature_provider.h"
#include "tensorflow/lite/micro/examples/micro_speech/audio_provider.h"
#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/micro/examples/micro_speech/micro_features/micro_model_settings.h"

namespace {
  int32_t previous_time = 0;
}

int main() {
  const tflite::Model* model = ::tflite::GetModel(g_micro_speech_model_data);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    MicroPrintf(
        "Model provided is schema version %d not equal "
        "to supported version %d.\n",
        model->version(), TFLITE_SCHEMA_VERSION);
  }

  tflite::MicroMutableOpResolver<4> micro_op_resolver;
  micro_op_resolver.AddDepthwiseConv2D();
  micro_op_resolver.AddFullyConnected();
  micro_op_resolver.AddReshape();
  micro_op_resolver.AddSoftmax();

  #if (defined(XTENSA) && defined(VISION_P6))
    constexpr int tensor_arena_size = 28 * 1024;
  #elif defined(XTENSA)
    constexpr int tensor_arena_size = 15 * 1024;
  #elif defined(HEXAGON)
    constexpr int tensor_arena_size = 25 * 1024;
  #else
    constexpr int tensor_arena_size = 10 * 1024;
  #endif
    alignas(16) uint8_t tensor_arena[tensor_arena_size];

  // Build an interpreter to run the model with.
  tflite::MicroInterpreter interpreter(model, micro_op_resolver, tensor_arena,
                                      tensor_arena_size);
  interpreter.AllocateTensors();

  TfLiteTensor* input = interpreter.input(0);
  TF_LITE_MICRO_EXPECT(input != nullptr);
  TF_LITE_MICRO_EXPECT_EQ(2, input->dims->size);
  TF_LITE_MICRO_EXPECT_EQ(1, input->dims->data[0]);
  TF_LITE_MICRO_EXPECT_EQ(1960, input->dims->data[1]);
  TF_LITE_MICRO_EXPECT_EQ(kTfLiteInt8, input->type);




  // Feature provider testing
  // const int8_t* yes_features_data = g_yes_micro_f2e59fea_nohash_1_data;
  int8_t feature_data[kFeatureElementCount];
  FeatureProvider feature_provider(kFeatureElementCount, feature_data);

  int32_t current_time = LatestAudioTimestamp();
  int how_many_new_slices = 0;
  TfLiteStatus populate_status = feature_provider.PopulateFeatureData(previous_time, current_time, &how_many_new_slices);
  TF_LITE_MICRO_EXPECT_EQ(kTfLiteOk, populate_status);
  TF_LITE_MICRO_EXPECT_EQ(kFeatureSliceCount, how_many_new_slices);
  previous_time = current_time;




  // // Copy a spectrogram created from a .wav audio file of someone saying "Yes",
  // into the memory area used for the input.
  for (size_t i = 0; i < input->bytes; ++i) {
    // input->data.int8[i] = yes_features_data[i];
    input->data.int8[i] = feature_data[i];
  }
  MicroPrintf("Input bytes: %d", input->bytes);

  // Run the model on this input and make sure it succeeds.
  TfLiteStatus invoke_status = interpreter.Invoke();
  if (invoke_status != kTfLiteOk) {
    MicroPrintf("Invoke failed\n");
  }
  TF_LITE_MICRO_EXPECT_EQ(kTfLiteOk, invoke_status);

  // Get the output from the model, make sure it's the expected size and type.
  TfLiteTensor* output = interpreter.output(0);
  TF_LITE_MICRO_EXPECT_EQ(2, output->dims->size);
  TF_LITE_MICRO_EXPECT_EQ(1, output->dims->data[0]);
  TF_LITE_MICRO_EXPECT_EQ(4, output->dims->data[1]);
  TF_LITE_MICRO_EXPECT_EQ(kTfLiteInt8, output->type);

  // Make sure that the expected "Yes" score is higher than the other classes.
  uint8_t silence_score = output->data.int8[kSilenceIndex] + 128;
  uint8_t unknown_score = output->data.int8[kUnknownIndex] + 128;
  uint8_t yes_score = output->data.int8[kYesIndex] + 128;
  uint8_t no_score = output->data.int8[kNoIndex] + 128;
  TF_LITE_MICRO_EXPECT_GT(yes_score, silence_score);
  TF_LITE_MICRO_EXPECT_GT(yes_score, unknown_score);
  TF_LITE_MICRO_EXPECT_GT(yes_score, no_score);

  MicroPrintf("Silence: %d, Unknown: %d, Yes: %d, No: %d\n",
      silence_score, unknown_score, yes_score, no_score);

  return kTfLiteOk;
}

namespace micro_test {
  int tests_passed;
  int tests_failed;
  bool is_test_complete;
  bool did_test_fail;
}
