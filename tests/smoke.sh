#!/bin/sh
# smoke test: run doomy on a virtual X server with every kind of input and option,
# and check how it ends. Needs: Xvfb, xdotool and xprop
set -u
BIN=${1:-./doomy}
export DISPLAY=${SMOKE_DISPLAY:-:99} # another display: SMOKE_DISPLAY=:98 make check
export XDG_RUNTIME_DIR=$(mktemp -d) # a doomy.txt of our own
F=$XDG_RUNTIME_DIR/doomy.txt
Xvfb "$DISPLAY" -screen 0 800x600x24 >/dev/null 2>&1 & XPID=$!
trap 'kill $XPID 2>/dev/null; rm -rf "$XDG_RUNTIME_DIR"' EXIT
# wait for Xvfb's socket, up to 5 seconds: a fixed sleep fails on a slow machine
i=0; until [ -e "/tmp/.X11-unix/X${DISPLAY#:}" ]; do
  i=$((i + 1)); [ $i -gt 50 ] && { echo "FAIL Xvfb did not start on $DISPLAY"; exit 1; }
  sleep 0.1
done
fail=0

# expect NAME GOT WANT: pass if the two values are equal
expect() { if [ "$2" = "$3" ]; then echo "ok   $1"; else echo "FAIL $1 (got $2, want $3)"; fail=1; fi; }
# alive NAME [OPTION]...: pass if doomy is still drawing $F 2 seconds later (timeout's 124)
alive() { name=$1; shift; timeout 2 "$BIN" "$@" 2>/dev/null; expect "$name" $? 124; }

printf '<span foreground="red">red</span> <b>bold</b> <span font="Serif 20">big</span> 🔋 日本\n' > "$F"
alive "markup, a bigger font, emojis and CJK"
printf 'Battery: 10%%\t5:30pm 🔋\tMonday\t\textra tabs\n' > "$F";    alive "left, centre and right tabs, and too many"
printf 'مرحبا\tCCC\tRRR\n' > "$F";                                    alive "a line that starts in Arabic"
echo 'a <b>broken' > "$F";                                          alive "broken markup shows its error"
awk 'BEGIN { while (i++ < 5000) printf "x"; print "" }' > "$F";     alive "a long line"
printf 'first\nsecond\n' > "$F";                                    alive "only the first line"
: > "$F";                                                           alive "an empty file"
echo text > "$F"
alive "see-through background" --background '#282a2e80'
alive "at the bottom" --bottom --background red

# links: a left click on <a href="COMMAND">text</a> runs COMMAND in a shell, a second click on it closes it and
# what it started ('sleep; true' keeps the shell waiting, like a script); text that is no link runs nothing
C=$XDG_RUNTIME_DIR/clicked
printf 'Battery\t<a href="touch %s.a &amp;&amp; touch %s.b"><b>5:30pm</b></a>\t<a href="sleep 31337; true">Monday</a>\n' "$C" "$C" > "$F"
"$BIN" 2>/dev/null & P=$!; sleep 1.5
running() { pgrep -f '^sleep 31337' >/dev/null && echo running || echo stopped; }
xdotool mousemove 5 5 click 1; sleep 0.3;   expect "a click on text that is no link runs nothing" "$(ls "$XDG_RUNTIME_DIR" | grep -c clicked)" 0
xdotool mousemove 200 5 click 1; sleep 0.3; expect "a click on a gap runs nothing" "$(ls "$XDG_RUNTIME_DIR" | grep -c clicked)" 0
xdotool mousemove 400 5 click 1; sleep 0.3; expect "a click on a link runs its command, &amp;&amp; as &&" "$(ls "$XDG_RUNTIME_DIR" | grep -c clicked)" 2
xdotool mousemove 795 5 click 1; sleep 0.3; expect "a click on a link opens its command" "$(running)" running
xdotool mousemove 795 5 click 1; sleep 0.3; expect "a second click closes it, the shell's child too" "$(running)" stopped
xdotool mousemove 795 5 click 1; sleep 0.3; expect "a third click opens it again" "$(running)" running
kill $P; wait $P 2>/dev/null; pkill -f '^sleep 31337'
# where a link is counts the text as Pango draws it: 8 &amp; are 8 bytes, not 40, or the link falls past the text
printf '&amp;&amp;&amp;&amp;&amp;&amp;&amp;&amp;<a href="touch %s.entity">LINKLINKLINK</a>\n' "$C" > "$F"
"$BIN" 2>/dev/null & P=$!; sleep 1.5
xdotool mousemove 100 5 click 1; sleep 0.3; expect "a link after entities is where it is drawn" "$([ -e "$C.entity" ] && echo ran)" ran
kill $P; wait $P 2>/dev/null

