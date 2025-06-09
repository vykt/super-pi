//C standard library
#include <stdio.h>
#include <time.h>

//system headers
#include <unistd.h>

//local headers
#include "common.h"
#include "input.h"


int main() {

    time_t prev, now;


    //initialise core data
    init_subsys_state();
    init_udev();
    init_js();

    //main loop
    prev = 0;
    do {

        //periodically update devices
        now = time(NULL);
        if ((now - prev) > 1) {

            prev = now;
            update_js_state();

            #if 0
            //dump meta
            printf("\nhave js: %s | ",
                   js_state.have_main_js ? "true" : "false");
            printf("main js: %d | controller_good: %s\n",
                   js_state.main_js_idx,
                   subsys_state.controller_good ? "true" : "false");

            //dump keys
            for (int i = 0; i < KEY_OPT_NUM; ++i) {
                printf("key %d : %s\n",
                i,
                js_state.js[js_state.main_js_idx].keys[i].is_present
                ? "true" : "false");
            }


            //dump controllers
            for (int i = 0; i < 4; ++i) {
                printf("js %d: present: %s, event: %s\n",
                       i,
                       js_state.js[i].is_present ? "true" : "false",
                       js_state.js[i].is_present
                       ? js_state.js[i].evdev_path : "<none>");
            }
            #endif

        } //end periodically update devices

        if (js_state.input_failed == false) process_input();
        
    } while (usleep(10000) || true); //always true
}
