#   08/02/2024
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

CFLAGS+=-g2 -Wall -Wextra -O2 -I/usr/include/freetype2 -I/usr/X11R6/include -L/usr/X11R6/lib -I/usr/include/X11 -lX11 -lXft -std=c99 -pipe -pedantic -Wundef -Wshadow -W -Wwrite-strings -Wcast-align -Wstrict-overflow=5 -Wconversion -Wpointer-arith -Wstrict-prototypes -Wformat=2 -Wsign-compare -Wendif-labels -Wredundant-decls -Winit-self
PACKAGE=doomy
PROG=main.c

# used by doomy in the past, since then argp is used
# -DUSE_FONT=\"'$(USE_FONT)'\" -DTEXT_COLOR=\"'$(TEXT_COLOR)'\" -DBG_COLOR=\"'$(BG_COLOR)'\" -DBAR_HEIGHT=\"'$(BAR_HEIGHT)'\" 

all:
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(PACKAGE) $(PROG)

install: 
	install -D -s -m 755 $(PACKAGE) /usr/bin/$(PACKAGE)

clean:
	rm -f $(PACKAGE)

uninstall:
	rm -f /usr/bin/$(PACKAGE)

.PHONY: all install clean uninstall
