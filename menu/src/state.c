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
    menu_state.current_win = MAIN;
    menu_state.current_win_ptr = NULL;

    //set main menu data
    menu_state.main_menu_pos = 0;

    //set ROMs menu data
    menu_state.roms_menu_pos = 0; //"BACK"
    menu_state.roms_menu_off = 0; //"BACK"

    //set info menu data
    menu_state.info_menu_pos = 0;

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
            menu_state.roms_menu_pos = 0;
            menu_state.roms_menu_off = 0;
            break;

        case 1: //INFO
            disp_main_exit();
            disp_info_entry();
            menu_state.current_win = INFO;
            menu_state.info_menu_pos = 0;

        case 2: //EXIT
            //system("systemctl poweroff");
            break;
        }

    //ROMs menu case
    } else if (menu_state.current_win == ROMS) {

        switch(menu_state.roms_menu_pos) {
            case 0: //BACK
                handle_exit();
                break;

            default:
                //TODO launch a ROM
                break;
        }

    //info menu case
    } else if (menu_state.current_win == INFO) {

        disp_info_exit();
        disp_main_entry();
        menu_state.current_win = MAIN;
        menu_state.main_menu_pos = 1;
        
    } //end if

    redraw();
    disp_refresh();
    return;
}


//handle window exit
void handle_exit() {
    
    //ROMs menu case
    if (menu_state.current_win == ROMS) {

        disp_roms_exit();
        disp_main_entry();
        menu_state.current_win = MAIN;
        menu_state.main_menu_pos = 0;

    //info menu case
    } else if (menu_state.current_win == INFO) {

        disp_info_exit();
        disp_main_entry();
        menu_state.current_win = MAIN;
        menu_state.main_menu_pos = 1;

    } //end if

    redraw();
    disp_refresh();
    return;
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
        if (menu_state.roms_menu_pos
            != (ROMS_MENU_OPTS + rom_basenames.len - 1))
            menu_state.roms_menu_pos += 1;

    } //end if

    redraw();
    disp_refresh();
    return;
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
        if (menu_state.roms_menu_pos != 0) {
            menu_state.roms_menu_pos -= 1;
        }

    } //end if

    redraw();
    disp_refresh();
    return;
}
