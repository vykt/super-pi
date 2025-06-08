//C standard library
#include <linux/input-event-codes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>

//system headers
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

//kernel headers
#include <linux/limits.h>
#include <linux/input.h>

//external libraries - devices
#include <libudev.h>
#include <libevdev-1.0/libevdev/libevdev.h>

//external libraries - ui
#include <ncurses.h>

//external libraries - miscellaneous
#include <cmore.h>


// --- [MACROS]

//lead joystick udev device path
#define JS0_DEVFS_PATH "/dev/input/js0"
#define JS1_DEVFS_PATH "/dev/input/js1"
#define JS2_DEVFS_PATH "/dev/input/js2"
#define JS3_DEVFS_PATH "/dev/input/js3"

//colours
#define RESET   "\x1b[0m"
#define RED     "\x1b[31m"
#define GREEN   "\x1b[32m"
#define YELLOW  "\x1b[33m"
#define BLUE    "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN    "\x1b[36m"
#define WHITE   "\x1b[37m"

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


// --- [DATA]

struct js_key {

    int keycode;
    char desc[KEY_DESC_LEN];

    bool is_present;
};


//individual joystick data
struct js_single_state {

    char evdev_path[PATH_MAX];
    char vendor[PATH_MAX];
    char model[PATH_MAX];

    bool is_present;
    bool is_setup;

    int evdev_fd;
    struct libevdev * evdev;

    struct js_key keys[KEY_OPT_NUM];
};

//joystick device meta-info
struct js_state {

    bool have_js;
    int main_js_idx;
    struct js_single_state js[4];
};

struct error_state {

    bool udev_good;
    bool evdev_good;
    bool controller_good;
};

//static data
static struct js_state ctrl_state;
static struct error_state err_state;
static struct udev * udev;

static const char * js_path[4] = {
    JS0_DEVFS_PATH,
    JS1_DEVFS_PATH,
    JS2_DEVFS_PATH,
    JS3_DEVFS_PATH
};


// --- [TEXT]

// -- code readability macros

//miscellaneous
#define CLAMP(max, sz) ((max < sz) ? max : sz)

//udev specific
#define COPY_STR_TO_JS(str, field) \
    { \
        len = strnlen(str, PATH_MAX - 1); \
        strncpy(ctrl_state.js[i].field, str, len + 1); \
        ctrl_state.js[i].field[len] = '\0'; \
    }
    
//error handling
#define FATAL_FAIL(msg) { _report_error(msg); exit(-1); }


// -- helper functions

//report a pretty error to STDERR
static void _report_error(const char * fmt, ...) {

    va_list args;
    va_start(args, fmt);

    //print error prefix & message
    fprintf(stderr, "%sERROR%s: ", RED, RESET);
    vfprintf(stderr, fmt, args);

    va_end(args);

    return;
}


#if 0
//malloc & exit if failed
static void * _must_alloc(size_t sz) {
    
    void * ptr = malloc(sz);
    if (ptr == NULL) FATAL_FAIL(err_mem);

    return ptr;
}
#endif


// -- main .text

//initialise error state
static void _init_err_state() {

    err_state.udev_good = true;
    err_state.evdev_good = true;
    err_state.controller_good = true;
    return;
}


//connect to udev
static void _init_udev() {

    //create a reference to a main udev object
    udev = udev_new();
    if (udev == NULL) FATAL_FAIL("Failed to connect to udev.");

    return;
}


//disconnect from udev
static void _fini_udev() {

    udev_unref(udev);
    return; 
}


//initialise static joystick
static void _init_joystick() {

    ctrl_state.have_js     = false;
    ctrl_state.main_js_idx = 0;
    
    return;
}


// -- js scan error handling macros
#define IF_NULL_ERR_JS_NO_CLEANUP(ptr) \
    if (ptr == NULL) { \
        err_state.udev_good = false; \
        goto _update_js_single_states_no_cleanup; \
    }

#define IF_NULL_ERR_JS_ENUM_SCAN(ptr) \
    if (ptr == NULL) { \
        err_state.udev_good = false; \
        goto _update_js_single_states_foreach_js_enum_cleanup; \
    }
    
