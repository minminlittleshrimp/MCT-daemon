set(TARGET_LIST mct-log-writer)

foreach(TARGET IN LISTS TARGET_LIST)
    set(TARGET_SRCS ${TARGET})
    add_executable(${TARGET} ${TARGET_SRCS})
    target_link_libraries(${TARGET} mct)
    set_target_properties(${TARGET} PROPERTIES LINKER_LANGUAGE C)
    install(TARGETS ${TARGET}
            RUNTIME DESTINATION bin
            COMPONENT base)
endforeach()
