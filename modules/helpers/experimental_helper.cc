extern "C" {

#include "experimental_helper.h"
#include <cstdlib>

class hi {
    public :
        hi(int i) : x(i), y(i + 1) {
            void* m = malloc(4096);
            free(m);
            x = abs(x);
        }
    
    private :
    int x;
    int y;
};

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
    hi(1);
    return x * x;
}

}
