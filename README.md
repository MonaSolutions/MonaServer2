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

## Unix-type OS

In general, you can checkout and compile MonaTiny like this:

```
    git clone https://github.com/MonaSolutions/MonaServer2.git
    cd MonaServer2
    make
```

### Linux

On linux there are 2 prerequisites :
 - g++ 5,
 - and Openssl with headers (usually libssl-dev openssl-devel).

### BSD/macOSX

On OSX, you should have installed: 

 - G++-Clang v11.0.0+ (always gets installed with XCode CLI tools)
 - OpenSSL v1.1.x+ (found on Homebrew)
 - LuaJIT v2.0.x (found on Homebrew) OR v2.1.x (you can have both installed side-by-side, our build will choose the latter)

#### Build & Install LuaJIT 2.1.0 from source:

You can install LuaJIT from source like so:

```
wget http://luajit.org/download/LuaJIT-2.1.0-beta3.tar.gz
   tar zxf LuaJIT-2.1.0-beta3.tar.gz
cd LuaJIT-2.1.0-beta3
   make && sudo make install
````

You might hit this issue: https://github.com/LuaJIT/LuaJIT/issues/484

### Android

TODO

## Windows

On Windows you have to install [Microsoft Visual Studio Express 2015 for Windows Desktop](https://my.visualstudio.com/Downloads?q=visual%20studio%202015&wt.mc_id=o~msft~vscom~older-downloads).

Then clone this repository, open the **Mona.sln** project file, right clic on "MonaTiny" then clic on "Build".

You can then start MonaTiny (CTRL+F5 or F5 in debug mode).

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

