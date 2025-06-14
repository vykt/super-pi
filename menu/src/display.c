//C standard library
#include <stdbool.h>
#include <string.h>

//system headers
#include <signal.h>
#include <sys/statvfs.h>

//kernel headers
#include <linux/limits.h>

//external libraries
#include <ncurses.h>
#include <cmore.h>

//local headers
#include "common.h"
#include "data.h"
#include "display.h"
#include "input.h"
#include "state.h"


// -- [macros] --

//colours (fmt: fg:bg)
#define WHITE_BLACK 1
#define WHITE_BLUE  2
#define WHITE_RED   3
#define BLACK_WHITE 4
#define RED_WHITE   5
#define GREEN_WHITE 6
#define BLUE_WHITE  7

//generic error string
#define ERR_GENERIC "Ncurses encountered a fatal error."

//draw template sizes
#define WIN_HDR_LEN 1
#define WIN_FTR_LEN 4

//menu sizes
#define INFO_STATIC_LEN 3

//stack draw buffer size
#define DRAW_BUF_SZ NAME_MAX


// -- [data] --

//screen data
struct scrn {

    //dimension constraints
    int min_y;
    int min_x;

    //dimensions
    int sz_y;
    int sz_x;
};

//window data
struct win {

    //dimension constraints
    int max_y;
    int max_x;
    int min_y;
    int min_x;

    //dimensions
    int sz_y;
    int sz_x;

    int body_sz_y;
    int body_sz_x;

    //draw coordinates
    int start_y;
    int start_x;

    int hdr_start_y;
    int hdr_start_x;

    int ftr_start_y;
    int ftr_start_x;

    int body_start_y;
    int body_start_x;
};

/*
 *  NOTE: The current implementation assumes the main window will never
 *        need to scroll through the input options. This is enforced
 *        by current minimum window size constraints.
 */

//menu selectable contents
struct menu {

    cm_vct opts;
    bool is_init;
    int scroll;
};


// -- [globals] --

//screen & window data
static struct scrn scrn;
static struct win win;

//windows
static WINDOW * main_win;
static WINDOW * roms_win;
static WINDOW * info_win;

//menues
struct menu main_menu;
struct menu roms_menu_0; //"back" option
struct menu roms_menu_1; //ROMs
struct menu info_menu_0; //"back" option
struct menu info_menu_1; //info lines


// -- [text] --

//initialise screen & window size constraints
static void _populate_sz_constraints() {
    
    scrn.min_y = 16;
    scrn.min_x = 34;
    win.max_y  = 32;
    win.max_x  = 80;
    win.min_y  = 14;
    win.min_x  = 32;

    return;
}


//populate colours
static void _populate_colours() {

    //initialise global colour pairs (fmt: fg:bg)
    init_pair(WHITE_BLACK, COLOR_WHITE, COLOR_BLACK);
    init_pair(WHITE_BLUE, COLOR_WHITE, COLOR_BLUE);
    init_pair(WHITE_RED, COLOR_WHITE, COLOR_RED);
    init_pair(BLACK_WHITE, COLOR_BLACK, COLOR_WHITE);
    init_pair(RED_WHITE, COLOR_RED, COLOR_WHITE);
    init_pair(GREEN_WHITE, COLOR_GREEN, COLOR_WHITE);
    init_pair(BLUE_WHITE, COLOR_BLUE, COLOR_WHITE);

    return;
}


