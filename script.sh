#!/bin/sh
# An example bar, every second, with a pop-up behind each part: a click opens it, a second click closes it.
# On the left a cow with a fortune; on the right: IPv6, wireless, ethernet, battery,
# free disk space, load, memory, and the volume and the date and time, each an icon and a short value.
# Linux, with iw for the Wi-Fi signal; only the pop-ups need xterm, yad, fortune, cowsay, htop, nmcli, upower, pactl
# and pavucontrol. Run it, then doomy:
#   ./script.sh; doomy &
f="${XDG_RUNTIME_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}}/doomy.txt"

# a terminal pop-up, SIZE+X+Y COMMAND: under the bar, away from the text whose second click closes it; read x keeps it open
term() { printf "xterm -class doomy-popup -geometry %s -e sh -c '%s; read x'" "$1" "$2"; }
# LINK COMMAND TEXT [BUTTON]: button 1 left (the default), 3 right, 4 and 5 the wheel
link() { printf '<a href="%s" button="%s">%s</a>' "$1" "${3:-1}" "$2"; }
# colours, and the separator
good='#00ff00'; bad='#ff0000'
color() { printf '<span foreground="%s">%s</span>' "$1" "$2"; }
sep='<span foreground="#666666"> | </span>'

# the first IPv4 address of an interface
ip4() { ip -4 -br addr show "$1" | awk '{sub("/.*", "", $3); print $3}'; }

# each block is short, an icon and a value; the details are in its pop-up. A red ✗: down
off() { color "$bad" '✗'; }

ipv6() {
  if ip -6 route get 2001:4860:4860::8888 >/dev/null 2>&1; then printf '🌐'; else printf '🌐'; off; fi
}

wireless() {
  # without iw there is no signal to read: show the icon alone, not a ✗ for a card that may well be connected
  command -v iw >/dev/null || { printf '📶'; return; }
  for d in /sys/class/net/*/wireless; do
    i=${d%/wireless}; i=${i##*/}
    # the signal of a connected card, from dBm to a percentage: 2 * (dBm + 100), 0 to 100
    s=$(iw dev "$i" link 2>/dev/null | awk '/signal:/ {q = 2 * ($2 + 100); if (q > 100) q = 100; if (q < 0) q = 0; print q}')
    if [ -n "$s" ]; then printf '📶%s%%' "$s"; return; fi
  done
  printf '📶'; off
}

ethernet() {
  for d in /sys/class/net/*; do
    # the first real wired card that is up: not lo, not wireless, not virtual (no device)
    if [ ! -e "$d/device" ] || [ -e "$d/wireless" ] || [ "$(cat "$d/operstate")" != up ]; then continue; fi
    printf '🔌'; return
  done
  printf '🔌'; off
}

battery() {
  for b in /sys/class/power_supply/BAT*; do
    [ -e "$b/capacity" ] || continue
    if [ "$(cat "$b/status")" = Charging ]; then printf '⚡'; else printf '🔋'; fi
    printf '%s%%' "$(cat "$b/capacity")"; return
  done
}

line() {
  vol=$(pactl get-sink-volume @DEFAULT_SINK@ 2>/dev/null | grep -o '[0-9]*%' | head -1)
  # left, then two tabs: the rest goes to the right
  link "$(term 60x24+0+24 'fortune -s | cowsay')" '🐮'
  printf '\t\t'
  link "$(term 90x12-0+24 'ip -6 addr')" "$(ipv6)"; printf '%s' "$sep"
  link "$(term 100x20-0+24 'ip -br addr; echo; nmcli device wifi list')" "$(wireless)"; printf '%s' "$sep"
  link "$(term 100x16-0+24 'ip -br addr; echo; ip route')" "$(ethernet)"; printf '%s' "$sep"
  bat=$(battery)
  [ -n "$bat" ] && { link "$(term 80x24-0+24 'upower -i $(upower -e | grep -m1 BAT)')" "$bat"; printf '%s' "$sep"; }
  link "$(term 80x16-0+24 'df -h')" "💾$(df -h / | awk 'NR == 2 {print $4}')"; printf '%s' "$sep"
  # the load is the first number uptime prints
  link "xterm -class doomy-popup -geometry 100x30-0+24 -e htop" "⚙️$(uptime | sed 's/.*averages*: //; s/[, ].*//')"; printf '%s' "$sep"
  # memory used: a left click shows free -h, a right click htop sorted by memory
  link "$(term 80x6-0+24 'free -h')" \
    "$(link 'xterm -class doomy-popup -geometry 100x30-0+24 -e htop --sort-key PERCENT_MEM' "🧠$(free -h | awk '/^Mem/ {sub("i$", "", $3); print $3}')" 3)"
  printf '%s' "$sep"
  # the volume: a left click opens the mixer, the wheel turns it up and down
  link pavucontrol "$(link 'pactl set-sink-volume @DEFAULT_SINK@ +5%' "$(link 'pactl set-sink-volume @DEFAULT_SINK@ -5%' "🔊${vol:-?}" 5)" 4)"
  printf '%s' "$sep"
  # the date and time: a left click opens a calendar, a right click the whole year
  link 'yad --calendar --center' "$(link "$(term 66x36-0+24 'cal -y')" "$(date '+%d %b %H:%M')" 3)"
  printf '\n'
}

line > "$f" # once now, so doomy finds the file when it starts
while true; do
  # mv swaps in the whole file at once, so doomy never reads it empty or half written
  line > "$f.tmp"; mv "$f.tmp" "$f"
  sleep 1
done &
