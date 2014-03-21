#!/bin/bash

#
#  Additional commands to finish setting up the chroot to be used for building the raddebugger project
#
#	This script needs to be run as a regular user who has access to the source:
#	e.g.: schroot --chroot <chroot name> -u bdeen chroot_configure.sh
#
#	You may be prompted for your root password as various components are installed
#

# exit on any script line that fails
set -o errexit
# bail on any unitialized variable reads
set -o nounset

if [ -f /etc/profile.d/radproj.sh ]; then
    # Read in our envar proxy settings (needed for wget).
    source /etc/profile.d/radproj.sh
fi

SCRIPT=$(readlink -f "$0")
SCRIPTPATH=$(dirname "$SCRIPT")
EXTBUILDDIR=$(readlink -f "${SCRIPTPATH}/../vogl_extbuild")
mkdir -p ${EXTBUILDDIR}

DO_PACKAGES=
if [[ $EUID -eq 0 ]]; then
   DO_PACKAGES="_packages"
elif [[ $# -eq 1 && "$1" == "--packages" ]]; then
   DO_PACKAGES="_packages"
fi

if [[ ! -v IN_CHROOT_CONFIGURE ]]; then
  # Launch ourselves with script so we can time this and get a log file
  export IN_CHROOT_CONFIGURE=1
  script --return --command "time $SCRIPT $@" ${EXTBUILDDIR}/chroot_configure${DO_PACKAGES}.$(uname -i).log
  exit $?
fi

#
#  Code to check if we're running inside a chroot
#  Abort this script if it's not being run in a chroot
#
if [ "$(sudo stat -c %d:%i /)" != "$(sudo stat -c %d:%i /proc/1/root/.)" ]; then
	echo "Chroot environment detected. Continuing..."
else
	echo "Script must be run in a chroot environment. Exiting..."
	exit
fi

function banner_spew()
{
    tput bold
    tput setaf 3
    printf '=%.0s' {1..50}
    echo -e "\n$@"
    printf '=%.0s' {1..50}
    echo ""
    tput sgr0
}

function banner_run()
{
    banner_spew "$@"
    $@
}

function apt_get_install()
{
    banner_run "apt-get install $@"
}

#
# Check if running as root - this will cause issues with temp build files being owned by root.
#
if [[ -n "$DO_PACKAGES" ]]; then

    #
    # We are running as root - do the package install stuff.
    #

    # don't ask questions when installing ubuntu-minimal, etc.
    export DEBIAN_FRONTEND=noninteractive

    # add the universe repo & some source pointers
    echo "deb http://archive.ubuntu.com/ubuntu precise main" > /etc/apt/sources.list
    echo "deb-src http://extras.ubuntu.com/ubuntu precise main" >> /etc/apt/sources.list
    echo "deb http://us.archive.ubuntu.com/ubuntu/ precise universe" >> /etc/apt/sources.list
    echo "deb-src http://us.archive.ubuntu.com/ubuntu/ precise universe" >> /etc/apt/sources.list

    echo "deb http://ppa.launchpad.net/ubuntu-toolchain-r/test/ubuntu precise main" > /etc/apt/sources.list.d/gcc_toolchain.list
    echo "deb-src http://ppa.launchpad.net/ubuntu-toolchain-r/test/ubuntu precise main" >> /etc/apt/sources.list.d/gcc_toolchain.list

    #
    #  http://www.pressingquestion.com/11145310/Badsig-Errors
    #  Gets public key to remove unverified signature when doing apt-get update
    #
    apt-key add key_437D05B5
    apt-key add key_3E5C1192

    apt-get -y update

    #
    #  Install the minimum Ubuntu tool set
    #
    apt_get_install -y ubuntu-minimal
    apt_get_install --force-yes -y build-essential
    apt_get_install --force-yes -y gcc-4.8 g++-4.8

    apt_get_install -y pkg-config

    #  Install the X11 package
    apt_get_install -y libx11-dev

    #  Install the mesa common files
    apt_get_install -y mesa-common-dev

    #  Install the GL common dev files
    apt_get_install -y libgl1-mesa-dev
    apt_get_install -y libglu1-mesa-dev
    apt_get_install -y freeglut3-dev

    #  Install zip tools
    apt_get_install -y zip

    #  Install wget tool
    apt_get_install -y wget

    #  Install ncurses and friends
    apt_get_install -y libncurses5-dev
    apt_get_install -y libgpm-dev

    #  Install tinyxml
    apt_get_install -y libtinyxml-dev

    #  Install lzma
    apt_get_install -y liblzma-dev

    #  Install readline
    apt_get_install -y libreadline6-dev

    # Install to get /usr/include/unwind.h file for libbacktrace
    #  (This is installed via our own build down below now.)
    apt-get purge -y libunwind7-dev libunwind7

    # Install to get /usr/include/dwarf.h for libbacktrace
    apt_get_install -y libdw-dev

    # To build and run the OpenGL g-truc samples
    apt_get_install -y libxrandr-dev
    apt_get_install -y libxi-dev
    apt_get_install -y libxxf86vm-dev

    # Install a toilet (banners in console)
    apt_get_install -y toilet

    # Install time utility
    apt_get_install -y time

    # Workaround bug 714890 in 32-bit clang. Gcc 4.8 changed the include paths.
    # http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=714890
    #   /usr/include/c++/4.6/i686-linux-gnu/bits/c++config.h
    #   /usr/include/i386-linux-gnu/c++/4.8/bits/c++config.h
    if [ -d /usr/include/i386-linux-gnu/c++/4.8 ]; then
        ln -s /usr/include/i386-linux-gnu/c++/4.8 /usr/include/c++/4.8/i686-linux-gnu
    fi

    echo ""
    echo "#########################################################################"
    echo "##### NOTICE - bug with schroot (fixed in 13.10):                    ####"
    echo "#####   https://bugs.launchpad.net/ubuntu/+source/sbuild/+bug/917339 ####"
    echo "#########################################################################"
    echo ""
    echo "If you see any \"E: 10mount:\" errors, you will need to reboot and run:"
    echo ""
    echo "  schroot --end-session --all-sessions"
    echo ""

    exit 0
fi

FORCE_REBUILD_ALL=0

#echo "OPTIND starts at $OPTIND"
while getopts "f" optname
 	do
		case "$optname" in
        "f")
            # echo "Force rebuild of all packages"
            FORCE_REBUILD_ALL=1
            ;;
		"?")
			#echo "Unknown option $OPTARG"
			;;
	 	":")
			#echo "No argument value for option $OPTARG"
			;;
		*)
	 		# Should not occur
			echo "Unknown error while processing options"
		;;
		esac
		# echo "OPTIND is now $OPTIND"
	done

