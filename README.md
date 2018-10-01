# Chatty

XMPP and SMS messaging via libpurple and Modemmanager


## Build and install

### Install dependencies

``` bash
sudo apt install git cmake libpurple-dev libmxml-dev libxml2-dev libsqlite3-dev libgcrypt20-dev
```

### Build and install Lurch

``` bash
git clone https://github.com/gkdr/lurch/
cd lurch
git submodule update --init --recursive
make install-home
```

### Build purism-chatty

``` bash
meson build
ninja -C build
```
