## About
wiz is a command line interface tool for controlling Wiz lights on your local network. It works best if you reserve a static IP address of each device.

## Installation
Update `wiz.h` to define the `WIZ_PATH` constant. The value of this constant should be the path of a csv file containing the name, ipv4 address, and room name (in that order) of each Wiz device on your network.

## Limitations
wiz is not cross-platform; it only works on Linux. There's also no IPV6 support yet, but it might be coming soon.