#define IF_NEG_ERR_JS_ENUM_SCAN(val) \
    if (val < 0) { \
        err_state.udev_good = false; \
        goto _update_js_single_states_foreach_js_enum_cleanup; \
    }

#define IF_NULL_ERR_JS_DEVICE_SCAN(ptr) \
    if (ptr == NULL) { \
        err_state.udev_good = false; \
        goto _update_js_single_states_foreach_js_device_cleanup; \
    }
    
#define IF_NEG_ERR_JS_DEVICE_SCAN(val) \
    if (val < 0) { \
        err_state.udev_good = false; \
        goto _update_js_single_states_foreach_js_device_cleanup; \
    }

#define IF_NULL_ERR_PARENT_ENUM_SCAN(ptr) \
    if (ptr == NULL) { \
        err_state.udev_good = false; \
        goto _update_js_single_states_foreach_parent_enum_cleanup; \
    }
    
#define IF_NEG_ERR_PARENT_ENUM_SCAN(val) \
    if (val < 0) { \
        err_state.udev_good = false; \
        goto _update_js_single_states_foreach_parent_enum_cleanup; \
    }

#define IF_NULL_ERR_EVENT_DEVICE(ptr) \
    if (ptr == NULL) { \
        err_state.udev_good = false; \
        goto _update_js_single_states_foreach_event_cleanup; \
    }
    
#define IF_NEG_ERR_EVENT_DEVICE(val) \
    if (val < 0) { \
        err_state.udev_good = false; \
        goto _update_js_single_states_foreach_event_cleanup; \
    }

/*
 *  NOTE: This function calls into libudev at ~20 different places, with
 *        many allocating resources, and all calls possibly failing. The
 *        best approach for gracefully failing without code bloat is
 *        either completing an iteration while treating zero'ed data as
 *        erroneous, or excessively using goto statements. The latter
 *        is more straight forward.
 *
 *        Keeping this code in a single function allows the goto logic
 *        to be substantially simpler.
 */

