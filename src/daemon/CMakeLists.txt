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
    message(STATUS "Added ${systemd_SRCS} to dlt-daemon")
endif()

set(dlt_daemon_SRCS
    dlt-daemon.c
    dlt_daemon_client.c
    dlt_daemon_common.c
    dlt_daemon_connection.c
    dlt_daemon_event_handler.c
    dlt_daemon_offline_logstorage.c
    dlt_daemon_serial.c
    dlt_daemon_socket.c
    dlt_daemon_unix_socket.c
    dlt_daemon_filter.c
    ${PROJECT_SOURCE_DIR}/src/lib/dlt_client.c
    ${PROJECT_SOURCE_DIR}/src/shared/dlt_common.c
    ${PROJECT_SOURCE_DIR}/src/shared/dlt_config_file_parser.c
    ${PROJECT_SOURCE_DIR}/src/shared/dlt_offline_trace.c
    ${PROJECT_SOURCE_DIR}/src/shared/dlt_protocol.c
    ${PROJECT_SOURCE_DIR}/src/shared/dlt_user_shared.c
    ${PROJECT_SOURCE_DIR}/src/offlinelogstorage/dlt_offline_logstorage.c
    ${PROJECT_SOURCE_DIR}/src/offlinelogstorage/dlt_offline_logstorage_behavior.c
    )

set(FILTER_CONFIG dlt_message_filter.conf)
add_executable(dlt-daemon ${dlt_daemon_SRCS} ${systemd_SRCS})

set(RT_LIBRARY rt)
set(SOCKET_LIBRARY "")

target_link_libraries(dlt-daemon ${RT_LIBRARY} ${SOCKET_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS dlt-daemon
        RUNTIME DESTINATION bin
        PERMISSIONS
            OWNER_EXECUTE OWNER_WRITE OWNER_READ
            GROUP_EXECUTE GROUP_READ
            WORLD_EXECUTE WORLD_READ
        COMPONENT base)

configure_file(${CMAKE_SOURCE_DIR}/src/daemon/dlt.conf.in ${PROJECT_BINARY_DIR}/src/daemon/dlt.conf)

INSTALL(FILES ${PROJECT_BINARY_DIR}/src/daemon/dlt.conf ${FILTER_CONFIG}
	DESTINATION ${CONFIGURATION_FILES_DIR}
	COMPONENT base)

