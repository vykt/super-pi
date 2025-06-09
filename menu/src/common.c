//C standard library
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

//local headers
#include "common.h"


// -- [globals] --

// global error state
struct subsys_state subsys_state;


// -- [text] --

//send an pretty-printed error to `stderr`
void report_error(const char * fmt, ...) {
    
    va_list args;
    va_start(args, fmt);

    //print error prefix & message
    fprintf(stderr, "%sERROR%s: ", RED, RESET);
    vfprintf(stderr, fmt, args);

    va_end(args);

    return;
}


//initialise global subsystem status struct 
void init_subsys_state() {

    subsys_state.udev_good       = true;
    subsys_state.evdev_good      = true;
    subsys_state.controller_good = true;
    subsys_state.rom_good        = true;
}
