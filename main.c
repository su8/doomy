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
#define _POSIX_C_SOURCE 200809L /* setpgid, kill, waitid */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <glib-unix.h>
#include <gio/gio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
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

/* The bar on one monitor: each monitor has one, all draw the same line */
struct output
{
  Window win;
  cairo_t *cr;
  PangoLayout *layout; /* laid out for this monitor's width, so it also says where a click on this bar is */
  int x, y, width, height; /* the monitor, in the whole screen */
  int barHeight; /* follows the text */
};

struct bar
{
  GMainLoop *loop;
  gchar *path;
  GArray *links;       /* of the line, where they are in its text: the same on every monitor */
  GHashTable *running; /* the commands of links that still run, with their process ids: the next click closes them */
  Display *display;
  struct output *outputs;
  int count;
  PangoColor bg;
  guint16 alpha;
  gboolean bottom;
};

/* Reading the line: <a> goes into the links, everything else on to Pango as it was */
struct parse
{
  GString *markup;
  GArray *links;
  gsize offset; /* bytes of text so far: Pango's text is the text between the tags */
  GArray *open; /* the links whose </a> is still to come, as indexes into links */
};

static void onStart(GMarkupParseContext *context, const gchar *name, const gchar **names, const gchar **values, gpointer data, GError **error);
static void onEnd(GMarkupParseContext *context, const gchar *name, gpointer data, GError **error);
static void onText(GMarkupParseContext *context, const gchar *text, gsize len, gpointer data, GError **error);
static void clearLink(gpointer data);
static void onExit(GPid pid, gint status, gpointer data);
static void ownGroup(gpointer data);
static int indexAt(PangoLayout *layout, int x, int y);
static gboolean onEvents(gint fd, GIOCondition condition, gpointer data);
static gchar *readLinks(const char *text, GArray *links, GError **error);
static void setStrut(Display *display, const struct output *out, int bottom);
static void paint(struct bar *bar, struct output *out, const char *markup, const char *message);
static void draw(struct bar *bar, const char *text);
static void onClick(struct bar *bar, const struct output *out, guint button, int x, int y);
static void openOutput(Display *display, struct output *out, XVisualInfo *vinfo, Colormap colormap);
static gboolean redraw(struct bar *bar);
static void onChange(GFileMonitor *monitor, GFile *file, GFile *other, GFileMonitorEvent change, gpointer data);

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
  /* only its own entry: a click may have opened the command again while this one was closing */
  if (GPOINTER_TO_INT(g_hash_table_lookup(popup->running, popup->command)) == pid)
  {
    g_hash_table_remove(popup->running, popup->command);
  }
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

