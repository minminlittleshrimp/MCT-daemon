
set(mct_control_common_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/mct-control-common.c)
add_library(mct_control_common_lib STATIC ${mct_control_common_SRCS})
target_link_libraries(mct_control_common_lib mct)

set(TARGET_LIST mct-log-reader mct-log-converter)
add_subdirectory(logstorage)

if(WITH_MCT_CONSOLE_SBTM)
    list(APPEND TARGET_LIST mct-sortbytimestamp)
endif()

foreach(target IN LISTS TARGET_LIST)
    set(target_SRCS ${target})
    add_executable(${target} ${target_SRCS})
    target_link_libraries(${target} mct)
    set_target_properties(${target} PROPERTIES LINKER_LANGUAGE C)
    install(TARGETS ${target}
            RUNTIME DESTINATION bin
            COMPONENT base)
endforeach()