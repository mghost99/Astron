cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
project(Astron)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Find Python 3 for running tests
find_package(Python3 COMPONENTS Interpreter REQUIRED)
enable_testing()
set(PYTHON_EXECUTABLE "${Python3_EXECUTABLE}")

### Configure CMake to use our extra Find modules ###
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")


### Define extra windows defines ###
if(WIN32)
	add_definitions(-DWIN32_LEAN_AND_MEAN)

	add_definitions(-D_WIN32_WINDOWS)
	add_definitions(-D_WIN32_WINNT=0x0600)
	add_definitions(-DNOMINMAX) # min() and max() macros conflict with <limits>
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	add_definitions(-D_WINSOCK_DEPRECATED_NO_WARNINGS)
	if(MSVC)
		add_definitions("/wd4267") # 64-bit size_t complaints
	endif()
else()
	option(STATIC_ASTRON "Builds a self-contained Astron binary (Non-Win32 only)" OFF)
endif()

if(STATIC_ASTRON)
	# !!! DO NOT SET THIS UNLESS YOU KNOW WHAT YOU'RE DOING !!!
	# Static Astron builds are still *very* experimental, and there are still issues with linking the C++ Mongo driver in some instances.
	set(CMAKE_EXE_LINK_DYNAMIC_C_FLAGS)
	set(CMAKE_EXE_LINK_DYNAMIC_CXX_FLAGS)
	set(CMAKE_SHARED_LIBRARY_C_FLAGS)
	set(CMAKE_SHARED_LIBRARY_CXX_FLAGS)
	set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS)
	set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS)
	set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
	set(CMAKE_EXE_LINKER_FLAGS "-static")
endif()

### Set build type to Release with Debug Info by default ###
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type.")
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
	RelWithDebInfo Release Debug)

### Define astron's git sha for use in program ###
if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
	find_package(Git)
	if(GIT_FOUND)
		execute_process(
			COMMAND "${GIT_EXECUTABLE}" describe --abbrev=8 --dirty --always
			WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
			OUTPUT_VARIABLE GIT_SHA_VERSION
			OUTPUT_STRIP_TRAILING_WHITESPACE
		)

		add_definitions(-DGIT_SHA1="${GIT_SHA_VERSION}")
	endif()
endif()


### Add per-compiler flags ###
# Common flags:
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR
   CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wno-error=deprecated-declarations -Wno-error=free-nonheap-object")
	
	# Release optimizations: aggressive optimization + LTO
	set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto -DNDEBUG" CACHE STRING "Release flags" FORCE)
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -march=native -flto -g -DNDEBUG" CACHE STRING "RelWithDebInfo flags" FORCE)
	set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-flto" CACHE STRING "Release linker flags" FORCE)
	set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "-flto" CACHE STRING "RelWithDebInfo linker flags" FORCE)
endif()

# Clang-specific flags:
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=deprecated-register")
endif()

### Debug Flags -- compile in spam/debug log messages ###
# Note: Ignoring 'free-nonheap-object' warns due to Bison issue in the generated parser.cpp file.
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g3 -O0 -DASTRON_DEBUG_MESSAGES -Wno-error=free-nonheap-object")


### OS Specific Settings ###
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")	### Windows requires winsock
	list(APPEND EXTRA_LIBS wsock32 ws2_32)
	# Windows system libraries required by MongoDB C driver
	list(APPEND EXTRA_LIBS dnsapi bcrypt ncrypt secur32 crypt32)
	# Compression libraries required by MongoDB C driver
	list(APPEND EXTRA_LIBS snappy zstd)
	# Fix for weak symbol multiple definitions with MinGW static libraries (MongoDB)
	if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-multiple-definition")
	endif()
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")		### Linux requires pthreads
	list(APPEND EXTRA_LIBS pthread rt)
endif()

### YAML Depedency -- required by config, used by yaml database ###
# Note: On Windows MinGW, use DLL version due to static lib having DLL exports
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	set(YAMLCPP_USE_STATIC_LIBS OFF CACHE BOOL "If true, will try to find static YamlCpp first instead of dynamic.")
else()
	set(YAMLCPP_USE_STATIC_LIBS ON CACHE BOOL "If true, will try to find static YamlCpp first instead of dynamic.")
