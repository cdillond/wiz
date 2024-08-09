## About
wiz is a command line interface tool for controlling Wiz lights on your local network. It works best if you reserve a static IP address for each device.

To install wiz, clone this repo and run `make wiz` and then `sudo make install`. When not run with the `--broadcast`, `--discover`, or `--ip` flags, wiz reads from a config file, which should be a csv file containing the name, ipv4 address, and room name (in that order) of each Wiz device on your network. See `example.csv` for an example of how this file should be formatted. The location of this config file can be specified by setting the `WIZ_PATH` environment variable. The default location is `$XDG_DATA_HOME/wiz.csv` if `XDG_DATA_HOME` is defined or `~/.local/share/wiz.csv` if it is not.

By default, wiz will send commands to all known devices listed in the config csv file unless the `-b` option is used (in which case it broadcasts the command to all devices on the network), the `-i` option is used (in which case it sends the command to only the provided ipv4 addresses), or the `-n` or `-r` options are used (in which cases it sends the commands only to known devices matching the provided name or room name).

This program is intended to be quick and simple. It doesn't wait for responses to the requests it sends before exiting. You may need to run wiz more than once if a device doesn't respond the first time. Using the `-t` option, you can specify the number of times you would like wiz to repeat the commands it sends.

## Limitations
wiz is not cross-platform; it only works on Linux. There's also no ipv6 support yet, but it might be coming soon.