# MonaServer
[Unix setup](#Unix-setup) | [Windows setup](#Windows-setup) | [MonaTiny](#MonaTiny) | [Documentation](#Documentation) | [About](#About)

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
- SRT, VTT
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
***Note:*** Mac-OSX target can require a MACOSX_DEPLOYMENT_TARGET definition with *make MACOSX_DEPLOYMENT_TARGET=10.6 && make install*, see this [LuaJIT issue](https://github.com/LuaJIT/LuaJIT/issues/484) for more details.

### Build
[Download] or [clone] MonaServer sources and compile everything simply with make:
```
    git clone https://github.com/MonaSolutions/MonaServer2.git
    cd MonaServer2
    make
```

### Start
Start executable file *./MonaServer/MonaServer*


## Windows setup

### Binaries
A [Windows 32-bit](https://sourceforge.net/projects/monaserver/files/MonaServer/MonaServer_Win32.zip/download) and a [ Windows 64-bit](https://sourceforge.net/projects/monaserver/files/MonaServer/MonaServer_Win64.zip/download) zipped packages are provided to test MonaServer. However we recommend to build the github version from the sources for production use.

### Requirements
- [C++ vc_redist.x86 Visual Studio 2015 package](https://www.microsoft.com/it-it/download/details.aspx?id=48145)
- [Microsoft Visual Studio Express 2015 for Windows Desktop](https://my.visualstudio.com/Downloads?q=visual%20studio%202015&wt.mc_id=o~msft~vscom~older-downloads).

***Note:*** [OpenSSL](https://www.openssl.org/) and [LuaJIT](http://luajit.org/) dependencies are already included in the project.

### Build
[Download] or [clone] MonaServer sources, open *Mona.sln* project file with Visual Studio, right clic on *MonaServer* project and clic on *Build*.

### Start
Start MonaServer by selecting *MonaServer* project and pressing *F5*.

## MonaTiny
MonaTiny is a version of MonaServer without LUA script applications.
Setup is identical excepting that there is no LuaJIT dependency, just start executable file *./MonaTiny/MonaTiny*

## Documentation
- Overview (in progress...)
- [Configuration](https://github.com/MonaSolutions/MonaServer2/blob/master/MonaServer/MonaServer.ini)
- Script application (in progress...)
- Media streaming (in progress...)
- HTTP protocol (in progress...)
- WebSocket protocol (in progress...)
- [SRT protocol](https://docs.google.com/presentation/d/1p6SpXUyXApOfVtG5s3K07Wygtgjh91wspkOc7XbfUNw/edit?usp=sharing)
- RTMP(E) and RTMFP protocols (in progress...)


## About
Asks your __questions or feedbacks__ relating MonaServer usage on the [MonaServer forum](https://groups.google.com/forum/#!forum/monaserver).

For all __issues problem__ with MonaServer please create an issue on the [github issues page](https://github.com/MonaSolutions/MonaServer2/issues).

MonaServer is licensed under the **[GNU General Public License]** and mainly **[powered by Haivision](https://www.haivision.com/)**, for any commercial questions or licence please contact us at ![contact_mail].


[Download]: https://codeload.github.com/MonaSolutions/MonaServer2/zip/master
[clone]: https://github.com/MonaSolutions/MonaServer2
[GNU General Public License]: http://www.gnu.org/licenses/
[contact_mail]: https://services.nexodyne.com/email/customicon/CUlFO7mGlaQmRdvbwDkob5dSi6L7Gw%3D%3D/FzgjAUw%3D/000000/ffffff/ffffff/0/image.png




