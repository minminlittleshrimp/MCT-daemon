

set(TARGET_LIST "")
list(APPEND TARGET_LIST dlt-receive)
add_subdirectory(logstorage)

foreach(target IN LISTS TARGET_LIST)
    set(target_SRCS ${target})
    add_executable(${target} ${target_SRCS})
    target_link_libraries(${target} dlt)
    set_target_properties(${target} PROPERTIES LINKER_LANGUAGE C)

    install(TARGETS ${target}
            RUNTIME DESTINATION bin
            COMPONENT base)
endforeach()
