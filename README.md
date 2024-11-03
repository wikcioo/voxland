# Getting started

## Clone the repository
```shell
git clone https://github.com/wikcioo/voxland.git
cd voxland
```

## Linux

### Install dependencies

**Debian**
```shell
$ sudo apt-get install g++ make libglfw3-dev libglew-dev libglm-dev libfreetype6-dev libsqlite3-dev
```

**Arch**
```shell
$ sudo pacman -S make glfw glew freetype2 sqlite
```

### Build the project
```shell
$ make -j config=[debug,release]
```

### Start the server
```shell
$ ./build/[debug,release]/server/server -p <port>
```

### Start the client
```shell
$ ./build/[debug,release]/client/client -ip <ip address> -p <port> --username <username> --password <password>
```