//populate screen dimensions
static void _populate_dimensions() {

    //get the screen dimensions
    getmaxyx(stdscr, scrn.sz_y, scrn.sz_x);
    if ((scrn.sz_y < scrn.min_y) || (scrn.sz_x < scrn.min_x)) {
        FATAL_FAIL(ERR_GENERIC);
    }

    //calculate window sizes
    win.sz_y = ((scrn.sz_y - 2) > win.max_y) ? win.max_y : scrn.sz_y - 2;
    win.sz_x = ((scrn.sz_x - 2) > win.max_x) ? win.max_x : scrn.sz_x - 2;

    //calculate window starting positions
    win.start_y = (scrn.sz_y / 2) - (win.sz_y / 2);
    win.start_x = (scrn.sz_x / 2) - (win.sz_x / 2);

    //calculate window header starting position
    win.hdr_start_y = 1;
    win.hdr_start_x = (win.sz_x / 2) - 9; //9 left of middle

    //calculate window footer starting position
    win.ftr_start_y = win.sz_y - WIN_FTR_LEN - 1;
    win.ftr_start_x = (win.sz_x / 2) - 8; //8 left of middle

    //calculate window body size & starting position
    win.body_start_y = win.hdr_start_y + WIN_HDR_LEN + 1;
    win.body_start_x = (win.sz_x / 5);

    win.body_sz_y = win.ftr_start_y - 1 - win.body_start_y;
    win.body_sz_x = win.sz_x - ((win.sz_x / 5) * 2);

    return;
}


//construct a menu options vector
static void _construct_opts(struct menu * menu) {

    int ret;


    //teardown the old menu options vector if it was initialised
    if (menu->is_init == true) {
        cm_del_vct(&menu->opts);
    }

    //construct a new entry vector
    ret = cm_new_vct(&menu->opts, win.body_sz_x + 2); //2 for leeway :)
    if (ret != 0) FATAL_FAIL(ERR_GENERIC);

    //setup status
    menu->is_init = true;
    menu->scroll = 0;
}


//destruct an entry vector
static void _destruct_opts(struct menu * menu) {

    //teardown the menu options vector
    cm_del_vct(&menu->opts);
    menu->is_init = false;

    return;
}


//build a display line
static void _build_line_buf(
    char * str, size_t len, size_t max_len, char * buf, bool centered) {

    size_t diff_len;


    //if this string fits perfectly
    if (len == max_len) {
        memcpy(buf, str, len);
    }

    //if this string is too small
    if (len < max_len) {
        diff_len = (max_len - len) / 2;
        if (centered == true) {
            memset(buf, ' ', diff_len);
            memcpy(buf + diff_len, str, len);
            memset(buf + diff_len + len, ' ', diff_len);
        } else {
            memcpy(buf, str, len);
            memset(buf + len, ' ', diff_len * 2);
        }

    //else this entry does not fit inside the option space
    } else {
        memcpy(buf, str, max_len - 2);
        buf[max_len - 2] = '.';
        buf[max_len - 1] = '.';
    }
    buf[max_len] = '\0';

    return;
}


//populate main menu entries
static void _populate_main_menu() {

    int ret;
    char draw_buf[DRAW_BUF_SZ];


    char * main_opts[MAIN_MENU_OPTS] = {
        "PLAY",
        "INFO",
        "POWER OFF"
    };
    size_t main_opts_sz[MAIN_MENU_OPTS] = {4, 4, 9};


    //reset the main menu options
    _construct_opts(&main_menu);

    //populate options
    for (int i = 0; i < MAIN_MENU_OPTS; ++i) {

        //build the option buffer
        _build_line_buf(
            main_opts[i], main_opts_sz[i],
            win.body_sz_x, draw_buf, true);

        //append this entry
        ret = cm_vct_apd(&main_menu.opts, draw_buf);
        if (ret != 0) FATAL_FAIL(ERR_GENERIC)
    }

    return;
}


//teardown ROMs menu entries
static void _teardown_main_menu() {
    
    //teardown the main menu options
    _destruct_opts(&main_menu);
    return;
}


