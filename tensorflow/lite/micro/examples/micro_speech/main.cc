#include "tensorflow/lite/micro/examples/micro_speech/main_functions.h"
#include <iostream>
int main(int argc, char* argv[]) {
  setup();
  while (true) {
    loop();
  }
  // std::cout << "hello" << std::endl;
}
