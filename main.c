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
#define _POSIX_C_SOURCE 200809L /* setpgid, kill */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glib-unix.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cairo-xlib.h>
#include <pango/pangocairo.h>

/* A link in the line, <a href="COMMAND" button="N">text</a>: where its text is, in bytes of the text Pango draws */
struct link
{
  gsize start;
  gsize end;
  gchar *href;
  guint button; /* X's mouse buttons: 1 left (without button=), 2 middle, 3 right, 4 and 5 the wheel up and down */
};

struct bar
{
  GMainLoop *loop;
  gchar *path;
  GArray *links;       /* of the line on the bar */
  GHashTable *running; /* the commands of links that still run, with their process ids: the next click closes them */
  Display *display;
  Window win;
  cairo_t *cr;
  PangoLayout *layout;
  PangoColor bg;
  guint16 alpha;
  gboolean bottom;
  int height; /* follows the text */
};

/* Reading the line: <a> goes into the links, everything else on to Pango as it was */
struct parse
{
  GString *markup;
  GArray *links;
  gsize offset; /* bytes of text so far: Pango's text is the text between the tags */
  GArray *open; /* the links whose </a> is still to come, as indexes into links */
};

static void onStart(GMarkupParseContext *context, const gchar *name, const gchar **names, const gchar **values, gpointer data, GError **error)
{
  struct parse *parse = data;
  struct link link = {parse->offset, parse->offset, NULL, 1};
  gchar *value = NULL;
  const gchar *href = NULL;
  gchar *end = NULL;
  guint index = 0;
  int i = 0;

  (void)context;
  if (strcmp(name, "a"))
  {
    g_string_append_printf(parse->markup, "<%s", name);
    for (i = 0; names[i]; i++)
    {
      value = g_markup_escape_text(values[i], -1);
      g_string_append_printf(parse->markup, " %s=\"%s\"", names[i], value);
      g_free(value);
    }
    g_string_append_c(parse->markup, '>');
    return;
  }
  for (i = 0; names[i]; i++)
  {
    if (!strcmp(names[i], "href"))
    {
      href = values[i];
    }
    else if (!strcmp(names[i], "button") && ((link.button = (guint)strtoul(values[i], &end, 10)) < 1 || link.button > 5 || *end || !*values[i]))
    {
      g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT, "button=\"%s\" in <a> must be 1 to 5", values[i]);
      return;
    }
  }
  /* links in links are fine, so a text can have one for each button, but not two for the same */
  for (i = 0; (guint)i < parse->open->len; i++)
  {
    index = g_array_index(parse->open, guint, i);
    if (g_array_index(parse->links, struct link, index).button == link.button)
    {
      g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT, "a link in a link needs another button=\"%u\"", link.button);
      return;
    }
  }
  if (!href)
  {
    g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT, "<a> needs href=\"COMMAND\"");
    return;
  }
  link.href = g_strdup(href);
  g_array_append_val(parse->links, link);
  index = parse->links->len - 1;
  g_array_append_val(parse->open, index);
}

static void onEnd(GMarkupParseContext *context, const gchar *name, gpointer data, GError **error)
{
  struct parse *parse = data;
  (void)context;
  (void)error;
  if (strcmp(name, "a"))
  {
    g_string_append_printf(parse->markup, "</%s>", name);
    return;
  }
  g_array_index(parse->links, struct link, g_array_index(parse->open, guint, parse->open->len - 1)).end = parse->offset;
  g_array_set_size(parse->open, parse->open->len - 1);
}

static void onText(GMarkupParseContext *context, const gchar *text, gsize len, gpointer data, GError **error)
{
  struct parse *parse = data;
  gchar *escaped = g_markup_escape_text(text, (gssize)len);
  (void)context;
  (void)error;
  g_string_append(parse->markup, escaped);
  g_free(escaped);
  parse->offset += len;
}


static void clearLink(gpointer data)
{
  g_free(((struct link *)data)->href);
}

/* A pop-up that ended, by a second click or by itself: its link opens it again */
struct popup
{
  GHashTable *running;
  gchar *command;
};