//populate ROMs menu entries
static void _populate_roms_menu() {

    int ret;

    size_t len;
    char * basename;
    char draw_buf[DRAW_BUF_SZ];


    //reset the ROMs menu options
    _construct_opts(&roms_menu_0);
    _construct_opts(&roms_menu_1);

    //populate the back option
    _build_line_buf("BACK", 4, win.body_sz_x, draw_buf, true);

    //append this entry
    ret = cm_vct_apd(&roms_menu_0.opts, draw_buf);
    if (ret != 0) FATAL_FAIL(ERR_GENERIC)

    //populate options
    for (int i = 0; i < rom_basenames.len; ++i) {

        //get the next ROM entry
        basename = cm_vct_get_p(&rom_basenames, i);
        if (basename == NULL) FATAL_FAIL(ERR_GENERIC)
        len = strnlen(basename, NAME_MAX);

        //build the option buffer
        _build_line_buf(basename, len, win.body_sz_x, draw_buf, false);

        //append this entry
        ret = cm_vct_apd(&roms_menu_1.opts, draw_buf);
        if (ret != 0) FATAL_FAIL(ERR_GENERIC)
    }
    
    return;
}


//teardown ROM menu entries
static void _teardown_rom_menu() {

    //teardown the main menu options
    _destruct_opts(&roms_menu_0);
    _destruct_opts(&roms_menu_1);
    return;
}


//populate info menu entries
static void _populate_info_menu() {

    int ret;
    int controller_count;

    struct statvfs stat;
    unsigned long free_mb;
    
    char draw_buf[NAME_MAX], line_buf[NAME_MAX];
    char * key_desc[KEY_OPT_NUM] = {
        "B / SOUTH:             %s",
        "A / EAST:              %s",
        "X / NORTH:             %s",
        "Y / WEST:              %s",
        "LEFT TRIGGER:          %s",
        "RIGHT TRIGGER:         %s",
        "SELECT:                %s",
        "START:                 %s",
        "D-PAD X-AXIS:          %s",
        "D-PAD Y-AXIS:          %s",
        "LEFT JOYSTICK X-AXIS:  %s",
        "RIGHT JOYSTICK Y-AXIS: %s"
    };


    //reset the info menu options
    _construct_opts(&info_menu_0);
    _construct_opts(&info_menu_1);

    //populate the back option
    _build_line_buf("BACK", 4, win.body_sz_x, draw_buf, true);

    //append this entry
    ret = cm_vct_apd(&info_menu_0.opts, draw_buf);
    if (ret != 0) FATAL_FAIL(ERR_GENERIC)


    //populate the ROM count
    snprintf(line_buf, win.body_sz_x, "ROMS:       %d",
             rom_basenames.len);
    _build_line_buf(line_buf, strnlen(line_buf, win.body_sz_x),
                    win.body_sz_x, draw_buf, false);

    //append this entry
    ret = cm_vct_apd(&info_menu_1.opts, draw_buf);
    if (ret != 0) FATAL_FAIL(ERR_GENERIC)


    //populate disk space
    ret = statvfs("/", &stat);
    if (ret != 0) FATAL_FAIL(ERR_GENERIC)
    free_mb = (stat.f_bfree * stat.f_frsize)  / (1024 * 1024);
    snprintf(line_buf, win.body_sz_x, "FREE SPACE: %lu MB", free_mb);
    _build_line_buf(line_buf, strnlen(line_buf, win.body_sz_x),
                    win.body_sz_x, draw_buf, false);

    //append this entry
    ret = cm_vct_apd(&info_menu_1.opts, draw_buf);
    if (ret != 0) FATAL_FAIL(ERR_GENERIC)


    //populate the version
    snprintf(line_buf, win.body_sz_x, "VERSION:    %s", VERSION);
    _build_line_buf(line_buf, strnlen(line_buf, win.body_sz_x),
                    win.body_sz_x, draw_buf, false);

    //append this entry
    ret = cm_vct_apd(&info_menu_1.opts, draw_buf);
    if (ret != 0) FATAL_FAIL(ERR_GENERIC)


    //populate a newline
    memset(draw_buf, 0, DRAW_BUF_SZ);
    
    //append this entry
    ret = cm_vct_apd(&info_menu_1.opts, draw_buf);
    if (ret != 0) FATAL_FAIL(ERR_GENERIC)


    //populate controller keymaps
    controller_count = 0;
    for (int i = 0; i < 4; ++i) {

        //skip this joystick if it isn't present
        if (js_state.js[i].is_present == false) continue;

        //add a new line for subsequent controllers
        if (controller_count != 0) {
            //populate a newline
            memset(draw_buf, 0, DRAW_BUF_SZ);
    
            //append this entry
            ret = cm_vct_apd(&info_menu_1.opts, draw_buf);
            if (ret != 0) FATAL_FAIL(ERR_GENERIC)
        }
        controller_count += 1;

        snprintf(line_buf, win.body_sz_x, "CONTROLLER %d KEYMAP:", i);
        _build_line_buf(line_buf, strnlen(line_buf, win.body_sz_x),
                        win.body_sz_x, draw_buf, true);

        //append this entry
        ret = cm_vct_apd(&info_menu_1.opts, draw_buf);
        if (ret != 0) FATAL_FAIL(ERR_GENERIC)

        //add all keys
        for (int j = 0; j < KEY_OPT_NUM; ++j) {

            //build the next key entry
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wformat-security"
            snprintf(line_buf, win.body_sz_x, key_desc[j],
                     (js_state.js[i].keys[j] == true) ? "YES" : "NO");
            #pragma GCC diagnostic pop
            _build_line_buf(line_buf, strnlen(line_buf, win.body_sz_x),
                            win.body_sz_x, draw_buf, false);

            //append this entry
            ret = cm_vct_apd(&info_menu_1.opts, draw_buf);
            if (ret != 0) FATAL_FAIL(ERR_GENERIC)

        } //end add all keys

    } //end populate controller keymaps

    return;
}


