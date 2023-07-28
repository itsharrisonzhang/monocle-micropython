#include "py/runtime.h"
#include "py/obj.h"

#include "tensorflow/lite/micro/examples/micro_speech/micro_features/no_micro_features_data.h"
#include "tensorflow/lite/micro/examples/micro_speech/micro_features/yes_micro_features_data.h"
#include "tensorflow/lite/micro/examples/micro_speech/micro_speech_model_data.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/testing/micro_test.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include <chrono>

STATIC void run();

int loop();
