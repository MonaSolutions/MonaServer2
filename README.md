[Unix setup](#Unix-setup) | [Windows setup](#Windows-setup) | [macOS setup](#macOS-setup) | [MonaTiny](#MonaTiny) | [Docker](#Docker) | [Documentation](#Documentation) | [About](#About)

# MonaServer

*Temporary project planned to replace [MonaServer](https://github.com/MonaSolutions/MonaServer).*

MonaServer is a lightweight web and media server customizable by [LUA](https://www.lua.org/) script applications.
MonaServer supports currently following protocols:
- HTTP(TLS) with a rendez-vous extension (cmd *RDV*) to meet peers together (e.g. SDP exchange for WebRTC)
- WebSocket(TLS)
- STUN
- RTMP(E)
- RTMFP
- SRT
- RTP, RTSP *(in progress...)*

MonaServer supports advanced features for the following media containers:
- MP4
- FLV, F4V
- TS
- HLS
- ADTS, AAC, MP3
- SubRip, VTT
- Mona

Start MonaServer developments by [reading the documentation](#Documentation)

## Unix setup

### Requirements
- [g++](https://gcc.gnu.org/) version >=5 (or compliant clang...)
- [OpenSSL](https://www.openssl.org/) libraries with headers, usually dev-package named `libssl-dev` or `openssl-devel`
- [LuaJIT](http://luajit.org/) library with headers, dev-package named libluajit-5.1-dev or prefer luajit 2.1 beta by building from sources:
```
git clone https://github.com/LuaJIT/LuaJIT.git
cd LuaJIT
make && sudo make install
```

- To enable [SRT Protocol] install [SRT]

### Build
[Download] or [clone] MonaServer sources and compile everything simply with make:
```
git clone https://github.com/MonaSolutions/MonaServer2.git
cd MonaServer2
make
```
To enable [SRT Protocol] define *ENABLE_SRT* variable
```
make ENABLE_SRT=1
```

### Start
Start executable :
```
cd MonaServer
./MonaServer
```

## Windows setup

### Binaries
A [Windows 32-bit](https://sourceforge.net/projects/monaserver/files/MonaServer/MonaServer_Win32.zip/download) and a [ Windows 64-bit](https://sourceforge.net/projects/monaserver/files/MonaServer/MonaServer_Win64.zip/download) zipped packages are provided to test MonaServer. However we recommend to build the github version from the sources for production use.

### Requirements
- [Microsoft Visual Studio Community 2019 for Windows Desktop](https://visualstudio.microsoft.com/thank-you-downloading-visual-studio/?sku=Community&rel=16)
- To enable [SRT Protocol] download and build [SRT], put the header files in *External/include* and the libraries in *External/lib* and *External/lib64* (with a *d* sufix for the debug librairies):
```
./External/lib>dir                 ./External/lib>dir
    srt/srt.h                                   pthread_lib.lib
    srt/logging_api.h                           pthread_libd.lib
    srt/platform_sys.h                          srt_static.lib
    srt/srt4udt.h                               srt_staticd.lib
    srt/version.h
    win/syslog_defs.h
```

***Note:*** [OpenSSL](https://www.openssl.org/) and [LuaJIT](http://luajit.org/) dependencies are already included in the project.

### Build
[Download] or [clone] MonaServer sources, open *Mona.sln* project file with Visual Studio, right clic on *MonaServer* project and clic on *Build*.

### Start
Start MonaServer by selecting *MonaServer* project and pressing *F5*.

## macOS setup

### Requirements
- [XCode](https://apps.apple.com/us/app/xcode/id497799835) version >=8 and Xcode dev Tools which you can install using the following command :
```
xcode-select --install
```
- [OpenSSL](https://www.openssl.org/) libraries with headers. The most simple way to install them is to use [Homebrew](https://brew.sh/index_it):

First, install homebrew :
```
/usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
```
Then install openssl :
```
brew install openssl
```
- [LuaJIT](http://luajit.org/) library with headers :
```
brew install luajit
```
Or prefer luajit 2.1 beta by building from sources :
```
git clone https://github.com/LuaJIT/LuaJIT.git
cd LuaJIT
make MACOSX_DEPLOYMENT_TARGET=10.14 && sudo make install
```
***Note:*** Mac-OSX target require a MACOSX_DEPLOYMENT_TARGET definition with *make MACOSX_DEPLOYMENT_TARGET=10.X* where (10.X is your macOS version), see [this LuaJIT issue](https://github.com/LuaJIT/LuaJIT/issues/484) for more details.

- To enable [SRT Protocol] install [SRT] :
```
brew install srt
```

### Build & Start
See [Unix setup](#Unix-setup) Build and Start chapters

## MonaTiny
MonaTiny is a version of MonaServer without LUA script applications.
Setup is identical excepting that there is no LuaJIT dependency, just start executable file *./MonaTiny/MonaTiny*

## Docker

A Docker image is available on [Docker Hub](https://hub.docker.com/) with the name `monaserver/monaserver`.
See more at https://github.com/MonaSolutions/docker-mona

To run monaserver in Docker use the following command :
```
docker run --rm -it --name=mona -p 80:80 -p 443:443 -p 1935:1935 -p 1935:1935/udp monaserver/monaserver
```

## Documentation
- [General (in progress...)](https://docs.google.com/document/d/1SS_mdvzDxkKZ7M1264BFGPMYXPjaV3yGUqqCzJEDs4g/edit?usp=sharing)
- [Configuration](https://github.com/MonaSolutions/MonaServer2/blob/master/MonaServer/MonaServer.ini)
- [Script application (in progress...)](https://docs.google.com/document/d/1CwgbPv8iIgIC8Z_hYEVa8eSzRUSz1O8EpH6ZzYq7YxA/edit?usp=sharing)
- [Media streaming(in progress...)](https://docs.google.com/document/d/1cFhsGHtlALM3AajktEhVzXzfn5rfa5_e7djUYWawsrM/edit?usp=sharing)
- [Script API (in progress...)](https://docs.google.com/document/d/1MJRvLOUW6i9aQEun7-67IQUm3j_veg2-r127i37n7GM/edit?usp=sharing)
- By protocols
    - HTTP protocol (in progress...)
    - WebSocket protocol (in progress...)
    - [SRT protocol](https://docs.google.com/presentation/d/1WB_K4omIQ6LFW-dIoK77MNy8LcQL28sTknD6DFfUXvI/edit?usp=sharing)
    - RTMP(E) and RTMFP protocols (in progress...)


## About
Asks your __questions or feedbacks__ relating MonaServer usage on the [MonaServer forum](https://groups.google.com/forum/#!forum/monaserver).

For all __issues__ with MonaServer please create an issue on the [github issues page](https://github.com/MonaSolutions/MonaServer2/issues).

MonaServer is licensed under the **[GNU General Public License]** and mainly **[powered by Haivision](https://www.haivision.com/)**, for any commercial questions or licence please contact us at ![contact_mail].

[SRT]: https://github.com/Haivision/srt
[SRT protocol]: https://www.srtalliance.org/
[Download]: https://codeload.github.com/MonaSolutions/MonaServer2/zip/master
[clone]: https://github.com/MonaSolutions/MonaServer2
[GNU General Public License]: http://www.gnu.org/licenses/
[contact_mail]: https://services.nexodyne.com/email/customicon/CUlFO7mGlaQmRdvbwDkob5dSi6L7Gw%3D%3D/FzgjAUw%3D/000000/ffffff/ffffff/0/image.png
