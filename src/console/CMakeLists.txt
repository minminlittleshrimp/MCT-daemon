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

set(TARGET_LIST mct-log-reader mct-log-converter)

if(WITH_DLT_CONSOLE_SBTM)
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
