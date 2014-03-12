nm -D /usr/lib/nvidia-experimental-310/libGL.so.1 | grep  gl | cut -f 3 -d" " | sed '/^$/d' >gl_glx_so_export_list.txt
