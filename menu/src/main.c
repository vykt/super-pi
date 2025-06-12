//C standard library
#include <stdio.h>
#include <time.h>

//system headers
#include <unistd.h>

//kernel headers
#include <linux/input.h>
#include <linux/input-event-codes.h>

//external libraries
#include <libevdev-1.0/libevdev/libevdev.h>
#include <cmore.h>

//local headers
#include "common.h"
#include "input.h"
#include "data.h"
#include "display.h"


static void _dispatch_input(struct input_event * in_event) {
    
    //if the input is a key type
    if (in_event->type == EV_KEY) {

        switch(in_event->code) {

            case BTN_SOUTH:
                break;

            case BTN_EAST:
                break;

            case BTN_NORTH:
                break;

            case BTN_WEST:
                break;

            case BTN_TL:
                break;

            case BTN_TR:
                break;

            case BTN_SELECT:
                break;

            case BTN_START:
                break;

            default:
                break;

        } //end switch
        
    } else if (in_event->type == EV_ABS) {

        switch(in_event->code) {

            case ABS_HAT0X:
                if (in_event->value < -0.25) {
                    printf("-X\n");
                } else if (in_event->value > 0.25) {
                    printf("+X\n");
                }
                break;

            case ABS_HAT0Y:
                if (in_event->value < -0.25) {
                    printf("-Y\n");
                } else if (in_event->value > 0.25) {
                    printf("+Y\n");
                }
                break;

        } //end switch

    } //end event type
}


int main() {

    time_t prev, now;


    //initialise core data
    init_subsys_state();
    init_udev();
    init_roms();
    init_js();


    init_ncurses();
    sleep(3);

    while (rom_node != NULL
           && (start == true || rom_node != rom_basenames.head)) {

        printf("rom: %s\n", (char *) rom_node->data);
        rom_node = rom_node->next;
        start = false;
    }
    
    sleep(100);

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
