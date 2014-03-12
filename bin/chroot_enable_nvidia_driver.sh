#!/bin/bash

# Check if we have any arguments.
if [ -z "$1" ]; then
    echo "Usage: enable_nvidia_chroot.sh [-i386] [-amd64]";
    exit 1
fi

# Check if there are any active schroot sessions right now and warn if so...
schroot_list=$(schroot --list --all-sessions | head -n 1)
if [ $schroot_list ]; then
    tput setaf 3
    echo -e "\nWARNING: Schroot says you have a currently active session!\n"
    tput sgr0
    echo "  ${schroot_list}"
    echo ""
    read -p "Are you sure you want to continue (y/n)? "
    if [[ "$REPLY" != [Yy] ]]; then
        echo -e "Cancelled...\n"
        exit 1
    fi
fi

enable_nvidia_chroot()
{
    Color_Off="\033[0m"
    Color_On="\033[1;93m"

    case "$1" in
      "-i386" )
        pkg="i386"
        plat="i386"
        libdir="lib32"
        ;;
      "-amd64" )
        pkg="amd64"
        plat="x86_64"
        libdir="lib"
        ;;
      * )
        echo "Error: Unrecognized argument: $1"
        exit 1
        ;;
    esac

    # update-alternatives --query x86_64-linux-gnu_gl_conf | grep Value
    # driver_version=$(dpkg -l | fgrep "ii" | egrep -o 'nvidia\-[[:digit:]]{1,3}[^[:space:]]*')
    driver_version=$(update-alternatives --query x86_64-linux-gnu_gl_conf | grep Value | egrep -o 'nvidia\-[[:digit:]]{1,3}[^/]*')

    # Check if we have any arguments.
    if [ -z "$driver_version" ]; then
      echo "Error: can't determine nvidia driver version."
      exit 1
    fi

    echo -e "\n${Color_On}driver version: ${driver_version}${Color_Off}"

    if [ ! -d "/usr/${libdir}/${driver_version}" ]; then
        echo "Error: /usr/${libdir}/${driver_version} does not exist.";
        exit 1;
    fi

    if [ ! -d "/var/chroots/vogl_precise_${pkg}/usr/lib" ]; then
        echo "Error: /var/chroots/vogl_precise_${pkg}/usr/lib does not exist.";
        exit 1;
    fi

    CMD="sudo cp -a /usr/${libdir}/${driver_version} /var/chroots/vogl_precise_${pkg}/usr/lib"
    echo $CMD
    $CMD

    echo -e "\n${Color_On}Updating chroot usr/lib/${driver_version}/ld.so.conf file...${Color_Off}"
    echo "/usr/lib/${driver_version}" | sudo tee /var/chroots/vogl_precise_${pkg}/usr/lib/${driver_version}/ld.so.conf

    echo -e "\n${Color_On}Add gl link.${Color_Off}"
    CMD="schroot -c vogl_precise_${pkg} --user root -- update-alternatives --install /etc/ld.so.conf.d/${plat}-linux-gnu_GL.conf ${plat}-linux-gnu_gl_conf /usr/lib/${driver_version}/ld.so.conf 50"
    echo $CMD
    $CMD

    echo -e "\n${Color_On}Set gl link.${Color_Off}"
    CMD="schroot -c vogl_precise_${pkg} --user root -- update-alternatives --set ${plat}-linux-gnu_gl_conf /usr/lib/${driver_version}/ld.so.conf"
    echo $CMD
    $CMD

    echo -e "\n${Color_On}Configure run time bindings.${Color_Off}"
    CMD="schroot -c vogl_precise_${pkg} --user root -- ldconfig"
    echo $CMD
    $CMD

    echo -e "\n${Color_On}adding DISPLAY=:0.0 to chroot etc/environment...${Color_Off}"
    echo "export DISPLAY=:0.0" | sudo tee -a /var/chroots/vogl_precise_${pkg}/etc/environment
    echo "export DISPLAY=:0.0" | sudo tee -a /var/chroots/vogl_precise_${pkg}/etc/profile.d/radproj.sh

    echo -e "\n${Color_On}Install mesa-utils...${Color_Off}"
    schroot -c vogl_precise_${pkg} --user root -- /usr/bin/apt-get install -y mesa-utils

    echo -e "\n${Color_On}Testing for direct rendering:${Color_Off}"
    schroot -c vogl_precise_${pkg} -- /usr/bin/glxinfo -display :0.0 | grep -i "direct rendering"
}


for var in "$@"; do
  enable_nvidia_chroot "$var"
done

echo -e "\n${Color_On}Done...${Color_Off}"
exit 1
