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
#define ERR_GENERIC "Ncurses raised an error."

//main menu options
#define WIN_HDR_LEN 1
#define WIN_FTR_LEN 4


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
    bool is_active;
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
struct menu roms_menu_0; //regular menu
struct menu roms_menu_1; //ROMs
struct menu info_menu;


// -- [text] --

//initialise screen & window size constraints
static void _populate_sz_constraints() {
    
    scrn.min_y = 16;
    scrn.min_x = 32;
    win.max_y  = 32;
    win.max_x  = 80;
    win.min_y  = 14;
    win.min_x  = 30;

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
    char * main_opt_buf;


    char * main_opts[MAIN_MENU_OPTS] = {
        "PLAY",
        "INFO",
        "POWER OFF"
    };
    size_t main_opts_sz[MAIN_MENU_OPTS] = {4, 4, 9};


    //reset the main menu options
    _construct_opts(&main_menu);

    //allocate a menu option string buffer
    main_opt_buf = malloc(win.body_sz_x + 2); //2 for leeway :)
    if (main_opt_buf == NULL) FATAL_FAIL(ERR_GENERIC)

    //populate options
    for (int i = 0; i < MAIN_MENU_OPTS; ++i) {

        //build the option buffer
        _build_line_buf(
            main_opts[i], main_opts_sz[i],
            win.body_sz_x, main_opt_buf, true);

        //append this entry
        ret = cm_vct_apd(&main_menu.opts, main_opt_buf);
        if (ret != 0) FATAL_FAIL(ERR_GENERIC)
    }

    //free the menu option buffer
    free(main_opt_buf);

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
    char * roms_opt_buf;


    //reset the ROMs menu options
    _construct_opts(&roms_menu_0);
    _construct_opts(&roms_menu_1);

    //allocate a string buffer of updated entry size
    roms_opt_buf = malloc(win.body_sz_x + 2);
    if (roms_opt_buf == NULL) FATAL_FAIL(ERR_GENERIC)

    //populate the back option
    _build_line_buf("BACK", 4, win.body_sz_x, roms_opt_buf, true);

    //append this entry
    ret = cm_vct_apd(&roms_menu_0.opts, roms_opt_buf);
    if (ret != 0) FATAL_FAIL(ERR_GENERIC)

    //populate options
    for (int i = 0; i < rom_basenames.len; ++i) {

        //get the next ROM entry
        basename = cm_vct_get_p(&rom_basenames, i);
        if (basename == NULL) FATAL_FAIL(ERR_GENERIC)
        len = strnlen(basename, NAME_MAX);

        //build the option buffer
        _build_line_buf(basename, len, win.body_sz_x, roms_opt_buf, false);

        //append this entry
        ret = cm_vct_apd(&roms_menu_1.opts, roms_opt_buf);
        if (ret != 0) FATAL_FAIL(ERR_GENERIC)
    }
    
    //free the menu option buffer
    free(roms_opt_buf);

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
    
    char * info_opt_buf;
    char * info_data_buf;

    struct statvfs stat;
    unsigned long free_mb;


    //reset the info menu options
    _construct_opts(&info_menu);

    //allocate menu buffers
    info_opt_buf = malloc(win.body_sz_x + 2); //2 for leeway :)
    if (info_opt_buf == NULL) FATAL_FAIL(ERR_GENERIC)

    info_data_buf = malloc(win.body_sz_x + 2); //2 for leeway :)
    if (info_data_buf == NULL) FATAL_FAIL(ERR_GENERIC)


    //populate the back option
    _build_line_buf("BACK", 4, win.body_sz_x, info_opt_buf, true);

    //append this entry
    ret = cm_vct_apd(&info_menu.opts, info_opt_buf);
    if (ret != 0) FATAL_FAIL(ERR_GENERIC)


    //populate the ROM count
    snprintf(info_data_buf, win.body_sz_x, "ROMS:       %d",
             rom_basenames.len);
    _build_line_buf(info_data_buf, strnlen(info_data_buf, win.body_sz_x),
                    win.body_sz_x, info_opt_buf, false);

    //append this entry
    ret = cm_vct_apd(&info_menu.opts, info_opt_buf);
    if (ret != 0) FATAL_FAIL(ERR_GENERIC)


    //populate disk space
    ret = statvfs("/", &stat);
    if (ret != 0) FATAL_FAIL(ERR_GENERIC)
    free_mb = (stat.f_bfree * stat.f_frsize)  / (1024 * 1024);
    snprintf(info_data_buf, win.body_sz_x, "FREE SPACE: %lu MB", free_mb);
    _build_line_buf(info_data_buf, strnlen(info_data_buf, win.body_sz_x),
                    win.body_sz_x, info_opt_buf, false);

    //append this entry
    ret = cm_vct_apd(&info_menu.opts, info_opt_buf);
    if (ret != 0) FATAL_FAIL(ERR_GENERIC)


    //populate the version
    snprintf(info_data_buf, win.body_sz_x, "VERSION:    %s", VERSION);
    _build_line_buf(info_data_buf, strnlen(info_data_buf, win.body_sz_x),
                    win.body_sz_x, info_opt_buf, false);

    //append this entry
    ret = cm_vct_apd(&info_menu.opts, info_opt_buf);
    if (ret != 0) FATAL_FAIL(ERR_GENERIC)


    //free the info option buffer
    free(info_opt_buf);
    free(info_data_buf);

    return;
}


