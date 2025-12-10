PROJ_ROOT := $(CURDIR)
SRC_DIR := src
OBJ_DIR := bin
LIB_DIR := lib

LIBS := $(wildcard $(LIB_DIR)/*)
INCLUDE_DIRS := $(addprefix -I,$(LIBS))

# Find all source files recursively
SRC_FILES := $(wildcard $(SRC_DIR)/*.c)
LIB_FILES := $(shell find $(LIB_DIR) -name '*.c' 2>/dev/null)

# Map source files to object files, preserving directory structure
SRC_OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/src/%.o,$(SRC_FILES))
LIB_OBJS := $(patsubst $(LIB_DIR)/%.c,$(OBJ_DIR)/lib/%.o,$(LIB_FILES))

ALL_OBJS := $(SRC_OBJS) $(LIB_OBJS)

CC = gcc
CFLAGS := -std=gnu17 -g -Wall -Wextra
CFLAGS += -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable
CFLAGS += $(INCLUDE_DIRS)

PFLAGS :=
ifeq ($(shell uname -s),Linux)
	PFLAGS := -lnsl -lev -lssl -lcrypto
	CFLAGS += -I/usr/include -Wno-variadic-macros
else
	PFLAGS := -L/opt/homebrew/lib -lssl -lcrypto -lev
	CFLAGS += -I/opt/homebrew/include -Wno-gnu-zero-variadic-macro-arguments -pedantic -Werror
endif

LDFLAGS = $(PFLAGS)

BOLD_BLUE := \x1b[1;34m
BOLD_GRAY := \x1b[1;37m
BOLD_DARKEST4 := \x1b[38,2;235m
BOLD_DARKEST3 := \x1b[30;2;236m
BOLD_DARKEST2 := \x1b[30;2;238m
BOLD_DARKEST1 := \x1b[30;2;239m
BOLD_DARKEST := \x1b[30;2;240m
BOLD_DARKER := \x1b[30;5;244m
BOLD_DARK := \x1b[30;5;245m
BOLD_VIOL := \x1b[38;5;104m
BOLD_TURQ := \x1b[38;5;37m
RESET := \x1b[0m


.DEFAULT_GOAL := all
.PHONY: all clean run rebuild-cc test demo
.DELETE_ON_ERROR:

all: clean proxy

proxy: $(ALL_OBJS)
	@echo "*--------------* building $@ *--------------*"
	@echo "Linking Binary: $@"
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "*--------------* build complete *--------------*"

# Compile src/*.c files, preserving directory structure
$(OBJ_DIR)/src/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling: $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

# Compile lib/**/*.c files, preserving directory structure
$(OBJ_DIR)/lib/%.o: $(LIB_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling: $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

# Create the output directory
$(OBJ_DIR):
	@(mkdir -p $@)

clean:
	@echo "*-------------* cleaning project *-------------*"
	@rm -rf $(ALL_OBJS) proxy
	@echo "Deleting: proxy, $(ALL_OBJS)"
