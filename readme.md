## About
wiz is a command line interface tool for controlling Wiz lights on your local network. It works best if you reserve a static IP address for each device.

To install wiz, update `wiz.h` to define the `WIZ_PATH` constant and run `make wiz`. The value of the `WIZ_PATH` constant should be the path of a csv file containing the name, ipv4 address, and room name (in that order) of each Wiz device on your network. See `example.csv` for an example of how this file should be formatted. By default, wiz will send commands to all known devices listed in the csv file found at `WIZ_PATH` unless the `-b` option is used (in which case it broadcasts the command to all devices on the network), the `-i` option is used (in which case it sends the command to only the provided ipv4 addresses), or the `-n` or `-r` options are used (in which cases it sends the commands only to known devices matching the provided name or room name).

## Limitations
wiz is not cross-platform; it only works on Linux. There's also no ipv6 support yet, but it might be coming soon.