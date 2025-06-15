#ifndef COMMON_H
#define COMMON_H

//C standard library
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>


// -- [macros] --

#define VERSION "v1.0.0"
#define USER "superpi"

//paths DEBUG
//#define PATH_ROMS "/superpi/rom"
#define PATH_ROMS "/home/vykt/projects/super-pi/menu/roms"

//colours
#define RESET   "\x1b[0m"
#define RED     "\x1b[31m"
#define GREEN   "\x1b[32m"
#define YELLOW  "\x1b[33m"
#define BLUE    "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN    "\x1b[36m"
#define WHITE   "\x1b[37m"

//tools
#define MIN(x, y) ((x < y) ? x : y)
#define FATAL_FAIL(msg) { report_error(msg); exit(-1); }


// -- [data] --

//status of subsystems
struct subsys_state {
    bool udev_good;
    bool rom_good;
    bool execve_good;
};


// -- [globals] --

// global error state
extern struct subsys_state subsys_state;


// -- [text] --

//send an pretty-printed error to `stderr`
void report_error(const char * fmt, ...);

//clamp an integer between some range
int int_clamp(int val, int min, int max);

//initialise global subsystem status struct 
void init_subsys_state();


#endif
