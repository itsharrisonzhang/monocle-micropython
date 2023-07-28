extern "C" {

#include "experimental_helper.h"

int _factorial(int const x) {
    if (x > 10) {
        return 0;
    }
    if (x == 0 || x == 1) {
        return 1;
    }
    return x * _factorial(x - 1);
}

int _square(int const x) {
    if (x > 1000) {
        return 0;
    }
    return x * x;
}
}
