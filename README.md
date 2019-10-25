# MonaServer2
Temporary project planned to replace [MonaServer](https://github.com/MonaSolutions/MonaServer).

The project MonaTiny is now ready for Linux and Windows, it is a very lightweight software without LUA support.
The MonaServer project is not compiling for now because we are concentrated on the protocols.

# Windows Binary

A [Windows 32-bit](https://sourceforge.net/projects/monaserver/files/MonaTiny/MonaTiny_Win32.zip/download) and a [ Windows 64-bit](https://sourceforge.net/projects/monaserver/files/MonaTiny/MonaTiny_Win64.zip/download) zipped packages are provided to quickly test *MonaTiny*.
We recommend you to clone the github version from the sources for production use.

In order to use it you need to install the [C++ vc_redist.x86 Visual Studio 2015 package](https://www.microsoft.com/it-it/download/details.aspx?id=48145).

# Fast compilation

You can compile *MonaTiny* using the following procedures.

## Windows

On Windows you have to install [Microsoft Visual Studio Express 2015 for Windows Desktop](https://my.visualstudio.com/Downloads?q=visual%20studio%202015&wt.mc_id=o~msft~vscom~older-downloads).

Then clone this repository, open the **Mona.sln** project file, right clic on "MonaTiny" then clic on "Build".

You can then start MonaTiny (CTRL+F5 or F5 in debug mode) or MonaServer.

## POSIX/Unix-type OS

In general, you can checkout and compile everything like this:

```
    git clone https://github.com/MonaSolutions/MonaServer2.git
    cd MonaServer2
    make
```

You can the run `MonaTiny` & `MonaServer` executables.

### GNU/Linux

In general here following are our system prerequisites:
 - g++ 5 (or compliant clang ...)
 - OpenSSL lib (with headers) (e.g dev-packages `libssl-dev` / `openssl-devel`)

#### Debian/Ubuntu setup steps (assumed prerequisites above are fulfilled)

1. `apt install libssl-dev`
2. Install LuaJIT as explained

#### Other distros

TODO

#### Issues looking up dynamic libraries when running

If when trying to run the executables, you hit errors about not found "shared objects" or "dylib" files, make sure: 
1. To call the executable directly from the folder it has been built to
2. That the built so/dylib files are to be found at the path the executable looks at

#### LuaJIT dependency

You will need to build/install manually a version of [LuaJIT](https://luajit.org) (preferrably 2.1.0, but the previous version 2.0.5 will also work). Find instructions for this further below (same as for macOS) or [here](https://luajit.org/download.html) and [here](https://luajit.org/install.html).

### macOSX

On OSX, you should have installed: 

 - G++-Clang v11.0.0+ (always gets installed with XCode CLI tools)
 - OpenSSL v1.1.x+ (found on Homebrew)
 - LuaJIT v2.0.x (found on Homebrew) OR v2.1.x (you can have both installed side-by-side, our build will choose the latter)

#### Important: Set environment vars for lib/include-paths

```
export LIBS=-L/usr/local/opt/openssl/lib
export INCLUDES=-I/usr/local/opt/openssl/include
```

#### Build & Install LuaJIT 2.1.0 from source:

You can install LuaJIT from source like so:

```
    wget http://luajit.org/download/LuaJIT-2.1.0-beta3.tar.gz
    tar zxf LuaJIT-2.1.0-beta3.tar.gz
    cd LuaJIT-2.1.0-beta3
    make && sudo make install
```

Depending on the macOSX version, you might hit this issue: https://github.com/LuaJIT/LuaJIT/issues/484

### Android

TODO

# Current state

## Operating systems

Feature                                      | State
---------------------------------------------|---------------------
Windows                                      | OK
Linux                                        | OK
BSD/macOSX                                   | OK
Android                                      | In progress

## Protocols

Feature                                      | State
---------------------------------------------|---------------------
RTMP(E)                                      | OK
RTMFP                                        | OK
HTTP/HTTPS                                   | OK
Websocket/Websocket SSL                      | OK
RTSP                                         | NOK

## Other Features

Feature                                      | State
---------------------------------------------|---------------------
Recording                                    | OK
Congestion control                           | OK
Multi-servers                                | NOK
LUA (MonaServer)                             | NOK
cache system                                 | NOK
VOD                                          | In progress

# Community

The [MonaServer1 forum](https://groups.google.com/forum/#!forum/monaserver) is always here if you have questions or something to share regarding live streaming with MonaServer.

If you find a problem with MonaServer2 please create an issue in the [Github Issues page](https://github.com/MonaSolutions/MonaServer2/issues).

You can also discuss in real-time with fans of MonaServer [on Gitter](https://gitter.im/MonaServer).

