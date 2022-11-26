cmake_minimum_required(VERSION 3.3)
project(mct VERSION 1.0.0)

mark_as_advanced(CMAKE_BACKWARDS_COMPATIBILITY)
set(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS TRUE)

include(GNUInstallDirs)
# Set version parameters
set(MCT_VERSION_STATE STABLE)

# set default build type, if not defined by user
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
                         "Choose build type: Debug, Release, RelWithDebInfo, MinSizeRel."
                         FORCE)
    message(STATUS "Build type not defined. Using default build type 'RelWithDebInfo'.")
endif()

# Set of indiviual options
option(BUILD_SHARED_LIBS      "Set to ON to build dynamic libraries"                                                         OFF)
option(WITH_MCT_CONSOLE       "Set to ON to build src/console binaries"                                                      ON)
option(WITH_MCT_CONSOLE_SBTM  "Set to ON to build mct-sortbytimestamp under src/console"                                     OFF)
set(MCT_IPC "FIFO" CACHE STRING "UNIX_SOCKET,FIFO")
set(MCT_USER "mct-vjpro" CACHE STRING "Set user for process not run as root")
option(WITH_MCT_LOGSTORAGE_CTRL_UDEV "PROTOTYPE! Set to ON to build logstorage control application with udev support"        OFF)
option(WITH_MCT_LOGSTORAGE_CTRL_PROP "PROTOTYPE! Set to ON to build logstorage control application with proprietary support" OFF)
option(WITH_MCT_DISABLE_MACRO "Set to ON to build code without Macro interface support"                                      OFF)



set(LICENSE "Mozilla Public License Version 2.0")

# Build, project and include settings
find_package(Threads REQUIRED)

include_directories(
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_BINARY_DIR}/include/mct
    ${PROJECT_SOURCE_DIR}/include/mct
    ${PROJECT_SOURCE_DIR}/src/shared
    ${PROJECT_SOURCE_DIR}/src/offlinelogstorage
    ${PROJECT_SOURCE_DIR}/src/lib
    ${PROJECT_SOURCE_DIR}/src/daemon
    ${PROJECT_SOURCE_DIR}/src/console
)

add_definitions(-D_GNU_SOURCE)

if(NOT MCT_IPC STREQUAL "UNIX_SOCKET" AND NOT MCT_IPC STREQUAL "FIFO")
    message(FATAL_ERROR "${MCT_IPC} is not a valid value for MCT_IPC")
endif()

add_definitions(-DMCT_DAEMON_USE_${MCT_IPC}_IPC)
add_definitions(-DMCT_LIB_USE_${MCT_IPC}_IPC)
if(NOT MCT_USER_IPC_PATH)
    set(MCT_USER_IPC_PATH "/tmp")
endif()
add_definitions(-DMCT_USER_IPC_PATH="${MCT_USER_IPC_PATH}")

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




if(WITH_MCT_LOGSTORAGE_CTRL_UDEV)
    add_definitions(-DMCT_LOGSTORAGE_CTRL_UDEV_ENABLE)
endif()
if(WITH_MCT_LOGSTORAGE_CTRL_PROP)
    add_definitions(-DHAS_PROPRIETARY_LOGSTORAGE)
endif()

add_subdirectory(doc)
add_subdirectory(src)
add_subdirectory(include)

message(STATUS)
message(STATUS "-------------------------------------------------------------------------------")
message(STATUS "Build for Version ${PROJECT_VERSION} build, version state ${MCT_VERSION_STATE}")
message(STATUS "BUILD_SHARED_LIBS = ${BUILD_SHARED_LIBS}")
message(STATUS "WITH_MCT_CONSOLE = ${WITH_MCT_CONSOLE}")
message(STATUS "WITH_MCT_CONSOLE_SBTM = ${WITH_MCT_CONSOLE_SBTM}")
message(STATUS "MCT_USER = ${MCT_USER}")
message(STATUS "CMAKE_INSTALL_PREFIX = ${CMAKE_INSTALL_PREFIX}")
message(STATUS "CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_HOST_SYSTEM_PROCESSOR = ${CMAKE_HOST_SYSTEM_PROCESSOR}")
message(STATUS "CMAKE_SYSTEM_PROCESSOR = ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "WITH_MCT_LOGSTORAGE_CTRL_UDEV = ${WITH_MCT_LOGSTORAGE_CTRL_UDEV}")
message(STATUS "WITH_MCT_LOGSTORAGE_CTRL_PROP = ${WITH_MCT_LOGSTORAGE_CTRL_PROP}")
message(STATUS "MCT_IPC = ${MCT_IPC} (Path: ${MCT_USER_IPC_PATH})")
message(STATUS "WITH_MCT_DISABLE_MACRO = ${WITH_MCT_DISABLE_MACRO}")
message(STATUS "Change a value with: cmake -D<Variable>=<Value>")
message(STATUS "-------------------------------------------------------------------------------")
message(STATUS)
