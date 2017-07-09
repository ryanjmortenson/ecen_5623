ECEN-5623 Real-Time Embedded Systems 
========================================

[ECEN-5623 Real-Time Embedded Systems](http://ecee.colorado.edu/~ecen5623/index_summer.html)

Repository Contents
-------------------

* **/app** - Platform independent source.
* **/scripts** - Helpful scripts for the project.
* **makefile** - Main make file for the project.
* **sources.mk** - List of sources for the platform independent source.
* **README.md** - This readme.

Device Documentation
--------------

* **[Nvidia TK1](http://www.nvidia.com/object/jetson-tk1-embedded-dev-kit.html)** - Nvidia TK1 used for in class projects and homework.

Make instructions
------------

This project requires the following tool-chains to be able to build all targets:

* **gcc-arm-linux-gnueabihf** - Used to build project for Beagle Bone Black.
* **gcc** - Used to build the project for your development workstation.

There are different targets in the make file that can be built.  When no
platform is supplied with the PLATFORM=*platform* option the build will use
the host machines compiler.

* **make** - Will build the current project for the host.
* **make *c_file*.asm** - Output an assembly file for the source file specified.
* **make allasm** - Output all assembly files for the project.
* **make *c_file*.i** - Output a preprocessor file for the source file specified.
* **make alli** - Output all preprocessor files for the project.
* **make *c_file*.o** - Output an object file for the source file specified.
* **make compile-all** - Output all object files project.
* **make *c_file*.map** - Output a map file for the source file specified.
* **make build-lib** - Create a static library for the project.
* **make clean** - Clean all files for the project.

There are two types for the PLATFORM option:

* **host** - Workstation Linux
* **tegra** - ARM Linux
