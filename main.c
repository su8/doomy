/*
Copyright 10/30/2026 08/02/2024 https://github.com/su8/doomy
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
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cairo-xlib.h>
#include <pango/pangocairo.h>

struct bar
{
  Display *display;
  Window win;
  cairo_t *cr;
  PangoLayout *layout;
  PangoColor bg;
  guint16 alpha;
  gboolean bottom;
  int height; /* follows the text */
};

/* The bar is as high as the text: the background, then the text, drawn in a group and copied in one go, so it does not flicker */
static void draw(struct bar *bar, const char *text)
{
  Display *display = bar->display;
  int screen = DefaultScreen(display);
  int height = 0;
  GError *error = NULL;

  if (pango_parse_markup(text, -1, 0, NULL, NULL, NULL, &error))
  {
    pango_layout_set_markup(bar->layout, text, -1);
  }
  else
  {
    /* show what is wrong where the text would be */
    pango_layout_set_attributes(bar->layout, NULL);
    pango_layout_set_text(bar->layout, error->message, -1);
    g_error_free(error);
  }
  pango_layout_get_pixel_size(bar->layout, NULL, &height);
  if (height != bar->height)
  {
    bar->height = height;
    XMoveResizeWindow(display, bar->win, 0, bar->bottom ? DisplayHeight(display, screen) - height : 0,
      (unsigned int)DisplayWidth(display, screen), (unsigned int)height);
    cairo_xlib_surface_set_size(cairo_get_target(bar->cr), DisplayWidth(display, screen), height);
  }

  cairo_push_group(bar->cr);
  cairo_set_operator(bar->cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba(bar->cr, bar->bg.red / 65535.0, bar->bg.green / 65535.0, bar->bg.blue / 65535.0, bar->alpha / 65535.0);
  cairo_paint(bar->cr);
  cairo_set_operator(bar->cr, CAIRO_OPERATOR_OVER);
  cairo_set_source_rgb(bar->cr, 0xb2 / 255.0, 0x94 / 255.0, 0xbb / 255.0); /* #b294bb, unless the markup says otherwise */
  cairo_move_to(bar->cr, 0.0, 0.0);
  pango_cairo_show_layout(bar->cr, bar->layout);
  cairo_pop_group_to_source(bar->cr);
  cairo_set_operator(bar->cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint(bar->cr);
  cairo_surface_flush(cairo_get_target(bar->cr));
  XFlush(display);
}

int main(int argc, char *argv[])
{
  struct bar bar = {0};
  gchar *background = NULL;
  GOptionEntry entries[] = {
    {"background", 0, 0, G_OPTION_ARG_STRING, &background,
      "A Pango colour: a name, #rrggbb, or #rrggbbaa to see through it with a compositor (#282a2e)", "COLOR"},
    {"bottom", 0, 0, G_OPTION_ARG_NONE, &bar.bottom, "At the bottom of the screen, not the top", NULL},
    {NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL}
  };
  GOptionContext *options = NULL;
  GError *error = NULL;
  gchar *path = NULL;
  gchar *text = NULL;
  int screen = 0;
  int width = 0;
  XVisualInfo vinfo;
  XSetWindowAttributes wa = {0};
  cairo_surface_t *surface = NULL;
  PangoFontDescription *font = NULL;
  PangoTabArray *tabs = NULL;

  options = g_option_context_new(NULL);
  g_option_context_set_summary(options,
    "Every second, draw the first line of $XDG_RUNTIME_DIR/doomy.txt as a bar.\n"
    "The line is Pango markup. Tabs split it: left<TAB>centre<TAB>right.");
  g_option_context_add_main_entries(options, entries, NULL);
  if (!g_option_context_parse(options, &argc, &argv, &error) || argc > 1)
  {
    fprintf(stderr, "doomy: %s, see --help\n", error ? error->message : "takes only options");
    return EXIT_FAILURE;
  }
  g_option_context_free(options);
  if (!pango_color_parse_with_alpha(&bar.bg, &bar.alpha, background ? background : "#282a2e"))
  {
    fprintf(stderr, "doomy: unknown colour '%s', use a name like red, #rrggbb or #rrggbbaa\n", background);
    return EXIT_FAILURE;
  }
  g_free(background);
  path = g_build_filename(g_get_user_runtime_dir(), "doomy.txt", NULL);

  if (!(bar.display = XOpenDisplay(NULL)))
  {
    fputs("doomy: cannot open X display.\n", stderr);
    return EXIT_FAILURE;
  }
  screen = DefaultScreen(bar.display);
  width = DisplayWidth(bar.display, screen);
  /* a window with an alpha channel: an opaque colour is solid, one with alpha is see-through when a compositor runs */
  if (!XMatchVisualInfo(bar.display, screen, 32, TrueColor, &vinfo))
  {
    fputs("doomy: the X server has no 32 bit visual.\n", stderr);
    return EXIT_FAILURE;
  }
  wa.override_redirect = True;
  wa.colormap = XCreateColormap(bar.display, RootWindow(bar.display, screen), vinfo.visual, AllocNone);
  /* 1 pixel high until draw() knows how high the text is */
  bar.win = XCreateWindow(bar.display, RootWindow(bar.display, screen), 0, 0, (unsigned int)width, 1U, 0,
    32, InputOutput, vinfo.visual, CWOverrideRedirect | CWColormap | CWBorderPixel | CWBackPixel, &wa);
  XMapWindow(bar.display, bar.win);

  surface = cairo_xlib_surface_create(bar.display, bar.win, vinfo.visual, width, 1);
  bar.cr = cairo_create(surface);
  bar.layout = pango_cairo_create_layout(bar.cr);
  /* the font unless the markup says otherwise; without one fontconfig may pick a serif */
  font = pango_font_description_from_string("Sans 9");
  pango_layout_set_font_description(bar.layout, font);
  pango_font_description_free(font);
  /* always left to right: a line that starts in Arabic or Hebrew would swap the left and the right part */
  pango_context_set_base_dir(pango_layout_get_context(bar.layout), PANGO_DIRECTION_LTR);
  pango_layout_set_auto_dir(bar.layout, FALSE);
  pango_layout_context_changed(bar.layout);
  /* left<TAB>centre<TAB>right: the first tab centres the text after it in the bar, the second puts it at the right */
  tabs = pango_tab_array_new(2, TRUE);
  pango_tab_array_set_tab(tabs, 0, PANGO_TAB_CENTER, width / 2);
  pango_tab_array_set_tab(tabs, 1, PANGO_TAB_RIGHT, width);
  pango_layout_set_tabs(bar.layout, tabs);
  pango_tab_array_free(tabs);

  /* deleting the file is how doomy is stopped */
  while (g_file_get_contents(path, &text, NULL, NULL))
  {
    text[strcspn(text, "\n")] = '\0';
    draw(&bar, text);
    g_free(text);
    g_usleep(G_USEC_PER_SEC);
  }
  fprintf(stderr, "doomy: couldn't read %s, exiting.\n", path);

  g_free(path);
  g_object_unref(bar.layout);
  cairo_destroy(bar.cr);
  cairo_surface_destroy(surface);
  XDestroyWindow(bar.display, bar.win);
  XFreeColormap(bar.display, wa.colormap);
  XCloseDisplay(bar.display);
  return EXIT_SUCCESS;
}
