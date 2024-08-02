# doomy

Dead simple X11 bar for use with scripts in a loop

# Compile

`make -j8` where 8 is your CPU cores.
sudo make install

# Usage

```bash
while true; do
# Your script must use "echo -en" in order to work properly
  ./script.sh # this is your shell script
done | doomy & # will run in the background
```
