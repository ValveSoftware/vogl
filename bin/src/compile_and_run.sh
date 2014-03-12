# Script you can include to have a shell script which compiles and launches a binary.
# Use like this:
#
#  #!/bin/bash
#  SRCFILE=src/mkvogl.cpp
#  . $(dirname $(readlink -f "$0"))/src/compile_and_run.sh
#  compile_and_run_func "$@"
#

# exit on any script line that fails
set -o errexit
# bail on any unitialized variable reads
set -o nounset
# uncomment to help with debugging
## set -x

compile_and_run_func()
{
    arch=$(uname -i)                           # x86_64 or i386
    script=$(readlink -f "$0")                 # full path for script
    scriptpath=$(dirname "$script")            # path for script
    base=$(basename "$0")                      # basename of script. ie: mkvogl.sh
    # extension="${base##*.}"                  # script extension
    filename="${base%.*}"                      # filename of script. ie: mkvogl
    exe="${scriptpath}/${arch}/${filename}"    # full path for executable
    cfile="${scriptpath}/${SRCFILE}"           # full path for cfile

    # make sure our directory exists 
    mkdir -p "${scriptpath}/${arch}"

    # let executable read this envvar in to find vogl project directory
    export VOGL_PROJ_DIR=$(readlink -f ${scriptpath}/..)

    delta=0
    if [ -f "${exe}" ]; then
        # if the executable exists, check times of this script and executable
        time0=$(date --utc --reference="${cfile}" +%s)
        time1=$(date --utc --reference="${exe}" +%s)
        delta=$(($time0-$time1))
    fi

    # if the script time is greater than the exe time, rebuild
    if [ $delta -ge 0 ]; then
        echo "$(tput setaf 3)Compiling ${cfile}...$(tput sgr0)"
        # remove and compile our executable
        rm -f ${exe}
        g++ "$cfile" -g -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -o ${exe}
    fi

    echo ${exe} "$@"
    ${exe} "$@"
    exit $?
}
