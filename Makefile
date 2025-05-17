# Makefile for yax86 library

# Compiler and flags
CC = gcc
CFLAGS = -nostdlib -fno-builtin
AR = ar
ARFLAGS = rcs

# Directories with their own Makefiles
SUBDIRS = tests

# Source and object files
SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)
LIB = libyax86.a

# Default target
all: $(LIB) subdirs

# Compile .c to .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Create static library
$(LIB): $(OBJ)
	$(AR) $(ARFLAGS) $@ $^

# Compile all subdirectories
.PHONY: subdirs $(SUBDIRS)
subdirs: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

# Clean generated files
clean:
	rm -f $(OBJ) $(LIB)
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

.PHONY: all clean subdirs