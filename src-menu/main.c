//C standard library
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>

//system headers
#include <unistd.h>

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

//report a pretty error to STDERR
void _report_error(const char * fmt, ...) {

    va_list args;
    va_start(args, fmt);

    //print error prefix & message
    fprintf(stderr, "%sERROR%s: ", RED, RESET);
    vfprintf(stderr, fmt, args);

    va_end(args);

    return;
}


//malloc & exit if failed
void * _must_alloc(size_t sz) {
    
    void * ptr = malloc(sz);
    if (ptr == NULL) {
        _report_error("Failed to allocate memory.");
        exit(-1);
    }

    return ptr;
}


//connect to udev
void _connect_udev() {
    
    udev = udev_new();
    if (udev == NULL) {
        _report_error("Failed to connect to udev.");
        exit(-1);
    }

    return;
}


//initialise static joystick
void _init_joystick() { info.is_present = false; }


//populate joystick device meta-info
int _update_joystick(const char * js_dev_path) {

    const char * str;
    size_t len;
    struct udev_device * dev;
    bool fail = false;


    //get joystick udev device object
    dev = udev_device_new_from_syspath(udev, js_dev_path);

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
        strncpy((char *) info.vendor, str, len);
    
        //copy model
        str = udev_device_get_property_value(dev, "ID_MODEL");
        len = strlen(str);
        info.model = _must_alloc(len + 1);
        strncpy((char *) info.model, str, len);

        udev_device_unref(dev);
        return 0;
    }
}


int main() {

    int ret;


    //get a udev connection
    _connect_udev();
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
