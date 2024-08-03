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

static inline void drawString(const char *);

static XftColor brown, pink;
static XftFont *xftfont;
static XftDraw *xdraw = NULL;

int main(void)
{
  Window win;
  Display *display;
  Screen *Screen;
  int screen;
  XSetWindowAttributes wa;
  Colormap cmap;
  Visual *visual;
  FILE *fp = NULL;
  unsigned int height = (unsigned int)strtoul(BAR_HEIGHT, NULL, 10);
  char buf[1000] = {'\0'};
  const char *use_font = *USE_FONT ? "xft#" USE_FONT : "xft#DejaVu Sans:size=9";

  display = XOpenDisplay(NULL);
  if (NULL == display) 
  {
    puts("Cannot open X display.");
    return EXIT_FAILURE;
  }

  screen = DefaultScreen(display);
  cmap = DefaultColormap(display, screen);
  visual = DefaultVisual(display, screen);
  Screen = ScreenOfDisplay(display, DefaultScreen(display));

  wa.override_redirect = 1;
  wa.background_pixmap = ParentRelative;
  wa.event_mask = ExposureMask|KeyPressMask;

  win = XCreateWindow(display, RootWindow(display, screen),
    0, 0, (unsigned int)Screen->width, height ? height : 15, 0,
    DefaultDepth(display, screen), CopyFromParent, visual,
    CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa
  );
  XSelectInput(display, win, StructureNotifyMask);

  Atom wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", 0);
  XSetWMProtocols(display, win, &wm_delete_window, 1);

  XftColorAllocName(display, visual, cmap,  *BG_COLOR ? BG_COLOR : "#282a2e",  &brown);
  XftColorAllocName(display, visual, cmap,  *TEXT_COLOR ? TEXT_COLOR : "#b294bb",  &pink);

  xftfont = XftFontOpenXlfd(display, screen, use_font);
  if (!xftfont)
  {
    xftfont = XftFontOpenName(display, screen, use_font);
    if (!xftfont)
    {
      return EXIT_FAILURE;
    }
  }

  XMapWindow(display, win);
  XFlush(display);

  while (1)
  {
    if (!(fp = fopen("/tmp/doomy.txt", "r")))
    {
      puts("Couldn't find /tmp/doomy.txt, exiting.");
      goto err;
    }
    fscanf(fp, "%[^\n]\n", buf);
    if ((fclose(fp)) == EOF)
    {
      puts("EOF in fclose(fp), exiting.");
      goto err;
    }

    xdraw = XftDrawCreate(display, win, visual, cmap);
    XftDrawRect(xdraw, &brown, 0, 0, (unsigned int)Screen->width, height ? height : 15);
    drawString(buf);
    XFlush(display);

    sleep(1);
    XftDrawDestroy(xdraw);
  }

err:
  XftColorFree(display, visual, cmap, &brown);
  XftColorFree(display, visual, cmap, &pink);
  XftFontClose(display, xftfont);
  if(xdraw != NULL)
  {
    XftDrawDestroy(xdraw);
  }
  if (display != NULL)
  {
    XDestroyWindow(display, win);
    XCloseDisplay(display);
  }

  return EXIT_SUCCESS;
}

static inline void drawString(const char *str)
{
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
