# FindSybase.cmake
# Find native Sybase OCS (Open Client/Server) library
#
# This module defines:
#  Sybase_FOUND - System has Sybase OCS
#  Sybase_INCLUDE_DIR - The Sybase include directory
#  Sybase_LIBS - The libraries needed to use Sybase

include(FindPackageHandleStandardArgs)

# Allow environment variables to override
if(DEFINED ENV{Sybase_INCLUDE_DIR})
    set(Sybase_INCLUDE_DIR $ENV{Sybase_INCLUDE_DIR})
endif()
if(DEFINED ENV{Sybase_LIBS})
    set(Sybase_LIBS $ENV{Sybase_LIBS})
endif()
if(DEFINED ENV{SYBASE})
    set(SYBASE_DIR $ENV{SYBASE})
endif()
if(DEFINED ENV{SYBASE_OCS})
    set(SYBASE_OCS_DIR $ENV{SYBASE}/$ENV{SYBASE_OCS})
endif()

# Search for include files in typical Sybase locations
find_path(Sybase_INCLUDE_DIR ctpublic.h
    PATHS
        ${SYBASE_OCS_DIR}/include
        ${SYBASE_DIR}/include
        /usr/sybase/include
        /usr/local/sybase/include
        /opt/sybase/include
        /usr/sybase/OCS-*/include
        /opt/sybase/OCS-*/include
    PATH_SUFFIXES OCS-15_0 OCS-16_0
)

# Search for Sybase ct-lib
find_library(Sybase_CT_LIB
    NAMES sybct sybct64
    PATHS
        ${SYBASE_OCS_DIR}/lib
        ${SYBASE_DIR}/lib
        /usr/sybase/lib
        /usr/local/sybase/lib
        /opt/sybase/lib
        /usr/sybase/OCS-*/lib
        /opt/sybase/OCS-*/lib
    PATH_SUFFIXES OCS-15_0 OCS-16_0
)

# Search for Sybase cs-lib
find_library(Sybase_CS_LIB
    NAMES sybcs sybcs64
    PATHS
        ${SYBASE_OCS_DIR}/lib
        ${SYBASE_DIR}/lib
        /usr/sybase/lib
        /usr/local/sybase/lib
        /opt/sybase/lib
        /usr/sybase/OCS-*/lib
        /opt/sybase/OCS-*/lib
    PATH_SUFFIXES OCS-15_0 OCS-16_0
)

# Search for Sybase db-lib
find_library(Sybase_DB_LIB
    NAMES sybdb sybdb64
    PATHS
        ${SYBASE_OCS_DIR}/lib
        ${SYBASE_DIR}/lib
        /usr/sybase/lib
        /usr/local/sybase/lib
        /opt/sybase/lib
        /usr/sybase/OCS-*/lib
        /opt/sybase/OCS-*/lib
    PATH_SUFFIXES OCS-15_0 OCS-16_0
)

# Search for Sybase intl-lib
find_library(Sybase_INTL_LIB
    NAMES sybintl sybintl64
    PATHS
        ${SYBASE_OCS_DIR}/lib
        ${SYBASE_DIR}/lib
        /usr/sybase/lib
        /usr/local/sybase/lib
        /opt/sybase/lib
        /usr/sybase/OCS-*/lib
        /opt/sybase/OCS-*/lib
    PATH_SUFFIXES OCS-15_0 OCS-16_0
)

# Search for Sybase comn-lib
find_library(Sybase_COMN_LIB
    NAMES sybcomn sybcomn64
    PATHS
        ${SYBASE_OCS_DIR}/lib
        ${SYBASE_DIR}/lib
        /usr/sybase/lib
        /usr/local/sybase/lib
        /opt/sybase/lib
        /usr/sybase/OCS-*/lib
        /opt/sybase/OCS-*/lib
    PATH_SUFFIXES OCS-15_0 OCS-16_0
)

# Assemble libraries list
if(Sybase_CT_LIB AND Sybase_CS_LIB AND Sybase_DB_LIB)
    set(Sybase_LIBS
        ${Sybase_DB_LIB}
        ${Sybase_CS_LIB}
        ${Sybase_CT_LIB}
    )
    if(Sybase_INTL_LIB)
        list(APPEND Sybase_LIBS ${Sybase_INTL_LIB})
    endif()
    if(Sybase_COMN_LIB)
        list(APPEND Sybase_LIBS ${Sybase_COMN_LIB})
    endif()
endif()

find_package_handle_standard_args(Sybase
    FOUND_VAR Sybase_FOUND
    REQUIRED_VARS Sybase_INCLUDE_DIR Sybase_LIBS
)

if(Sybase_FOUND)
    message(STATUS "Sybase OCS include dir: ${Sybase_INCLUDE_DIR}")
    message(STATUS "Sybase OCS libraries: ${Sybase_LIBS}")
endif()
