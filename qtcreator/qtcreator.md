QtCreator 3.0
-------------

### Create Project ###

* Select "File / Open File or Project..." (Ctrl+O)
* Open the "vogl/qtcreator/CMakeLists.txt" file
* Select a location for the cmake build files. I just the choose vogl/qtcreator directory.
* Hit "Next >" button.
* Change Generator to "Ninja (Desktop)" if you're going to build with Ninja.
* Hit "Run CMake" button.
 * ... Wait a few seconds to parse and add files. ...
 * ... You should see something like "2358 files added." ...
* Hit "Finish" button.

You should now be able to click in bottom left "Type to locate" control (or hit Ctrl+K), type in names of vogl files (ie vogl_intercept.cpp), search for symbols, etc.

### Build ###

* Click on "Projects" icon on the left.
* Under Build Steps:
 * Mouse over the "Make: make" step and click the 'x' to delete it.
 * Click <Add Build Step>, and select "Custom Process Step".

    <b>Command:</b> /home/mikesart/dev/voglproj/vogl/bin/mkvogl.sh  
    <b>Arguments:</b> --amd64 --debug 3>&1 1>&2 2>&3  

* Now select "Choose Build / Build Project..." or (Ctrl+B)
* (Alt+4) will flip QtCreator to show the compile output

### Clean ###

* Under Clean Steps:
 * Mouse over the "Make: make clean" step and click the 'x' to delete it.
 * Click "Add Clean Step", select "Custom Process Step".

    <b>Command:</b> /home/mikesart/dev/voglproj/vogl/bin/mkvogl.sh  
    <b>Arguments:</b> --amd64 --debug --cleanonly  


### Other build configurations ###

* Within the <Projects> tab, next to "Edit build configurations"
 * Click "Rename..."; change the text from "all" to "amd64_debug"
 * Click "Add"; select "Clone selected"; follow steps below

* Repeat the steps above for the remaining build configurations:
 * <b>amd64_release":</b> Arguments: --amd64 --release --verbose 3>&1 1>&2 2>&3
 * <b>i386_debug:</b> Arguments: --i386 --debug --verbose 3>&1 1>&2 2>&3
 * <b>i386_release":</b> Arguments: --i386 --release --verbose 3>&1 1>&2 2>&3


Not that you can use the "--usemake" flag with mkvolg.sh if you don't want to use Ninja. If you do this, remove the "3>&1 1>&2 2>&3" redirections also.