endif()
find_package(YamlCpp REQUIRED)
include_directories(${YAMLCPP_INCLUDE_DIR})
link_directories(${YAMLCPP_LIBRARY_DIR})
message(STATUS "Found the following YamlCpp library:")
message("  ${YAMLCPP_LIBRARY}\n")

set(BUILD_YAML ON) # ON CACHE BOOL "Stuff"
if(BUILD_YAML)
	add_definitions(-DASTRON_WITH_YAML)
endif()

# libuv dependency -- required by networking
find_package(libuv REQUIRED)
include_directories(${LIBUV_INCLUDE_DIR})
link_directories(${LIBUV_LIBRARY_DIR})
message(STATUS "Found the libuv library:")
message("	${LIBUV_LIBRARY}\n")

# We only really need this for Boost's ICL at this point
# Use CONFIG mode for modern CMake to avoid deprecated FindBoost module
find_package(Boost REQUIRED CONFIG)
if(Boost_FOUND)
	include_directories(${Boost_INCLUDE_DIRS})
endif()
add_definitions(-DBOOST_ERROR_CODE_HEADER_ONLY)

set(USE_32BIT_DATAGRAMS OFF CACHE BOOL
	"If on, datagrams and dclass fields will use 32-bit length tags instead of 16-bit.")
if(USE_32BIT_DATAGRAMS)
	add_definitions(-DASTRON_32BIT_DATAGRAMS)
	add_definitions(-DDCLASS_32BIT_SIZETAG)
endif()
set(USE_128BIT_CHANNELS OFF CACHE BOOL
	"If on, channels will be 128-bit and doids and zones will be 64-bit (instead of 64/32).")
if(USE_128BIT_CHANNELS)
	add_definitions(-DASTRON_128BIT_CHANNELS)
endif()

set(BUILD_TESTS OFF CACHE BOOL "If set to true, test files will be compiled in")
if(BUILD_TESTS)
	set(TEST_FILES
		src/tests/MDParticipantTest.cpp
		src/tests/MDPerformanceTest.cpp
	)
endif()

# DB backends can add libraries here as needed:
set(DB_LIBRARY_NAMES)

