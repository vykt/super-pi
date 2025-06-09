//C standard library
#include <stdbool.h>

//external libraries
#include <ncurses.h>

//local headers
#include "common.h"
#include "display.h"


// -- [globals] --

//maximum screen size
static int max_x;
static int max_y;

//maximum menu size
static int max_menu_x;
static int max_menu_y;

//menu location & size
static int menu_start_x;
static int menu_start_y;

static int menu_sz_x;
static int menu_sz_y;

//windows
static WINDOW * root_win;
static WINDOW * menu_win;


// -- [text] --
void init_ncurses() {

    initscr();
    cbreak();
    keypad(stdscr, false);
    noecho();

    return;
}
