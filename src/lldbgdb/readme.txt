How to setup lldb with extra GDB functionality.

Bring this directory down to your machine with Merurial.

add the following line to your ~/.lldbinit

command script import <path-to-lldbgdb-dir>/gdbmode.py

If you don't have a .lldbinit you might want to add:

settings set prompt "${ansi.bold}${ansi.fg.green}(lldb)${ansi.normal} "

also to get colored prompts.

Now your lldb will have the following GDB-style commands:

break
info break
info watch
info args
info locals
info registers
info all-registers
info shared
delete
jump
pwd
cd
system
shell
jump
