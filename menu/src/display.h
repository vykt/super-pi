#ifndef DISPLAY_H
#define DISPLAY_H

//external libraries
#include <ncurses.h>


// -- [text] --

//initialise & release ncurses global state
void init_ncurses();
void fini_ncurses();

//redraw the header & footer
void redraw_template();

//redraw the body
void redraw_menu();

//main window updates
void disp_main_entry();
void disp_main_exit();
void disp_main_down();
void disp_main_up();

//ROMs window updates
void disp_roms_entry();
void disp_roms_exit();
void disp_roms_down();
void disp_roms_up();

#endif
