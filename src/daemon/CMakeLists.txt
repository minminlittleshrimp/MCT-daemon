#######
# SPDX license identifier: MPL-2.0
#
# Copyright (C) 2011-2015, BMW AG
#
# This file is part of GENIVI Project DLT - Diagnostic Log and Trace.
#
# This Source Code Form is subject to the terms of the
# Mozilla Public License (MPL), v. 2.0.
# If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
#
# For further information see http://www.genivi.org/.
#######


if(WITH_SYSTEMD_WATCHDOG OR WITH_SYSTEMD)
    message(STATUS "Added ${systemd_SRCS} to mct-daemon")
endif()

set(mct_daemon_SRCS
    mct-daemon.c
    mct_daemon_client.c
    mct_daemon_common.c
    mct_daemon_connection.c
    mct_daemon_event_handler.c
    mct_daemon_offline_logstorage.c
    mct_daemon_serial.c
    mct_daemon_socket.c
    mct_daemon_unix_socket.c
    mct_daemon_filter.c
    ${PROJECT_SOURCE_DIR}/src/lib/mct_client.c
    ${PROJECT_SOURCE_DIR}/src/shared/mct_common.c
    ${PROJECT_SOURCE_DIR}/src/shared/mct_config_file_parser.c
    ${PROJECT_SOURCE_DIR}/src/shared/mct_offline_trace.c
    ${PROJECT_SOURCE_DIR}/src/shared/mct_protocol.c
    ${PROJECT_SOURCE_DIR}/src/shared/mct_user_shared.c
    ${PROJECT_SOURCE_DIR}/src/offlinelogstorage/mct_offline_logstorage.c
    ${PROJECT_SOURCE_DIR}/src/offlinelogstorage/mct_offline_logstorage_behavior.c
    )

set(FILTER_CONFIG mct_message_filter.conf)
add_executable(mct-daemon ${mct_daemon_SRCS} ${systemd_SRCS})

set(RT_LIBRARY rt)
set(SOCKET_LIBRARY "")

target_link_libraries(mct-daemon ${RT_LIBRARY} ${SOCKET_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS mct-daemon
        RUNTIME DESTINATION bin
        PERMISSIONS
            OWNER_EXECUTE OWNER_WRITE OWNER_READ
            GROUP_EXECUTE GROUP_READ
            WORLD_EXECUTE WORLD_READ
        COMPONENT base)

configure_file(${CMAKE_SOURCE_DIR}/src/daemon/mct.conf.in ${PROJECT_BINARY_DIR}/src/daemon/mct.conf)

INSTALL(FILES ${PROJECT_BINARY_DIR}/src/daemon/mct.conf ${FILTER_CONFIG}
	DESTINATION ${CONFIGURATION_FILES_DIR}
	COMPONENT base)

