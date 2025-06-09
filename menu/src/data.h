#ifndef DATA_H
#define DATA_H

//external libraries
#include <cmore.h>

//local headers
#include "common.h"


// -- [data] --

//rom storage
extern cm_lst rom_basenames; //type: char[NAME_MAX]


// -- [text] --

//initialise & release the global rom list
void init_roms();
void fini_roms();

//repopulate the rom list
void update_roms();





#endif
