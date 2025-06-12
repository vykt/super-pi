//C standard library
#include <stdbool.h>
#include <string.h>

//system headers
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

//kernel headers
#include <linux/input.h>
#include <linux/input-event-codes.h>

//external libraries
#include <libudev.h>
#include <libevdev-1.0/libevdev/libevdev.h>

//local headers
#include "common.h"
#include "input.h"

//DEBUG
#include <stdio.h>


// -- [globals] --

//global joystick state
struct js_state js_state;

//global joystick device pathnames
const char * _js_path[4] = {
    JS0_DEVFS_PATH,
    JS1_DEVFS_PATH,
    JS2_DEVFS_PATH,
    JS3_DEVFS_PATH
};

//global udev context
struct udev * _udev_ctx;


// -- [text] --

//initialise global udev context
void init_udev() {

    _udev_ctx = udev_new();
    if (_udev_ctx == NULL) FATAL_FAIL("Failed to create a udev context.");
    return;
}


//release global udev context
void fini_udev() {

    udev_unref(_udev_ctx);
    return;
}


//initialise global joystick state
void init_js() {

    js_state.have_main_js = true;
    js_state.main_js_idx  = 0;
    return;
}


/*
 *  NOTE: Calls to libudev often allocate resources. Almost every libudev
 *        call can fail. To exhaustively handle all fail cases without 
 *        code duplication or spaghetti, labels are defined for each
 *        cleanup state. The following macros jump to the appropriate
 *        cleanup label in case a libudev call fails. Code readability
 *        is maximised by keeping error handling to one line per call.
 */

//joystick:event device pair enumeration - error handling macros
#define _IF_NULL_ERR_JS_NO_CLEANUP(ptr) \
    if (ptr == NULL) { \
        subsys_state.udev_good = false; \
        goto _no_cleanup; \
    }

#define _IF_NULL_ERR_JS_ENUM(ptr) \
    if (ptr == NULL) { \
        subsys_state.udev_good = false; \
        goto _js_enum_cleanup; \
    }
    
#define _IF_NEG_ERR_JS_ENUM(val) \
    if (val < 0) { \
        subsys_state.udev_good = false; \
        goto _js_enum_cleanup; \
    }

#define _IF_NULL_ERR_JS_DEVICE(ptr) \
    if (ptr == NULL) { \
        subsys_state.udev_good = false; \
        goto _js_device_cleanup; \
    }
    
#define _IF_NEG_ERR_JS_DEVICE(val) \
    if (val < 0) { \
        subsys_state.udev_good = false; \
        goto _js_device_cleanup; \
    }

#define _IF_NULL_ERR_PARENT_ENUM(ptr) \
    if (ptr == NULL) { \
        subsys_state.udev_good = false; \
        goto _parent_enum_cleanup; \
    }
    
#define _IF_NEG_ERR_PARENT_ENUM(val) \
    if (val < 0) { \
        subsys_state.udev_good = false; \
        goto _parent_enum_cleanup; \
    }

#define _IF_NULL_ERR_EVENT_DEVICE(ptr) \
    if (ptr == NULL) { \
        subsys_state.udev_good = false; \
        goto _event_cleanup; \
    }
    
#define _IF_NEG_ERR_EVENT_DEVICE(val) \
    if (val < 0) { \
        subsys_state.udev_good = false; \
        goto _event_cleanup; \
    }

//joystick:event device pair enumeration - helper macros
#define _COPY_STR_JS(str, field) \
    { len = strnlen(str, PATH_MAX - 1); \
      strncpy(js_state.js[i].field, str, len + 1); \
      js_state.js[i].field[len] = '\0'; }


