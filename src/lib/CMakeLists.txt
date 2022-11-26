set(mct_LIB_SRCS
    mct_user.c
    mct_client.c
    mct_env_ll.c
    ${PROJECT_SOURCE_DIR}/src/shared/mct_common.c
    ${PROJECT_SOURCE_DIR}/src/shared/mct_protocol.c
    ${PROJECT_SOURCE_DIR}/src/shared/mct_user_shared.c
    )


add_library(mct ${mct_LIB_SRCS})

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set(RT_LIBRARY rt)
    set(SOCKET_LIBRARY "")
else()
    set(RT_LIBRARY "")
    set(SOCKET_LIBRARY socket)
endif()

if(HAVE_FUNC_PTHREAD_SETNAME_NP)
    add_definitions(-DMCT_USE_PTHREAD_SETNAME_NP)
    message(STATUS "Using pthread_setname_np API to set thread name")
else()
    message(STATUS "pthread_setname_np API not available on this platform")
endif()

target_link_libraries(mct ${RT_LIBRARY} ${SOCKET_LIBRARY} Threads::Threads)

target_include_directories(mct
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/mct>
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include/mct>
        $<INSTALL_INTERFACE:include>
)

set_target_properties(mct PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})

add_library(MCT::mct ALIAS mct)

install(TARGETS mct
    EXPORT
        ${PROJECT_NAME}-targets
    RUNTIME
        DESTINATION bin
        COMPONENT base
    LIBRARY
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT base
    ARCHIVE
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/static
        COMPONENT base)

# Install *-targets.cmake file
install(
    EXPORT ${PROJECT_NAME}-targets
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
    NAMESPACE MCt::
)

# Create *-config.cmake file
include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/${PROJECT_NAME}-config.cmake.in
    ${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
    NO_SET_AND_CHECK_MACRO
)

# Install *-config.cmake file
install(
    FILES
        ${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config.cmake
    DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)