# button="N": links in links, one for each mouse button, on the same text
printf '<a href="touch %s.1"><a button="3" href="touch %s.3"><a button="4" href="touch %s.4">Volume</a></a></a>\n' "$C" "$C" "$C" > "$F"
"$BIN" 2>/dev/null & P=$!; sleep 1.5
for b in 1 3 4 2; do xdotool mousemove 10 5 click $b; sleep 0.3; done
expect "left, right and wheel up run their own link, the middle button none" "$(cd "$XDG_RUNTIME_DIR" && ls clicked.[1-5] | tr '\n' ' ')" "clicked.1 clicked.3 clicked.4 "
kill $P; wait $P 2>/dev/null

# a pop-up that takes a second to close (a trap on TERM), clicked again while it closes: its late exit must not make
# doomy forget a newer one, or the next click opens another next to it instead of closing it. That click closes the one
# that is open, or opens one
printf '<a href="trap &quot;sleep 1; exit&quot; TERM; sleep 31337">xxxxxxxxxxxx</a>\n' > "$F"
"$BIN" 2>/dev/null & P=$!; sleep 1.5
xdotool mousemove 5 5 click 1; sleep 0.5
xdotool click --repeat 2 --delay 100 1; sleep 1.5
n=$(pgrep -fc '^sleep 31337'); xdotool click 1; sleep 0.5
expect "after a close and an open at once, a click closes or opens, never a second pop-up" "$n $(pgrep -fc '^sleep 31337')" "$n $((1 - n))"
kill $P; wait $P 2>/dev/null; pkill -f '^sleep 31337'

echo '<a>no href</a>' > "$F";                                  alive "a link without href shows its error"
echo '<a href="x"><a href="y">nested</a></a>' > "$F";          alive "a link in a link with the same button shows its error"
echo '<a href="x" button="9">nine</a>' > "$F";                 alive "button=\"9\" shows its error"

# a dock window with a strut as high as the bar, at the top or the bottom: window managers keep that strip free
dock() { # OPTION...: the window type and the strut's top and bottom
  "$BIN" "$@" 2>/dev/null & P=$!; sleep 1.5
  w=$(xdotool search --name '' 2>/dev/null | while read -r id; do xprop -id "$id" _NET_WM_WINDOW_TYPE 2>/dev/null | grep -q DOCK && echo "$id"; done | head -1)
  h=$(xdotool getwindowgeometry --shell "$w" | sed -n 's/^HEIGHT=//p')
  echo "$(xprop -id "$w" _NET_WM_WINDOW_TYPE | sed 's/.*= //') $(xprop -id "$w" _NET_WM_STRUT_PARTIAL | cut -d, -f3,4 | tr -d ' ' | sed "s/$h/H/g")"
  kill $P; wait $P 2>/dev/null
}
echo text > "$F"
expect "a dock, its strut at the top as high as the bar" "$(dock)" "_NET_WM_WINDOW_TYPE_DOCK H,0"
expect "with --bottom, its strut at the bottom" "$(dock --bottom)" "_NET_WM_WINDOW_TYPE_DOCK 0,H"

# deleting the file is how doomy is stopped
timeout 5 "$BIN" 2>/dev/null & P=$!; sleep 1.5; rm "$F"; wait $P
expect "deleting doomy.txt exits cleanly" $? 0

echo text > "$F"
for bad in "--background nosuch" "-b red" "--alpha 50" "extra"; do
  timeout 2 "$BIN" $bad 2>/dev/null
  expect "rejects $bad" $? 1
done

exit $fail
