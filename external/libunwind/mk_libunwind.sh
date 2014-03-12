#
# Simple script to build libunwind clean (-d for debug builds)
#

#  Abort if not being run in a chroot
if [ "$(sudo stat -c %d:%i /)" != "$(sudo stat -c %d:%i /proc/1/root/.)" ]; then 
    echo "Chroot environment detected. Continuing..."
else
    echo "Script must be run in a chroot environment. Exiting..."
    exit 
fi

debug=0

#echo "OPTIND starts at $OPTIND"
while getopts "d" optname
    do
        case "$optname" in
        "d") 
            debug=1
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

configure="--enable-debug-frame --enable-shared=no --enable-static=yes"

if [ "$debug" == "1" ]; then
    configure="$configure --enable-debug"
fi

make clean
CFLAGS="-g -fPIC" ./configure $configure
make CFLAGS="-g -fPIC" LDFLAGS="-g -fPIC" -j 8 
sudo make CFLAGS="-g -fPIC" LDFLAGS="-g -fPIC" install

if [ "$debug" == "1" ]; then
    echo -e "\nBe sure to set export UNW_DEBUG_LEVEL=4...\n"
fi

