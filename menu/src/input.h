#ifndef INPUT_H
#define INPUT_H

//C standard library
#include <stdbool.h>

//system headers
#include <linux/limits.h>

//local headers
#include "common.h"


// -- [macros] --

//lead joystick udev device path
#define JS0_DEVFS_PATH "/dev/input/js0"
#define JS1_DEVFS_PATH "/dev/input/js1"
#define JS2_DEVFS_PATH "/dev/input/js2"
#define JS3_DEVFS_PATH "/dev/input/js3"

//keys - other
#define KEY_DESC_LEN 32

//keys - required
#define KEY_REQ_NUM 10
#define MENU_KEY_SOUTH 0
#define MENU_KEY_EAST 1
#define MENU_KEY_NORTH 2
#define MENU_KEY_WEST 3
#define MENU_KEY_TL 4
#define MENU_KEY_TR 5
#define MENU_KEY_SELECT 6
#define MENU_KEY_START 7
#define MENU_KEY_DPAD_X 8
#define MENU_KEY_DPAD_Y 9

//keys - optional
#define KEY_OPT_NUM 12
#define MENU_KEY_ABS_X 10
#define MENU_KEY_ABS_Y 11


// -- [data] --

//macro-to-linux button mapping
struct js_key {

    int keycode;
    char desc[KEY_DESC_LEN];

    bool is_present;
};


//joystick global state
struct js_single_state {

    char evdev_path[PATH_MAX];
    char vendor[PATH_MAX];
    char model[PATH_MAX];

    bool is_present;
    bool is_open;

    int evdev_fd;
    struct libevdev * evdev;
    struct js_key keys[KEY_OPT_NUM];
};


//joystick-meta global state
struct js_state {

    bool have_main_js;
    bool input_failed;

    int main_js_idx;    
    struct js_single_state js[4];
};


// -- [globals] --

//global joystick state
extern struct js_state js_state;


// -- [text] --

//initialise & release global udev context
void init_udev();
void fini_udev();

//initialise global joystick state
void init_js();

//populate joystick-meta global state
void update_js_state();

//receive the next input event from libevdev & dispatch an action
int next_input(struct input_event * in_event);


#endif
