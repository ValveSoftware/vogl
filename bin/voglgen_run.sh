#
# run_voglgen.sh
#

SCRIPT=$(readlink -f "$0")
SCRIPTPATH=$(dirname "$SCRIPT")
BUILDDIR=$(readlink -f "${SCRIPTPATH}/../vogl_build")

if [[ -f ${BUILDDIR}/bin/voglgen64 ]]; then
  echo "Running ${BUILDDIR}/bin/voglgen64..."
  ${BUILDDIR}/bin/voglgen64
elif [[ -f ${BUILDDIR}/bin/voglgen32 ]]; then
  echo "Running ${BUILDDIR}/bin/voglgen32..."
  ${BUILDDIR}/bin/voglgen32
else
  echo "ERROR: ${BUILDDIR}/bin/voglgen64 binary not found."
fi

