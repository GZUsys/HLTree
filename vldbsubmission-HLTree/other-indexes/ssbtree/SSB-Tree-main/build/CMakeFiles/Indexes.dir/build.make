# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.22

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
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
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /root/HXY/ssbtree/SSB-Tree-main

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /root/HXY/ssbtree/SSB-Tree-main/build

# Include any dependencies generated for this target.
include CMakeFiles/Indexes.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/Indexes.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/Indexes.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/Indexes.dir/flags.make

CMakeFiles/Indexes.dir/SSBTree.cpp.o: CMakeFiles/Indexes.dir/flags.make
CMakeFiles/Indexes.dir/SSBTree.cpp.o: ../SSBTree.cpp
CMakeFiles/Indexes.dir/SSBTree.cpp.o: CMakeFiles/Indexes.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/root/HXY/ssbtree/SSB-Tree-main/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/Indexes.dir/SSBTree.cpp.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/Indexes.dir/SSBTree.cpp.o -MF CMakeFiles/Indexes.dir/SSBTree.cpp.o.d -o CMakeFiles/Indexes.dir/SSBTree.cpp.o -c /root/HXY/ssbtree/SSB-Tree-main/SSBTree.cpp

CMakeFiles/Indexes.dir/SSBTree.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/Indexes.dir/SSBTree.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /root/HXY/ssbtree/SSB-Tree-main/SSBTree.cpp > CMakeFiles/Indexes.dir/SSBTree.cpp.i

CMakeFiles/Indexes.dir/SSBTree.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/Indexes.dir/SSBTree.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /root/HXY/ssbtree/SSB-Tree-main/SSBTree.cpp -o CMakeFiles/Indexes.dir/SSBTree.cpp.s

CMakeFiles/Indexes.dir/Epoche.cpp.o: CMakeFiles/Indexes.dir/flags.make
CMakeFiles/Indexes.dir/Epoche.cpp.o: ../Epoche.cpp
CMakeFiles/Indexes.dir/Epoche.cpp.o: CMakeFiles/Indexes.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/root/HXY/ssbtree/SSB-Tree-main/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object CMakeFiles/Indexes.dir/Epoche.cpp.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/Indexes.dir/Epoche.cpp.o -MF CMakeFiles/Indexes.dir/Epoche.cpp.o.d -o CMakeFiles/Indexes.dir/Epoche.cpp.o -c /root/HXY/ssbtree/SSB-Tree-main/Epoche.cpp

CMakeFiles/Indexes.dir/Epoche.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/Indexes.dir/Epoche.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /root/HXY/ssbtree/SSB-Tree-main/Epoche.cpp > CMakeFiles/Indexes.dir/Epoche.cpp.i

CMakeFiles/Indexes.dir/Epoche.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/Indexes.dir/Epoche.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /root/HXY/ssbtree/SSB-Tree-main/Epoche.cpp -o CMakeFiles/Indexes.dir/Epoche.cpp.s

# Object files for target Indexes
Indexes_OBJECTS = \
"CMakeFiles/Indexes.dir/SSBTree.cpp.o" \
"CMakeFiles/Indexes.dir/Epoche.cpp.o"

# External object files for target Indexes
Indexes_EXTERNAL_OBJECTS =

libIndexes.a: CMakeFiles/Indexes.dir/SSBTree.cpp.o
libIndexes.a: CMakeFiles/Indexes.dir/Epoche.cpp.o
libIndexes.a: CMakeFiles/Indexes.dir/build.make
libIndexes.a: CMakeFiles/Indexes.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/root/HXY/ssbtree/SSB-Tree-main/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Linking CXX static library libIndexes.a"
	$(CMAKE_COMMAND) -P CMakeFiles/Indexes.dir/cmake_clean_target.cmake
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/Indexes.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/Indexes.dir/build: libIndexes.a
.PHONY : CMakeFiles/Indexes.dir/build

CMakeFiles/Indexes.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/Indexes.dir/cmake_clean.cmake
.PHONY : CMakeFiles/Indexes.dir/clean

CMakeFiles/Indexes.dir/depend:
	cd /root/HXY/ssbtree/SSB-Tree-main/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /root/HXY/ssbtree/SSB-Tree-main /root/HXY/ssbtree/SSB-Tree-main /root/HXY/ssbtree/SSB-Tree-main/build /root/HXY/ssbtree/SSB-Tree-main/build /root/HXY/ssbtree/SSB-Tree-main/build/CMakeFiles/Indexes.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/Indexes.dir/depend

