# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.10

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/cyz/user-land-filesystem/fs/cyzfs

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/cyz/user-land-filesystem/fs/cyzfs/build

# Include any dependencies generated for this target.
include CMakeFiles/cyzfs.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/cyzfs.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/cyzfs.dir/flags.make

CMakeFiles/cyzfs.dir/src/cyzfs.c.o: CMakeFiles/cyzfs.dir/flags.make
CMakeFiles/cyzfs.dir/src/cyzfs.c.o: ../src/cyzfs.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/cyz/user-land-filesystem/fs/cyzfs/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object CMakeFiles/cyzfs.dir/src/cyzfs.c.o"
	/usr/bin/x86_64-linux-gnu-gcc-7 $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/cyzfs.dir/src/cyzfs.c.o   -c /home/cyz/user-land-filesystem/fs/cyzfs/src/cyzfs.c

CMakeFiles/cyzfs.dir/src/cyzfs.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/cyzfs.dir/src/cyzfs.c.i"
	/usr/bin/x86_64-linux-gnu-gcc-7 $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/cyz/user-land-filesystem/fs/cyzfs/src/cyzfs.c > CMakeFiles/cyzfs.dir/src/cyzfs.c.i

CMakeFiles/cyzfs.dir/src/cyzfs.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/cyzfs.dir/src/cyzfs.c.s"
	/usr/bin/x86_64-linux-gnu-gcc-7 $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/cyz/user-land-filesystem/fs/cyzfs/src/cyzfs.c -o CMakeFiles/cyzfs.dir/src/cyzfs.c.s

CMakeFiles/cyzfs.dir/src/cyzfs.c.o.requires:

.PHONY : CMakeFiles/cyzfs.dir/src/cyzfs.c.o.requires

CMakeFiles/cyzfs.dir/src/cyzfs.c.o.provides: CMakeFiles/cyzfs.dir/src/cyzfs.c.o.requires
	$(MAKE) -f CMakeFiles/cyzfs.dir/build.make CMakeFiles/cyzfs.dir/src/cyzfs.c.o.provides.build
.PHONY : CMakeFiles/cyzfs.dir/src/cyzfs.c.o.provides

CMakeFiles/cyzfs.dir/src/cyzfs.c.o.provides.build: CMakeFiles/cyzfs.dir/src/cyzfs.c.o


# Object files for target cyzfs
cyzfs_OBJECTS = \
"CMakeFiles/cyzfs.dir/src/cyzfs.c.o"

# External object files for target cyzfs
cyzfs_EXTERNAL_OBJECTS =

cyzfs: CMakeFiles/cyzfs.dir/src/cyzfs.c.o
cyzfs: CMakeFiles/cyzfs.dir/build.make
cyzfs: /usr/lib/x86_64-linux-gnu/libfuse.so
cyzfs: /home/cyz/lib/libddriver.a
cyzfs: CMakeFiles/cyzfs.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/cyz/user-land-filesystem/fs/cyzfs/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking C executable cyzfs"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/cyzfs.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/cyzfs.dir/build: cyzfs

.PHONY : CMakeFiles/cyzfs.dir/build

CMakeFiles/cyzfs.dir/requires: CMakeFiles/cyzfs.dir/src/cyzfs.c.o.requires

.PHONY : CMakeFiles/cyzfs.dir/requires

CMakeFiles/cyzfs.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/cyzfs.dir/cmake_clean.cmake
.PHONY : CMakeFiles/cyzfs.dir/clean

CMakeFiles/cyzfs.dir/depend:
	cd /home/cyz/user-land-filesystem/fs/cyzfs/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/cyz/user-land-filesystem/fs/cyzfs /home/cyz/user-land-filesystem/fs/cyzfs /home/cyz/user-land-filesystem/fs/cyzfs/build /home/cyz/user-land-filesystem/fs/cyzfs/build /home/cyz/user-land-filesystem/fs/cyzfs/build/CMakeFiles/cyzfs.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/cyzfs.dir/depend