//teardown info menu entries
static void _teardown_info_menu() {
    
    //teardown the info menu options
    _destruct_opts(&info_menu_0);
    _destruct_opts(&info_menu_1);

    return;
}


//initialise windows
static void _init_wins() {

    int ret;


    //create windows
    main_win = newwin(win.sz_y, win.sz_x, win.start_y, win.start_x);
    if (main_win == NULL) FATAL_FAIL(ERR_GENERIC)

    roms_win  = newwin(win.sz_y, win.sz_x, win.start_y, win.start_x);
    if (roms_win == NULL) FATAL_FAIL(ERR_GENERIC)

    info_win  = newwin(win.sz_y, win.sz_x, win.start_y, win.start_x);
    if (info_win == NULL) FATAL_FAIL(ERR_GENERIC)

    //set window background colours
    ret = wbkgd(main_win, COLOR_PAIR(BLACK_WHITE));
    if (ret == ERR) FATAL_FAIL(ERR_GENERIC)

    ret = wbkgd(roms_win, COLOR_PAIR(BLACK_WHITE));
    if (ret == ERR) FATAL_FAIL(ERR_GENERIC)

    ret = wbkgd(info_win, COLOR_PAIR(BLACK_WHITE));
    if (ret == ERR) FATAL_FAIL(ERR_GENERIC)

    return;
}


//release windows
static void _fini_wins() {

    int ret;


    //release windows
    ret = delwin(info_win);
    if (ret == ERR) FATAL_FAIL(ERR_GENERIC)
    info_win = NULL;

    ret = delwin(roms_win);
    if (ret == ERR) FATAL_FAIL(ERR_GENERIC)
    roms_win = NULL;

    ret = delwin(main_win);
    if (ret == ERR) FATAL_FAIL(ERR_GENERIC)
    main_win = NULL;

    return;
}


//perform initialisation
void _initialise() {

    int ret;
    void * ret_ptr;

    
    //general setup
    ret_ptr = initscr();
    if (ret_ptr == NULL) FATAL_FAIL(ERR_GENERIC)
    
    ret = cbreak();
    if (ret == ERR) FATAL_FAIL(ERR_GENERIC)

    ret = keypad(stdscr, false);
    if (ret == ERR) FATAL_FAIL(ERR_GENERIC)

    ret = noecho();
    if (ret == ERR) FATAL_FAIL(ERR_GENERIC)

    ret = start_color();
    if (ret == ERR) FATAL_FAIL(ERR_GENERIC)

    //populate globals
    _populate_sz_constraints();
    _populate_colours();
    _populate_dimensions();
    _populate_main_menu();

    //setup root window
    ret = bkgd(COLOR_PAIR(WHITE_BLACK));
    if (ret == ERR) FATAL_FAIL(ERR_GENERIC)


    return;
}


