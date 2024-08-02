# doomy

Dead simple X11 bar for use with scripts in a loop

# Compile

```
make BG_COLOR="#282a2e" TEXT_COLOR=""#b294bb"" -j8 # where 8 are your CPU cores.

sudo make install
```

# Usage

```bash
while true; do
# Your script must use "echo -en" in order to work properly
# doomy sleeps for 1 second every cicle
  ./script.sh # this is your shell script
done | doomy & # will run in the background
```