static void onExit(GPid pid, gint status, gpointer data)
{
  struct popup *popup = data;
  (void)status;
  g_spawn_close_pid(pid);
  g_hash_table_remove(popup->running, popup->command);
  g_free(popup->command);
  g_free(popup);
}

/* in the child, before the command starts: a process group of its own, so the second click closes all of it */
static void ownGroup(gpointer data)
{
  (void)data;
  setpgid(0, 0);
}

/* The byte of the text under x, y; -1 on a gap or past the text, where Pango gives the nearest one, whose box is elsewhere */
static int indexAt(PangoLayout *layout, int x, int y)
{
  PangoRectangle box;
  int index = 0;
  int trailing = 0;

  pango_layout_xy_to_index(layout, x * PANGO_SCALE, y * PANGO_SCALE, &index, &trailing);
  pango_layout_index_to_pos(layout, index, &box);
  return x * PANGO_SCALE < box.x || x * PANGO_SCALE >= box.x + box.width ? -1 : index;
}

/* Clicks, as soon as they come */
static gboolean onEvents(gint fd, GIOCondition condition, gpointer data)
{
  struct bar *bar = data;
  XEvent event;
  (void)fd;
  (void)condition;
  while (XPending(bar->display))
  {
    XNextEvent(bar->display, &event);
    if (ButtonPress == event.type)
    {
      onClick(bar, event.xbutton.button, event.xbutton.x, event.xbutton.y);
    }
  }
  return G_SOURCE_CONTINUE;
}

/* Pango markup with GTK's <a href="COMMAND">text</a> links, and button="N" for other mouse buttons: links gets where each one is, Pango the rest. NULL on bad markup */
static gchar *readLinks(const char *text, GArray *links, GError **error)
{
  static const GMarkupParser parser = {onStart, onEnd, onText, NULL, NULL};
  struct parse parse = {g_string_new(NULL), links, 0, g_array_new(FALSE, FALSE, sizeof(guint))};
  gchar *line = g_strconcat("<markup>", text, "</markup>", NULL); /* one root, as Pango reads it */
  GMarkupParseContext *context = g_markup_parse_context_new(&parser, 0, &parse, NULL);
  gboolean ok = g_markup_parse_context_parse(context, line, -1, error) && g_markup_parse_context_end_parse(context, error);

  g_markup_parse_context_free(context);
  g_array_unref(parse.open);
  g_free(line);
  return g_string_free(parse.markup, !ok);
}

