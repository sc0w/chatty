# Chatty

XMPP and SMS messaging via libpurple and Modemmanager


## Build and install

### Install dependencies

``` bash
sudo apt install git meson libgtk-3-dev libpurple-dev modemmanager libmxml-dev libxml2-dev libsqlite3-dev libgcrypt20-dev libhandy-0.0-dev
```

Libhandy (libhandy-0.0-dev) is available in [PureOS][0] and  Debian's
[experimental distribution][1].  If you don't want to fetch it from there you
can [build it from souce][2].

### Build and install the SMS plugin
``` bash
git clone git@source.puri.sm:Librem5/purple-mm-sms.git
make
make install
```

### Build and install the OMEMO plugin
Please go to the git page where you'll find all the information on how to build and use the
[libpurple-omemo-plugin](https://github.com/manchito/libpurple-omemo-plugin)

### Build Chatty
``` bash
meson build
ninja -C build
```

[0]: http://software.pureos.net/search_pkg?term=libhandy-0.0-dev
[1]: https://packages.debian.org/search?keywords=libhandy-0.0-dev
[2]: https://source.puri.sm/Librem5/libhandy