//handle a terminal resize
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
void _handle_winch(int sig) {

    int ret;


    //erase screen contents
    ret = erase();
    if (ret == ERR) FATAL_FAIL(ERR_GENERIC)
    ret = refresh();
    if (ret == ERR) FATAL_FAIL(ERR_GENERIC)

    //release windows
    _fini_wins();

    //reset ncurses
    ret = endwin();
    if (ret == ERR) FATAL_FAIL(ERR_GENERIC)

    //re-initialise ncurses
    _initialise();

    //re-initialise windows
    _init_wins();
    if (menu_state.current_win == MAIN)
        menu_state.current_win_ptr = main_win;
    if (menu_state.current_win == ROMS) {
        menu_state.current_win_ptr = roms_win;
        _populate_roms_menu();
    }
    if (menu_state.current_win == INFO) {
        menu_state.current_win_ptr = info_win;
        _populate_info_menu();
    }

    return;
}
#pragma GCC diagnostic pop


//initialise ncurses global state
void init_ncurses() {

    __sighandler_t ret_hdlr;


    //initialise ncurses
    _initialise();

    //register a SIGWINCH handler
    ret_hdlr = signal(SIGWINCH, _handle_winch);
    if (ret_hdlr == SIG_ERR)
        FATAL_FAIL("Failed to register a SIGWINCH handler.")

    //initialise windows
    _init_wins();

    //update menu state
    menu_state.current_win_ptr = main_win;

    return;
}


//release ncurses global state
void fini_ncurses() {

    int ret;


    //release windows
    _fini_wins();

    //disable ncurses
    ret = endwin();
    if (ret == ERR) FATAL_FAIL(ERR_GENERIC)

    return;
}


//draw a single line, in colour
static void _draw_colour(WINDOW * win, int colour,
                         int * y, int * x, char * str,
                         int off_y, int off_x) {

    //enable a colour attribute
    if (wattron(win, COLOR_PAIR(colour)) == ERR)
        FATAL_FAIL(ERR_GENERIC)

    //perform the draw
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wformat-security"
    mvwprintw(win, *y, *x, str);
    #pragma GCC diagnostic pop

    //disable a colour attribute
    if (wattroff(win, COLOR_PAIR(colour)) == ERR)
        FATAL_FAIL(ERR_GENERIC)

    //advance y and x
    *y += off_y;
    *x += off_x;

    return;
}


//return the colour for a menu option
static int _get_menu_opt_colour(int pos, int cur_pos) {

    return (cur_pos == pos) ? WHITE_BLUE : BLACK_WHITE;
}


//return the colour for a ROMs menu option
static int _get_roms_menu_opt_colour(int pos, int cur_pos) {
    
    if (cur_pos == pos) {
        return menu_state.rom_running ? WHITE_RED : WHITE_BLUE;
    } else { return BLACK_WHITE; }
}


static int _get_submenu_sz(struct menu * menu_0, struct menu * menu_1) {

    int submenu_sz = win.body_sz_y - menu_0->opts.len - 1;
    return (menu_1->opts.len > submenu_sz)
             ? submenu_sz : roms_menu_1.opts.len;
    
}


