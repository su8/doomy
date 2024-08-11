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

#include <argp.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>

static inline void drawString(const char *);
static error_t parse_opt(int , char *, struct argp_state *);

static XftColor brown;
static XftColor pink;
static XftFont *xftfont;
static XftDraw *xdraw = NULL;

static const char *BG_COLOR    = "";
static const char *TEXT_COLOR  = "";
static const char *USE_FONT    = "";
static unsigned int BAR_HEIGHT = 18U;
static unsigned int TOP_BAR = 1U;

static char doc[] = "Dead simple x11 bar/statusline program.\v";
const char *argp_program_version = "1.0";
const char *argp_program_bug_address = "https://github.com/su8/doomy ";
static struct argp_option options[] =
{
    { .doc = "" },
    { .name = "bgcolor",   .key = 'b', .arg = "#282a2e", .doc = "Set background color"},
    { .name = "textcolor", .key = 't', .arg = "#b294bb", .doc = "Set text color"},
    { .name = "top",       .key = 's', .arg = "1",        .doc = "Show the program on top == 1, else 0 bottom position"},
    { .name = "font",      .key = 'f', .arg = "xft#DejaVu Sans:size=9:style=bold", .doc = "Use custom font size and bold type"},
    { .name = "barheight", .key = 'h', .arg = "15",      .doc = "Set the height of the bar"},
    { .doc = NULL }
};

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
  (void)state;
  switch(key)
  {
    case 'b':
      BG_COLOR = arg;
      break;

    case 't':
      TEXT_COLOR = arg;
      break;

    case 'f':
      USE_FONT = arg;
      break;

    case 'h':
      BAR_HEIGHT = (unsigned int)strtoul(arg, (char **)NULL, 10);
      break;

    case 's':
      TOP_BAR = (unsigned int)strtoul(arg, (char **)NULL, 10);
      break;

    default:
      return ARGP_ERR_UNKNOWN;
  }
  return EXIT_SUCCESS;
}

static const struct argp arg_parser = {
  .parser = parse_opt,
  .options = options,
  .doc = doc
};

int main(int argc, char *argv[])
{
  Window win;
  Display *display;
  Screen *Screen;
  int screen;
  XSetWindowAttributes wa;
  Colormap cmap;
  Visual *visual;
  FILE *fp = NULL;
  unsigned int showToTop = 0U;
  char buf[1000] = {'\0'};
  const char *use_font = *USE_FONT ? USE_FONT : "xft#DejaVu Sans:size=9:style=bold";

  argp_parse(&arg_parser, argc, argv, ARGP_IN_ORDER, NULL, NULL);

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
  showToTop = (unsigned int)XDisplayHeight(display, screen) - (unsigned int)BAR_HEIGHT;

  wa.override_redirect = 1;
  wa.background_pixmap = ParentRelative;
  wa.event_mask = ExposureMask|KeyPressMask;

  win = XCreateWindow(display, RootWindow(display, screen),
    0, !TOP_BAR ? showToTop : 0,
    (unsigned int)Screen->width, BAR_HEIGHT ? BAR_HEIGHT : 15U, 0,
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
    XftDrawRect(xdraw, &brown, 0, 0, (unsigned int)Screen->width, BAR_HEIGHT ? BAR_HEIGHT : 15U);
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