/* The bar is as high as the text: the background, then the text, drawn in a group and copied in one go, so it does not flicker */
static void draw(struct bar *bar, const char *text)
{
  Display *display = bar->display;
  int screen = DefaultScreen(display);
  int height = 0;
  gchar *markup = NULL;
  GError *error = NULL;

  g_array_set_size(bar->links, 0);
  if ((markup = readLinks(text, bar->links, &error)) && pango_parse_markup(markup, -1, 0, NULL, NULL, NULL, &error))
  {
    pango_layout_set_markup(bar->layout, markup, -1);
  }
  else
  {
    /* show what is wrong where the text would be */
    g_array_set_size(bar->links, 0);
    pango_layout_set_attributes(bar->layout, NULL);
    pango_layout_set_text(bar->layout, error ? error->message : "doomy: bad markup", -1);
    g_clear_error(&error);
  }
  g_free(markup);
  g_clear_error(&error);

  pango_layout_get_pixel_size(bar->layout, NULL, &height);
  if (height <= 0) height = 1; /* X accepts 0, but then the bar is invisible and unclickable */
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

/* A click on a link with its button runs its command in a shell; a click on it while that still runs closes it,
 * and what it started, like a pop-up */
static void onClick(struct bar *bar, guint button, int x, int y)
{
  int index = indexAt(bar->layout, x, y);
  struct link *link = NULL;
  struct popup *popup = NULL;
  gchar sh[] = "/bin/sh";
  gchar dashC[] = "-c";
  gchar *argv[] = {sh, dashC, NULL, NULL};
  GError *error = NULL;
  GPid pid = 0;
  guint i = 0;

  for (i = 0; index >= 0 && i < bar->links->len; i++)
  {
    link = &g_array_index(bar->links, struct link, i);
    if (link->button == button && (gsize)index >= link->start && (gsize)index < link->end)
    {
      break;
    }
    link = NULL;
  }
  if (!link)
  {
    return;
  }
  if ((pid = GPOINTER_TO_INT(g_hash_table_lookup(bar->running, link->href))))
  {
    /* stop the popup, and forget it: its onExit will still run, but the entry is gone */
    kill(-pid, SIGTERM);
    g_hash_table_remove(bar->running, link->href);
    return;
  }
  argv[2] = link->href;
  if (!g_spawn_async(NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD, ownGroup, NULL, &pid, &error))
  {
    fprintf(stderr, "doomy: %s\n", error->message);
    g_error_free(error);
    return;
  }
  g_hash_table_insert(bar->running, g_strdup(link->href), GINT_TO_POINTER(pid));
  popup = g_new(struct popup, 1);
  popup->running = bar->running;
  popup->command = g_strdup(link->href);
  g_child_watch_add(pid, onExit, popup);
}

/* Every second: draw the first line of the file. Deleting the file is how doomy is stopped */
static gboolean onTick(gpointer data)
{
  struct bar *bar = data;
  gchar *text = NULL;
  if (!g_file_get_contents(bar->path, &text, NULL, NULL))
  {
    fprintf(stderr, "doomy: couldn't read %s, exiting.\n", bar->path);
    g_main_loop_quit(bar->loop);
    return G_SOURCE_REMOVE;
  }
  text[strcspn(text, "\r\n")] = '\0';   /* handle CRLF files too */
  draw(bar, text);
  g_free(text);
  onEvents(0, G_IO_IN, bar); /* Xlib may have read events while drawing, which the socket no longer shows */
  return G_SOURCE_CONTINUE;
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
    "The line is Pango markup. Tabs split it: left<TAB>centre<TAB>right.\n"
    "A left click on <a href=\"COMMAND\">text</a> runs COMMAND, a second click closes it;\n"
    "<a href=\"COMMAND\" button=\"N\"> for button N: 2 middle, 3 right, 4 and 5 the wheel.");
  g_option_context_add_main_entries(options, entries, NULL);
  if (!g_option_context_parse(options, &argc, &argv, &error) || argc > 1)
  {
    fprintf(stderr, "doomy: %s, see --help\n", error ? error->message : "takes only options");
    return EXIT_FAILURE;
  }
  g_option_context_free(options);
  if (!pango_color_parse_with_alpha(&bar.bg, &bar.alpha, background ? background : "#282a2e"))
  {
    fprintf(stderr, "doomy: unknown colour '%s', use a name like red, #rrggbb or #rrggbbaa\n", background ? background : "(default)");
    return EXIT_FAILURE;
  }
  g_free(background);
  background = NULL;
  bar.path = g_build_filename(g_get_user_runtime_dir(), "doomy.txt", NULL);

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
  if (!bar.win)
  {
    fputs("doomy: cannot create the window.\n", stderr);
    return EXIT_FAILURE;
  }
  XSelectInput(bar.display, bar.win, ButtonPressMask);
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

  bar.links = g_array_new(FALSE, TRUE, sizeof(struct link));
  g_array_set_clear_func(bar.links, clearLink);
  bar.running = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  bar.loop = g_main_loop_new(NULL, FALSE);
  if (onTick(&bar))
  {
    g_timeout_add_seconds(1, onTick, &bar);
    g_unix_fd_add(ConnectionNumber(bar.display), G_IO_IN, onEvents, &bar);
    g_main_loop_run(bar.loop);
  }

  g_array_unref(bar.links);
  g_hash_table_unref(bar.running);
  g_main_loop_unref(bar.loop);
  g_free(bar.path);
  g_object_unref(bar.layout);
  cairo_destroy(bar.cr);
  cairo_surface_destroy(surface);
  XDestroyWindow(bar.display, bar.win);
  XFreeColormap(bar.display, wa.colormap);
  XCloseDisplay(bar.display);
  return EXIT_SUCCESS;
}
