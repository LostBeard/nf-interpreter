#
# Copyright (c) .NET Foundation and Contributors
# See LICENSE file in the project root for full license information.
#

########################################################################################
# make sure that a valid path is set bellow                                            #
# this is an Interop module so this file should be placed in the CMakes module folder  #
# usually CMake\Modules                                                                #
########################################################################################

# native code directory
set(BASE_PATH_FOR_THIS_MODULE ${PROJECT_SOURCE_DIR}/InteropAssemblies/SpawnDev.WebRTC)


# set include directories
list(APPEND SpawnDev.WebRTC_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/src/CLR/Core)
list(APPEND SpawnDev.WebRTC_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/src/CLR/Include)
list(APPEND SpawnDev.WebRTC_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/src/HAL/Include)
list(APPEND SpawnDev.WebRTC_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/src/PAL/Include)
list(APPEND SpawnDev.WebRTC_INCLUDE_DIRS ${BASE_PATH_FOR_THIS_MODULE})
# libpeer (sepfy) public headers (peer.h / peer_connection.h) - the registry component's
# public INCLUDE_DIRS is ./include. IDF_PATH is set for the whole ESP32 build.
list(APPEND SpawnDev.WebRTC_INCLUDE_DIRS $ENV{IDF_PATH}/components/libpeer/include)


# source files
set(SpawnDev.WebRTC_SRCS

    SpawnDev_WebRTC.cpp


    SpawnDev_WebRTC_SpawnDev_WebRTC_PeerConnection_mshl.cpp
    SpawnDev_WebRTC_SpawnDev_WebRTC_PeerConnection.cpp

)

foreach(SRC_FILE ${SpawnDev.WebRTC_SRCS})

    set(SpawnDev.WebRTC_SRC_FILE SRC_FILE-NOTFOUND)

    find_file(SpawnDev.WebRTC_SRC_FILE ${SRC_FILE}
        PATHS
	        ${BASE_PATH_FOR_THIS_MODULE}
	        ${TARGET_BASE_LOCATION}
            ${PROJECT_SOURCE_DIR}/src/SpawnDev.WebRTC

	    CMAKE_FIND_ROOT_PATH_BOTH
    )

    if (BUILD_VERBOSE)
        message("${SRC_FILE} >> ${SpawnDev.WebRTC_SRC_FILE}")
    endif()

    list(APPEND SpawnDev.WebRTC_SOURCES ${SpawnDev.WebRTC_SRC_FILE})

endforeach()

include(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(SpawnDev.WebRTC DEFAULT_MSG SpawnDev.WebRTC_INCLUDE_DIRS SpawnDev.WebRTC_SOURCES)
