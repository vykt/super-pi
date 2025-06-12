.RECIPEPREFIX=>

# define cross compiler
CROSS=

# generic options
CC=${CROSS}gcc
CFLAGS=
LDFLAGS=-ludev -levdev -lncurses -lcmore
WARN_OPTS=-Wall -Wextra -Werror -Wno-stringop-overread -Wno-unused-function

# build constants
SRC_DIR=./src
BUILD_DIR=./build

# set build options
ifeq ($(build),debug)
	CFLAGS += -O0 -ggdb3 -fsanitize=address
else
	CFLAGS += -O2 -flto
endif

SRCS=$(wildcard $(SRC_DIR)/*.c)
OBJS=$(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

menu: $(OBJS)
> $(CC) $(CFLAGS) $(WARN_OPTS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
> $(CC) $(CFLAGS) $(WARN_OPTS) -c $< -o $@

clean:
> -rm -f $(BUILD_DIR)/*.o menu