//teardown info menu entries
static void _teardown_info_menu() {
    
    //teardown the info menu options
    _destruct_opts(&info_menu);
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

    info_win  = newwin(win.sz_y, win.sz_x, win.start_y, win.start_x);
    if (info_win == NULL) FATAL_FAIL(ERR_GENERIC);

    //set window background colours
    ret = wbkgd(main_win, COLOR_PAIR(BLACK_WHITE));
    if (ret == ERR) subsys_state.ncurses_good = false;

    ret = wbkgd(roms_win, COLOR_PAIR(BLACK_WHITE));
    if (ret == ERR) subsys_state.ncurses_good = false;

    ret = wbkgd(info_win, COLOR_PAIR(BLACK_WHITE));
    if (ret == ERR) subsys_state.ncurses_good = false;

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
    if (ret == ERR) subsys_state.ncurses_good = false;

    //populate globals
    _populate_sz_constraints();
    _populate_colours();
    _populate_dimensions();
    _populate_main_menu();

    //setup root window
    ret = bkgd(COLOR_PAIR(WHITE_BLACK));
    if (ret == ERR) subsys_state.ncurses_good = false;


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
        mvwprintw(window, y, x, "CONTROLLER %d: ", i + 1);
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


    y = win.body_start_y;
    x = win.body_start_x;

    //for each main menu option
    for (int i = 0; i < MAIN_MENU_OPTS; ++i) {

        //get the next option
        next_opt = cm_vct_get_p(&main_menu.opts, i);
        if (next_opt == NULL) FATAL_FAIL(ERR_GENERIC)

        //display the next option
        if (menu_state.main_menu_pos == i) {
            _draw_colour(main_win, WHITE_BLUE, y, x, next_opt);
        } else {
            _draw_colour(main_win, BLACK_WHITE, y, x, next_opt);
        }
        y += 1;
    }
}


//draw the roms menu
static void _draw_roms_menu() {

    int y, x;

    int list_sz, range;
    int effective_i;
    char * next_opt;


    y = win.body_start_y;
    x = win.body_start_x;

    //fetch the "BACK" option
    next_opt = cm_vct_get_p(&roms_menu_0.opts, 0);
    if (next_opt == NULL) FATAL_FAIL(ERR_GENERIC)

    //display the "BACK" option
    if (menu_state.roms_menu_pos == 0) {
        _draw_colour(roms_win,
                     roms_menu_0.is_active ? WHITE_RED : WHITE_BLUE,
                     y, x, next_opt);
    } else {
        _draw_colour(roms_win, BLACK_WHITE, y, x, next_opt);
    }

    //for each ROM menu option
    y += 2;
    list_sz = win.body_sz_y - 2;
    range = (roms_menu_1.opts.len > list_sz)
             ? list_sz : roms_menu_1.opts.len;
    for (int i = 0; i < range; ++i) {

        //get an i that accounts for menu scroll
        effective_i = roms_menu_1.scroll + i;

        //get the next option
        next_opt = cm_vct_get_p(&roms_menu_1.opts, effective_i);
        if (next_opt == NULL) FATAL_FAIL(ERR_GENERIC)

        //display the next option
        if (menu_state.roms_menu_pos - 1 == effective_i) {
            _draw_colour(roms_win,
                         roms_menu_1.is_active ? WHITE_RED : WHITE_BLUE,
                         y, x, next_opt);
        } else {
            _draw_colour(roms_win, BLACK_WHITE, y, x, next_opt);
        }

        y += 1;
    }
}


//draw the info menu
static void _draw_info_menu() {

    int y, x;
    char * next_opt;


    y = win.body_start_y;
    x = win.body_start_x;

    //fetch the "BACK" option
    next_opt = cm_vct_get_p(&info_menu.opts, 0);
    if (next_opt == NULL) FATAL_FAIL(ERR_GENERIC)

    //display the "BACK" option
    _draw_colour(info_win,
                 info_menu.is_active ? WHITE_RED : WHITE_BLUE,
                 y, x, next_opt);

    //for each info menu data line
    y += 2;
    for (int i = 0; i < INFO_MENU_DATA; ++i) {

        //get the next data line
        next_opt = cm_vct_get_p(&info_menu.opts, i + INFO_MENU_OPTS);
        if (next_opt == NULL) FATAL_FAIL(ERR_GENERIC)

        //display the next data line
        _draw_colour(info_win, BLACK_WHITE, y, x, next_opt);

        y += 1;
    }
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

    int list_sz = win.body_sz_y - 2;

    //if already reached the bottom, ignore
    if (menu_state.roms_menu_pos
        == ROMS_MENU_OPTS + roms_menu_1.opts.len - 1) return;

    //if already on the bottom of the menu, scroll the menu down
    if (menu_state.roms_menu_pos
        == ROMS_MENU_OPTS + roms_menu_1.scroll + list_sz - 1) {

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

    //set the current position as active
    roms_menu_0.is_active = true;

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
    return;
}


//user presses the up key inside the info window
void disp_info_up() {
    return;
}
