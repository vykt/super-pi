#ifndef STATE_H
#define STATE_H

//external libraries
#include <ncurses.h>


// -- [data] --

enum menu_window {
    MAIN,
    ROMS,
    UNDEF
};

//menu state
struct menu_state {

    //window meta-data
    enum menu_window current_win;
    WINDOW * current_win_ptr;

    //main menu data
    int main_menu_pos;

    //ROMs menu data
    int roms_menu_pos;
    int roms_menu_off;
};


// -- [text] --

//initialise menu state
void init_menu_state();

#endif