# http://stackoverflow.com/questions/64786/error-handling-in-bash
function cleanup()
{
    echo -e "\nenv is:\n$(env)\n"

    tput setaf 3
    echo "ERROR: $0 just hit error handler."
    echo "  BASH_COMMAND is \"${BASH_COMMAND}\""
    echo "  BASH_VERSION is $BASH_VERSION"
    echo "  DESTDIR is \"$DESTDIR\""
    echo "  EXTDIR is \"$EXTDIR\""
    echo "  BUILDDIR is \"$BUILDDIR\""
    echo "  pwd is \"$(pwd)\""
    echo "  PATH is \"$PATH\""
    echo ""
    echo "If DESTDIR is set, that directory is most likely in a bad way."
    echo "We're not removing it so you can debug the issue..."
    echo ""
    tput sgr0
}
# from "man bash":
#   If a sigspec is EXIT (0) the command arg is executed on exit from the shell.
trap cleanup EXIT


#  To differentiate a 32 from a 64 bit system
ARCH=`uname -i`

# Our build directory. Could be a full path or relative. Ie: ./x86_64
EXTDIR=$(readlink -f "${SCRIPTPATH}/../external")
BUILDDIR=${EXTBUILDDIR}/${ARCH}

echo "Set values:"
echo "  EXTDIR=$EXTDIR"
echo "  BUILDDIR=$BUILDDIR"
echo "  FORCE_REBUILD_ALL=$FORCE_REBUILD_ALL (-f)"
echo ""

#
# You can change the REV to get just that specific package to be
#   rebuilt and reinstalled without making folks do an entire
#   force rebuild all.
#

#
# Set up our compiler links
#  Make sure we're using gcc 4.6 to build all the below packages.
#
DESTDIR="Set up gcc compiler links"
echo -e "\nExecuting: ${DESTDIR}..."

sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-4.6 100
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-4.6 100
sudo update-alternatives --install /usr/bin/cpp cpp-bin /usr/bin/cpp-4.6 100

sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-4.8 50
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-4.8 50
sudo update-alternatives --install /usr/bin/cpp cpp-bin /usr/bin/cpp-4.8 50

sudo update-alternatives --set gcc /usr/bin/gcc-4.6
sudo update-alternatives --set g++ /usr/bin/g++-4.6
sudo update-alternatives --set cpp-bin /usr/bin/cpp-4.6

#  Install CMake
REV=rev1
DESTDIR=${BUILDDIR}/cmake-2.8.10.2_${REV}
banner_spew "Building ${DESTDIR}..."
[ $FORCE_REBUILD_ALL -eq 1 ] && rm -rf "$DESTDIR"
if [ ! -w "$DESTDIR/bin/cmake" ]; then
  rm -rf "$DESTDIR"
  mkdir -p "$DESTDIR" && tar -xvf ${EXTDIR}/cmake-2.8.10.2.tar.gz --strip=1 -C $_
  ( cd $DESTDIR && ./configure && make -j 8 )
fi
( cd $DESTDIR && sudo make install )

#  Install nasm
REV=rev1
DESTDIR=${BUILDDIR}/nasm-2.10.09_${REV}
banner_spew "Building ${DESTDIR}..."
[ $FORCE_REBUILD_ALL -eq 1 ] && rm -rf "$DESTDIR"
if [ ! -w "$DESTDIR/nasm" ]; then
  rm -rf "$DESTDIR"
  mkdir -p "$DESTDIR" && tar -xvf ${EXTDIR}/nasm-2.10.09.tar.xz --strip=1 -C $_
  ( cd $DESTDIR && ./configure --prefix=/usr && make -j 8 )
fi
( cd $DESTDIR && sudo make install )

#  Install libjpeg-turbo
REV=rev1
DESTDIR=$BUILDDIR/libjpeg-turbo-1.3.0_${REV}
banner_spew "Building ${DESTDIR}..."
[ $FORCE_REBUILD_ALL -eq 1 ] && rm -rf "$DESTDIR"
if [ ! -w "$DESTDIR/.libs/libturbojpeg.a" ]; then
  rm -rf "$DESTDIR"
  mkdir -p "$DESTDIR" && tar -xvf ${EXTDIR}/libjpeg-turbo-1.3.0.tar.gz --strip=1 -C $_
  pushd $DESTDIR
  ./configure --prefix=/usr --mandir=/usr/share/man --with-jpeg8 --enable-static CFLAGS="-O3 -fPIC" && sed -i -e '/^docdir/ s/$/\/libjpeg-turbo-1.3.0/' -e '/^exampledir/ s/$/\/libjpeg-turbo-1.3.0/' Makefile && make clean && make -j 8
  #sudo make install prefix=/usr/local libdir=/usr/local/lib
  popd
fi
( cd $DESTDIR && sudo make install )

#  Install Qt
REV=rev1
DESTDIR=${BUILDDIR}/qt-everywhere-opensource-src-4.8.5_${REV}
banner_spew "Building ${DESTDIR}..."
[ $FORCE_REBUILD_ALL -eq 1 ] && rm -rf "$DESTDIR"
if [ ! -w "$DESTDIR/lib/libQtCore.so" ]; then
  # The official qt link fails with 'ERROR 403: Forbidden' occasionally so use Valve hosted file.
  # [ ! -f ${BUILDDIR}/qt-everywhere-opensource-src-4.8.5.tar.gz ] && wget http://download.qt-project.org/official_releases/qt/4.8/4.8.5/qt-everywhere-opensource-src-4.8.5.tar.gz -O ${BUILDDIR}/qt-everywhere-opensource-src-4.8.5.tar.gz
  [ ! -f ${BUILDDIR}/qt-everywhere-opensource-src-4.8.5.tar.gz ] && wget --no-check-certificate http://developer.valvesoftware.com/w/images/files/qt-everywhere-opensource-src-4.8.5.tar.gz -O ${BUILDDIR}/qt-everywhere-opensource-src-4.8.5.tar.gz

  rm -rf "$DESTDIR"
  mkdir -p "$DESTDIR" && tar -xvf ${BUILDDIR}/qt-everywhere-opensource-src-4.8.5.tar.gz --strip=1 -C $_
  pushd $DESTDIR
  echo yes | ./configure -opensource
  make -j 8
  popd
fi
( cd $DESTDIR && sudo make install )

