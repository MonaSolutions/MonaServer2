# MonaServer2
Temporary project planned to replace MonaServer

The project MonaTiny is now ready for Linux and Windows, it is a very lightweight software without lua support.
The MonaServer project is not compiling for now because we are concentrated on the protocols.

# Windows Binary

A [32-bit Windows zipped package](https://sourceforge.net/projects/monaserver/files/MonaTiny/MonaTiny.zip/download) is provided to quickly test *MonaTiny*.
We recommend you to clone the github version from the sources for production use.

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

# Communauty

[MonaServer1 forum](https://groups.google.com/forum/#!forum/monaserver)

[Chat to discuss in real-time around Mona](https://gitter.im/MonaServer)

