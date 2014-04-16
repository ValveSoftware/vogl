vogl
=============

#### NOTE ####

April 16, 2014: Vogl history has been completely and utterly trounced. The original repository had an entire chroot build system that most folks weren't interested in. A few contributors (thanks Carl & Sir Anthony) took the time to build a much smaller source only vogl repository which we've replaced the original one with.

A separate chroot repository (which will build this source repository) is now here:

https://bitbucket.org/raddebugger/vogl_chroot

## Warning ##

This project is alpha^2. If you are up for suffering through a bit of pain with early releases, please jump in - we'd love to have your help...

## Dependencies ##

The chroot configuration script should be a good reference for vogl dependencies. It is located here:

https://bitbucket.org/raddebugger/vogl_chroot/src/3b278d4a40d936e75554a194f8eb24a42f4464b1/bin/chroot_configure.sh?at=master

## Get Source and Build ##

```
git clone https://github.com/ValveSoftware/vogl.git  
mkdir -p vogl/vogl_build/bin/release64 && cd $_  
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_X64=On ../../..  
make -j 10
```

The binaries are placed in the vogl/vogl_build/bin directory.

For debug builds, use "-DCMAKE_BUILD_TYPE=Debug"  
For 32-bit builds, use "-DBUILD_X64=On"  

## Capturing ##

```
cd vogl_build/bin  
VOGL_CMD_LINE="--vogl_tracefile vogltrace.glxspheres64.bin" LD_PRELOAD=$(readlink -f libvogltrace64.so) ./glxspheres64  
```

For capturing Steam games, please see the steamlauncher.sh script in the chroot repository:

https://bitbucket.org/raddebugger/vogl_chroot/src/3b278d4a40d936e75554a194f8eb24a42f4464b1/bin/src/sl.cpp?at=master

We are currently working on making it much easier to launch and profile Steam apps.

## Replay ##

```
./voglreplay64 vogltrace.glxspheres64.bin
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

Specifications:

* 4.0: http://www.opengl.org/registry/doc/glspec40.core.20100311.pdf
* 3.3: http://www.opengl.org/registry/doc/glspec33.core.20100311.withchanges.pdf
* 2.1: http://www.opengl.org/documentation/specs/version2.1/glspec21.pdf
