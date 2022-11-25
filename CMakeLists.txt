cmake_minimum_required(VERSION 3.3)
project(automotive-dlt VERSION 2.18.8)

mark_as_advanced(CMAKE_BACKWARDS_COMPATIBILITY)
set(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS TRUE)

include(GNUInstallDirs)
# Set version parameters
set(DLT_VERSION_STATE STABLE)

execute_process(COMMAND git describe --tags WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE DLT_REVISION
    ERROR_VARIABLE GIT_ERROR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE)
if(DLT_REVISION MATCHES "^$")
    set(PRINT_REVISION "Git revision unavailable")
else(DLT_REVISION MATCHES "")
    string(REPLACE "-" "_" DLT_REVISION ${DLT_REVISION})
    set(PRINT_REVISION ${DLT_REVISION})
endif()

# set default build type, if not defined by user
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
                         "Choose build type: Debug, Release, RelWithDebInfo, MinSizeRel."
                         FORCE)
    message(STATUS "Build type not defined. Using default build type 'RelWithDebInfo'.")
endif()

# Set of indiviual options
option(BUILD_SHARED_LIBS      "Set to OFF to build static libraries"                                                         ON)
option(WITH_DLT_CONSOLE       "Set to ON to build src/console binaries"                                                      ON)
option(WITH_DLT_CONSOLE_SBTM  "Set to ON to build dlt-sortbytimestamp under src/console"                                     OFF)
option(WITH_DLT_EXAMPLES      "Set to ON to build src/examples binaries"                                                     ON)
set(DLT_IPC "FIFO" CACHE STRING "UNIX_SOCKET,FIFO")
set(DLT_USER "genivi" CACHE STRING "Set user for process not run as root")
option(WITH_DLT_LOGSTORAGE_CTRL_UDEV "PROTOTYPE! Set to ON to build logstorage control application with udev support"        OFF)
option(WITH_DLT_LOGSTORAGE_CTRL_PROP "PROTOTYPE! Set to ON to build logstorage control application with proprietary support" OFF)
option(WITH_DLT_DISABLE_MACRO "Set to ON to build code without Macro interface support"                                      OFF)



set(LICENSE "Mozilla Public License Version 2.0")

# Build, project and include settings
find_package(Threads REQUIRED)

include_directories(
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_BINARY_DIR}/include/dlt
    ${PROJECT_SOURCE_DIR}/include/dlt
    ${PROJECT_SOURCE_DIR}/src/shared
    ${PROJECT_SOURCE_DIR}/src/offlinelogstorage
    ${PROJECT_SOURCE_DIR}/src/lib
    ${PROJECT_SOURCE_DIR}/src/daemon
    ${PROJECT_SOURCE_DIR}/src/console
)

add_definitions(-D_GNU_SOURCE)

if(NOT DLT_IPC STREQUAL "UNIX_SOCKET" AND NOT DLT_IPC STREQUAL "FIFO")
    message(FATAL_ERROR "${DLT_IPC} is not a valid value for DLT_IPC")
endif()

add_definitions(-DDLT_DAEMON_USE_${DLT_IPC}_IPC)
add_definitions(-DDLT_LIB_USE_${DLT_IPC}_IPC)
if(NOT DLT_USER_IPC_PATH)
    set(DLT_USER_IPC_PATH "/tmp")
endif()
add_definitions(-DDLT_USER_IPC_PATH="${DLT_USER_IPC_PATH}")

add_compile_options(
    $<$<COMPILE_LANGUAGE:C>:-std=gnu99>
    $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++11>
    -Wall
    -Wextra
    # -pedantic
    -Wno-variadic-macros
    -Wno-strict-aliasing
    )

set(CONFIGURATION_FILES_DIR ${CMAKE_INSTALL_FULL_SYSCONFDIR})
add_definitions(-DCONFIGURATION_FILES_DIR="${CONFIGURATION_FILES_DIR}")

add_subdirectory(cmake)




if(WITH_DLT_LOGSTORAGE_CTRL_UDEV)
    add_definitions(-DDLT_LOGSTORAGE_CTRL_UDEV_ENABLE)
endif()


add_subdirectory(doc)
add_subdirectory(src)
add_subdirectory(include)

message(STATUS)
message(STATUS "-------------------------------------------------------------------------------")
message(STATUS "Build for Version ${PROJECT_VERSION} build ${DLT_REVISION} version state ${DLT_VERSION_STATE}")
message(STATUS "WITH_DLT_CONSOLE = ${WITH_DLT_CONSOLE}")
message(STATUS "WITH_DLT_CONSOLE_SBTM = ${WITH_DLT_CONSOLE_SBTM}")
message(STATUS "WITH_DLT_EXAMPLES = ${WITH_DLT_EXAMPLES}")
message(STATUS "DLT_USER = ${DLT_USER}")
message(STATUS "CMAKE_INSTALL_PREFIX = ${CMAKE_INSTALL_PREFIX}")
message(STATUS "CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_HOST_SYSTEM_PROCESSOR = ${CMAKE_HOST_SYSTEM_PROCESSOR}")
message(STATUS "CMAKE_SYSTEM_PROCESSOR = ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "WITH_DLT_LOGSTORAGE_CTRL_UDEV = ${WITH_DLT_LOGSTORAGE_CTRL_UDEV}")
message(STATUS "WITH_DLT_LOGSTORAGE_CTRL_PROP = ${WITH_DLT_LOGSTORAGE_CTRL_PROP}")
message(STATUS "DLT_IPC = ${DLT_IPC} (Path: ${DLT_USER_IPC_PATH})")
message(STATUS "WITH_DLT_DISABLE_MACRO = ${WITH_DLT_DISABLE_MACRO}")
message(STATUS "Change a value with: cmake -D<Variable>=<Value>")
message(STATUS "-------------------------------------------------------------------------------")
message(STATUS)
