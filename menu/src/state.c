//external libraries
#include <ncurses.h>

//local headers
#include "state.h"


// -- [globals] --

//menu state
struct menu_state menu_state;


// -- [text] --

//initialise menu state
void init_menu_state() {

    //set window meta-data
    menu_state.current_win = UNDEF;
    menu_state.current_win_ptr = NULL;

    //set main menu data
    menu_state.main_menu_pos = 0;

    //set ROMs menu data
    menu_state.roms_menu_pos = -1; //"BACK"
    menu_state.roms_menu_off = -1; //"BACK"

    return;
}
