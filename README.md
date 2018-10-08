# Chatty

XMPP and SMS messaging via libpurple and Modemmanager


## Build and install

### Install dependencies

``` bash
sudo apt install git meson libgtk-3-dev libpurple-dev libmxml-dev libxml2-dev libsqlite3-dev libgcrypt20-dev
```

### Build and install the OMEMO plugin

Please go to the git page where you'll find all the information on how to build and use the
[libpurple-omemo-plugin](https://github.com/manchito/libpurple-omemo-plugin)

### Build and install libhandy

``` bash
git clone git@source.puri.sm:Librem5/libhandy.git
cd libhandy
meson . build
ninja -C build
ninja -C build install
```

### Build Chatty
``` bash
meson build
ninja -C build
```
