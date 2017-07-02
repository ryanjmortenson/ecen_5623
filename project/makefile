##############################################################################
#
# There are different targets in the make file that can be built.  When no
# platform is supplied with the PLATFORM=<platform> option the build will use
# the host machines compiler.
#
# * **make** - Will build the current project for the host.
# * **make *<c_file*.asm** - Output an assembly file for the source file
#                            specified.
# * **make allasm** - Output all assembly files for the project.
# * **make *c_file*.i** - Output a preprocessor file for the source file
#                         specified.
# * **make alli** - Output all preprocessor files for the project.
# * **make *c_file*.o** - Output an object file for the source file
#                         specified.
# * **make compile-all** - Output all object files project.
# * **make *c_file*.map** - Output a map file for the source file specified.
# * **make test** - Create the unit tests for host.
# * **make build-lib** - Create a static library for the project.
# * **make clean** - Clean all files for the project
# * **make super-clean** - Clean all files for the project and the cmocka out
#                          directory
#
##############################################################################

# Some commands used for building
RM:=rm -rf
MK_DIR:=mkdir -p
CD:=cd
MAKE_CMD=make
CMAKE_CMD=cmake

# Directory names used for building targets
OUT=out
ARM=linux-arm
X86=linux-x86

# Export variables to use in submake
export $(X86)
export $(ARM)
export $(MK_DIR)
export $(CFLAGS)

# Unit test output filename
UNIT_TEST_OUT=test.out

# CMOCKA unit test variables
CMOCKA_BUILD_DIR=cmocka-out
CMOCKA_DIR=cmocka
CMOCKA_LIB_OUT_DIR=$(CMOCKA_BUILD_DIR)/lib
CMOCKA_INC_OUT_DIR=$(CMOCKA_BUILD_DIR)/include
CMOCKA_LIB=$(CMOCKA_LIB_OUT_DIR)/libcmocka.a

# Target out directories when building
X86_OUT_DIR=$(OUT)/$(X86)
ARM_OUT_DIR=$(OUT)/$(ARM)

# Target names for 2 built types
ARM_OUT=$(ARM_OUT_DIR)/$(OUT_FILE)
X86_OUT=$(X86_OUT_DIR)/$(OUT_FILE)

# List of map files used for cleaning
MAPS=$(X86_OUT_DIR)/$(MAP_FILE) \
     $(ARM_OUT_DIR)/$(MAP_FILE)

# Call sub make files to build dependencies for each target
-include sources.mk

# Set define of project 2 as default
ifeq ($(PROJECT),)
	PROJECT=1
else
	PROJECT=$(PROJECT)
endif

# Project output file names
PROJECT_FILE=project
PROJECT_OUT_FILE=$(PROJECT_FILE)$(PROJECT).out
LIB_OUT_FILE=lib$(PROJECT_FILE)$(PROJECT).a

# Set CFLAGS used by every build type
CFLAGS=-I$(APP_INC_DIR) \
       -Wall \
       -Werror \
       -D _POSIX_C_SOURCE=199309L \
       -D PROJECT$(PROJECT) \
       -std=c99

# Set colored logs if specified
ifneq ($(COLOR_LOGS),)
	CFLAGS+=-D COLOR_LOGS
endif

# System log turned on
ifneq ($(SYS_LOG),)
	CFLAGS+=-D SYS_LOG
endif

# Set log level if specified otherwise set to make level
ifeq ($(LOG_LEVEL),)
	CFLAGS+=-D LOG_LEVEL=4
else
	CFLAGS+=-D LOG_LEVEL=$(LOG_LEVEL)
endif

# Set a map flag to be added CFLAGS for certain targets
MAP_FLAG=-Wl,-Map,"$@.map"

# Set of 3 compilers use
ARM_CC=arm-linux-gnueabihf-gcc
X86_CC=x86_64-linux-gnu-gcc

# Set compiler, flags, and objects based on platform type
ifeq ($(PLATFORM),tegra)
	CC=$(ARM_CC)
	CFLAGS+=-lrt \
          -D TEGRA \
          -I$(ARM_PROP_INC_DIR)
	OBJS=$(ARM_PROP_OBJS) \
       $(ARM_OBJS)
	TEST_OBJS=$(ARM_TEST_OBJS)
	OUT_DIR=$(ARM_APP_OUT)
else ifneq ($(findstring armv7,$(shell uname -a)),)
	CC=gcc
	CFLAGS+=-lrt \
          -D TEGRA \
          -I$(ARM_PROP_INC_DIR)
	OBJS=$(ARM_PROP_OBJS) \
       $(ARM_OBJS)
	TEST_OBJS=$(ARM_TEST_OBJS)
	OUT_DIR=$(ARM_APP_OUT)
else
	CC=gcc
	OBJS=$(X86_OBJS)
	TEST_OBJS=$(X86_TEST_OBJS)
	OUT_DIR=$(X86_APP_OUT)
endif

# Create names for size, ar, and objdump binaries
SIZE=$(subst gcc,size,$(CC))
AR=$(subst gcc,ar,$(CC))
OBJDUMP=$(subst gcc,objdump,$(CC))

# If release isn't specfied build with debug symbols
ifneq ($(TYPE),release)
	CFLAGS+=-g3 -Og
