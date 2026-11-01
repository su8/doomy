#!/bin/sh
# smoke test: run doomy on a virtual X server with every kind of input and option,
# and check how it ends. Needs: Xvfb
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

# deleting the file is how doomy is stopped
timeout 5 "$BIN" 2>/dev/null & P=$!; sleep 1.5; rm "$F"; wait $P
expect "deleting doomy.txt exits cleanly" $? 0

echo text > "$F"
for bad in "--background nosuch" "-b red" "--alpha 50" "extra"; do
  timeout 2 "$BIN" $bad 2>/dev/null
  expect "rejects $bad" $? 1
done

exit $fail
