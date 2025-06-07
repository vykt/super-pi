.RECIPEPREFIX=>

CROSS=

release:
> ${CROSS}gcc -O2 -Wall -Wextra -Werror -o menu main.c -ludev -levdev

debug:
> ${CROSS}gcc -O0 -Wall -Wextra -Werror -ggdb3 -fsanitize=address -o menu_dbg main.c -ludev -levdev

clean:
> -rm menu menu_dbg
