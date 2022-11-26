if(WITH_DLT_DISABLE_MACRO)
    set(DLT_DISABLE_MACRO 1)
endif()

configure_file(mct_user.h.in mct_user.h)

set(HEADER_LIST mct.h mct_user_macros.h mct_client.h mct_protocol.h
                mct_common.h mct_types.h mct_offline_trace.h
                mct_common_api.h
                ${CMAKE_CURRENT_BINARY_DIR}/mct_version.h
                ${CMAKE_CURRENT_BINARY_DIR}/mct_user.h)

if(WITH_DLT_DISABLE_MACRO)
    list(REMOVE_ITEM HEADER_LIST mct_user_macros.h)
endif()

install(FILES ${HEADER_LIST}
        DESTINATION include/mct
        COMPONENT devel)

