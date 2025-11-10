#ifndef __TEST_H
#define __TEST_H

#include <stdio.h>
#include <string>

class Test {
public:
    Test () {};
    ~Test () {};

    bool test_start = false;
    bool test_stop = false;
    int countdown = 0;

    bool test_running = false;

    void run();

    std::string test_name;

    
};

#endif //__TEST_H