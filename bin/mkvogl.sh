#!/bin/sh

# Source file to build and run.
SRCFILE=src/mkvogl.cpp

# Include the compile_and_run_func function.
. $(dirname $(readlink -f "$0"))/src/compile_and_run.sh

# Tell it to compile and run.
compile_and_run_func "$@"
