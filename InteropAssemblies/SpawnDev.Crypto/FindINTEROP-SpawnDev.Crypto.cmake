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
set(BASE_PATH_FOR_THIS_MODULE ${PROJECT_SOURCE_DIR}/InteropAssemblies/SpawnDev.Crypto)


# set include directories
list(APPEND SpawnDev.Crypto_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/src/CLR/Core)
list(APPEND SpawnDev.Crypto_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/src/CLR/Include)
list(APPEND SpawnDev.Crypto_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/src/HAL/Include)
list(APPEND SpawnDev.Crypto_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/src/PAL/Include)
list(APPEND SpawnDev.Crypto_INCLUDE_DIRS ${BASE_PATH_FOR_THIS_MODULE})


# source files
set(SpawnDev.Crypto_SRCS

    SpawnDev_Crypto.cpp


    SpawnDev_Crypto_SpawnDev_Crypto_Ed25519_mshl.cpp
    SpawnDev_Crypto_SpawnDev_Crypto_Ed25519.cpp
    SpawnDev_Crypto_SpawnDev_Crypto_X25519_mshl.cpp
    SpawnDev_Crypto_SpawnDev_Crypto_X25519.cpp

)

foreach(SRC_FILE ${SpawnDev.Crypto_SRCS})

    set(SpawnDev.Crypto_SRC_FILE SRC_FILE-NOTFOUND)

    find_file(SpawnDev.Crypto_SRC_FILE ${SRC_FILE}
        PATHS
	        ${BASE_PATH_FOR_THIS_MODULE}
	        ${TARGET_BASE_LOCATION}
            ${PROJECT_SOURCE_DIR}/src/SpawnDev.Crypto

	    CMAKE_FIND_ROOT_PATH_BOTH
    )

    if (BUILD_VERBOSE)
        message("${SRC_FILE} >> ${SpawnDev.Crypto_SRC_FILE}")
    endif()

    list(APPEND SpawnDev.Crypto_SOURCES ${SpawnDev.Crypto_SRC_FILE})

endforeach()

include(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(SpawnDev.Crypto DEFAULT_MSG SpawnDev.Crypto_INCLUDE_DIRS SpawnDev.Crypto_SOURCES)
