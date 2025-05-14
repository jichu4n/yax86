# Makefile for assembling .asm files to .com files using fasm

# Compiler settings
FASM = fasm
RM = rm -f

# Find all .asm files in the current directory
ASM_SOURCES = $(wildcard *.asm)
COM_FILES = $(ASM_SOURCES:.asm=.com)

# Default target - build all COM files
all: $(COM_FILES)

# Rule to assemble .asm to .com
%.com: %.asm
	$(FASM) $< $@

# Clean generated files
clean:
	$(RM) $(COM_FILES)

# List target files that aren't actual files
.PHONY: all clean