else
	CFLAGS+=-O3
endif

# Setup some build strings
BUILD_TARGET=@echo "Building Target $@"
BUILD_WITH=@echo "Building with $<"

# Set PHONY for all targets that don't have outputs for tracking
.PHONY: build compile-all debug allasm alli allobjdump clean super-clean test

# Build will build project
build: $(OBJS)
	$(MAKE) $(PROJECT_OUT_FILE)

# PHONY target so you don't have to type .out
test:
	@echo $(MAKECMDGOALS)
	@echo $(CFLAGS)
	$(MAKE) $(UNIT_TEST_OUT)

# Build the unit test binary
$(UNIT_TEST_OUT): CFLAGS+=-I$(CMOCKA_INC_OUT_DIR) -D UNITTEST
$(UNIT_TEST_OUT): $(CMOCKA_LIB) $(TEST_OBJS)
	$(CC) $(CFLAGS) -o "$@" $(TEST_OBJS) $(CMOCKA_LIB)

# Build the cmocka library
$(CMOCKA_LIB):
	@echo "Installing and making CMOCKA static library"
	$(MK_DIR) $(CMOCKA_BUILD_DIR)
	$(CD) $(CMOCKA_BUILD_DIR) && \
	$(CMAKE_CMD) \
	-DCMAKE_INSTALL_PREFIX=. \
	-DCMAKE_BUILD_TYPE=debug \
	-DCMAKE_C_COMPILER=$(CC) \
	-DWITH_STATIC_LIB=ON \
	../cmocka/ && \
	$(MAKE_CMD) install

# Build will build project library
build-lib: $(OBJS)
	$(MAKE) $(LIB_OUT_FILE)

# Debug target is just used for debugging the make file
debug:
	@echo "Debug output"
	@echo $(ARM_PROP_OBJS)
	@echo $(ARM_PROP_SRCS)
	@echo $(OUT_DIR)
	@echo $(TEST_OBJS)

# Compiles the project into project binary which can be executed with ./project*.out
$(PROJECT_OUT_FILE): CFLAGS+=$(MAP_FLAG) $(DEFINE) $(VERB) -pthread
$(PROJECT_OUT_FILE): $(OBJS)
	$(BUILD_TARGET)
	$(CC) $(CFLAGS) -o "$@" $(OBJS)
	$(SIZE) $@

# Build the library file for static linking
$(LIB_OUT_FILE): $(OBJS)
	$(BUILD_TARGET)
	$(AR) cr $@ $(OBJS)

# Builds all objects files but doesn't link
compile-all: $(OBJS)

# Builds an asm file
%.asm : $(APP_SRC_DIR)/%.c
	$(BUILD_TARGET)
	$(BUILD_WITH)
	$(CC) -S $(CFLAGS) -c -o "$@" "$<"

# Build all asm files
allasm :
	@echo "Building all asm files"
	$(MAKE) $(patsubst %.c,%.asm,$(SRC_LIST))

# Build an i file
%.i : $(APP_SRC_DIR)/%.c
	$(BUILD_TARGET)
	$(BUILD_WITH)
	$(CC) -E $(CFLAGS) -c -o "$@" "$<"

# Build all i files
alli :
	@echo "Building all i files"
	$(MAKE) $(patsubst %.c,%.i,$(SRC_LIST))

# Builds .o file when a .c file is specified by command line
%.o: $(APP_SRC_DIR)/%.c
	$(BUILD_TARGET)
	$(BUILD_WITH)
	$(CC) $(CFLAGS) -c -o "$@" "$<"

# Covert all object file to objdump files
allobjdump: $(OBJS) $(PROJECT_OUT_FILE)
	@echo "Build all objdump files"
	$(MAKE) $(patsubst %.o,%.objdump,$(OBJS))
	$(MAKE) $(PROJECT_OUT_FILE).objdump

# Do an objdump on a file
%.objdump: %.o
	$(BUILD_TARGET)
	$(BUILD_WITH)
	$(OBJDUMP) -D "$<" > "$@"

# Do an objdump on the project output file
%.out.objdump: %.out
	$(BUILD_TARGET)
	$(BUILD_WITH)
	$(OBJDUMP) -D "$<" > "$@"

# Builds .o files from .c files
$(OUT_DIR)/%.o: CFLAGS+=-MD -MP $(DEFINE) $(VERB)
$(OUT_DIR)/%.o: $(APP_SRC_DIR)/%.c
	$(BUILD_TARGET)
	$(BUILD_WITH)
	$(shell $(MK_DIR) $(OUT_DIR))
	$(CC) $(CFLAGS) -c -o "$@" "$<"

# List of items that need to be cleaned
CLEAN+=$(OUT) \
       $(MAPS) \
       $(PROJECT_FILE)* \
       *.i \
       *.asm \
       *.o \
       *.objdump \
       $(UNIT_TEST_OUT) \
       $(LIB_OUT_FILE) \
       $(CMOCKA_LIB)

# Clean should clean up all .elf, .map, .o, .d, and .S files created during
# build
clean:
	$(RM) $(CLEAN)

# Cleans everything including the cmocka build dir
super-clean:
	$(RM) $(CLEAN) $(CMOCKA_BUILD_DIR)
