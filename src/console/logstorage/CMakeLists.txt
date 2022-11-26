add_definitions(-Werror)

set(mct-logstorage-ctrl_SRCS
    mct-logstorage-ctrl.c
    mct-logstorage-common.c
    mct-logstorage-list.c)

if(WITH_MCT_LOGSTORAGE_CTRL_UDEV)
    set(mct-logstorage-ctrl_SRCS
        ${mct-logstorage-ctrl_SRCS}
        mct-logstorage-udev.c)
    set(LOGSTORAGE_LIBRARY udev)
endif()

if(WITH_MCT_LOGSTORAGE_CTRL_PROP)
    set(mct-logstorage-ctrl_SRCS
        ${mct-logstorage-ctrl_SRCS}
        mct-logstorage-prop.c)
    set(LOGSTORAGE_LIBRARY automounter)
endif()

add_executable(mct-logstorage-ctrl
               ${mct-logstorage-ctrl_SRCS})
target_link_libraries(mct-logstorage-ctrl mct ${LOGSTORAGE_LIBRARY} mct_control_common_lib)
set_target_properties(mct-logstorage-ctrl PROPERTIES LINKER_LANGUAGE C)

install(TARGETS mct-logstorage-ctrl
        RUNTIME DESTINATION bin
        COMPONENT base)
