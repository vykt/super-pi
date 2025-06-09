//C standard library
#include <string.h>

//system headers
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

//kernel headers
#include <linux/limits.h>

//external libraries
#include <cmore.h>

//local headers
#include "common.h"
#include "data.h"


// -- [globals] --

//cmore list of rom pathnames
cm_lst rom_basenames;


// -- [text] --

//initialise the global rom list
void init_roms() {

    cm_new_lst(&rom_basenames, sizeof(char[NAME_MAX]));
    return;
}


//release the global rom list
void fini_roms() {

    cm_del_lst(&rom_basenames);
    return;
}


//repopulate the rom list
void update_roms() {

    int ret;
    void * ret_ptr;
    
    size_t len, path_roms_len;

    DIR * rom_dir;
    struct dirent * dirent;
    struct stat statbuf;

    char pathname_buf[PATH_MAX];


    //empty the roms list
    ret = cm_lst_emp(&rom_basenames);
    if (ret != 0) {
        subsys_state.rom_good = false;
        fini_roms();
        init_roms();
        return;
    }

    //open the roms directory
    rom_dir = opendir(PATH_ROMS);
    if (rom_dir == NULL) {
        subsys_state.rom_good = false;
        return;
    }

    //prepare the pathname buffer
    path_roms_len = strnlen(PATH_ROMS, PATH_MAX);
    memcpy(pathname_buf, PATH_ROMS, path_roms_len);
    pathname_buf[path_roms_len] = '/';

    //for all directory entries
    while ((dirent = readdir(rom_dir)) != NULL) {

        //build the pathname for this entry
        len = strnlen(dirent->d_name, NAME_MAX);
        if ((path_roms_len + len + 2) > PATH_MAX) {
            subsys_state.rom_good = false;
            goto _update_roms_cleanup_dir;
        }
        memcpy(pathname_buf + path_roms_len + 1, dirent->d_name, len + 1);

        //skip non-regular file entries
        ret = stat(pathname_buf, &statbuf);
        if (ret != 0 || S_ISREG(statbuf.st_mode) == false) continue;

        //skip non `.sfc` extension entries
        if (len < 4) continue;
        if (strncmp(dirent->d_name + len - 4, ".sfc", NAME_MAX)) continue;

        //add this basename to the rom list
        ret_ptr = cm_lst_apd(&rom_basenames, dirent->d_name);
        if (ret_ptr == NULL) {

            //on failure, re-start the list
            subsys_state.rom_good = false;
            fini_roms();
            init_roms();

            goto _update_roms_cleanup_dir;
        }
    }

    _update_roms_cleanup_dir:
    closedir(rom_dir);

    return;
}
