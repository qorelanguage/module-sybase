# FindFreeTDS.cmake
# Find FreeTDS library for Sybase/MS SQL Server connectivity
#
# This module defines:
#  FreeTDS_FOUND - System has FreeTDS
#  FreeTDS_INCLUDE_DIR - The FreeTDS include directory
#  FreeTDS_LIBS - The libraries needed to use FreeTDS

include(FindPackageHandleStandardArgs)

# Allow environment variables to override
if(DEFINED ENV{FreeTDS_INCLUDE_DIR})
    set(FreeTDS_INCLUDE_DIR $ENV{FreeTDS_INCLUDE_DIR})
endif()
if(DEFINED ENV{FreeTDS_LIBS})
    set(FreeTDS_LIBS $ENV{FreeTDS_LIBS})
endif()

# Search for include files
find_path(FreeTDS_INCLUDE_DIR ctpublic.h
    PATHS
        /usr/include
        /usr/include/freetds
        /usr/local/include
        /usr/local/include/freetds
        /opt/freetds/include
        /opt/local/include
        /opt/local/include/freetds
        /sw/include
        /sw/include/freetds
    PATH_SUFFIXES freetds
)

# Search for ct-lib
find_library(FreeTDS_CT_LIB
    NAMES ct
    PATHS
        /usr/lib
        /usr/lib64
        /usr/local/lib
        /usr/local/lib64
        /opt/freetds/lib
        /opt/freetds/lib64
        /opt/local/lib
        /sw/lib
    PATH_SUFFIXES freetds
)

# Search for sybdb
find_library(FreeTDS_SYBDB_LIB
    NAMES sybdb
    PATHS
        /usr/lib
        /usr/lib64
        /usr/local/lib
        /usr/local/lib64
        /opt/freetds/lib
        /opt/freetds/lib64
        /opt/local/lib
        /sw/lib
    PATH_SUFFIXES freetds
)

if(FreeTDS_CT_LIB AND FreeTDS_SYBDB_LIB)
    set(FreeTDS_LIBS ${FreeTDS_SYBDB_LIB} ${FreeTDS_CT_LIB})
endif()

find_package_handle_standard_args(FreeTDS
    FOUND_VAR FreeTDS_FOUND
    REQUIRED_VARS FreeTDS_INCLUDE_DIR FreeTDS_LIBS
)

if(FreeTDS_FOUND)
    message(STATUS "FreeTDS include dir: ${FreeTDS_INCLUDE_DIR}")
    message(STATUS "FreeTDS libraries: ${FreeTDS_LIBS}")
endif()