//use udev to scan sysfs & populate available joystick devices
static void _update_js_devices() {

    int ret;
    bool do_break;
    size_t len;

    const char * str;
    const char  * js_devfs_path, * devfs_path, * sysfs_path,
                * parent_sysfs_path;

    struct udev_device * js_device, * parent_device, * event_device;
    struct udev_enumerate * js_enumerate, * parent_enumerate;
    struct udev_list_entry * js_devices, * js_device_entry,
                           * parent_devices, * parent_device_entry;


    //cleanup joystick states
    for (int i = 0; i < 4; ++i) {    
        ctrl_state.js[i].is_present = false;
    }

    //perform a joystick device scan
    js_enumerate = udev_enumerate_new(udev);
    IF_NULL_ERR_JS_NO_CLEANUP(js_enumerate)

    ret = udev_enumerate_add_match_subsystem(js_enumerate, "input");
    IF_NEG_ERR_JS_ENUM_SCAN(ret)

    ret = udev_enumerate_scan_devices(js_enumerate);
    IF_NEG_ERR_JS_ENUM_SCAN(ret)

    js_devices = udev_enumerate_get_list_entry(js_enumerate);
    IF_NULL_ERR_JS_ENUM_SCAN(js_devices)


    //iterate over discovered devices 
    udev_list_entry_foreach(js_device_entry, js_devices) {

        //find the devfs path of this device
        sysfs_path = udev_list_entry_get_name(js_device_entry);
        IF_NULL_ERR_JS_ENUM_SCAN(js_devices)

        js_device = udev_device_new_from_syspath(udev, sysfs_path);
        IF_NULL_ERR_JS_ENUM_SCAN(js_devices)

        js_devfs_path = udev_device_get_devnode(js_device);
        if (js_devfs_path == NULL) //not an error
            goto _update_js_single_states_foreach_js_device_cleanup;

        //if this is one of the four joysticks
        for (int i = 0; i < 4; ++i) {
            if (strncmp(js_devfs_path, js_path[i],
                        strnlen(js_devfs_path, PATH_MAX)) != 0) continue;

            //locate the corresponding event
            parent_device = udev_device_get_parent_with_subsystem_devtype(
                               js_device, "input", NULL);
            IF_NULL_ERR_JS_DEVICE_SCAN(parent_device)

            parent_sysfs_path = udev_device_get_syspath(parent_device);
            IF_NULL_ERR_JS_DEVICE_SCAN(parent_sysfs_path)

            parent_enumerate = udev_enumerate_new(udev);
            IF_NULL_ERR_JS_DEVICE_SCAN(parent_enumerate)

            ret = udev_enumerate_add_match_subsystem(
                      parent_enumerate, "input");
            IF_NEG_ERR_PARENT_ENUM_SCAN(ret)

            ret = udev_enumerate_add_match_property(
                parent_enumerate, "ID_INPUT", "1");
            IF_NEG_ERR_PARENT_ENUM_SCAN(ret)

            ret = udev_enumerate_scan_devices(parent_enumerate);
            IF_NEG_ERR_PARENT_ENUM_SCAN(ret)

            parent_devices = udev_enumerate_get_list_entry(
                                  parent_enumerate);
            IF_NULL_ERR_PARENT_ENUM_SCAN(parent_devices)

            do_break = false;
            udev_list_entry_foreach(parent_device_entry, parent_devices) {

                sysfs_path
                    = udev_list_entry_get_name(parent_device_entry);
                IF_NULL_ERR_PARENT_ENUM_SCAN(sysfs_path)

                event_device = udev_device_new_from_syspath(
                                   udev, sysfs_path);
                IF_NULL_ERR_PARENT_ENUM_SCAN(event_device)

                sysfs_path = udev_device_get_syspath(
                                 udev_device_get_parent(event_device));
                IF_NULL_ERR_EVENT_DEVICE(sysfs_path)

                //if this event device shares a parent with the js device
                if (strncmp(sysfs_path, parent_sysfs_path, PATH_MAX) == 0) {

                    devfs_path = udev_device_get_devnode(event_device);
                    if (devfs_path == NULL) //not an error
                        goto _update_js_single_states_foreach_event_cleanup;

                    
                    if (strstr(devfs_path, "event") != NULL) {
                        COPY_STR_TO_JS(devfs_path, evdev_path)
                        do_break = true;
                    }
                }

                _update_js_single_states_foreach_event_cleanup:
                udev_device_unref(event_device);
                if (do_break == true) break;
                
            } //end for-each (parent)

            //save vendor
            str = udev_device_get_property_value(js_device, "ID_VENDOR");
            if (str != NULL) COPY_STR_TO_JS(str, vendor)
            else COPY_STR_TO_JS("Generic vendor", vendor);

            //save model
            str = udev_device_get_property_value(js_device, "ID_MODEL");
            if (str != NULL) COPY_STR_TO_JS(str, model)
            else COPY_STR_TO_JS("Generic controller", model);

            //mark joystick as present
            ctrl_state.js[i].is_present = true;


            _update_js_single_states_foreach_parent_enum_cleanup:
            udev_enumerate_unref(parent_enumerate);

        } //end if four joysticks
        

        //cleanup for this iteration
        _update_js_single_states_foreach_js_device_cleanup:
        udev_device_unref(js_device);

    } //end for-each (joystick)

    _update_js_single_states_foreach_js_enum_cleanup:
    udev_enumerate_unref(js_enumerate);

    _update_js_single_states_no_cleanup:
    return;
}


static inline int _lookup_key_desc(int keycode, const char ** desc) {

    switch (keycode) {

        case BTN_SOUTH:
            *desc = "Bottom button";
            return MENU_KEY_SOUTH;

        case BTN_EAST:
            *desc = "Right button";
            return MENU_KEY_EAST;

        case BTN_NORTH:
            *desc = "Top button";
            return MENU_KEY_NORTH;

        case BTN_WEST:
            *desc = "Left button";
            return MENU_KEY_WEST;

        case BTN_TL:
            *desc = "Left bumper";
            return MENU_KEY_TL;

        case BTN_TR:
            *desc = "Right bumper";
            return MENU_KEY_TR;

        case BTN_SELECT:
            *desc = "Select";
            return MENU_KEY_SELECT;

        case BTN_START:
            *desc = "Start";
            return MENU_KEY_START;

        default:
            *desc = NULL;
            return -1;

    } //end switch
}