//joystick:event device pair enumeration - function
static void _update_js_devices() {

    int ret;
    size_t len;
    bool do_break;

    const char * str;
    
    const char * devfs_path, * js_devfs_path;
    const char * sysfs_path, * parent_sysfs_path;

    struct udev_device * js_device, * parent_device, * event_device;
    struct udev_enumerate * js_enumerate, * parent_enumerate;
    
    struct udev_list_entry * js_devices, * js_device_entry;
    struct udev_list_entry * parent_devices, * parent_device_entry;


    //cleanup joystick states
    for (int i = 0; i < 4; ++i) { js_state.js[i].is_present = false; }

    //perform a joystick device scan
    js_enumerate = udev_enumerate_new(_udev_ctx);
    _IF_NULL_ERR_JS_NO_CLEANUP(js_enumerate)

    ret = udev_enumerate_add_match_subsystem(js_enumerate, "input");
    _IF_NEG_ERR_JS_ENUM(ret)

    ret = udev_enumerate_scan_devices(js_enumerate);
    _IF_NEG_ERR_JS_ENUM(ret)

    js_devices = udev_enumerate_get_list_entry(js_enumerate);
    _IF_NULL_ERR_JS_ENUM(js_devices)


    //iterate over discovered devices 
    udev_list_entry_foreach(js_device_entry, js_devices) {

        //find the devfs path of this device
        sysfs_path = udev_list_entry_get_name(js_device_entry);
        _IF_NULL_ERR_JS_ENUM(js_devices)

        js_device = udev_device_new_from_syspath(_udev_ctx, sysfs_path);
        _IF_NULL_ERR_JS_ENUM(js_devices)

        js_devfs_path = udev_device_get_devnode(js_device);
        if (js_devfs_path == NULL) //not an error
            goto _js_device_cleanup;

        //if this is one of the four joysticks
        for (int i = 0; i < 4; ++i) {
            if (strncmp(js_devfs_path, _js_path[i],
                        strnlen(js_devfs_path, PATH_MAX)) != 0) continue;

            //locate the corresponding event
            parent_device = udev_device_get_parent_with_subsystem_devtype(
                               js_device, "input", NULL);
            _IF_NULL_ERR_JS_DEVICE(parent_device)

            parent_sysfs_path = udev_device_get_syspath(parent_device);
            _IF_NULL_ERR_JS_DEVICE(parent_sysfs_path)

            parent_enumerate = udev_enumerate_new(_udev_ctx);
            _IF_NULL_ERR_JS_DEVICE(parent_enumerate)

            ret = udev_enumerate_add_match_subsystem(
                      parent_enumerate, "input");
            _IF_NEG_ERR_PARENT_ENUM(ret)

            ret = udev_enumerate_add_match_property(
                parent_enumerate, "ID_INPUT", "1");
            _IF_NEG_ERR_PARENT_ENUM(ret)

            ret = udev_enumerate_scan_devices(parent_enumerate);
            _IF_NEG_ERR_PARENT_ENUM(ret)

            parent_devices = udev_enumerate_get_list_entry(
                                  parent_enumerate);
            _IF_NULL_ERR_PARENT_ENUM(parent_devices)

            do_break = false;
            udev_list_entry_foreach(parent_device_entry, parent_devices) {

                sysfs_path
                    = udev_list_entry_get_name(parent_device_entry);
                _IF_NULL_ERR_PARENT_ENUM(sysfs_path)

                event_device = udev_device_new_from_syspath(
                                   _udev_ctx, sysfs_path);
                _IF_NULL_ERR_PARENT_ENUM(event_device)

                sysfs_path = udev_device_get_syspath(
                                 udev_device_get_parent(event_device));
                _IF_NULL_ERR_EVENT_DEVICE(sysfs_path)

                //if this event device shares a parent with the js device
                if (strncmp(sysfs_path, parent_sysfs_path, PATH_MAX) == 0) {

                    devfs_path = udev_device_get_devnode(event_device);
                    if (devfs_path == NULL) //not an error
                        goto _event_cleanup;

                    
                    if (strstr(devfs_path, "event") != NULL) {
                        _COPY_STR_JS(devfs_path, evdev_path)
                        do_break = true;
                    }
                }

                _event_cleanup:
                udev_device_unref(event_device);
                if (do_break == true) break;
                
            } //end for-each (parent)

            //save vendor
            str = udev_device_get_property_value(js_device, "ID_VENDOR");
            if (str != NULL) _COPY_STR_JS(str, vendor)
            else _COPY_STR_JS("Generic vendor", vendor);

            //save model
            str = udev_device_get_property_value(js_device, "ID_MODEL");
            if (str != NULL) _COPY_STR_JS(str, model)
            else _COPY_STR_JS("Generic controller", model);

            //mark joystick as present
            js_state.js[i].is_present = true;


            _parent_enum_cleanup:
            udev_enumerate_unref(parent_enumerate);

        } //end if four joysticks
        

        //cleanup for this iteration
        _js_device_cleanup:
        udev_device_unref(js_device);

    } //end for-each (joystick)

    _js_enum_cleanup:
    udev_enumerate_unref(js_enumerate);

    _no_cleanup:
    return;
}


//linux-to-macro button mapping for keys (+ description string)
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


//linux-to-macro button mapping for axes (+ description string)
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


