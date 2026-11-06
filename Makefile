#   10/30/2026 08/02/2024 https://github.com/su8/doomy
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.

#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.

#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
#   MA 02110-1301, USA.

# != runs a command; GNU make (4.0 or newer) and BSD make both know it
PKGS=x11 xrandr cairo-xlib pangocairo gio-2.0
DEPS_CFLAGS!=pkg-config --cflags $(PKGS)
DEPS_LIBS!=pkg-config --libs $(PKGS)
CPPFLAGS+=$(DEPS_CFLAGS)
LDLIBS+=$(DEPS_LIBS)
CFLAGS+=-g2 -Wall -Wextra -O2 -std=c99 -pipe -pedantic -Wundef -Wshadow -W -Wwrite-strings -Wcast-align -Wstrict-overflow=5 -Wconversion -Wpointer-arith -Wstrict-prototypes -Wformat=2 -Wsign-compare -Wendif-labels -Wredundant-decls -Winit-self
PREFIX?=/usr/local
PACKAGE=doomy
PROG=main.c

all:
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $(PACKAGE) $(PROG) $(LDFLAGS) $(LDLIBS)

install:
	install -d $(DESTDIR)$(PREFIX)/bin
	install -s -m 755 $(PACKAGE) $(DESTDIR)$(PREFIX)/bin/$(PACKAGE)

check: all
	sh tests/smoke.sh ./$(PACKAGE)

clean:
	rm -f $(PACKAGE)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(PACKAGE)

.PHONY: all install check clean uninstall
