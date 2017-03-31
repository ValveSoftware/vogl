vogl
=============
[![Build Status](https://travis-ci.org/deepfire/vogl.svg?branch=master)](https://travis-ci.org/deepfire/vogl)

## Warning ##

This project is alpha^2. If you are up for suffering through a bit of pain with early releases, please jump in - we'd love to have your help...

## Dependencies ##

The build dependencies for ubuntu (14.04) can be installed either using APT:
```
sudo apt-get install build-essential pkg-config cmake libx11-dev \
                     zip wget libtinyxml-dev liblzma-dev libunwind8-dev \
                     libturbojpeg libdwarf-dev mesa-common-dev qt5-qmake\
                     freeglut3-dev qt5-default libqt5x11extras5-dev git \
                     libsdl2-gfx-dev libsdl2-image-dev libsdl2-ttf-dev libjpeg-turbo8-dev
```
> .. or using Nix, for which keep reading.

### Get Source and Build ###

There are two build methods, due to development environment setup mechanisms: Nix and chroot.

1. The Nix-based method:

  - distribution-agnostic, but requires the Nix package manager to be installed
  - fire-and-forget
  - integrated with Travis CI
  - allows fine dependency specification
  - 100% declarative -- you get what you ask for (nothing is implied by any kind of "state")
  - currently suffers from https://github.com/NixOS/nixpkgs/issues/9415
    - ..but there is a remedy (nVidia-only at the moment): https://github.com/deepfire/nix-install-vendor-gl

2. The old, chroot-based method.

#### Nix-based ####

##### Linux (Mac OS X untested, but not impossible) ####

First, you need to have Nix installed (citing http://nixos.org/nix/manual/#ch-installing-binary):

```
desktop@steamos:~/src/vogl$ bash <(curl https://nixos.org/nix/install)
...

## At the end, this will instruct you to either re-login,
## or source the Nix environment setup script.  Do that.
## Validation:

desktop@steamos:~/src/vogl$ nix-env --query --installed   # ..which should list just `nix`, initially.
```
> NOTE: If security implications of this particular step worry you, please read
> the following piece by Domen Kožar:
> https://www.domenkozar.com/2015/09/27/friends-sometimes-let-friends-curl-to-shell/)

Once that's behind us, the following two options become available:

1. Building and installing the package into the current user environment:

```
desktop@steamos:~/src/vogl$ nix-env --no-build-output --cores 0 -iE 'f: (import ./shell.nix {})'
these derivations will be built:
  /nix/store/zvdz4dhprf6kbnjf5sjcc8731rnbmd1z-vogl-2016-05-13.drv
building path(s) ‘/nix/store/b88a5flgwjghw204wg5j8ahqs50l2qba-vogl-2016-05-13’
_makeQtWrapperSetup
unpacking sources
unpacking source archive /nix/store/qm11psc7pp4nk2273msgln3x519iv1pb-vogl-cbc5f18
...

...
```

2. Entering the fully-dependency endowed build environment:
```
nix-shell
```


#### Chroot-based ####

##### Linux ####

```
git clone https://github.com/ValveSoftware/vogl.git  
mkdir -p vogl/vogl_build/release64 && cd $_  
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_X64=On ../..
make -j 10
```

The binaries are placed in the vogl/vogl_build directory.

For debug builds, use "-DCMAKE_BUILD_TYPE=Debug"  
For 32-bit builds, use "-DBUILD_X64=Off"  

##### Windows ####

```
git clone https://github.com/ValveSoftware/vogl.git
mkdir -p vogl\vogl_build\x64
cd vogl\vogl_build\x64
cmake -DQt5_DIR="C:\Qt\5.3\msvc2013_64_opengl\lib\cmake\Qt5" -G "Visual Studio 12 2013 Win64" ..\..
cd ..\..\..\
mkdir -p vogl\vogl_build\win32
cd vogl\vogl_build\win32
cmake -DQt5_DIR="C:\Qt\5.3\msvc2013_opengl\lib\cmake\Qt5" -G "Visual Studio 12 2013" ..\..
```

Note: The path to Qt5 might need to be adjusted depending on your install location.
After compiling with Visual Studio, the binaries are placed in the vogl\vogl_build\Release (or Debug) directory.

##### Mac OS X ###

Install the required build dependencies using Homebrew:

```
brew install pkg-config cmake sdl2 qt5 xz jpeg-turbo tinyxml
```

Then build with:

```
git clone https://github.com/ValveSoftware/vogl.git

mkdir -p vogl/vogl_build/mac
cd       vogl/vogl_build/mac

cmake ../.. -DCMAKE_PREFIX_PATH=/usr/local/Cellar/qt5/5.3.2/
make
```

Note: the Mac port is not yet functional.

## Capturing ##

```
cd vogl_build
VOGL_CMD_LINE="--vogl_tracefile vogltrace.glxspheres64.bin" LD_PRELOAD=$(readlink -f libvogltrace64.so) ./glxspheres64  
```

For capturing Steam games, please see the vogl_trace.sh script in the chroot repository:

https://bitbucket.org/raddebugger/vogl_chroot/src/master/bin/src/sl.cpp?at=master

We are currently working on making it much easier to launch and profile Steam apps.

Note, that `vogleditor` used to attempt to launch the target application while
preloading both 32-bit and 64-bit versions of the capture libraries, but this was
error-prone -- in certain systems that would prevent the editor-based capturing at
all.  So this was disabled, and now `vogleditor` only preloads captures of a
single architecture -- one matching the particular `vogl` build, 32 or 64-bit.

## Replay ##

```
./voglreplay64 play vogltrace.glxspheres64.bin
```

or launch `vogleditor64` and open trace file.

## QtCreator tagging and building ##

  See qtcreator/qtcreator.md file: [qtcreator.md](qtcreator/qtcreator.md)

## Vogl Dev List ##

  http://lists.voglproj.com/listinfo.cgi/dev-voglproj.com

## Useful Links ##

Vogl Wiki

* https://github.com/ValveSoftware/vogl/wiki

OpenGL documentation/references

* 4.x: http://www.opengl.org/sdk/docs/man/
* 3.3: http://www.opengl.org/sdk/docs/man3/
* 2.1: http://www.opengl.org/sdk/docs/man2/

Specifications

* 4.0: http://www.opengl.org/registry/doc/glspec40.core.20100311.pdf
* 3.3: http://www.opengl.org/registry/doc/glspec33.core.20100311.withchanges.pdf
* 2.1: http://www.opengl.org/documentation/specs/version2.1/glspec21.pdf

## License and Credits ##

Vogl code is [MIT](http://opensource.org/licenses/MIT) licensed. 

```
Copyright 2013-2014 RAD Game Tools and Valve Software

All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

libbacktrace is BSD licensed. [Libbacktrace license](https://github.com/mirrors/gcc/blob/master/libbacktrace/README).

apitrace is MIT licensed: [Apitrace license](https://github.com/apitrace/apitrace/blob/master/LICENSE).

Loki is MIT licensed: [Loki license](http://loki-lib.sourceforge.net/index.php?n=Main.License).

Valgrind header files are BSD-style licensed: [Valgrind header files](http://valgrind.org/docs/manual/manual-intro.html).

GL header files are MIT licensed: [GL header files](http://www.opengl.org/registry/).

glxspheres (from VirtualGL) is wxWindows Library licensed: [wxWindows Library License](http://www.virtualgl.org/About/License).

SDL is zlib licensed: [zlib license](http://www.libsdl.org/license.php).

stb files (from Sean Barrett) are public domain.

TinyXML-2 is zlib licensed: [zlib license](https://github.com/leethomason/tinyxml2).

