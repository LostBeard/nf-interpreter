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

# SpawnWear: libpeer public include (peer.h) - PeerConnection.cpp needs it; libpeers ESP-IDF
# component includes stopped propagating to NF_NativeAssemblies after idf_component.yml was removed.
list(APPEND SpawnDev.WebRTC_INCLUDE_DIRS C:/Espressif/frameworks/esp-idf-v5.5.4/components/libpeer/include)


# source files
set(SpawnDev.WebRTC_SRCS

    SpawnDev_WebRTC.cpp


    SpawnDev_WebRTC_SpawnDev_WebRTC_NativeText_mshl.cpp
    SpawnDev_WebRTC_SpawnDev_WebRTC_NativeText.cpp
    SpawnDev_WebRTC_SpawnDev_WebRTC_PeerConnection_mshl.cpp
    SpawnDev_WebRTC_SpawnDev_WebRTC_PeerConnection.cpp

)

foreach(SRC_FILE ${SpawnDev.WebRTC_SRCS})

    # SpawnWear: unset the CACHE entry (not just a normal var) so find_file actually re-searches
    # each file every configure - otherwise the cached first result is reused for all files and
    # newly-added interop sources (e.g. NativeText) never get compiled -> undefined-reference link.
    unset(SpawnDev.WebRTC_SRC_FILE CACHE)

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
