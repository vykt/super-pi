//C standard library
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>

//system headers
#include <unistd.h>
#include <limits.h>

//external libraries - devices
#include <libudev.h>
#include <libevdev-1.0/libevdev/libevdev.h>

//external libraries - ui
#include <ncurses.h>


// --- [MACROS]

//lead joystick udev device path
#define JS_DEV_PATH "/dev/input/js0"

//colours
#define RESET   "\x1b[0m"
#define RED     "\x1b[31m"
#define GREEN   "\x1b[32m"
#define YELLOW  "\x1b[33m"
#define BLUE    "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN    "\x1b[36m"
#define WHITE   "\x1b[37m"


// --- [DATA]

//joystick device meta-info
struct js_info {

    bool is_present;
    const char * vendor;
    const char * model;
};

//static data
static struct js_info info;
static struct udev * udev;


// --- [TEXT]

#define FATAL_FAIL(msg) { _report_error(msg); exit(-1); }

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


//malloc & exit if failed
static void * _must_alloc(size_t sz) {
    
    void * ptr = malloc(sz);
    if (ptr == NULL) FATAL_FAIL("Failed to allocate memory.");

    return ptr;
}


//connect to udev
static void _init_udev() {

    //create a reference to a main udev object
    udev = udev_new();
    if (udev == NULL) FATAL_FAIL(
        "[ubus] Failed to connect to ubus.");

    return;
}


//disconnect from udev
static void _fini_udev() {

    udev_unref(udev);
    return; 
}


//initialise static joystick
static void _init_joystick() { info.is_present = false; }


//scan sysfs input devices to locate the `js_dev_path` device
static struct udev_device *
    _find_joystick_device(const char * js_dev_path) {

    int ret;

    const char * sysfs_path, * devfs_path;

    struct udev_device * device;
    struct udev_enumerate * enumerate;
    struct udev_list_entry * devices, * device_entry;


    //setup a device scan
    enumerate = udev_enumerate_new(udev);
    if (enumerate == NULL) FATAL_FAIL(
        "[ubus] Failed to initialise a device enumerator.");

    ret = udev_enumerate_add_match_subsystem(enumerate, "input");
    if (ret < 0) FATAL_FAIL(
        "[ubus] Failed to add the \"input\" enumeration class.");

    ret = udev_enumerate_scan_devices(enumerate);
    if (ret < 0) FATAL_FAIL(
        "[ubus] Failed to scan input devices.");

    devices = udev_enumerate_get_list_entry(enumerate);
    if (devices == NULL) FATAL_FAIL(
        "[ubus] Zero input devices discovered.");


    //iterate over discovered devices 
    udev_list_entry_foreach(device_entry, devices) {

        //extract the devfs path for this device entry
        sysfs_path = udev_list_entry_get_name(device_entry);
        if (sysfs_path == NULL) FATAL_FAIL(
            "[ubus] Corrupt device entry in scanned devices list.");

        device = udev_device_new_from_syspath(udev, sysfs_path);
        if (device == NULL) FATAL_FAIL(
            "[ubus] Failed to create a device reference.");

        devfs_path = udev_device_get_devnode(device);
        if (devfs_path == NULL) FATAL_FAIL(
            "[ubus] Device entry is not in devfs.");

        //check if this is the requested devfs path
        if (strncmp(devfs_path, js_dev_path, PATH_MAX) == 0) {

            udev_enumerate_unref(enumerate);
            return device;
        }

        //cleanup for this iteration
        udev_device_unref(device);
        
    } //end foreach

    udev_enumerate_unref(enumerate);
    return NULL;
}


//populate joystick device meta-info
static int _update_joystick(const char * js_dev_path) {

    const char * str;
    size_t len;

    struct udev_device * dev;


    

    //perform a udev scan for the joystick device
    dev = _find_joystick_device(js_dev_path);


    // -- update state machine

    //if joystick isn't present
    if (dev == NULL && info.is_present == false) {

        return -1;

    //if joystick just became not present
    } else if (dev == NULL && info.is_present == true) {

        info.is_present = false;
        free((void *) info.model);
        free((void *) info.vendor);
        return -1;
    }

    //if joystick is present
    else if (dev != NULL && info.is_present == true) {

        udev_device_unref(dev);
        return 0;
    }

    //if joystick just became present
    else {

        //copy vendor
        str = udev_device_get_property_value(dev, "ID_VENDOR");
        len = strlen(str);
        info.vendor = _must_alloc(len + 1);
        strncpy((char *) info.vendor, str, len + 1);
    
        //copy model
        str = udev_device_get_property_value(dev, "ID_MODEL");
        len = strlen(str);
        info.model = _must_alloc(len + 1);
        strncpy((char *) info.model, str, len + 1);

        udev_device_unref(dev);
        return 0;
    }
}


int main() {

    int ret;


    //get a udev connection
    _init_udev();
    _init_joystick();

    //main loop
    do {

        ret = _update_joystick(JS_DEV_PATH);
        printf("ret: %d | ", ret);
        if (ret == 0) {
            printf("%s : %s", info.vendor, info.model);
        }
        putchar('\n');

    } while (sleep(1) || true);

}