static inline int _lookup_axis_desc(int keycode, const char ** desc) {

    switch (keycode) {

        case ABS_X:
            *desc = "Analog stick X-axis";
            return MENU_KEY_ABS_X;

        case ABS_Y:
            *desc = "Analog stick Y-axis";
            return MENU_KEY_ABS_Y;

        case ABS_HAT0X:
            *desc = "D-pad X-axis";
            return MENU_KEY_DPAD_X;

        case ABS_HAT0Y:
            *desc = "D-pad Y-axis";
            return MENU_KEY_DPAD_Y;

        default:
            *desc = NULL;
            return -1;

    } //end switch
}


static void _setup_js(int idx) {

    int ret;
    const char * key_desc;


    //reset error state
    err_state.evdev_good = true;

    //open the event device for this joystick
    ctrl_state.js[idx].evdev_fd = open(ctrl_state.js[idx].evdev_path,
                                       O_RDONLY | O_NONBLOCK);
    if (ctrl_state.js[idx].evdev_fd < 0) {
        err_state.evdev_good = false;
        return;
    }

    //create a libevdev struct
    ret = libevdev_new_from_fd(ctrl_state.js[idx].evdev_fd,
                               &ctrl_state.js[idx].evdev);
    if (ret != 0) {
        err_state.evdev_good = true;
        goto _prepare_js_cleanup_fd;
    }

    //populate keys
    for (int i = 0; i < KEY_MAX; i++) {        

        if (libevdev_has_event_code(ctrl_state.js[idx].evdev, EV_KEY, i)) {
            ret = _lookup_key_desc(i, &key_desc);
            if (ret >= 0) {
                strncpy(ctrl_state.js[idx].keys[ret].desc,
                        key_desc, KEY_DESC_LEN);
                ctrl_state.js[idx].keys[ret].keycode = ret;
                ctrl_state.js[idx].keys[ret].is_present = true;
            }
        }
    }

    //populate axes
    for (int i = 0; i < ABS_MAX; i++) {        

        if (libevdev_has_event_code(ctrl_state.js[idx].evdev, EV_ABS, i)) {
            ret = _lookup_axis_desc(i, &key_desc);
            if (ret >= 0) {
                strncpy(ctrl_state.js[idx].keys[ret].desc,
                        key_desc, KEY_DESC_LEN);
                ctrl_state.js[idx].keys[ret].keycode = ret;
                ctrl_state.js[idx].keys[ret].is_present = true;
            }
        }
    }

    //verify that required keys are available
    err_state.controller_good = true;
    for (int i = 0; i < KEY_REQ_NUM; ++i) {
        if (ctrl_state.js[idx].keys[i].is_present == false) {
            err_state.controller_good = false;
        }
    }
    ctrl_state.js[idx].is_setup = true;

    //normal return
    return;


    _prepare_js_cleanup_evdev:
    libevdev_free(ctrl_state.js[idx].evdev);

    _prepare_js_cleanup_fd:
    close(ctrl_state.js[idx].evdev_fd);

    //error return
    return;
}


static void _teardown_js(int idx) {

    //cleanup evdev
    libevdev_free(ctrl_state.js[idx].evdev);
    close(ctrl_state.js[idx].evdev_fd);

    //cleanup keymap
    memset(ctrl_state.js[idx].keys, 0, sizeof(struct js_key) * KEY_OPT_NUM);

    //mark to not destruct again
    ctrl_state.js[idx].is_setup = false;

    return;
}


