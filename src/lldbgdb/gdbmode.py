#!/usr/bin/python

import os
import lldb

def __lldb_init_module(debugger, internal_dict):
    path,file = os.path.split(__file__)
    gdbmodefile = os.path.join(path, "gdbmode.lldb")
    jumpfile = os.path.join(path, "jump.py")
    utilfile = os.path.join(path, "utils.py")
    debugger.HandleCommand("command script import " + jumpfile)
    debugger.HandleCommand("command script import " + utilfile)
    debugger.HandleCommand("command source -s 1 " + gdbmodefile)

    print "GDB mode has now been activated.  Many common GDB commands will now work."










