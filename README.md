vogl
=============

## Warning ##

This project is alpha^2 right now. If you are up for suffering through a bit of pain with early releases, please continue on - we'd love to have your help...

## Build ##

To build the vogl chroots (uses schroot), do the following:

    vogl/bin/chroot_build.sh --i386 --amd64

You should now be ready to build in your chroots. Something like any of these:

    vogl/bin/mkvogl.sh --release --amd64
    vogl/bin/mkvogl.sh --debug --amd64 --i386 --clang34 --verbose
    vogl/bin/mkvogl.sh --release --amd64 --i386 --gcc48 --CRNLIB_ENABLE_ASSERTS

Note that you do _not_ have to use the chroots or mkvogl.sh to build. You could do your own cmake (cmake vogl/src) and go from there. It's up to you to get the dependencies correct though. Look at vogl/bin/chroot_configure.sh to see how the chroots are set up. The source for mkvogl is in vogl/bin/src/mkvogl.cpp - it's just a simple cpp wrapper around cmake.

## Capturing ##

    vogl/bin/steamlauncher.sh --gameid vogl/vogl_build/bin/glxspheres32
    vogl/bin/steamlauncher.sh --gameid vogl/vogl_build/bin/glxspheres64 --amd64

You should now have something like the following in your temp directory:

    /tmp/vogltrace.glxspheres64.2014_01_20-16_19_34.bin

## Replay ##

    vogl/vogl_build/bin/voglreplay64 /tmp/vogltrace.glxspheres64.2014_01_20-16_19_34.bin

or

    vogl/vogl_build/bin/vogleditor64 ; and then open the trace file...

## Directory structure ##

The directory structure for vogl currently looks like this:

        vogl/
            bin/
                chroot_build.sh ; script to build/rebuild chroots
                chroot_configure.sh ; script to build libs to chroots (used by chroot_build.sh)
                gligen_run.sh ; run vogl_build/bin64/gligen.sh (put in glspec)
                gligen_copy_inc_files.sh ; copy glspec/*.inc
                set_compiler.sh ; switch chroot default compiler
            external/ ; external source (libunwind, etc.)
            glspec/
            src/ ; vogl source
            vogl_build/
                bin/ ; destination for binaries
            vogl_extbuild/
                i386/   ; external projects untar'd & built here
                x86_64/ ;

## QtCreator tagging and building ##

  See qtcreator/qtcreator.md file: [qtcreator.md](qtcreator/qtcreator.md)

## Vogl Dev List ##

    http://lists.voglproj.com/listinfo.cgi/dev-voglproj.com

## Useful Links ##

OpenGL documentation/references

* 4.x: http://www.opengl.org/sdk/docs/man/
* 3.3: http://www.opengl.org/sdk/docs/man3/
* 2.1: http://www.opengl.org/sdk/docs/man2/

Specifications:

* 4.0: http://www.opengl.org/registry/doc/glspec40.core.20100311.pdf
* 3.3: http://www.opengl.org/registry/doc/glspec33.core.20100311.withchanges.pdf
* 2.1: http://www.opengl.org/documentation/specs/version2.1/glspec21.pdf