set(BUILD_DBSERVER ON CACHE BOOL "If on, Database Server will be built into binary")
if(BUILD_DBSERVER)
	add_definitions(-DBUILD_DBSERVER)
	set(DBSERVER_FILES
		src/database/DatabaseServer.h
		src/database/DatabaseServer.cpp
		src/database/DatabaseBackend.h
		src/database/DatabaseBackend.cpp
		src/database/DBOperation.h
		src/database/DBOperation.cpp
		src/database/DBOperationQueue.h
		src/database/DBOperationQueue.cpp
		src/database/OldDatabaseBackend.h
		src/database/OldDatabaseBackend.cpp
		src/database/DBBackendFactory.h
		src/database/DBBackendFactory.cpp
	)

	set(BUILD_DB_YAML ON CACHE BOOL "If on, will support a YAML-based database (for development)")
	if(BUILD_DB_YAML)
		add_definitions(-DBUILD_DB_YAML)
		set(DBSERVER_FILES
			${DBSERVER_FILES}
			src/database/YAMLDatabase.cpp
		)
		add_test(db_yaml "${PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/test/test_dbserver_yaml.py")
		add_test(validate_config_dbyaml "${PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/test/test_config_dbyaml.py")
		set(PYTHON_TESTS ${PYTHON_TESTS} db_yaml validate_config_dbyaml)
	endif()

	### Check for the presence of the MongoDB client library ###
	# Modern MongoDB C++ driver uses pkg-config
	find_package(PkgConfig QUIET)
	if(PKG_CONFIG_FOUND)
		pkg_check_modules(LIBMONGOCXX QUIET libmongocxx)
		pkg_check_modules(LIBBSONCXX QUIET libbsoncxx)
	endif()

	# Fall back to legacy Find modules if needed
	if(NOT LIBMONGOCXX_FOUND)
		find_package(libmongocxx QUIET)
		find_package(libbsoncxx QUIET)
		find_package(libmongocxx-static QUIET)
		find_package(libbsoncxx-static QUIET)
	endif()

	if(LIBMONGOCXX_FOUND OR LIBMONGOCXX_LIBRARIES OR LIBMONGOCXX_STATIC_LIBRARIES)
		set(BUILD_DB_MONGO ON CACHE BOOL "If on, will support MongoDB.")
		message(STATUS "MongoDB client library found")
		if(LIBMONGOCXX_FOUND)
			message(STATUS "  libmongocxx version: ${LIBMONGOCXX_VERSION}")
			message(STATUS "  libbsoncxx version: ${LIBBSONCXX_VERSION}")
		endif()
	else()
		unset(BUILD_DB_MONGO CACHE)
		message(STATUS "MongoDB client library not found")
	endif()
	
	if(BUILD_DB_MONGO)
		list(APPEND DBSERVER_FILES
			src/database/MongoDatabase.cpp)
		
		# Use pkg-config results if available
		if(LIBMONGOCXX_FOUND AND PKG_CONFIG_FOUND)
			include_directories(${LIBMONGOCXX_INCLUDE_DIRS} ${LIBBSONCXX_INCLUDE_DIRS})
			link_directories(${LIBMONGOCXX_LIBRARY_DIRS} ${LIBBSONCXX_LIBRARY_DIRS})
			list(APPEND DB_LIBRARY_NAMES ${LIBMONGOCXX_LIBRARIES} ${LIBBSONCXX_LIBRARIES})
		# Fall back to legacy detection
		elseif(LIBMONGOCXX_LIBRARIES)
			include_directories(${LIBMONGOCXX_INCLUDE_DIRS} ${LIBBSONCXX_INCLUDE_DIRS})
			link_directories(${LIBMONGOCXX_LIBRARY_DIRS} ${LIBBSONCXX_LIBRARY_DIRS})
			list(APPEND DB_LIBRARY_NAMES ${LIBMONGOCXX_LIBRARIES} ${LIBBSONCXX_LIBRARIES})
		elseif(LIBMONGOCXX_STATIC_LIBRARIES)
			include_directories(${LIBMONGOCXX_STATIC_INCLUDE_DIRS} ${LIBBSONCXX_STATIC_INCLUDE_DIRS})
			link_directories(${LIBMONGOCXX_STATIC_LIBRARY_DIRS} ${LIBBSONCXX_STATIC_LIBRARY_DIRS})
			list(APPEND DB_LIBRARY_NAMES ${LIBMONGOCXX_STATIC_LIBRARIES} ${LIBBSONCXX_STATIC_LIBRARIES})
		else()
			message(FATAL_ERROR "BUILD_DB_MONGO is set but no libraries were found, this is never supposed to happen!")	
		endif()
		
		add_test(db_mongo "${PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/test/test_dbserver_mongodb.py")
		add_test(validate_config_dbmongo "${PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/test/test_config_dbmongo.py")
		list(APPEND PYTHON_TESTS db_mongo)
	endif()

	### Check for soci and if available, compile SQL database support ###
	find_package(Soci COMPONENTS postgresql mysql sqlite3)

	if(SOCI_FOUND)
		message(STATUS "Found the following Soci library:")
		message("  ${SOCI_LIBRARY}\n")

		message(STATUS "Found the following Soci plugins:")
		### PostgreSQL support ##
		if(SOCI_postgresql_FOUND)
			set(BUILD_DB_POSTGRESQL ON CACHE BOOL "If on, will support PostgreSQL.")
			message(STATUS "  PostgreSQL found")
			message("	 ${SOCI_postgresql_PLUGIN}")
		else()
			unset(BUILD_DB_POSTGRESQL CACHE)
			message(STATUS "  PostgreSQL not found")
			message("	 won't add PostgreSQl support")
		endif()

		### MySQL support ###
		if(SOCI_mysql_FOUND)
			set(BUILD_DB_MYSQL ON CACHE BOOL "If on, will support MySQL.")
			message(STATUS "  MySQL found")
			message("	 ${SOCI_mysql_PLUGIN}")
		else()
			unset(BUILD_DB_MYSQL CACHE)
			message(STATUS "  MySQL not found")
			message("	 won't add MySQL support")
		endif()

		### SQLite3 support ###
		if(SOCI_sqlite3_FOUND)
			set(BUILD_DB_SQLITE ON CACHE BOOL "If on, will support SQLite3.")
			message(STATUS "  SQLite3 found")
			message("	 ${SOCI_sqlite3_PLUGIN}")
		else()
			unset(BUILD_DB_SQLITE CACHE)
			message(STATUS "  SQLite3 not found")
			message("	 won't add SQLite3 support")
		endif()

		message("")

		### Add files to compiler and linker ###
		if(BUILD_DB_SQLITE OR BUILD_DB_MYSQL OR BUILD_DB_POSTGRESQL)
			add_definitions(-DBUILD_DB_SQL)
			include_directories(${SOCI_INCLUDE_DIRS})
			link_directories(${SOCI_LIBRARY_DIR})
		set(DBSERVER_FILES
			${DBSERVER_FILES}
			src/database/SociSQLDatabase.cpp
		)

			list(APPEND DB_LIBRARY_NAMES ${SOCI_LIBRARY})
			if(NOT WIN32)
				list(APPEND DB_LIBRARY_NAMES dl)
			endif()
			if(BUILD_DB_POSTGRESQL)
			list(APPEND DB_LIBRARY_NAMES pq)
			add_test(db_pgsql 
				"${PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/test/test_dbserver_postgres.py")
			add_test(validate_config_dbpostgres
				"${PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/test/test_config_dbpostgres.py")
				set(PYTHON_TESTS ${PYTHON_TESTS} db_pgsql validate_config_dbpostgres)
			endif()
		endif()
	else()
		message(STATUS "Soci not found")
		message("  won't add SQL support")
		unset(BUILD_DB_MYSQL CACHE)
		unset(BUILD_DB_POSTGRESQL CACHE)
		unset(BUILD_DB_SQLITE CACHE)
	endif()
