#ifndef __TEST_H
#define __TEST_H

#include <stdio.h>
#include <string>

class Test {
public:
    Test () {};
    ~Test () {};

    bool test_start {false};
    bool test_stop {false};    

    bool test_running {false};

    void run();

    std::string test_name;

public://                            30 sec
     int64_t test_max_time_sec {30000000};
     int64_t test_time_sec_{0};
     int64_t test_start_time_;

    
};

#endif //__TEST_H