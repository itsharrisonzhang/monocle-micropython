#include "tensorflow/lite/micro/examples/micro_speech/command_responder.h"
#include "tensorflow/lite/micro/micro_log.h"

void RespondToCommand(int32_t current_time, const char* found_command,
                      uint8_t score, bool is_new_command) {
  if (is_new_command) {
    MicroPrintf("Heard %s (%d) @%dms", found_command, score, current_time);
  }
}
