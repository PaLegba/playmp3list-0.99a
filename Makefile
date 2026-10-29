# playmp3list - Makefile
# See LICENSE for copyright details.

.POSIX:

CXX      = g++
CXXFLAGS = -g -O2 -Wall -W -fno-strength-reduce -DNCURSES_WIDECHAR -DVERSION=\"0.99\"

# Prefer the ncursesw installation reported by pkg-config. This avoids
# picking up an incompatible libncursesw from a hardcoded path such as
# /usr/local/lib, which shadows the system library and breaks linking.
PKG_CONFIG ?= pkg-config
NCURSES_LIBS := $(shell $(PKG_CONFIG) --libs ncursesw 2>/dev/null)
ifeq ($(strip $(NCURSES_LIBS)),)
NCURSES_LIBS = -lncursesw
endif
LIBS     = $(NCURSES_LIBS)

# cputwchar_fix is a workaround for a bug in NetBSD curses.
# On NetBSD, keep this. On other systems (Linux, FreeBSD, OpenBSD), leave empty.
#CPUTWCHAR = cputwchar_fix.cc
CPUTWCHAR =

PREFIX      = /usr/local
MANPREFIX   = $(PREFIX)/share/man

SRC = main.cc interface.cc frontend.cc playlist.cc $(CPUTWCHAR)
OBJ = $(SRC:.cc=.o)

all: playmp3list

.cc.o:
	$(CXX) $(CXXFLAGS) -c -o $@ $<

playmp3list: $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ) $(LIBS)

clean:
	rm -f playmp3list $(OBJ) cputwchar_fix.o

install: playmp3list
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f playmp3list $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/playmp3list
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	cp -f playmp3list.1 $(DESTDIR)$(MANPREFIX)/man1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/playmp3list.1

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/playmp3list
	rm -f $(DESTDIR)$(MANPREFIX)/man1/playmp3list.1

.PHONY: all clean install uninstall
