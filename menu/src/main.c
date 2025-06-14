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

//keep track of key presses & don't affect menu if a ROM is running
static void _handle_key(int key, void(* cb)()) {

    if (is_down[key] == false) { 
        if ((menu_state.rom_running == false) && (cb != NULL)) cb();
        is_down[key] = true;
    } else {
        is_down[key] = false;
    }

    return;
}


//shutdown emulator & graphical server
static void _exit_rom() {

    int ret;


    //update the ROM running state
    menu_state.rom_running = false;

    //call terminator script
    ret = system("exit_rom.sh > /dev/null 2>&1 < /dev/null");
    if (ret != -1) {
        menu_state.rom_running = false;
        redraw();
        disp_refresh();
    }

    return;
}


//dispatch a received input
static void _dispatch_input(struct input_event * in_event) {

    bool exit_rom = false;

    
    //if the input is a key type
    if (in_event->type == EV_KEY) {

        switch(in_event->code) {

            case BTN_SOUTH:
                _handle_key(MENU_KEY_SOUTH, handle_activate);
                break;

            case BTN_EAST:
                _handle_key(MENU_KEY_EAST, handle_exit);
                break;

            case BTN_SELECT:
                _handle_key(MENU_KEY_SELECT, handle_activate);
                break;

            case BTN_START:
                _handle_key(MENU_KEY_START, handle_activate);
                break;

            case BTN_TL:
                _handle_key(MENU_KEY_TL, NULL);
                break;

            case BTN_TR:
                _handle_key(MENU_KEY_TR, NULL);
                break;

            default:
                break;

        } //end switch
        
    } else if (in_event->type == EV_ABS) {

        switch(in_event->code) {

            case ABS_HAT0Y:
                if (in_event->value < -0.25) {
                    if (menu_state.rom_running == false) handle_up();
                } else if (in_event->value > 0.25) {
                    if (menu_state.rom_running == false) handle_down();
                }
                break;

        } //end switch

    } //end event type


    //check if an exit from a running ROM was requested
    if (menu_state.rom_running == true) {

        //START + SELECT + TriggerL + TriggerR
        exit_rom = is_down[MENU_KEY_SELECT]
                   && is_down[MENU_KEY_START]
                   && is_down[MENU_KEY_TL]
                   && is_down[MENU_KEY_TR];

        //perform the exit if requested
        if (exit_rom == true) _exit_rom();
    } 

    return;
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
        }

        //process the next input
        if (js_state.have_main_js == true
            && js_state.input_failed == false) {

            ret = next_input(&in_event);
            if (ret == 1) _dispatch_input(&in_event);
        }
        
    } while (usleep(10000) || true); //always true
}
