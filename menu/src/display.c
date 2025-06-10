//C standard library
#include <stdbool.h>
#include <string.h>

//system headers
#include <signal.h>

//external libraries
#include <ncurses.h>
#include <cmore.h>

//local headers
#include "common.h"
#include "data.h"
#include "display.h"


// -- [macros] --

//colours (fmt: fg:bg)
#define WHITE_BLACK 1
#define BLACK_WHITE 2
#define RED_WHITE   3
#define BLUE_WHITE  4

//generic error string
#define ERR_GENERIC "Ncurses encountered an error."

//window header
#define WIN_HEADER "---[ SUPER-PI ]---"
#define WIN_HEADER_LEN 18

#define WIN_HEADER_DECOR_0 "---[ "
#define WIN_HEADER_DECOR_1 " ]---"
#define WIN_HEADER_TITLE_0 "SUPER"
#define WIN_HEADER_TITLE_1 "PI"


// -- [data] --

//current window
enum current_win {
    MAIN=0,
    ROMS=1
};


// display state
struct disp_state {

    enum current_win current_win;
    int roms_count;

    int main_menu_idx;
    int roms_menu_idx;
    int roms_roms_idx;
};


// -- [globals] --

//maximum screen size
static int max_x;
static int max_y;

static int min_x = 32;
static int min_y = 16;

//maximum menu size
static int max_menu_x = 96;
static int max_menu_y = 32;

//static int min_menu_x = 30;
//static int min_menu_y = 14;

//window location & size
static int win_start_x;
static int win_start_y;

static int win_sz_x;
static int win_sz_y;

static int win_hdr_start_x;
static int win_hdr_start_y;

static int win_ftr_start_x;
static int win_ftr_start_y;

static int menu_opts_start_y;
static int menu_opts_items;

//windows
static WINDOW * main_win;
static WINDOW * roms_win;

//display state
struct disp_state disp_state;


// -- [text] --

//populate colours
static void _populate_colours() {

    //initialise global colour pairs (fmt: fg:bg)
    init_pair(WHITE_BLACK, COLOR_WHITE, COLOR_BLACK);
    init_pair(BLACK_WHITE, COLOR_BLACK, COLOR_WHITE);
    init_pair(RED_WHITE, COLOR_RED, COLOR_WHITE);
    init_pair(BLUE_WHITE, COLOR_BLUE, COLOR_WHITE);

    return;
}


//populate screen dimensions
static void _populate_dimensions() {

    //get the screen dimensions
    getmaxyx(stdscr, max_y, max_x);
    if ((max_y < min_y) || (max_x < min_x)) {
        subsys_state.ncurses_good = false;
        return;
    }

    //calculate window sizes
    win_sz_x = ((max_x - 2) > max_menu_x) ? max_menu_x : max_x - 2;
    win_sz_y = ((max_y - 2) > max_menu_y) ? max_menu_y : max_y - 2;

    //calculate window starting positions
    win_start_x = (max_x / 2) - (win_sz_x / 2);
    win_start_y = (max_y / 2) - (win_sz_y / 2);

    //calculate window header starting position
    win_hdr_start_x = win_start_x + (win_sz_x / 2) - (WIN_HEADER_LEN / 2);
    win_hdr_start_y = win_start_y + 1;

    //calculate window footer starting position
    win_ftr_start_x = win_start_x + (win_sz_x / 2) - 8;
    win_ftr_start_y = win_start_y + win_sz_y - 3;

    //calculate menu options starting position & count
    menu_opts_start_y = win_start_y + 5;
    menu_opts_items   = win_ftr_start_y - menu_opts_start_y - 2;

    return;
}


//initialise windows
static void _init_wins() {

    main_win = newwin(win_sz_y, win_sz_x, win_start_y, win_start_x);
    if (main_win == NULL) FATAL_FAIL(ERR_GENERIC);

    roms_win  = newwin(win_sz_y, win_sz_x, win_start_y, win_start_x);
    if (roms_win == NULL) FATAL_FAIL(ERR_GENERIC);
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

    //re-initialise windows
    _init_wins();

    //TODO Regenerate ROMs display strings

    return;
}
#pragma GCC diagnostic pop


//initialise ncurses global state
void init_ncurses() {

    int ret;
    void * ret_ptr;
    __sighandler_t ret_hdlr;


    //general setup
    memset(&disp_state, 0, sizeof(disp_state));

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
    _populate_dimensions();
    _populate_colours();

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


//draw the window template
void draw_template(WINDOW * win) {

    int ret;

    
}


//roms entry input
void roms_entry() {

    //update roms
    update_roms();

    //update state
    disp_state.current_win = ROMS;
    disp_state.roms_count  = rom_basenames.len;    

    //TODO highlight back option
}


//redraw the screen
void redraw() {

    //TODO
}
