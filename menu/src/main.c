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
#include "state.h"


// -- [data] --

//tracking whether a key is pressed
bool is_down[KEY_REQ_NUM] = {0};


// -- [text] --

//dispatch a received input
static void _dispatch_input(struct input_event * in_event) {
    
    //if the input is a key type
    if (in_event->type == EV_KEY) {

        switch(in_event->code) {

            case BTN_SOUTH:
                if (is_down[MENU_KEY_SOUTH] == false) {
                    handle_activate();
                    is_down[MENU_KEY_SOUTH] = true;
                } else {
                    is_down[MENU_KEY_SOUTH] = false;
                }
                break;

            case BTN_EAST:
                if (is_down[MENU_KEY_EAST] == false) {
                    handle_exit();
                    is_down[MENU_KEY_EAST] = true;
                } else {
                    is_down[MENU_KEY_EAST] = false;
                }
                break;

            case BTN_SELECT:
                if (is_down[MENU_KEY_SELECT] == false) {
                    is_down[MENU_KEY_SELECT] = true;
                    handle_activate();
                } else {
                    is_down[MENU_KEY_SELECT] = false;
                }
                break;

            case BTN_START:
                if (is_down[MENU_KEY_START] == false) {
                    is_down[MENU_KEY_START] = true;
                    handle_activate();
                } else {
                    is_down[MENU_KEY_START] = false;
                }
                break;

            default:
                break;

        } //end switch
        
    } else if (in_event->type == EV_ABS) {

        switch(in_event->code) {

            case ABS_HAT0Y:
                if (in_event->value < -0.25) {
                    handle_up();
                } else if (in_event->value > 0.25) {
                    handle_down();
                }
                break;

        } //end switch

    } //end event type
}


int main() {

    int ret;

    time_t prev, now;
    struct input_event in_event;
    

    //initialise core data
    init_subsys_state();
    init_udev();
    init_roms();
    init_js();
    init_menu_state();
    init_ncurses();

    //draw the original menu
    redraw();
    disp_refresh();

    //main loop
    prev = 0;
    do {

        //periodically update devices
        now = time(NULL);
        if ((now - prev) > 1) {

            prev = now;
            update_js_state();
            redraw();
            disp_refresh();

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


        //process the next input
        if (js_state.have_main_js == true
            && js_state.input_failed == false) {

            ret = next_input(&in_event);
            if (ret == 1) _dispatch_input(&in_event);
        }
        
    } while (usleep(10000) || true); //always true
}