//populate joystick device meta-info
static void _update_js_state() {

    bool skip_idx[4] = {0};
    bool found_next;


    //reset error state
    err_state.udev_good = true;

    //update individual joystick states
    _update_js_devices();


    //if the main joystick is present and setup
    if (ctrl_state.js[ctrl_state.main_js_idx].is_present == true
        && ctrl_state.js[ctrl_state.main_js_idx].is_setup == true) return;

    //if the main joystick just became not present, tear it down
    if (ctrl_state.js[ctrl_state.main_js_idx].is_setup == true
        && ctrl_state.js[ctrl_state.main_js_idx].is_present == false) {

        _teardown_js(ctrl_state.main_js_idx);
    }

    //if the main joystick is not present
    if (ctrl_state.js[ctrl_state.main_js_idx].is_present == false) {

        //find a new main joystick
        found_next = false;
        for (int i = 0; i < 4; ++i) {
            if (ctrl_state.js[i].is_present) {
                ctrl_state.main_js_idx = i;
                found_next = true;
                break;
            }
        }

        //if no other joysticks are available, give up
        if (found_next == false) {
            ctrl_state.main_js_idx = 0;
            return;
        }
    }


    //try to setup all joysticks starting with the current main
    for (int i = 0; i < 4; ++i) {

        //for the next unskipped joystick
        for (int j = 0; j < 4; ++j) {

            found_next = false;

            //try to setup twice before giving up
            for (int k = 0; k < 2; ++k) {
            
                _setup_js(ctrl_state.main_js_idx);
                if (ctrl_state.js[ctrl_state.main_js_idx].is_setup) {
                    ctrl_state.have_js = true;
                    break;
                }    
            } //end setup attempts

            //if setup succeeded, return
            if (ctrl_state.have_js == true) {
                return;

            //else, mark this joystick as skipped
            } else {
                skip_idx[ctrl_state.main_js_idx] = true;
            } 

            //find the next main joystick index
            found_next = false;
            for (int k = 0; k < 4; ++k) {

                if (ctrl_state.js[k].is_present == true
                    && skip_idx[k] == false) {

                    ctrl_state.main_js_idx = k;
                    found_next = true;
                    break;
                }
            }

            //if no joysticks left, stop trying
            if (found_next == false) return;

        } //end next unskipped joystick

    } //end try to setup all joysticks

    return;
}


static int _process_input() {

    int ret;
    struct input_event in_event;


    ret = libevdev_next_event(
              ctrl_state.js[ctrl_state.main_js_idx].evdev,
              LIBEVDEV_READ_FLAG_NORMAL, &in_event);

    //return no input cases
    if (ret == -EAGAIN) {
        return 0;
    } else if (ret != 0) {
        return -2;
    }

    //if the input is a key type
    if (in_event.type == EV_KEY) {

        switch(in_event.code) {

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
        
    } else if (in_event.type == EV_ABS) {

        switch(in_event.code) {

            case ABS_HAT0X:
                if (in_event.value < -0.25) {
                    
                } else if (in_event.value > 0.25) {
                    
                }
                break;

            case ABS_HAT0Y:
                if (in_event.value < -0.25) {
                    
                } else if (in_event.value > 0.25) {
                    
                }
                break;

            
        } //end switch

    } //end event type
}


int main() {

    //get a udev connection
    _init_err_state();
    _init_udev();
    _init_joystick();

    //main loop
    do {

        _update_js_state();

        //dump meta
        printf("\nhave js: %s | ", ctrl_state.have_js ? "true" : "false");
        printf("main js: %d | controller_good: %s\n",
               ctrl_state.main_js_idx,
               err_state.controller_good ? "true" : "false");

        //dump keys
        for (int i = 0; i < KEY_OPT_NUM; ++i) {
            printf("key %d : %s\n",
            i,
            ctrl_state.js[ctrl_state.main_js_idx].keys[i].is_present
            ? "true" : "false");
        }


        //dump controllers
        for (int i = 0; i < 4; ++i) {
            printf("js %d: present: %s, event: %s\n",
                   i,
                   ctrl_state.js[i].is_present ? "true" : "false",
                   ctrl_state.js[i].is_present
                   ? ctrl_state.js[i].evdev_path : "<none>");
        }

    } while (sleep(1) || true);

    _fini_udev();
}