/* Clicks, as soon as they come, and a redraw when a covered part of the bar shows again */
static gboolean onEvents(gint fd, GIOCondition condition, gpointer data)
{
  struct bar *bar = data;
  XEvent event;
  int i = 0;
  (void)fd;
  (void)condition;
  while (XPending(bar->display))
  {
    XNextEvent(bar->display, &event);
    for (i = 0; ButtonPress == event.type && i < bar->count; i++)
    {
      if (bar->outputs[i].win == event.xbutton.window)
      {
        onClick(bar, &bar->outputs[i], event.xbutton.button, event.xbutton.x, event.xbutton.y);
      }
    }
    if (Expose == event.type && !event.xexpose.count && !redraw(bar))
    {
      break;
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

/* The strut: the strip at the top or bottom of its monitor, as high as the bar, that window managers keep free of
 * other windows. It counts from the edge of the whole screen, and runs only along its monitor */
static void setStrut(Display *display, const struct output *out, int bottom)
{
  /* left, right, top, bottom, then where each runs: left_start_y, left_end_y, right_start_y, right_end_y,
   * top_start_x, top_end_x, bottom_start_x, bottom_end_x */
  long strut[12] = {0};
  if (bottom)
  {
    strut[3] = DisplayHeight(display, DefaultScreen(display)) - (out->y + out->height) + out->barHeight;
    strut[10] = out->x;
    strut[11] = out->x + out->width - 1;
  }
  else
  {
    strut[2] = out->y + out->barHeight;
    strut[8] = out->x;
    strut[9] = out->x + out->width - 1;
  }
  XChangeProperty(display, out->win, XInternAtom(display, "_NET_WM_STRUT_PARTIAL", False), XA_CARDINAL, 32,
    PropModeReplace, (unsigned char *)strut, 12);
  XChangeProperty(display, out->win, XInternAtom(display, "_NET_WM_STRUT", False), XA_CARDINAL, 32,
    PropModeReplace, (unsigned char *)strut, 4); /* the first four, for older window managers */
}

/* One monitor's bar, as high as its text: the background, then the text, drawn in a group and copied in one go, so it
 * does not flicker. markup is the line, or NULL: then message is drawn, what is wrong with it */
static void paint(struct bar *bar, struct output *out, const char *markup, const char *message)
{
  int height = 0;

  if (markup)
  {
    pango_layout_set_markup(out->layout, markup, -1);
  }
  else
  {
    pango_layout_set_attributes(out->layout, NULL);
    pango_layout_set_text(out->layout, message, -1);
  }
  pango_layout_get_pixel_size(out->layout, NULL, &height);
  if (height <= 0) height = 1; /* X accepts 0, but then the bar is invisible and unclickable */
  if (height != out->barHeight)
  {
    out->barHeight = height;
    XMoveResizeWindow(bar->display, out->win, out->x, bar->bottom ? out->y + out->height - height : out->y,
      (unsigned int)out->width, (unsigned int)height);
    cairo_xlib_surface_set_size(cairo_get_target(out->cr), out->width, height);
    setStrut(bar->display, out, bar->bottom);
  }

  cairo_push_group(out->cr);
  cairo_set_operator(out->cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba(out->cr, bar->bg.red / 65535.0, bar->bg.green / 65535.0, bar->bg.blue / 65535.0, bar->alpha / 65535.0);
  cairo_paint(out->cr);
  cairo_set_operator(out->cr, CAIRO_OPERATOR_OVER);
  cairo_set_source_rgb(out->cr, 0xb2 / 255.0, 0x94 / 255.0, 0xbb / 255.0); /* #b294bb, unless the markup says otherwise */
  cairo_move_to(out->cr, 0.0, 0.0);
  pango_cairo_show_layout(out->cr, out->layout);
  cairo_pop_group_to_source(out->cr);
  cairo_set_operator(out->cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint(out->cr);
  cairo_surface_flush(cairo_get_target(out->cr));
}

/* The line on every monitor's bar, read once for all of them; broken markup shows what is wrong with it */
static void draw(struct bar *bar, const char *text)
{
  gchar *markup = NULL;
  GError *error = NULL;
  int i = 0;

  g_array_set_size(bar->links, 0);
  if (!(markup = readLinks(text, bar->links, &error)) || !pango_parse_markup(markup, -1, 0, NULL, NULL, NULL, &error))
  {
    g_array_set_size(bar->links, 0);
    g_clear_pointer(&markup, g_free);
  }
  for (i = 0; i < bar->count; i++)
  {
    paint(bar, &bar->outputs[i], markup, error ? error->message : "doomy: bad markup");
  }
  XFlush(bar->display);
  g_free(markup);
  g_clear_error(&error);
}

/* A click on a link with its button runs its command in a shell; a click on it while that still runs closes it,
 * and what it started, like a pop-up */
static void onClick(struct bar *bar, const struct output *out, guint button, int x, int y)
{
  int index = indexAt(out->layout, x, y);
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
    siginfo_t info;
    memset(&info, 0, sizeof(info));
    /* still running: close it, and what it started; its onExit forgets it once it is gone. Already exited, with its
     * onExit still to come: open it again now (WNOWAIT leaves the process for GLib to reap) */
    if (waitid(P_PID, (id_t)pid, &info, WEXITED | WNOHANG | WNOWAIT) ? errno != ECHILD : !info.si_pid)
    {
      kill(-pid, SIGTERM);
      return;
    }
    g_hash_table_remove(bar->running, link->href);
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

/* Draw the first line of the file. Deleting the file is how doomy is stopped: FALSE then */
static gboolean redraw(struct bar *bar)
{
  gchar *text = NULL;
  if (!g_file_get_contents(bar->path, &text, NULL, NULL))
  {
    fprintf(stderr, "doomy: couldn't read %s, exiting.\n", bar->path);
    g_main_loop_quit(bar->loop);
    return FALSE;
  }
  text[strcspn(text, "\r\n")] = '\0';   /* handle CRLF files too */
  draw(bar, text);
  g_free(text);
  return TRUE;
}

/* The file changed, was replaced (mv) or deleted: draw it at once */
static void onChange(GFileMonitor *monitor, GFile *file, GFile *other, GFileMonitorEvent change, gpointer data)
{
  (void)monitor;
  (void)file;
  (void)other;
  (void)change;
  if (redraw(data))
  {
    onEvents(0, G_IO_IN, data); /* Xlib may have read events while drawing, which the socket no longer shows */
  }
}

/* One monitor's bar: a dock window across the monitor, 1 pixel high until draw() knows how high the text is */
static void openOutput(Display *display, struct output *out, XVisualInfo *vinfo, Colormap colormap)
{
  XSetWindowAttributes wa = {0};
  Atom dock = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DOCK", False);
  PangoFontDescription *font = NULL;
  PangoTabArray *tabs = NULL;

  wa.colormap = colormap;
  out->win = XCreateWindow(display, RootWindow(display, DefaultScreen(display)), out->x, out->y,
    (unsigned int)out->width, 1U, 0, 32, InputOutput, vinfo->visual, CWColormap | CWBorderPixel | CWBackPixel, &wa);
  /* a dock: window managers do not tile it or give it a frame, keep it on every desktop, and keep its strut free */
  XChangeProperty(display, out->win, XInternAtom(display, "_NET_WM_WINDOW_TYPE", False), XA_ATOM, 32,
    PropModeReplace, (unsigned char *)&dock, 1);
  XSelectInput(display, out->win, ButtonPressMask | ExposureMask);
  XMapWindow(display, out->win);

  out->cr = cairo_create(cairo_xlib_surface_create(display, out->win, vinfo->visual, out->width, 1));
  cairo_surface_destroy(cairo_get_target(out->cr)); /* out->cr holds it */
  out->layout = pango_cairo_create_layout(out->cr);
  /* the font unless the markup says otherwise; without one fontconfig may pick a serif */
  font = pango_font_description_from_string("Sans 9");
  pango_layout_set_font_description(out->layout, font);
  pango_font_description_free(font);
  /* always left to right: a line that starts in Arabic or Hebrew would swap the left and the right part */
  pango_context_set_base_dir(pango_layout_get_context(out->layout), PANGO_DIRECTION_LTR);
  pango_layout_set_auto_dir(out->layout, FALSE);
  pango_layout_context_changed(out->layout);
  /* left<TAB>centre<TAB>right: the first tab centres the text after it on the monitor, the second puts it at its right */
  tabs = pango_tab_array_new(2, TRUE);
  pango_tab_array_set_tab(tabs, 0, PANGO_TAB_CENTER, out->width / 2);
  pango_tab_array_set_tab(tabs, 1, PANGO_TAB_RIGHT, out->width);
  pango_layout_set_tabs(out->layout, tabs);
  pango_tab_array_free(tabs);
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
  int i = 0;
  XVisualInfo vinfo;
  Colormap colormap = 0;
  XRRMonitorInfo *monitors = NULL;
  GFile *file = NULL;
  GFileMonitor *monitor = NULL;

  options = g_option_context_new(NULL);
  g_option_context_set_summary(options,
    "Draw the first line of $XDG_RUNTIME_DIR/doomy.txt as a bar, again whenever it changes.\n"
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
  /* a window with an alpha channel: an opaque colour is solid, one with alpha is see-through when a compositor runs */
  if (!XMatchVisualInfo(bar.display, screen, 32, TrueColor, &vinfo))
  {
    fputs("doomy: the X server has no 32 bit visual.\n", stderr);
    return EXIT_FAILURE;
  }
  colormap = XCreateColormap(bar.display, RootWindow(bar.display, screen), vinfo.visual, AllocNone);
  /* one bar for each monitor, from XRandR; the whole screen when it names none.
   * shortcut: the monitors are read once, a monitor plugged in later gets its bar when doomy starts again */
  monitors = XRRGetMonitors(bar.display, RootWindow(bar.display, screen), True, &bar.count);
  if (!monitors || bar.count < 1)
  {
    bar.count = 0;
  }
  bar.outputs = g_new0(struct output, MAX(bar.count, 1));
  for (i = 0; i < MAX(bar.count, 1); i++)
  {
    bar.outputs[i].x = bar.count ? monitors[i].x : 0;
    bar.outputs[i].y = bar.count ? monitors[i].y : 0;
    bar.outputs[i].width = bar.count ? monitors[i].width : DisplayWidth(bar.display, screen);
    bar.outputs[i].height = bar.count ? monitors[i].height : DisplayHeight(bar.display, screen);
    openOutput(bar.display, &bar.outputs[i], &vinfo, colormap);
  }
  bar.count = MAX(bar.count, 1);
  XRRFreeMonitors(monitors);

  bar.links = g_array_new(FALSE, TRUE, sizeof(struct link));
  g_array_set_clear_func(bar.links, clearLink);
  bar.running = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  bar.loop = g_main_loop_new(NULL, FALSE);
  /* no rate limit: a change is drawn as soon as it is written */
  file = g_file_new_for_path(bar.path);
  if (!(monitor = g_file_monitor_file(file, G_FILE_MONITOR_WATCH_MOVES, NULL, &error)))
  {
    fprintf(stderr, "doomy: cannot watch %s: %s\n", bar.path, error->message);
    return EXIT_FAILURE;
  }
  g_file_monitor_set_rate_limit(monitor, 0);
  g_signal_connect(monitor, "changed", G_CALLBACK(onChange), &bar);
  if (redraw(&bar))
  {
    g_unix_fd_add(ConnectionNumber(bar.display), G_IO_IN, onEvents, &bar);
    g_main_loop_run(bar.loop);
  }
  g_object_unref(monitor);
  g_object_unref(file);

  g_array_unref(bar.links);
  g_hash_table_unref(bar.running);
  g_main_loop_unref(bar.loop);
  g_free(bar.path);
  for (i = 0; i < bar.count; i++)
  {
    g_object_unref(bar.outputs[i].layout);
    cairo_destroy(bar.outputs[i].cr);
    XDestroyWindow(bar.display, bar.outputs[i].win);
  }
  g_free(bar.outputs);
  XFreeColormap(bar.display, colormap);
  XCloseDisplay(bar.display);
  return EXIT_SUCCESS;
}