//open a joystick event device & its associated libevdev context
static void _open_js(int idx) {

    int ret;
    const char * key_desc;


    //reset libevdev subsystem state
    subsys_state.evdev_good = true;

    //open the joystick event device for nonblocking reading
    js_state.js[idx].evdev_fd = open(js_state.js[idx].evdev_path,
                                       O_RDONLY | O_NONBLOCK);
    if (js_state.js[idx].evdev_fd < 0) {
        subsys_state.evdev_good = false;
        return;
    }

    //create a libevdev context
    ret = libevdev_new_from_fd(js_state.js[idx].evdev_fd,
                               &js_state.js[idx].evdev);
    if (ret != 0) {
        subsys_state.evdev_good = true;
        goto _prepare_js_cleanup_fd;
    }

    //populate available button mappings
    for (int i = 0; i < KEY_MAX; i++) {        

        if (libevdev_has_event_code(js_state.js[idx].evdev, EV_KEY, i)) {
            ret = _lookup_key_desc(i, &key_desc);

            if (ret >= 0) {
                strncpy(js_state.js[idx].keys[ret].desc,
                        key_desc, KEY_DESC_LEN);
                js_state.js[idx].keys[ret].keycode = ret;
                js_state.js[idx].keys[ret].is_present = true;
            }
        }
    }

    //populate available axis mappings
    for (int i = 0; i < ABS_MAX; i++) {        

        if (libevdev_has_event_code(js_state.js[idx].evdev, EV_ABS, i)) {
            ret = _lookup_axis_desc(i, &key_desc);
            
            if (ret >= 0) {
                strncpy(js_state.js[idx].keys[ret].desc,
                        key_desc, KEY_DESC_LEN);
                js_state.js[idx].keys[ret].keycode = ret;
                js_state.js[idx].keys[ret].is_present = true;
            }
        }
    }

    //check if all required buttons/axes are available
    subsys_state.controller_good = true;
    for (int i = 0; i < KEY_REQ_NUM; ++i) {
        if (js_state.js[idx].keys[i].is_present == false) {
            subsys_state.controller_good = false;
        }
    }
    js_state.js[idx].is_open = true;

    //normal return
    return;

    //error return
    _prepare_js_cleanup_fd:
    close(js_state.js[idx].evdev_fd);
    return;
}


//close a joystick event device & its associated libevdev context
static void _teardown_js(int idx) {

    //cleanup the libevdev context
    libevdev_free(js_state.js[idx].evdev);
    close(js_state.js[idx].evdev_fd);

    //cleanup the key mappings
    memset(js_state.js[idx].keys, 0, sizeof(struct js_key) * KEY_OPT_NUM);

    //mark this joystick as no longer open
    js_state.js[idx].is_open = false;

    return;
}


//populate joystick-meta global state
void update_js_state() {

    bool skip_idx[4] = {0};
    bool found_next;


    //reset error state
    subsys_state.udev_good = true;

    //update individual joystick states
    _update_js_devices();


    //if the main joystick is present and setup
    if (js_state.js[js_state.main_js_idx].is_present == true
        && js_state.js[js_state.main_js_idx].is_open == true)
            goto _update_js_state_success;

    //if the main joystick just became not present, tear it down
    if (js_state.js[js_state.main_js_idx].is_open == true
        && js_state.js[js_state.main_js_idx].is_present == false)
            _teardown_js(js_state.main_js_idx);

    //if the main joystick is not present
    if (js_state.js[js_state.main_js_idx].is_present == false) {

        //find a new main joystick
        found_next = false;
        for (int i = 0; i < 4; ++i) {
            if (js_state.js[i].is_present) {
                js_state.main_js_idx = i;
                found_next = true;
                break;
            }
        }

        //if no other joysticks are available, give up
        if (found_next == false) {
            js_state.main_js_idx = 0;
            goto _update_js_state_fail;
        }
    }

    //try to setup all joysticks starting with the current main
    for (int i = 0; i < 4; ++i) {

        //for the next unskipped joystick
        for (int j = 0; j < 4; ++j) {

            found_next = false;

            //try to setup twice before giving up
            for (int k = 0; k < 2; ++k) {
            
                _open_js(js_state.main_js_idx);
                if (js_state.js[js_state.main_js_idx].is_open) {
                    js_state.have_main_js = true;
                    break;
                }    
            } //end setup attempts

            //if setup succeeded, return
            if (js_state.have_main_js == true) {
                goto _update_js_state_success;

            //else, mark this joystick as skipped
            } else {
                skip_idx[js_state.main_js_idx] = true;
            } 

            //find the next main joystick index
            found_next = false;
            for (int k = 0; k < 4; ++k) {

                if (js_state.js[k].is_present == true
                    && skip_idx[k] == false) {

                    js_state.main_js_idx = k;
                    found_next = true;
                    break;
                }
            }

            //if no joysticks left, stop trying
            if (found_next == false) goto _update_js_state_fail;

        } //end next unskipped joystick

    } //end try to setup all joysticks

    _update_js_state_fail:
    js_state.have_main_js = false;
    return;

    _update_js_state_success:
    js_state.have_main_js = true;
    js_state.input_failed = false;
    return;
}


//receive the next input event from libevdev & dispatch an action
int next_input(struct input_event * in_event) {

    int ret;


    //receive the next input event
    ret = libevdev_next_event(
              js_state.js[js_state.main_js_idx].evdev,
              LIBEVDEV_READ_FLAG_NORMAL, in_event);
    if (ret == -EAGAIN) {
        return 0;
    } else if (ret != 0) {
        js_state.input_failed = true;
        return 0;
    } else { return 1; }
}
