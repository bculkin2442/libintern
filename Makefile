# Dependancy boilerplate
#
DEPDIR := .d
$(shell mkdir -p $(DEPDIR) >/dev/null)
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td

COMPILE.c   = $(CC) $(DEPFLAGS) $(CFLAGS) $(CPPFLAGS)
POSTCOMPILE = @mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d && touch $@

OBJDIR := bin
$(shell mkdir -p $(OBJDIR) >/dev/null)

%.o : %.c
$(OBJDIR)/%.o : %.c $(DEPDIR)/%.d
	$(COMPILE.c) -c $< -o $@
	$(POSTCOMPILE)

$(DEPDIR)/%.d: ;
.PRECIOUS: $(DEPDIR)/%.d

# Customize here
# --------------

# Rules for typesetting manpages
%.ps: %.1
	man -T $< > $@

%.pdf: %.ps
	ps2pdf $< $@

#  All our main source files
SRCS := $(wildcard *.c)

# All of our documentation
DOCS := $(wildcard help/*.1)

# The flags we want to pass to the C compiler
CFLAGS := -std=c11 -O0 -g -Wall -Wextra -Wpedantic -Wno-unused-variable -Wno-unused-parameter -Wno-implicit-function-declaration -Wno-unused-but-set-variable -fdiagnostics-color=auto

# None of these targets correspond to actual files
.PHONY: all clean run libs docs

all: libintern.a
	-@ctags -R .
	-@cscope -b -q -R


# Make libintern
libintern.a: bin/intern.o
	ar rcs libintern.a bin/intern.o

bin/intern.o: intern.c intern.h
	$(CC) $(CFLAGS) -o bin/intern.o -c intern.c

# Delete the binary and any object/library files, as well as the printed documentation
clean: 
	-@rm -rf bin/
	-@rm $(wildcard help/*.pdf)

docs: $(patsubst %.1,%.pdf,$(DOCS))

# Include all of our dependancy info
include $(wildcard $(patsubst %,$(DEPDIR)/%.d,$(basename $(SRCS))))
