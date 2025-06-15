#ifndef STATE_H
#define STATE_H

//external libraries
#include <ncurses.h>


// -- [macros] --

#define MAIN_MENU_OPTS 3
#define ROMS_MENU_OPTS 1
#define INFO_MENU_OPTS 1


// -- [data] --

enum menu_window {
    MAIN,
    ROMS,
    INFO
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

    //info menu data
    int info_menu_pos;
    int info_menu_off;
};


// -- [globals] --

//global menu state
extern struct menu_state menu_state;

//execve parameters
extern char * const argv[1];
extern char ** envp;


// -- [text] --

//initialise menu state
void init_menu_state();

//initialise execve parameters
void init_execve_params();

//handle inputs
void handle_activate();
void handle_exit();
void handle_down();
void handle_up();


#endif