//draw the window template
static void _draw_template(WINDOW * window) {

    int y, x;
    char draw_buf[DRAW_BUF_SZ] = {0};


    //draw the header
    y = win.hdr_start_y; x = win.hdr_start_x;
    _draw_colour(window, BLACK_WHITE, &y, &x, "--- [", 0, 5);
    _draw_colour(window, RED_WHITE, &y, &x, "SUPER", 0, 5);
    _draw_colour(window, GREEN_WHITE, &y, &x, "-", 0, 1);
    _draw_colour(window, BLUE_WHITE, &y, &x, "PI", 0, 2);
    _draw_colour(window, BLACK_WHITE, &y, &x, "] ---", 0, 5);


    //draw the footer - for each controller
    for (int i = 0; i < 4; ++i) {

        //reset the draw position
        y = win.ftr_start_y + i; x = win.ftr_start_x;

        //draw controller index
        snprintf(draw_buf, DRAW_BUF_SZ, "CONTROLLER %d: ", i + 1);
        _draw_colour(window, BLACK_WHITE, &y, &x, draw_buf, 0, 14);
        
        //if the controller is present
        if (js_state.js[i].is_present == true) {

            //draw controller status
            if (js_state.js[i].is_good == false) {
                _draw_colour(window, RED_WHITE, &y, &x, "?? ", 0, 3);
            } else {
                _draw_colour(window, GREEN_WHITE, &y, &x, "OK ", 0, 3);
            }

            //draw an additional tag
            if (js_state.main_js_idx == i) {

                _draw_colour(window, BLACK_WHITE, &y, &x, "[", 0, 1);
                _draw_colour(window, BLUE_WHITE, &y, &x, "M", 0, 1);
                _draw_colour(window, BLACK_WHITE, &y, &x, "]", 0, 1);
            }
            
        } //end if controller is present

    } //end for each controller

    return;
}


//draw the main menu
static void _draw_main_menu() {

    int y, x;
    int colour;
    char * main_opt;


    y = win.body_start_y;
    x = win.body_start_x;

    //for each main menu option
    for (int i = 0; i < MAIN_MENU_OPTS; ++i) {

        //get the next option
        main_opt = cm_vct_get_p(&main_menu.opts, i);
        if (main_opt == NULL) FATAL_FAIL(ERR_GENERIC)

        //display the next option
        colour = _get_menu_opt_colour(i, menu_state.main_menu_pos);
        _draw_colour(main_win, colour, &y, &x, main_opt, 1, 0);
    }

    return;
}


//draw the roms menu
static void _draw_roms_menu() {

    int y, x;

    int colour;
    int range;
    int scroll_i;

    char * back_opt, * roms_opt;


    y = win.body_start_y;
    x = win.body_start_x;

    //fetch the "BACK" option
    back_opt = cm_vct_get_p(&roms_menu_0.opts, 0);
    if (back_opt == NULL) FATAL_FAIL(ERR_GENERIC)

    //display the "BACK" option
    colour = _get_menu_opt_colour(0, menu_state.roms_menu_pos);
    _draw_colour(roms_win, colour, &y, &x, back_opt, 2, 0);


    //for each ROM menu option
    range = _get_submenu_sz(&roms_menu_0, &roms_menu_1);
    for (int i = 0; i < range; ++i) {

        //get an i that accounts for menu scroll
        scroll_i = roms_menu_1.scroll + i;

        //get the next option
        roms_opt = cm_vct_get_p(&roms_menu_1.opts, scroll_i);
        if (roms_opt == NULL) FATAL_FAIL(ERR_GENERIC)

        //display the next option
        colour = _get_roms_menu_opt_colour(
                     scroll_i, menu_state.roms_menu_pos - 1);
        _draw_colour(roms_win, colour, &y, &x, roms_opt, 1, 0);
    }

    return;
}


//draw the info menu
static void _draw_info_menu() {

    int y, x;

    int colour;
    int range;
    int scroll_i;

    char * back_opt, * info_opt;


    y = win.body_start_y;
    x = win.body_start_x;

    //fetch the "BACK" option
    back_opt = cm_vct_get_p(&info_menu_0.opts, 0);
    if (back_opt == NULL) FATAL_FAIL(ERR_GENERIC)

    //display the "BACK" option
    colour = _get_menu_opt_colour(0, menu_state.info_menu_pos);
    _draw_colour(info_win, colour, &y, &x, back_opt, 2, 0);


    //for each info menu data line
    range = _get_submenu_sz(&info_menu_0, &info_menu_1);
    for (int i = 0; i < range; ++i) {

        //get an i that accounts for menu scroll
        scroll_i = info_menu_1.scroll + i;

        //get the next data line
        info_opt = cm_vct_get_p(&info_menu_1.opts, scroll_i);
        if (info_opt == NULL) FATAL_FAIL(ERR_GENERIC)

        //display the next data line
        _draw_colour(info_win, BLACK_WHITE, &y, &x, info_opt, 1, 0);
    }

    return;
}


