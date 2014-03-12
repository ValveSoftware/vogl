#!/bin/bash

# Run this from either the i386 or amd64 chroot!

if [ "$(sudo stat -c %d:%i /)" == "$(sudo stat -c %d:%i /proc/1/root/.)" ]; then
    # run this command in both our chroots
    echo -e "\nvogl_precise_i386:"
    schroot -c vogl_precise_i386 -- $(readlink -f $0) "$@"
    echo -e "\nvogl_precise_amd64:"
    schroot -c vogl_precise_amd64 -- $(readlink -f $0) "$@"
    exit 0
fi

USE_GCC=0
USE_GCC48=0
USE_CLANG34=0
QUERY_ONLY=0

while getopts "g8cq" optname
 	do
		case "$optname" in
		"g")
			USE_GCC=1
			;;
		"8")
			USE_GCC48=1
			;;
		"c")
			USE_CLANG34=1
			;;
        "q")
            QUERY_ONLY=1
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
	done

ARCH=$(uname -i)
SCRIPT=$(readlink -f "$0")
SCRIPTPATH=$(dirname "$SCRIPT")
CLANG34ROOT=$(readlink -f "${SCRIPTPATH}/../vogl_extbuild/${ARCH}/clang34_rev1/build")

if [ "$QUERY_ONLY" == "0" ]; then
    # Set which compiler to use
    #
    if [ "$USE_GCC48" == "1" ]; then
        sudo update-alternatives --set gcc /usr/bin/gcc-4.8
        sudo update-alternatives --set g++ /usr/bin/g++-4.8
        sudo update-alternatives --set cpp-bin /usr/bin/cpp-4.8
    elif [ "$USE_GCC" == "1" ]; then
        sudo update-alternatives --auto gcc
        sudo update-alternatives --auto g++
        sudo update-alternatives --auto cpp-bin
    else
        sudo update-alternatives --set gcc ${CLANG34ROOT}/bin/clang
        sudo update-alternatives --set g++ ${CLANG34ROOT}/bin/clang++
        sudo update-alternatives --set cpp-bin /usr/bin/cpp-4.8
    fi
fi

tput bold
tput setaf 3
printf '=%.0s' {1..50}
echo -e "\nUsing \"$(gcc --version | head -n 1)\""
printf '=%.0s' {1..50}
echo ""
tput sgr0

