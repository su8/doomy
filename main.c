/*
Copyright 08/02/2024 https://github.com/su8/doomy
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
MA 02110-1301, USA.

Compile with:
gcc -Wall -Wextra -O2 -I/usr/include/freetype2 -lX11 -lXft -o doomy doomy.c
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>

#define HEIGHT 15

static inline void drawString(const char *);

static XftColor brown, pink;
static XftFont *xftfont;
static XftDraw *xdraw;

int main(void)
{
  Window win;
  Display *display;
  Screen *Screen;
  int screen;
  XEvent event;
  XSetWindowAttributes wa;
  Colormap cmap;
  Visual *visual;
  int keep_running = -1;
  char use_font[] = "xft#" USE_FONT;
  char buf[1000] = {'\0'};

  display = XOpenDisplay(NULL);
  if (NULL == display) 
  {
    puts("Cannot open X display.");
    exit(EXIT_FAILURE);
  }

  screen = DefaultScreen(display);
  cmap = DefaultColormap(display, screen);
  visual = DefaultVisual(display, screen);
  Screen = ScreenOfDisplay(display, DefaultScreen(display));

  wa.override_redirect = 1;
  wa.background_pixmap = ParentRelative;
  wa.event_mask = ExposureMask|KeyPressMask;

  win = XCreateWindow(display, RootWindow(display, screen),
    0, 0, (unsigned int)Screen->width, HEIGHT, 0,
    DefaultDepth(display, screen), CopyFromParent, visual,
    CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa
  );
  XSelectInput(display, win, StructureNotifyMask);

  Atom wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", 0);
  XSetWMProtocols(display, win, &wm_delete_window, 1);

  xdraw = XftDrawCreate(display, win, visual, cmap);

  XftColorAllocName(display, visual, cmap,  BG_COLOR,  &brown);
  XftColorAllocName(display, visual, cmap,  TEXT_COLOR,  &pink);

  xftfont = XftFontOpenName(display, screen, use_font);

  XMapWindow(display, win);
  XFlush(display);
  XftDrawRect(xdraw, &brown, 0, 0, (unsigned int)Screen->width, HEIGHT);

  while (keep_running)
  {
    XNextEvent(display, &event);
    switch(event.type)
    {
      case ClientMessage:
      {
        if ((event.xclient.message_type == XInternAtom(display, "WM_PROTOCOLS", 1)) && ((Atom)event.xclient.data.l[0] == XInternAtom(display, "WM_DELETE_WINDOW", 1)))
        {
          keep_running = 0;
        }
      }
      break;

    default:
      break;
    }

    fgets(buf, 999, stdin);
    drawString(buf);
    sleep(1);
  }

  XftColorFree(display, visual, cmap, &brown);
  XftColorFree(display, visual, cmap, &pink);
  XftFontClose(display, xftfont);
  XftDrawDestroy(xdraw);
  XDestroyWindow(display, win);
  XCloseDisplay(display);

  return EXIT_SUCCESS;
}

static inline void drawString(const char *str)
{
  (void)str;
  char buf[1000] = {'\0'};
  char *bufPtr = buf;

  snprintf(buf, sizeof(buf) - 1, "%s", str);
  for (; *bufPtr; bufPtr++)
  {
    if (*bufPtr == '\n')
    {
      *bufPtr = '\0';
      break;
    }
  }

  XftDrawStringUtf8(xdraw, &pink, xftfont, 0, 1 + xftfont->ascent, (const FcChar8 *)buf, (int)strlen(buf));
}
