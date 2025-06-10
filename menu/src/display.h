#ifndef DISPLAY_H
#define DISPLAY_H

//external libraries
#include <ncurses.h>


// -- [text] --

//initialise & release ncurses global state
void init_ncurses();
void fini_ncurses();
void redraw();

#endif
