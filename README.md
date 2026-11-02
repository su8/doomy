# doomy

![doomy](https://raw.githubusercontent.com/su8/hellxcb/refs/heads/master/hellxcb.png)

Dead simple X11 bar. It draws one line of text that your script writes to a file, again as soon as the file changes.

The bar in the picture above runs on [hellxcb](https://github.com/su8/hellxcb), with its text from [pinky-bar](https://github.com/su8/pinky-bar).

## Dependencies

A C compiler, `make`, `pkg-config`, `libX11`, `cairo`, `pango` (1.50 or newer) and `glib` with `gio`, which come with pango.

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

`script.sh` in this repository holds the same loop, with a full bar to start from: see [Links](#links).

## Usage

doomy reads the first line of `$XDG_RUNTIME_DIR/doomy.txt`, a folder only you can write to (`$XDG_CACHE_HOME/doomy.txt`, or `~/.cache/doomy.txt`, when it is not set).

- The bar is as high as the text (about 17 pixels with `Sans 9`) and as wide as the screen.
- It is a dock window: window managers keep its strip free (its strut) and do not tile it.
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

### Links

`<a href="COMMAND">text</a>`, the link tag of GTK's labels, makes text clickable. A left click on it runs COMMAND in a shell; a second click closes it and everything it started, so a window opens and closes like a pop-up. Closing the window yourself works too, and the next click opens it again. Markup works inside a link, and a click anywhere else does nothing.

```sh
echo '<a href="yad --calendar --center">Sun 01 Nov 17:33</a>' > "${XDG_RUNTIME_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}}/doomy.txt"
```

`button="N"` makes a link answer another mouse button; links in links give one text a command for each button:

| `button` | Mouse button |
|---|---|
| `1`, or none | left |
| `2` | middle |
| `3` | right |
| `4` | wheel up |
| `5` | wheel down |

```sh
# a left click opens the mixer, the wheel turns the volume up and down
echo '<a href="pavucontrol"><a button="4" href="pactl set-sink-volume @DEFAULT_SINK@ +5%"><a button="5" href="pactl set-sink-volume @DEFAULT_SINK@ -5%">🔊 40%</a></a></a>' > "${XDG_RUNTIME_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}}/doomy.txt"
```

- Unlike in GTK, `href` is a command, not a web address.
- In `href` too, write `&amp;` for `&` (so `&amp;&amp;` for `&&`) and `&lt;` for `<`; use single quotes inside it, or `&quot;`.
- **Escape text that comes from outside**, such as a song or window title: the line is markup, so a title like `</a><a href="rm -rf ~">Play` would put that command on the bar. `sed 's/&/\&amp;/g; s/</\&lt;/g; s/>/\&gt;/g'` does it.
- Open the pop-up away from the link's text: on top of it, the second click lands on the pop-up instead.
- The second click closes what the first one started. A command that hands over to a program already running and ends, such as `firefox URL` with Firefox open, or one ending in `&`, has nothing left to close: the next click runs it again.
- A link in a link needs another button; an `<a>` without `href`, or a `button` that is not 1 to 5, shows its error in the bar.

`script.sh` in this repository is a bar to start from, with a pop-up behind each part:

```
🐮                                                    🌐✗ | 📶80% | 🔌✗ | 🔋41% | 💾792G | ⚙️1.18 | 🧠9.4G | 🔊40% | 01 Nov 18:12
```

| Part | Shows | A click opens | Other buttons |
|---|---|---|---|
| 🐮 | | a fortune from `cowsay` | |
| 🌐 | IPv6, a red ✗ without | `ip -6 addr` | |
| 📶 | the Wi-Fi signal | the networks, `nmcli device wifi list` | |
| 🔌 | wired, a red ✗ when down | `ip -br addr` and `ip route` | |
| 🔋 | the battery, ⚡ while charging; nothing on a desktop | `upower -i` | |
| 💾 | free space on `/` | `df -h` | |
| ⚙️ | the load | `htop` | |
| 🧠 | memory used | `free -h` | right: `htop` by memory |
| 🔊 | the volume | the mixer, `pavucontrol` | wheel: louder, quieter |
| the time | `01 Nov 18:12` | a yad calendar | right: the whole year, `cal -y` |

```sh
./script.sh
doomy &
```

It is for Linux. The 📶 signal needs `iw`; without it, the icon shows alone. The pop-ups need the programs in the table.

Any program works as a pop-up. These are not needed for doomy, only for the links that run them:

| Pop-up | Command |
|---|---|
| a calendar, GTK 3 | `yad --calendar --center`; `--posx` and `--posy` place it |
| a calendar, GTK 4 | `zenity --calendar`; the window manager places it |
| a terminal | `xterm -class doomy-popup -geometry 66x9+0+24 -e sh -c 'cal -3; read x'`; `read x` keeps it open |
| a program | `pavucontrol`, `htop` in a terminal, ... |

Whether a pop-up floats over the other windows is up to the window manager. yad and zenity are dialogs, which most window managers float. For a terminal, give it a class, like `-class doomy-popup` above, and make that class float:

- [hellxcb](https://github.com/su8/hellxcb), in `rules` in `config.h`: `{ "doomy-popup", -1, False, True },`
- i3: `for_window [class="doomy-popup"] floating enable`

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

The smoke test runs doomy on a virtual X server and needs `Xvfb`, `xdotool`, which clicks the bar, and `xprop`:

```sh
make check                     # run the smoke test
SMOKE_DISPLAY=:98 make check   # use another display
```
