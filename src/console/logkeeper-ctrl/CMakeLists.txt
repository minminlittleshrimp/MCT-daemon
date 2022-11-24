add_definitions(-Werror)

set(mct-logkeeper-ctrl_SRCS
    mct-logkeeper-ctrl.c
    mct-logkeeper-common.c
    mct-logkeeper-list.c)

if(WITH_DLT_LOGSTORAGE_CTRL_UDEV)
    set(mct-logkeeper-ctrl_SRCS
        ${mct-logkeeper-ctrl_SRCS}
        mct-logkeeper-udev.c)
    set(LOGSTORAGE_LIBRARY udev)
endif()

add_executable(mct-logkeeper-ctrl
               ${mct-logkeeper-ctrl_SRCS}
               ${dlt_control_common_SRCS})

target_link_libraries(mct-logkeeper-ctrl dlt ${LOGSTORAGE_LIBRARY})
set_target_properties(mct-logkeeper-ctrl PROPERTIES LINKER_LANGUAGE C)

install(TARGETS mct-logkeeper-ctrl
        RUNTIME DESTINATION bin
        COMPONENT base)
