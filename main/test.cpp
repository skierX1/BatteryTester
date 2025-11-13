

#include <stdio.h>
#include <string>
#include <dirent.h>
#include <sstream>
#include <iomanip>
#include <string>

#include "test.h"
#include "defines.h"
#include "files_rw.h"
#include "voltage.h"
extern "C" {
    #include "esp_timer.h"
}

class Test* test_;

extern VoltageReader* voltage_reader_;;

void Test::run() {

    bool start_test_now = false;

    if (test_running) {
        test_start = false;    
    }
    
    if (!test_running && test_start) {
        test_start = false;
        test_running = true;        
        start_test_now = true;
    }

    if (test_stop) {
        test_stop = false;
        test_start = false;
        test_running = false;                
    }
     
    if (!test_running) {
        test_time_sec_ = test_max_time_sec;          
        return;
    }

    // .......... test cycle running .............
    
    std::string file_name("/spiffs/data/");
    file_name = file_name + test_->test_name+".csv";

    if (start_test_now) {
                
        // stop test if filename exists at start
        if (file_name.length()<18 || Spiffs::file_exists(file_name)) {
            test_stop = true; 
            return;
        }
        
        // create a new csv file with the name of test received from the web
        FILE *f = fopen(file_name.c_str(), "w");
        if(f==NULL) {
            test_stop = true;
            return;  // file cannot be created
        }

        //add header to csv file
        fprintf(f, "%s", "Time_S,Battery_V,Cell1_V,Resistance_Ohm\n");
        fclose(f);

        // remember the start test time
        test_start_time_ = esp_timer_get_time();  // microseconds since boot
    }

    int64_t now = esp_timer_get_time();  // microseconds since boot
    test_time_sec_ = now - test_start_time_;

    // stop when timer exceeds
    if (test_time_sec_ > test_max_time_sec) {
        test_stop = true;
        test_time_sec_ = test_max_time_sec;
        Spiffs::ListCSVFiles();
        return;
    }


    // open the csv file in append mode
    FILE *f = fopen(file_name.c_str(), "a");
    if(f==NULL) {
        test_stop = true;  // file cannot be opened
        return;
    }

    //append values to csv file
    double Vbatt = voltage_reader_->voltage[0].voltage;
    double Vcell1 = voltage_reader_->voltage[1].voltage;
    double R = 2.0;

    std::ostringstream time_str_stream;
    time_str_stream << std::fixed << std::setprecision(1) << test_time_sec_ / 1000000;

    fprintf(f, "%s,%.3f,%.3f,%.2f\n",time_str_stream.str().c_str(),Vbatt,Vcell1,R);

    fclose(f);



    return;
}
