#include "test.h"

class Test* test_;

void Test::run() {
    if (test_running) {
        test_start = false;    
    }
    
    if (!test_running && test_start) {
        test_start = false;
        test_running = true;
        countdown = 20;
    }

    if (test_stop) {
        test_stop = false;
        test_running = false;
        countdown = 0;
    }
     
    if (!test_running)
        return;

    
    countdown--;

    if (countdown == 0)
        test_stop = true;

    return;
}
