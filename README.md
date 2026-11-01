# doomy

![doomy](https://raw.githubusercontent.com/su8/hellxcb/refs/heads/master/hellxcb.png)

Dead simple X11 bar. Every second it draws one line of text that your script writes to a file.

The bar in the picture above runs on [hellxcb](https://github.com/su8/hellxcb), with its text from [pinky-bar](https://github.com/su8/pinky-bar).

## Dependencies

A C compiler, `make`, `pkg-config`, `libX11`, `cairo`, `pango` (1.50 or newer) and `glib`, which comes with pango.

Debian:

```sh
apt install gcc make pkg-config libx11-dev libcairo2-dev libpango1.0-dev
```

Fedora:

```sh
dnf install gcc make pkgconf libX11-devel cairo-devel pango-devel
```

FreeBSD:

```sh
pkg install pkgconf libX11 cairo pango
```

## Installation

```sh
make                  # the same Makefile on Linux and BSD
sudo make install     # to /usr/local/bin; PREFIX=/usr and DESTDIR=... work too
sudo make uninstall   # to remove it
```

Put these lines in `~/.xinitrc`, before the line that starts your window manager:

```sh
f="${XDG_RUNTIME_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}}/doomy.txt"
your_script.sh > "$f"   # once now, so doomy finds the file when it starts
while true; do
  # mv swaps in the whole file at once, so doomy never reads it empty or half written
  your_script.sh > "$f.tmp"; mv "$f.tmp" "$f"
  sleep 1
done &
doomy &
```

`script.sh` in this repository holds the same loop.

## Usage

doomy reads the first line of `$XDG_RUNTIME_DIR/doomy.txt`, a folder only you can write to (`$XDG_CACHE_HOME/doomy.txt`, or `~/.cache/doomy.txt`, when it is not set).

- The bar is as high as the text (about 17 pixels with `Sans 9`) and as wide as the screen.
- An empty file gives an empty bar.
- Deleting the file stops doomy.

| Option | Default | What it does |
|---|---|---|
| `--background=COLOR` | `#282a2e` | a [Pango colour](https://docs.gtk.org/Pango/method.Color.parse_with_alpha.html): a name like `red`, `#rrggbb`, or `#rrggbbaa` to see through it |
| `--bottom` | | at the bottom of the screen, not the top |
| `--help` | | the options |

The font, the text colour and everything else come from the markup. doomy exits with a message on a colour or option it does not know.

### Markup

The line is [Pango markup](https://docs.gtk.org/Pango/pango_markup.html): colours, fonts, sizes, bold and more, for any part of the text. Without markup it is `Sans 9` in `#b294bb`:

```sh
echo 'Hello <span foreground="red">World</span> <span font="Hack 12">big</span> <b>bold</b>' > "${XDG_RUNTIME_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}}/doomy.txt"
```

Write `&amp;`, `&lt;` and `&gt;` for `&`, `<` and `>`. When the markup is broken, the bar shows what is wrong with it.

One font and size for the whole bar, with some room above and below the text:

```sh
echo '<span font="Hack 10" line_height="1.4">Battery: 10%</span>' > "${XDG_RUNTIME_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}}/doomy.txt"
```

### Left, centre and right

Tabs split the line: the text after the first tab is centred in the bar, the text after the second is at the right.
`printf` writes a tab as `\t`; a plain `echo` does not.

| Line | Left | Centre | Right |
|---|---|---|---|
| `left\tcentre\tright` | `left` | `centre` | `right` |
| `left\t\tright` | `left` | | `right` |
| `\tcentre` | | `centre` | |
| `\t\tright` | | | `right` |
| `left\tright` | `left` | `right` | |

The last one is a common slip: with one tab, the text goes to the centre. Use two tabs for left and right only.

```sh
printf 'Battery: 10%%\t5:30pm\tMonday\n' > "${XDG_RUNTIME_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}}/doomy.txt"   # left, centre, right
printf 'Battery: 10%%\t\tMonday\n' > "${XDG_RUNTIME_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}}/doomy.txt"         # left and right
```

### Emojis

Characters the font does not have, such as emojis or Chinese, are drawn with an installed font that has them, colour emoji fonts included:

```sh
echo '🔋 85% 🎵 music 🌡 45°C' > "${XDG_RUNTIME_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}}/doomy.txt"
```

### Transparency

A `--background` with alpha lets the windows and the wallpaper behind the bar show through.
It needs a compositor such as [picom](https://github.com/yshui/picom) or xcompmgr; without one, the see-through part is black.

```sh
doomy --background '#282a2eb3'   # b3 is 70% opaque
```

### With hellxcb

[hellxcb](https://github.com/su8/hellxcb) writes its desktop, mode and window counts to `$XDG_RUNTIME_DIR/hellxcb.txt`.
Its README shows a loop that puts that line into doomy's file.

## Testing

The smoke test runs doomy on a virtual X server and needs `Xvfb`:

```sh
make check                     # run the smoke test
SMOKE_DISPLAY=:98 make check   # use another display
```