# Install libunwind
REV=rev1
DESTDIR=${BUILDDIR}/libunwind_${REV}
rm -rf ${BUILDDIR}/libunwind_rev1
banner_spew "Building ${DESTDIR}..."
[ $FORCE_REBUILD_ALL -eq 1 ] && rm -rf "$DESTDIR"
if [ ! -w "$DESTDIR/src/.libs/libunwind.a" ]; then
  rm -rf "$DESTDIR"
  mkdir -p $(dirname "$DESTDIR")
  cp -a ${EXTDIR}/libunwind $DESTDIR
  pushd $DESTDIR
  # to debug:
  #   ./configure --enable-debug --enable-debug-frame --enable-shared=no --enable-static=yes
  #   export UNW_DEBUG_LEVEL=4
  CFLAGS="-g -fPIC" ./configure --enable-debug-frame --enable-shared=no --enable-static=yes
  make CFLAGS="-g -fPIC" LDFLAGS="-g -fPIC" -j 8
  popd
fi
( cd $DESTDIR && sudo make CFLAGS="-g -fPIC" LDFLAGS="-g -fPIC" install )

# Install SDL2
REV=rev1
DESTDIR=${BUILDDIR}/SDL_${REV}
banner_spew "Building ${DESTDIR}..."
[ $FORCE_REBUILD_ALL -eq 1 ] && rm -rf "$DESTDIR"
if [ ! -w "$DESTDIR/build/.libs/libSDL2.a" ]; then
  rm -rf "$DESTDIR"
  mkdir -p $(dirname "$DESTDIR")
  cp -a ${EXTDIR}/SDL $DESTDIR
  ( cd $DESTDIR && ./configure CFLAGS=-fPIC CPPFLAGS=-fPIC && make -j 8 )
fi
( cd $DESTDIR && sudo make install )

# Create a link in /usr/local/bin to our ninja script
sudo rm -rf /usr/local/bin/ninja
sudo ln -s ${SCRIPTPATH}/ninja /usr/local/bin

#  Install and build clang 3.4
REV=rev1
DESTDIR=$BUILDDIR/clang34_${REV}
banner_spew "Building ${DESTDIR}..."
[ $FORCE_REBUILD_ALL -eq 1 ] && rm -rf "$DESTDIR"
if [ ! -w "$DESTDIR/build/bin/clang" ]; then
  rm -rf "$DESTDIR"
  mkdir -p "$DESTDIR/llvm-3.4" && tar -xvf ${EXTDIR}/clang34/llvm-3.4.src.tar.gz --strip=1 -C $_
  mkdir -p "$DESTDIR/llvm-3.4/tools/clang" && tar -xvf ${EXTDIR}/clang34/clang-3.4.src.tar.gz --strip=1 -C $_
  mkdir -p "$DESTDIR/llvm-3.4/tools/clang/tools/extra" && tar -xvf ${EXTDIR}/clang34/clang-tools-extra-3.4.src.tar.gz --strip=1 -C $_
  mkdir -p "$DESTDIR/llvm-3.4/projects/compiler-rt" && tar -xvf ${EXTDIR}/clang34/compiler-rt-3.4.src.tar.gz --strip=1 -C $_
  mkdir -p "$DESTDIR/build" && cd $_
  pushd $DESTDIR/build
  banner_run cmake -DCMAKE_BUILD_TYPE=Release -G Ninja $DESTDIR/llvm-3.4
  banner_run ninja
  popd
fi

#
# Set up our compiler links
#
banner_spew "Setting up clang compiler links..."

# clang 3.4 links
sudo update-alternatives --install /usr/bin/gcc gcc $DESTDIR/build/bin/clang 50
sudo update-alternatives --install /usr/bin/g++ g++ $DESTDIR/build/bin/clang++ 50
#sudo update-alternatives --install /usr/bin/cpp cpp-bin /usr/bin/cpp-4.6 100

#  Use CLANG 3.4 by default
echo "Setting [ $(gcc --version | head -n 1) ] to be the default compiler"
sudo update-alternatives --set gcc $DESTDIR/build/bin/clang
sudo update-alternatives --set g++ $DESTDIR/build/bin/clang++
sudo update-alternatives --set cpp-bin /usr/bin/cpp-4.8

# Update dynamic link bindings.
sudo ldconfig

# Clear our exit trap handler
trap - EXIT
