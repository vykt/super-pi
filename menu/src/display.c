/*
 *  Watching paint dry: C edition.
 */

//C standard library
#include <stdbool.h>
#include <string.h>

//system headers
#include <signal.h>

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
#define WHITE_GREEN 2
#define BLACK_WHITE 3
#define RED_WHITE   4
#define GREEN_WHITE 5
#define BLUE_WHITE  6

//generic error string
#define ERR_GENERIC "Ncurses raised an error."

//main menu options
#define MAIN_MENU_OPTS 3
#define WIN_HDR_LEN 1
#define WIN_FTR_LEN 4


// -- [data] --

//screen data
struct scrn {

    //dimension constraints
    /* const */ int min_y; //16
    /* const */ int min_x; //32

    //dimensions
    int sz_y;
    int sz_x;
};

//window data
struct win {

    //dimension constraints
    int max_y; //32
    int max_x; //96
    int min_y; //14
    int min_x; //30

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

//menu selectable contents
struct menu {

    cm_vct opts;
    bool is_init;
    int scroll;
};


// -- [globals] --

//menu state
struct menu_state menu_state;

//screen & window data
static struct scrn scrn;
static struct win win;

//windows
static WINDOW * main_win;
static WINDOW * roms_win;

//menues
struct menu main_menu;
struct menu roms_menu;


// -- [text] --

//initialise screen & window size constraints
static void _populate_sz_constraints() {
    
    scrn.min_y = 16;
    scrn.min_x = 32;
    win.max_y  = 32;
    win.max_x  = 96;
    win.min_y  = 14;
    win.min_x  = 30;

    return;
}


//populate colours
static void _populate_colours() {

    //initialise global colour pairs (fmt: fg:bg)
    init_pair(WHITE_BLACK, COLOR_WHITE, COLOR_BLACK);
    init_pair(WHITE_GREEN, COLOR_WHITE, COLOR_GREEN);
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
    win.hdr_start_y = win.start_y + 1;
    win.hdr_start_x = win.start_x + (win.sz_x / 2) - (WIN_FTR_LEN / 2);

    //calculate window footer starting position
    win.ftr_start_y = win.start_y + win.sz_y - WIN_FTR_LEN - 1;
    win.ftr_start_x = win.start_x + (win.sz_x / 2) - 8; //8 left of middle

    //calculate window body size & starting position
    win.body_start_y = win.hdr_start_y + WIN_HDR_LEN + 1;
    win.body_start_x = win.start_x + (win.sz_x / 5);

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


//centralise the provided string inside the provided buffer
static void _build_opt_buf(
    char * str, size_t len, size_t max_len, char * buf) {

    size_t diff_len;


    //if this string fits perfectly
    if (len == max_len) {
        memcpy(buf, str, len);
    }

    //if this string is too small
    if (len < max_len) {
        diff_len = (max_len - len) / 2;
        memset(buf, ' ', diff_len);
        memcpy(buf + diff_len, str, len);
        memset(buf + diff_len + len, ' ', diff_len);

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
static void _populate_main_menu_opts() {

    int ret;
    char * main_opt_buf;


    char * main_opts[MAIN_MENU_OPTS] = {
        "PLAY",
        "INFO",
        "EXIT"
    };
    size_t main_opts_sz[MAIN_MENU_OPTS] = {4, 4, 4};


    //reset the main menu options
    _construct_opts(&main_menu);

    //allocate a menu option string buffer
    main_opt_buf = malloc(win.body_sz_x + 2); //2 for leeway :)
    if (main_opt_buf == NULL) FATAL_FAIL(ERR_GENERIC)

    //populate options
    for (int i = 0; i < MAIN_MENU_OPTS; ++i) {

        //build the option buffer
        _build_opt_buf(
            main_opts[i], main_opts_sz[i], win.body_sz_x, main_opt_buf);

        //append this entry
        ret = cm_vct_apd(&main_menu.opts, main_opt_buf);
        if (ret != 0) FATAL_FAIL(ERR_GENERIC)
    }

    //free the menu option buffer
    free(main_opt_buf);

    return;
}


//teardown ROMs menu entries
static void _teardown_main_menu_opts() {
    
    //teardown the main menu options
    _destruct_opts(&main_menu);
    return;
}


//populate ROMs menu entries
static void _populate_roms_menu_opts() {

    int ret;

    size_t len;
    char * basename;
    char * rom_opt_buf;


    //reset the ROMs menu options
    _construct_opts(&roms_menu);

    //allocate a string buffer of updated entry size
    rom_opt_buf = malloc(win.body_sz_x + 2);
    if (rom_opt_buf == NULL) FATAL_FAIL(ERR_GENERIC)

    //populate options
    for (int i = 0; i < rom_basenames.len; ++i) {

        //get the next ROM entry
        basename = cm_vct_get_p(&rom_basenames, i);
        if (basename == NULL) FATAL_FAIL(ERR_GENERIC)
        len = strnlen(basename, NAME_MAX);

        //build the option buffer
        _build_opt_buf(basename, len, win.body_sz_x, rom_opt_buf);

        //append this entry
        ret = cm_vct_apd(&roms_menu.opts, rom_opt_buf);
        if (ret != 0) FATAL_FAIL(ERR_GENERIC)
    }
    
    //free the menu option buffer
    free(rom_opt_buf);

    return;
}


//teardown ROM menu entries
static void _teardown_rom_menu_opts() {

    //teardown the main menu options
    _destruct_opts(&roms_menu);
    return;
}


//initialise windows
static void _init_wins() {

    int ret;


    //create windows
    main_win = newwin(win.sz_y, win.sz_x, win.start_y, win.start_x);
    if (main_win == NULL) FATAL_FAIL(ERR_GENERIC);

    roms_win  = newwin(win.sz_y, win.sz_x, win.start_y, win.start_x);
    if (roms_win == NULL) FATAL_FAIL(ERR_GENERIC);

    //set window background colours
    ret = wbkgd(main_win, COLOR_PAIR(BLACK_WHITE));
    if (ret == ERR) subsys_state.ncurses_good = false;

    ret = wbkgd(roms_win, COLOR_PAIR(BLACK_WHITE));
    if (ret == ERR) subsys_state.ncurses_good = false;

    return;
}


//release windows
static void _fini_wins() {

    int ret;


    //release windows
    ret = delwin(roms_win);
    if (ret == ERR) FATAL_FAIL(ERR_GENERIC)

    ret = delwin(main_win);
    if (ret == ERR) FATAL_FAIL(ERR_GENERIC)

    return;
}


//handle a terminal resize
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
void _handle_winch(int sig) {

    int ret;


    //release windows
    _fini_wins();

    //reset ncurses
    ret = endwin();
    if (ret == ERR) FATAL_FAIL(ERR_GENERIC)

    ret = refresh();
    if (ret == ERR) FATAL_FAIL(ERR_GENERIC)

    ret = clear();
    if (ret == ERR) FATAL_FAIL(ERR_GENERIC)

    //re-populate global dimensions
    _populate_dimensions();
    _populate_main_menu_opts();
    _populate_roms_menu_opts();

    //re-initialise windows
    _init_wins();

    return;
}
#pragma GCC diagnostic pop


//initialise ncurses global state
void init_ncurses() {

    int ret;
    void * ret_ptr;
    __sighandler_t ret_hdlr;


    //general setup
    init_menu_state();

    ret_ptr = initscr();
    if (ret_ptr == NULL) FATAL_FAIL(ERR_GENERIC)
    
    ret = cbreak();
    if (ret == ERR) FATAL_FAIL(ERR_GENERIC)

    ret = keypad(stdscr, false);
    if (ret == ERR) FATAL_FAIL(ERR_GENERIC)

    ret = noecho();
    if (ret == ERR) FATAL_FAIL(ERR_GENERIC)

    ret = start_color();
    if (ret == ERR) subsys_state.ncurses_good = false;   
    
    //populate globals
    _populate_sz_constraints();
    _populate_colours();
    _populate_dimensions();

    //register a SIGWINCH handler
    ret_hdlr = signal(SIGWINCH, _handle_winch);
    if (ret_hdlr == SIG_ERR)
        FATAL_FAIL("Failed to register a SIGWINCH handler.")

    //setup root window
    ret = bkgd(COLOR_PAIR(WHITE_BLACK));
    if (ret == ERR) subsys_state.ncurses_good = false;

    //initialise windows
    _init_wins();

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
static void _draw_colour(
    WINDOW * win, int colour, int y, int x, char * str) {

    //enable a colour attribute
    if (wattron(win, COLOR_PAIR(colour)) == ERR) {
        subsys_state.ncurses_good = false;
    }

    //perform the draw
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wformat-security"
    mvwprintw(win, y, x, str);
    #pragma GCC diagnostic pop

    //disable a colour attribute
    if (wattroff(win, COLOR_PAIR(colour)) == ERR) {
        subsys_state.ncurses_good = false;
    }

    return;
}


//draw the window template
static void _draw_template(WINDOW * window) {

    int y, x;


    //draw the header
    y = win.hdr_start_y; x = win.hdr_start_x;
    _draw_colour(window, BLACK_WHITE, y, x, "--- [");

    x += 5;
    _draw_colour(window, RED_WHITE, y, x, "SUPER");

    x += 5;
    _draw_colour(window, GREEN_WHITE, y, x, "-");

    x += 1;
    _draw_colour(window, BLUE_WHITE, y, x, "PI");

    x += 2;
    _draw_colour(window, BLACK_WHITE, y, x, "] ---");


    //draw the footer - for each controller
    for (int i = 0; i < 4; ++i) {

        //draw controller index
        y = win.ftr_start_y + i; x = win.ftr_start_x;
        if (wattron(window, COLOR_PAIR(BLACK_WHITE)) == ERR) {
            subsys_state.ncurses_good = false;
        }
        mvwprintw(window, y, x, "CONTROLLER %d: ", i);
        if (wattroff(window, COLOR_PAIR(BLACK_WHITE)) == ERR) {
            subsys_state.ncurses_good = false;
        }
        
        //if the controller is present
        x += 14;
        if (js_state.js[i].is_present == true) {

            //draw controller status
            if (js_state.main_js_idx == i
                && subsys_state.controller_good == false) {

                _draw_colour(window, RED_WHITE, y, x, "?? ");
                x += 3;
            } else {
                _draw_colour(window, GREEN_WHITE, y, x, "OK ");
                x += 3;
            }

            //draw the main tag if this controller is present
            if (js_state.main_js_idx == i) {

                _draw_colour(window, BLACK_WHITE, y, x, "[");

                x += 1;
                _draw_colour(window, BLUE_WHITE, y, x, "M");

                x += 1;
                _draw_colour(window, BLACK_WHITE, y, x, "]");
            }
            
        } //end if controller is present

    } //end for each controller

    return;
}


//draw the main menu
static void _draw_main_menu() {

    int y, x;
    char * next_opt;

    y = menu_opts_start_y;
    x = menu_opts_start_x;

    //for each main menu option
    for (int i = 0; i < MAIN_MENU_OPTS; ++i) {

        //get the next option
        next_opt = cm_vct_get_p(&main_menu_opts, i);
        if (next_opt == NULL) FATAL_FAIL(ERR_GENERIC)

        //display the next option
        if (disp_state.main_menu_idx == i) {
            _draw_colour(main_win, WHITE_GREEN, y, x, next_opt);
        } else {
            _draw_colour(main_win, BLACK_WHITE, y, x, next_opt);
        }
        y += 1;
    }
}


//draw the roms menu
static void _draw_roms_menu() {

    int y, x;
    char * next_opt;


    y = menu_opts_start_y;
    x = menu_opts_start_x;

    //draw the back option
    if (disp_state.main_menu_idx == 0) {
        _draw_colour(main_win, WHITE_GREEN, y, x, "BACK");
    } else {
        _draw_colour(main_win, BLACK_WHITE, y, x, "BACK");
    }

    //for each ROM menu option
    y += 2;
    for (int i = 0; i < menu_opts_items; ++i) {

        //get the next option
        next_opt = cm_vct_get_p(&main_menu_opts, rom_menu_scroll_off + i);
        if (next_opt == NULL) FATAL_FAIL(ERR_GENERIC)

        //display the next option
        if (disp_state.roms_menu_idx == i) {
            _draw_colour(main_win, WHITE_GREEN, y, x, next_opt);
        } else {
            _draw_colour(main_win, BLACK_WHITE, y, x, next_opt);
        }
        y += 1;
    }
}


//main window input functions
void main_entry() {

    disp_state.current_win = main_win;
    return;
}


void main_exit() {

    return;
}


void main_down() {

    if (disp_state.roms_menu_idx < menu_opts_items)
        disp_state.roms_menu_idx += 1;

    return;
}


void main_up() {

    if (disp_state.roms_menu_idx > 0)
        disp_state.roms_menu_idx -= 1;

    return;
}


//roms window input functions
void roms_entry() {

    //update roms
    update_roms();
    
    //update state
    disp_state.current_win   = roms_win;
    disp_state.roms_count    = rom_basenames.len;    
    disp_state.roms_menu_idx = 0;
    disp_state.roms_roms_idx = 0;

    return;
}


void roms_exit() {

    return;
}


void roms_down() {

    if (disp_state.roms_menu_idx < menu_opts_items) {
        disp_state.roms_menu_idx += 1;
        
        if (disp_state.roms_roms_idx
            == (rom_menu_scroll_off + menu_opts_items - 1))
            rom_menu_scroll_off += 1;
        disp_state.roms_roms_idx += 1;
    }

    return;
}


void roms_up() {

    //I can't describe in words how fucking boring this is to write

#if 0
    if (disp_state.roms_menu_idx > 0) {
        disp_state.roms_menu_idx -= 1;
        if (rom_menu_scroll_cur == 0) {
            rom_menu_scroll_off -= 1;
        } else {
            rom_menu_scroll_cur -= 1;
        }
    }
#endif

    return;
}


//redraw the header & footer
void redraw_template() {

    _draw_template(disp_state.current_win);
}


//redraw the body
void redraw_menu() {

    if (disp_state.current_win == main_win) _draw_main_menu();
    if (disp_state.current_win == roms_win) _draw_roms_menu();
}
