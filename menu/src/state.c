//C standard library
#include <stdlib.h>

//external libraries
#include <ncurses.h>

//local headers
#include "data.h"
#include "display.h"
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


//handle window entry or ROM launch
void handle_activate() {

    //main menu case
    if (menu_state.current_win == MAIN) {

        switch(menu_state.main_menu_pos) {
        case 0: //PLAY
            disp_main_exit();
            disp_roms_entry();
            menu_state.current_win = ROMS;
            menu_state.roms_menu_pos = -1;
            menu_state.roms_menu_off = -1;
            break;

        case 1: //INFO
            break;

        case 2: //EXIT
            system("systemctl poweroff");
            break;
        }

    //ROMs menu case
    } else if (menu_state.current_win == ROMS) {

        switch(menu_state.roms_menu_pos) {
            case -1: //BACK
                handle_exit();
                break;

            default:
                //TODO launch a ROM
                break;
        }
    } //end if

    return;
}


//handle window exit
void handle_exit() {
    
    //main menu case
    if (menu_state.current_win == MAIN) {

        disp_main_exit();
        disp_roms_entry();
        menu_state.current_win = ROMS;
        menu_state.main_menu_pos = 0;

    //ROMs menu case
    } else if (menu_state.current_win == ROMS) {

        disp_roms_exit();
        disp_main_entry();
        menu_state.current_win = MAIN;
        menu_state.main_menu_pos = 0;

    } //end if
}


//handle a down input
void handle_down() {
    
    //main menu case
    if (menu_state.current_win == MAIN) {

        disp_main_down();
        if (menu_state.main_menu_pos != (MAIN_MENU_OPTS - 1))
            menu_state.main_menu_pos += 1;

    //ROMs menu case
    } else if (menu_state.current_win == ROMS) {

        disp_roms_down();
        if (menu_state.roms_menu_pos != (rom_basenames.len -1))
            menu_state.roms_menu_pos += 1;

    } //end if
}


//handle an up input
void handle_up() {
    
    //main menu case
    if (menu_state.current_win == MAIN) {

        disp_main_up();
        if (menu_state.main_menu_pos != 0)
            menu_state.main_menu_pos -= 1;

    //ROMs menu case
    } else if (menu_state.current_win == ROMS) {

        disp_roms_up();
        if (menu_state.roms_menu_pos != -1) {
            menu_state.roms_menu_pos -= 1;
        }

    } //end if
}
