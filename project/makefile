###############################################################################
# See README.md for build information                                         #
###############################################################################

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
ifeq ($(EXERCISE),)
	EXERCISE=4
else
	EXERCISE=$(EXERCISE)
endif

ifeq ($(PROBLEM),)
	PROBLEM=5
else
	PROBLEM=$(PROBLEM)
endif

# Project output file names
EXERCISE_OUT_FILE=exercise$(EXERCISE)problem$(PROBLEM).out
LIB_OUT_FILE=lib$(EXERCISE_FILE)$(EXERCISE).a

# Set CFLAGS used by every build type
CFLAGS=-I$(APP_INC_DIR) \
       -Wall \
       -Werror \
       -std=c99 \
       -D_POSIX_C_SOURCE=199309L \
       -D_DEFAULT_SOURCE \
       -D PROBLEM$(PROBLEM)

# Set colored logs if specified
ifneq ($(COLOR_LOGS),)
	CFLAGS+=-D COLOR_LOGS
endif

# Set no compression
ifeq ($(NO_COMP),)
	CFLAGS+=-D JPEG_COMPRESSION
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

# Set of 2 compilers use
ARM_CC=arm-linux-gnueabihf-gcc
X86_CC=gcc

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
.PHONY: build compile-all debug allasm alli allobjdump clean super-clean

# Build will build project
build: $(OBJS)
	$(MAKE) $(EXERCISE_OUT_FILE)

# Build will build project library
build-lib: $(OBJS)
	$(MAKE) $(LIB_OUT_FILE)

# Debug target is just used for debugging the make file
debug:
	@echo "Debug output"
	@echo $(X86_OBJS)

# Compiles the project into project binary which can be executed with ./project*.out
$(EXERCISE_OUT_FILE): CFLAGS+=$(MAP_FLAG) $(DEFINE) $(VERB) -pthread
$(EXERCISE_OUT_FILE): $(OBJS)
	$(BUILD_TARGET)
	$(CC) $(CFLAGS) -o "$@" $(OBJS) -lm -lrt `pkg-config --libs opencv` -L/usr/lib -lopencv_core -lopencv_flann -lopencv_video
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

# Builds .o file when a .cpp file is specified by command line
%.o: $(APP_SRC_DIR)/%.cpp
	$(BUILD_TARGET)
	$(BUILD_WITH)
	$(CC) $(CFLAGS) -c -o "$@" "$<"

# Builds .o file when a .c file is specified by command line
%.o: $(APP_SRC_DIR)/%.c
	$(BUILD_TARGET)
	$(BUILD_WITH)
	$(CC) $(CFLAGS) -c -o "$@" "$<"

# Covert all object file to objdump files
allobjdump: $(OBJS) $(EXERCISE_OUT_FILE)
	@echo "Build all objdump files"
	$(MAKE) $(patsubst %.o,%.objdump,$(OBJS))
	$(MAKE) $(EXERCISE_OUT_FILE).objdump

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

# Builds .o files from .cpp files
$(OUT_DIR)/%.o: CFLAGS+=-MD -MP $(DEFINE) $(VERB)
$(OUT_DIR)/%.o: $(APP_SRC_DIR)/%.cpp
	$(BUILD_TARGET)
	$(BUILD_WITH)
	$(shell $(MK_DIR) $(OUT_DIR))
	$(CC) $(CFLAGS) -c -o "$@" "$<"

# List of items that need to be cleaned
CLEAN+=$(OUT) \
       $(MAPS) \
       *.i \
       *.asm \
       *.o \
       *.opp \
       exercise*.out \
       *.map \
       *.objdump \
       $(UNIT_TEST_OUT) \
       $(LIB_OUT_FILE) \
       $(CMOCKA_LIB)

# Clean should clean up all .elf, .map, .o, .d, and .S files created during
# build
clean:
	$(RM) $(CLEAN)
