# MonaServer2
Temporary project planned to replace [MonaServer](https://github.com/MonaSolutions/MonaServer).

The project MonaTiny is now ready for Linux and Windows, it is a very lightweight software without LUA support.
The MonaServer project is not compiling for now because we are concentrated on the protocols.

# Windows Binary

A [Windows 32-bit](https://sourceforge.net/projects/monaserver/files/MonaTiny/MonaTiny_1.106_Win32.zip/download) and a [ Windows 64-bit](https://sourceforge.net/projects/monaserver/files/MonaTiny/MonaTiny_1.106_Win64.zip/download) zipped packages are provided to quickly test *MonaTiny*.
We recommend you to clone the github version from the sources for production use.

In order to use it you need to install the [C++ vc_redist.x86 Visual Studio 2015 package](https://www.microsoft.com/it-it/download/details.aspx?id=48145).

# Fast compilation

You can compile *MonaTiny* using the following procedures.

## Linux

On linux there are 2 prerequisites :
 - g++ 5,
 - and Openssl with headers (usually libssl-dev openssl-devel).

Then you can checkout and compile MonaTiny :

    git checkout https://github.com/MonaSolutions/MonaServer2.git
    cd MonaServer2
    make

## Windows

On Windows you have to install [Microsoft Visual Studio Express 2015 for Windows Desktop](https://www.visualstudio.com/fr/post-download-vs/?sku=xdesk&clcid=0x409&telem=ga).

Then checkout this repository, open the **Mona.sln** project file, right clic on "MonaTiny" then clic on "Build".

You can then start MonaTiny (CTRL+F5 or F5 in debug mode).

# Current state

## Operating systems

Feature                                      | State
---------------------------------------------|---------------------
Windows                                      | OK
Linux                                        | OK
Android                                      | In progress
BSD/OS X                                     | In progress

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
Multi-servers		                              | NOK
LUA (MonaServer)                             | NOK
cache system                                 | NOK
VOD                                          | In progress

# Community

The [MonaServer1 forum](https://groups.google.com/forum/#!forum/monaserver) is always here if you have questions or something to share regarding live streaming with MonaServer.

If you find a problem with MonaServer2 please create an issue in the [Github Issues page](https://github.com/MonaSolutions/MonaServer2/issues).

You can also discuss in real-time with fans of MonaServer [on Gitter](https://gitter.im/MonaServer).

