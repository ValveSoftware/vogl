#
# cpy_inc_files.sh
#

SCRIPT=$(readlink -f "$0")
SCRIPTPATH=$(dirname "$SCRIPT")
SRCDIR=$(readlink -f "${SCRIPTPATH}/../glspec")
DSTDIR=$(readlink -f "${SCRIPTPATH}/../src/voglinc")
DSTDIR2=$(readlink -f "${SCRIPTPATH}/../src/vogltrace")

cp -v ${SRCDIR}/*.inc ${DSTDIR}
cp -v ${SRCDIR}/libvogltrace_linker_script.txt ${DSTDIR2}

