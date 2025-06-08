.RECIPEPREFIX=>

CROSS=

release:
> ${CROSS}gcc -O2 -Wall -Wextra -Werror -Wno-stringop-overread -o menu main.c -ludev -levdev

debug:
> ${CROSS}gcc -O0 -Wall -Wextra -Wno-stringop-overread -ggdb3 -fsanitize=address -o menu_dbg main.c -ludev -levdev

clean:
> -rm menu menu_dbg
