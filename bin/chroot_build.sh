#!/bin/bash

# Check if we have any arguments.
if [ -z "$1" ]; then
  echo "Usage: build_chroot.sh [--i386] [--amd64]";
  exit 1
fi

SCRIPT=$(readlink -f "$0")
SCRIPTPATH=$(dirname "$SCRIPT")

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

build_chroot()
{
  Color_Off="\033[0m"
  Color_On="\033[1;93m"

  case "$1" in
    "--i386" )
      pkg="i386"
      personality="linux32"
      ;;
    "--amd64" )
      pkg="amd64"
      personality="linux"
      ;;
    * )
      echo "Error: Unrecognized argument: $1"
      exit 1
      ;;
  esac

  # install some packages
  echo -e "\n${Color_On}Installing debootstrap schroot p7zip-full...$Color_Off"
  sudo apt-get install -y debootstrap schroot p7zip-full

  # blow away existing directories and recreate empty ones
  echo -e "\n${Color_On}Creating /var/chroots/vogl_precise_${pkg}..."  
  sudo rm -rf "/var/chroots/vogl_precise_${pkg}"
  sudo mkdir -p "/var/chroots/vogl_precise_${pkg}"

  # Create our schroot .conf file
  echo -e "\n${Color_On}Creating /etc/schroot/chroot.d/vogl_precise_${pkg}.conf...${Color_Off}" 
  printf "[vogl_precise_${pkg}]\ndescription=Ubuntu 12.04 Precise for ${pkg}\ndirectory=/var/chroots/vogl_precise_${pkg}\npersonality=${personality}\nroot-users=${USER}\ntype=directory\n" | sudo tee /etc/schroot/chroot.d/vogl_precise_${pkg}.conf

  # Create our chroot
  echo -e "\n${Color_On}Bootstrap the chroot...${Color_Off}" 
  sudo debootstrap --arch=${pkg} precise /var/chroots/vogl_precise_${pkg} http://archive.ubuntu.com/ubuntu/

  # Copy over proxy settings from host machine
  echo -e "\n${Color_On}Adding proxy info to chroot (if set)...${Color_Off}" 
  env | grep -i "_proxy=" | grep -v PERSISTENT_HISTORY_LAST | xargs -i echo export {} | sudo tee /var/chroots/vogl_precise_${pkg}/etc/profile.d/radproj.sh
  env | grep -i "_proxy=" | grep -v PERSISTENT_HISTORY_LAST | xargs -i echo export {} | sudo tee -a /var/chroots/vogl_precise_${pkg}/etc/environment
  sudo rm -rf "/var/chroots/vogl_precise_${pkg}/etc/apt/apt.conf"
  if [ -f /etc/apt/apt.conf ]; then sudo cp "/etc/apt/apt.conf" "/var/chroots/vogl_precise_${pkg}/etc/apt"; fi  

  # Make sure that vogl_extbuild exists so schroot_configure.sh doesn't create it as root.
  mkdir -p "${SCRIPTPATH}/../vogl_extbuild"

  echo -e "\n${Color_On}Running chroot_configure.sh --packages...${Color_Off}" 
  schroot --chroot vogl_precise_${pkg} -d ${SCRIPTPATH} --user root -- ./chroot_configure.sh --packages

  echo -e "\n${Color_On}Allow sudo to run in chroot without prompting for password...${Color_Off}" 
  echo -e "# Allow members of group sudo to execute any command\n%sudo   ALL= NOPASSWD: ALL\n" | sudo tee /var/chroots/vogl_precise_${pkg}/etc/sudoers.d/nopassword
  sudo chmod 440 /var/chroots/vogl_precise_${pkg}/etc/sudoers.d/nopassword

  echo -e "\n${Color_On}Running chroot_configure.sh...${Color_Off}" 
  schroot --chroot vogl_precise_${pkg} -d ${SCRIPTPATH} -- ./chroot_configure.sh
}

tput setaf 3
echo -e "\nWe are going to blow away /var/chroot/vogl_precise_XX (for $@) and re-install..."
read -p "  This ok (y/n)? "
tput sgr0

if [[ "$REPLY" != [Yy] ]]; then
  echo -e "Cancelled...\n"
  exit 1
fi

for var in "$@"; do
  build_chroot "$var"
done

echo -e "\n${Color_On}Done...${Color_Off}"
exit 1
