cmake_minimum_required(VERSION 3.15)
project(DistributedClasses)

find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

# Generate parser files with Bison
add_custom_command(
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/file/parser.ypp
    COMMAND ${BISON_EXECUTABLE} --defines=parser.h -o parser.cpp parser.ypp
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/file
    COMMENT "Generating dclass/parser.cpp"
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/file/parser.cpp
           ${CMAKE_CURRENT_SOURCE_DIR}/file/parser.h
)

# Generate lexer files with Flex
add_custom_command(
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/file/lexer.lpp
    COMMAND ${FLEX_EXECUTABLE} -olexer.cpp lexer.lpp
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/file
    COMMENT "Generating dclass/lexer.cpp"
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/file/lexer.cpp
)

add_library(dclass
	# Helper Classes
	util/PrimeNumberGenerator.cpp
	util/HashGenerator.cpp

	# Distributed Object Model
	dc/DistributedType.cpp
	dc/NumericType.cpp
	dc/ArrayType.cpp
	dc/Struct.cpp
	dc/Class.cpp
	dc/Method.cpp

	dc/KeywordList.cpp
	dc/File.cpp
	dc/Field.cpp
	dc/MolecularField.cpp
	dc/Parameter.cpp

	# File and Network I/O
	file/parser.cpp
	file/lexer.cpp
	file/write.cpp
	file/read.cpp
	file/hash_legacy.cpp
	value/format.cpp
	value/parse.cpp
	value/default.cpp
)

# Link flex library (not needed on Windows/MinGW)
if(NOT WIN32)
	target_link_libraries(dclass PRIVATE fl)
endif()

# Set include directories for the target
target_include_directories(dclass PUBLIC 
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
	$<INSTALL_INTERFACE:include>
)

# Workaround for Flex bug #140:
if(NOT MSVC)
	set_source_files_properties(file/lexer.cpp PROPERTIES COMPILE_FLAGS -Wno-sign-compare)
endif()
