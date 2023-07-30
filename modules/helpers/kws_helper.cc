
#include "../kws.h"
#include "tensorflow/lite/micro/examples/micro_speech/micro_features/no_micro_features_data.h"
#include "tensorflow/lite/micro/examples/micro_speech/micro_features/yes_micro_features_data.h"
#include "tensorflow/lite/micro/examples/micro_speech/micro_speech_model_data.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/testing/micro_test.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include <chrono>

int dummy_loop() {
  return kTfLiteOk;
}

extern "C" {

int loop();

mp_obj_t run() {
    // auto const work_duration = std::chrono::seconds{10};
    // auto const start = std::chrono::high_resolution_clock::now();
    // auto stop = std::chrono::high_resolution_clock::now();
    // auto duration = std::chrono::duration_cast<std::chrono::seconds>(stop - start);

    // int run_status = kTfLiteOk;
    // while (duration < work_duration && run_status == kTfLiteOk) {
    //     run_status = loop();
    //     stop = std::chrono::high_resolution_clock::now();
    //     duration = std::chrono::duration_cast<std::chrono::seconds>(stop - start);
    // }
    loop();
    return mp_const_none;
}

int loop() {
  MicroPrintf("Testing TestInvoke");
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
  // TF_LITE_MICRO_EXPECT(input != nullptr);
  // TF_LITE_MICRO_EXPECT_EQ(2, input->dims->size);
  // TF_LITE_MICRO_EXPECT_EQ(1, input->dims->data[0]);
  // TF_LITE_MICRO_EXPECT_EQ(1960, input->dims->data[1]);
  // TF_LITE_MICRO_EXPECT_EQ(kTfLiteInt8, input->type);

  // Copy a spectrogram created from a .wav audio file of someone saying "Yes",
  // into the memory area used for the input.

  // TODO: Interface microphone here
  const int8_t* yes_features_data = g_yes_micro_f2e59fea_nohash_1_data;
  for (size_t i = 0; i < input->bytes; ++i) {
    input->data.int8[i] = yes_features_data[i];
  }

  // Run the model on this input and make sure it succeeds.
  TfLiteStatus invoke_status = interpreter.Invoke();
  if (invoke_status != kTfLiteOk) {
    MicroPrintf("Invoke failed\n");
  }
  // TF_LITE_MICRO_EXPECT_EQ(kTfLiteOk, invoke_status);

  // Get the output from the model, make sure it's the expected size and type.
  TfLiteTensor* output = interpreter.output(0);
  // TF_LITE_MICRO_EXPECT_EQ(2, output->dims->size);
  // TF_LITE_MICRO_EXPECT_EQ(1, output->dims->data[0]);
  // TF_LITE_MICRO_EXPECT_EQ(4, output->dims->data[1]);
  // TF_LITE_MICRO_EXPECT_EQ(kTfLiteInt8, output->type);

  // There are four possible classes in the output, each with a score.
  int constexpr const kSilenceIndex = 0;
  int constexpr const kUnknownIndex = 1;
  int constexpr const kYesIndex = 2;
  int constexpr const kNoIndex = 3;

  // Make sure that the expected "Yes" score is higher than the other classes.
  uint8_t silence_score = output->data.int8[kSilenceIndex] + 128;
  uint8_t unknown_score = output->data.int8[kUnknownIndex] + 128;
  uint8_t yes_score = output->data.int8[kYesIndex] + 128;
  uint8_t no_score = output->data.int8[kNoIndex] + 128;
  // TF_LITE_MICRO_EXPECT_GT(yes_score, silence_score);
  // TF_LITE_MICRO_EXPECT_GT(yes_score, unknown_score);
  // TF_LITE_MICRO_EXPECT_GT(yes_score, no_score);

  MicroPrintf("Silence: %d, Unknown: %d, Yes: %d, No: %d\n",
      silence_score, unknown_score, yes_score, no_score);

  // --------------------------------------------------------------------------------- //

  // Now test with a different input, from a recording of "No".
  const int8_t* no_features_data = g_no_micro_f9643d42_nohash_4_data;
  for (size_t i = 0; i < input->bytes; ++i) {
    input->data.int8[i] = no_features_data[i];
  }

  // Run the model on this "No" input.
  invoke_status = interpreter.Invoke();
  if (invoke_status != kTfLiteOk) {
    MicroPrintf("Invoke failed\n");
  }
  // TF_LITE_MICRO_EXPECT_EQ(kTfLiteOk, invoke_status);

  // Get the output from the model, make sure it's the expected size and type.
  output = interpreter.output(0);
  // TF_LITE_MICRO_EXPECT_EQ(2, output->dims->size);
  // TF_LITE_MICRO_EXPECT_EQ(1, output->dims->data[0]);
  // TF_LITE_MICRO_EXPECT_EQ(4, output->dims->data[1]);
  // TF_LITE_MICRO_EXPECT_EQ(kTfLiteInt8, output->type);

  // Make sure that the expected "No" score is higher than the other classes.
  silence_score = output->data.int8[kSilenceIndex] + 128;
  unknown_score = output->data.int8[kUnknownIndex] + 128;
  yes_score = output->data.int8[kYesIndex] + 128;
  no_score = output->data.int8[kNoIndex] + 128;
  // TF_LITE_MICRO_EXPECT_GT(no_score, silence_score);
  // TF_LITE_MICRO_EXPECT_GT(no_score, unknown_score);
  // TF_LITE_MICRO_EXPECT_GT(no_score, yes_score);

  MicroPrintf("Silence: %d, Unknown: %d, Yes: %d, No: %d\n",
      silence_score, unknown_score, yes_score, no_score);

  MicroPrintf("Ran successfully.\n");
  return kTfLiteOk;
}

}