else()
	unset(BUILD_DB_FILESYSTEM CACHE)
	unset(BUILD_DB_YAML CACHE)
	unset(BUILD_DB_MYSQL CACHE)
	unset(BUILD_DB_POSTGRESQL CACHE)
	unset(BUILD_DB_SQLITE CACHE)
	unset(BUILD_DB_MONGO CACHE)
endif()

set(BUILD_STATESERVER ON CACHE BOOL "If on, State Server will be built into binary")
if(BUILD_STATESERVER)
	set(STATESERVER_FILES
		src/stateserver/StateServer.cpp
		src/stateserver/StateServer.h
		src/stateserver/DistributedObject.cpp
		src/stateserver/DistributedObject.h
	)
	add_test(stateserver "${PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/test/test_stateserver.py")
	add_test(validate_config_stateserver "${PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/test/test_config_stateserver.py")
	set(PYTHON_TESTS ${PYTHON_TESTS} stateserver validate_config_stateserver)

	set(BUILD_STATESERVER_DBSS ON CACHE BOOL "If on, the Database-State Server will be built into binary")
	if(BUILD_STATESERVER_DBSS)
		set(STATESERVER_FILES
			${STATESERVER_FILES}
			src/stateserver/DBStateServer.h
			src/stateserver/DBStateServer.cpp
			src/stateserver/LoadingObject.h
			src/stateserver/LoadingObject.cpp
		)
		add_test(dbss "${PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/test/test_dbss.py")
		add_test(validate_config_dbss "${PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/test/test_config_dbss.py")
		set(PYTHON_TESTS ${PYTHON_TESTS} dbss validate_config_dbss)
	endif()
else()
	unset(BUILD_STATESERVER_DBSS CACHE)
endif()