//redraw the screen
void redraw() {

    werase(menu_state.current_win_ptr);
    _draw_template(menu_state.current_win_ptr);
    if (menu_state.current_win == MAIN) _draw_main_menu();
    if (menu_state.current_win == ROMS) _draw_roms_menu();
    if (menu_state.current_win == INFO) _draw_info_menu();

    return;
}


//refresh the active window
void disp_refresh() {

    int ret;


    ret = wrefresh(menu_state.current_win_ptr);
    if (ret == ERR) FATAL_FAIL(ERR_GENERIC)

    return;
}


/*
 *  NOTE: These functions expect to be called before `menu_state` is
 *        updated to reflect the state after the input. Redraw the
 *        screen separately.
 */

//user enters the main window
void disp_main_entry() {

    menu_state.current_win_ptr = main_win;
    return;
}


//user exits the main window
void disp_main_exit() {
    return;
}


//user presses the down key inside the main window
void disp_main_down() {
    return;
}


//user presses the up key inside the main window
void disp_main_up() {
    return;
}


//user enters the ROMs window
void disp_roms_entry() {

    //update ROMs
    update_roms();
    _populate_roms_menu();
    
    //update state
    menu_state.current_win_ptr = roms_win;
    roms_menu_1.scroll = 0;

    return;
}


//user exits the ROMs window
void disp_roms_exit() {
    return;
}


//user presses the down key inside the ROMs window
void disp_roms_down() {

    int submenu_sz = _get_submenu_sz(&roms_menu_0, &roms_menu_1);


    //if already reached the bottom, ignore
    if (menu_state.roms_menu_pos
        == ROMS_MENU_OPTS + roms_menu_1.opts.len - 1) return;

    //if already on the bottom of the menu, scroll the menu down
    if (menu_state.roms_menu_pos
        == ROMS_MENU_OPTS + roms_menu_1.scroll + submenu_sz - 1) {

        roms_menu_1.scroll += 1;
    }

    return;
}


//user presses the up key inside the ROMs window
void disp_roms_up() {

    //if already reached the "BACK" option, ignore
    if (menu_state.roms_menu_pos == 0) return;

    //if already on the top of the menu, scroll the menu up
    if (menu_state.roms_menu_pos
        == ROMS_MENU_OPTS + roms_menu_1.scroll) {

        if (roms_menu_1.scroll > 0) roms_menu_1.scroll -= 1;
    }
    
    return;
}


//user presses the start/select/a key inside the ROMs window
void disp_roms_select() {

    //if on the "BACK" option, ignore    
    if (menu_state.roms_menu_pos == 0) return;

    return;
}


//user enters the info window
void disp_info_entry() {

    //update info lines
    update_roms();
    _populate_info_menu();

    //update state
    menu_state.current_win_ptr = info_win;

    return;
}


//user exits the info window
void disp_info_exit() {
    return;
}


//user presses the down key inside the info window
void disp_info_down() {

    int submenu_sz = _get_submenu_sz(&info_menu_0, &info_menu_1);


    //if already reached the bottom, ignore
    if (info_menu_1.scroll + submenu_sz == info_menu_1.opts.len)
        return;

    //scroll the menu down
    info_menu_1.scroll += 1;
    return;
}


//user presses the up key inside the info window
void disp_info_up() {

    //if already reached the top, ignore
    if (info_menu_1.scroll == 0) return;

    //scroll the menu up
    info_menu_1.scroll -= 1;
    return;
}