set(BUILD_EVENTLOGGER ON CACHE BOOL "If on, Event Logger will be built into binary")
if(BUILD_EVENTLOGGER)
	set(EVENTLOGGER_FILES
		src/eventlogger/EventLogger.cpp
		src/eventlogger/EventLogger.h
	)
	add_test(eventlogger "${PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/test/test_eventlogger.py")
	add_test(validate_config_eventlogger "${PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/test/test_config_eventlogger.py")
endif()

set(BUILD_CLIENTAGENT ON CACHE BOOL "If on, Client Agent will be built into binary")
if(BUILD_CLIENTAGENT)
	set(CLIENTAGENT_FILES
		src/clientagent/Client.h
		src/clientagent/Client.cpp
		src/clientagent/ClientAgent.h
		src/clientagent/ClientAgent.cpp
		src/clientagent/ClientMessages.h
		src/clientagent/ClientFactory.cpp
		src/clientagent/ClientFactory.h
		src/clientagent/AstronClient.cpp
	)
	add_test(clientagent "${PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/test/test_clientagent.py")
	add_test(validate_config_clientagent "${PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/test/test_config_clientagent.py")
	set(PYTHON_TESTS ${PYTHON_TESTS} clientagent validate_config_clientagent)
endif()

### Define core components ###
set(CORE_FILES
	src/core/global.h
	src/core/global.cpp
	src/core/main.cpp
	src/core/Logger.h
	src/core/Logger.cpp
	src/core/Role.h
	src/core/Role.cpp
	src/core/RoleFactory.h
	src/core/RoleFactory.cpp
	src/core/shutdown.h
	src/core/shutdown.cpp
	src/core/types.h
	src/core/msgtypes.h
)

set(CONFIG_FILES
	src/config/ConfigGroup.h
	src/config/ConfigGroup.cpp
	src/config/ConfigVariable.h
	src/config/ConfigVariable.cpp
	src/config/constraints.cpp
)

set(MESSAGEDIRECTOR_FILES
	src/messagedirector/ChannelMap.cpp
	src/messagedirector/ChannelMap.h
	src/messagedirector/MessageDirector.cpp
	src/messagedirector/MessageDirector.h
	src/messagedirector/MDNetworkParticipant.cpp
	src/messagedirector/MDNetworkParticipant.h
	src/messagedirector/MDNetworkUpstream.cpp
	src/messagedirector/MDNetworkUpstream.h
)

set(UTIL_FILES
	src/util/Datagram.h
	src/util/DatagramIterator.h
	src/util/EventSender.cpp
	src/util/EventSender.h
	src/config/ConfigSchema.cpp
	src/config/ConfigValidator.cpp
	src/config/ConfigSection.cpp
	src/util/NetContext.cpp
	src/util/NetContext.h
	src/util/Timeout.cpp
	src/util/Timeout.h
	src/util/TaskQueue.cpp
	src/util/TaskQueue.h
)

set(NET_FILES
	src/net/address_utils.cpp
	src/net/address_utils.h
	src/net/HAProxyHandler.cpp
	src/net/HAProxyHandler.h
	src/net/NetworkAcceptor.cpp
	src/net/NetworkAcceptor.h
	src/net/NetworkClient.cpp
	src/net/NetworkClient.h
	src/net/NetworkConnector.cpp
	src/net/NetworkConnector.h
	src/net/TcpAcceptor.cpp
	src/net/TcpAcceptor.h
)

include_directories(src)
add_test(validate_config_core "${PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/test/test_config.py")
add_test(messagedirector "${PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/test/test_messagedirector.py")
set(PYTHON_TESTS ${PYTHON_TESTS} messagedirector)
add_executable(astrond
	${CORE_FILES}
	${CONFIG_FILES}
	${MESSAGEDIRECTOR_FILES}
	${UTIL_FILES}
	${NET_FILES}
	${STATESERVER_FILES}
	${EVENTLOGGER_FILES}
	${CLIENTAGENT_FILES}
	${TEST_FILES}
	${DBSERVER_FILES}
)

### Setup source groups and defines for configurables ###
source_group("Core" FILES ${CORE_FILES})
source_group("Config" FILES ${CONFIG_FILES})
source_group("Util" FILES ${UTIL_FILES})
source_group("MessageDirector" FILES ${MESSAGEDIRECTOR_FILES})
source_group("Net" FILES ${NET_FILES})
if(STATESERVER_FILES)
	add_definitions(-DBUILD_STATESERVER)
	source_group("StateServer" FILES ${STATESERVER_FILES})
endif()
if(EVENTLOGGER_FILES)
	add_definitions(-DBUILD_EVENTLOGGER)
	source_group("EventLogger" FILES ${EVENTLOGGER_FILES})
endif()
if(CLIENTAGENT_FILES)
	add_definitions(-DBUILD_CLIENTAGENT)
	source_group("ClientAgent" FILES ${CLIENTAGENT_FILES})
endif()
if(DBSERVER_FILES)
	add_definitions(-DBUILD_STATESERVER_DBSS)
	source_group("DatabaseServer" FILES ${DBSERVER_FILES})
endif()
if(TEST_FILES)
	source_group("DaemonTests" FILES ${TEST_FILES})
endif()

### Header source group ###
#foreach(sourcefile IN LISTS ASTROND_SOURCES)
#	if(sourcefile MATCHES .*\\.h)
#		set(ASTROND_HEADERS "${ASTROND_HEADERS} ${sourcefile}")
#	endif()
#endforeach(sourcefile)
#source_group("Header Files" FILES ${ASTROND_HEADERS})

add_subdirectory(src/dclass)

add_dependencies(astrond dclass)
target_link_libraries(astrond
    dclass
    ${YAMLCPP_LIBRARY}
    ${DB_LIBRARY_NAMES}
    ${LIBUV_LIBRARY}
    ${EXTRA_LIBS}
)

# Copy runtime DLLs and executable to built/ folder on Windows MinGW builds
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	add_custom_command(TARGET astrond POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E echo "Creating built/ folder for standalone distribution..."
		COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/built"
		# Copy astrond.exe
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:astrond>" "${CMAKE_SOURCE_DIR}/built/"
		# Core MinGW runtime DLLs
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "C:/msys64/mingw64/bin/libgcc_s_seh-1.dll" "${CMAKE_SOURCE_DIR}/built/"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "C:/msys64/mingw64/bin/libwinpthread-1.dll" "${CMAKE_SOURCE_DIR}/built/"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "C:/msys64/mingw64/bin/libstdc++-6.dll" "${CMAKE_SOURCE_DIR}/built/"
		# Dependency DLLs
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "C:/msys64/mingw64/bin/libyaml-cpp.dll" "${CMAKE_SOURCE_DIR}/built/"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "C:/msys64/mingw64/bin/libuv-1.dll" "${CMAKE_SOURCE_DIR}/built/"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "C:/msys64/mingw64/bin/libsoci_core_4_1.dll" "${CMAKE_SOURCE_DIR}/built/"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "C:/msys64/mingw64/bin/libsnappy.dll" "${CMAKE_SOURCE_DIR}/built/"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "C:/msys64/mingw64/bin/libzstd.dll" "${CMAKE_SOURCE_DIR}/built/"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "C:/msys64/mingw64/bin/libmongocrypt.dll" "${CMAKE_SOURCE_DIR}/built/"
		COMMAND ${CMAKE_COMMAND} -E echo "Portable build created in: ${CMAKE_SOURCE_DIR}/built/"
		COMMENT "Creating standalone distribution in built/ folder"
	)
endif()

install(TARGETS astrond DESTINATION bin)

### Handle some final testing configuration ###
if(USE_32BIT_DATAGRAMS)
	set(PYTHON_TEST_ENV ${PYTHON_TEST_ENV} "USE_32BIT_DATAGRAMS=true")
endif()
if(USE_128BIT_CHANNELS)
	set(PYTHON_TEST_ENV ${PYTHON_TEST_ENV} "USE_128BIT_CHANNELS=true")
endif()
if(PYTHON_TEST_ENV)
	set_tests_properties(${PYTHON_TESTS} PROPERTIES ENVIRONMENT "${PYTHON_TEST_ENV}")
endif()
